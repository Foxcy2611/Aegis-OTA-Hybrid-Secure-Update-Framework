#!/usr/bin/env python3
"""Tạo FULL artifact: SignedHeader[32] || firmware || signature[64]."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import os
import pathlib
import struct
import tempfile

from monocypher_host import ph_check, ph_sign, read_private_seed


DOMAIN = b"AEGIS-F103-FW-V1"
FORMAT_VERSION = 1
APPLICATION_SIZE = 46 * 1024
SIGNED_HEADER_SIZE = 32
SIGNATURE_SIZE = 64


def make_header(image: bytes, version: int) -> bytes:
    if(not image or len(image) > APPLICATION_SIZE):
        raise ValueError("Firmware phải nằm trong khoảng 1..46 KB")
    if(not 0 < version <= 0xFFFFFFFF):
        raise ValueError("Version phải nằm trong khoảng 1..0xFFFFFFFF")

    header = struct.pack(
        "<16sIIII",
        DOMAIN,
        FORMAT_VERSION,
        len(image),
        version,
        binascii.crc32(image) & 0xFFFFFFFF,
    )
    if(len(header) != SIGNED_HEADER_SIZE):
        raise RuntimeError("Lỗi nội bộ: SignedHeader không còn đúng 32 byte")

    return header


def _atomic_write(path: pathlib.Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_name: str | None = None

    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=path.name + ".",
            dir=path.parent,
            delete=False,
        ) as temporary_file:
            temporary_name = temporary_file.name
            temporary_file.write(data)
            temporary_file.flush()
            os.fsync(temporary_file.fileno())

        os.replace(temporary_name, path)
        temporary_name = None
    finally:
        if(temporary_name is not None):
            pathlib.Path(temporary_name).unlink(missing_ok=True)


def _parse_version(value: str) -> int:
    try:
        version = int(value, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "version phải là số decimal hoặc 0x..."
        ) from error

    if(not 0 < version <= 0xFFFFFFFF):
        raise argparse.ArgumentTypeError(
            "version phải nằm trong khoảng 1..0xFFFFFFFF"
        )

    return version


def _ensure_distinct_paths(
    image_path: pathlib.Path,
    key_path: pathlib.Path,
    output_path: pathlib.Path,
    public_key_path: pathlib.Path | None,
) -> None:
    image = image_path.resolve()
    key = key_path.resolve()
    output = output_path.resolve()

    if(output == image or output == key):
        raise ValueError("Output không được ghi đè firmware hoặc private key")

    if(public_key_path is not None):
        public_key = public_key_path.resolve()
        if(public_key in (image, key, output)):
            raise ValueError(
                "Public-key output phải là một file riêng biệt"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", required=True, type=pathlib.Path)
    parser.add_argument("--version", required=True, type=_parse_version)
    parser.add_argument(
        "--private-key",
        required=True,
        type=pathlib.Path,
        help="Seed Ed25519 32 byte dạng raw hoặc 64 ký tự hex",
    )
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--public-key-output", type=pathlib.Path)
    arguments = parser.parse_args()

    try:
        if(arguments.output.suffix != ".aegisfw"):
            raise ValueError("FULL artifact output phải có đuôi .aegisfw")

        _ensure_distinct_paths(
            arguments.bin,
            arguments.private_key,
            arguments.output,
            arguments.public_key_output,
        )

        image = arguments.bin.read_bytes()
        header = make_header(image, arguments.version)
        seed = read_private_seed(arguments.private_key)
        prehash = hashlib.sha512(header + image).digest()
        signature, public_key = ph_sign(prehash, seed)

        if(not ph_check(prehash, signature, public_key)):
            raise RuntimeError(
                "Self-check Ed25519ph thất bại; không xuất artifact"
            )

        artifact = header + image + signature
        _atomic_write(arguments.output, artifact)
        if(arguments.public_key_output is not None):
            _atomic_write(arguments.public_key_output, public_key)

        print(
            f"FULL: {arguments.output} "
            f"({len(image)} byte image, version {arguments.version})"
        )
        print(
            "Public key SHA-256: "
            + hashlib.sha256(public_key).hexdigest()
        )
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(2, f"sign_firmware: error: {error}\n")


if(__name__ == "__main__"):
    raise SystemExit(main())
