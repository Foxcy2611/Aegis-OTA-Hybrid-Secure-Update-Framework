#!/usr/bin/env python3
"""Kiểm tra Python signer tạo artifact mà production C verifier chấp nhận."""

from __future__ import annotations

import argparse
import pathlib
import struct
import subprocess
import sys
import tempfile


DOMAIN = b"AEGIS-F103-FW-V1"
HEADER_SIZE = 32
SIGNATURE_SIZE = 64
APPLICATION_SIZE = 46 * 1024


def run(command: list[str], expect_success: bool) -> subprocess.CompletedProcess:
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
    )
    if(expect_success and result.returncode != 0):
        raise RuntimeError(
            "Command phải PASS nhưng đã fail:\n"
            + " ".join(command)
            + "\nstdout:\n"
            + result.stdout
            + "\nstderr:\n"
            + result.stderr
        )
    if(not expect_success and result.returncode == 0):
        raise RuntimeError(
            "Command phải bị từ chối nhưng lại PASS:\n"
            + " ".join(command)
        )

    return result


def signer_command(
    signer: pathlib.Path,
    image: pathlib.Path,
    key: pathlib.Path,
    output: pathlib.Path,
    public_key: pathlib.Path,
) -> list[str]:
    return [
        sys.executable,
        str(signer),
        "--bin",
        str(image),
        "--version",
        "0x01020304",
        "--private-key",
        str(key),
        "--output",
        str(output),
        "--public-key-output",
        str(public_key),
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--signer", required=True, type=pathlib.Path)
    parser.add_argument("--verifier", required=True, type=pathlib.Path)
    arguments = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="aegis-phase3-") as temp_name:
        temp = pathlib.Path(temp_name)
        image = temp / "application.bin"
        key = temp / "test_seed.hex"
        artifact_a = temp / "application_a.aegisfw"
        artifact_b = temp / "application_b.aegisfw"
        public_a = temp / "public_a.bin"
        public_b = temp / "public_b.bin"

        image_bytes = bytes(
            ((index * 37) + 11) & 0xFF
            for index in range(513)
        )
        image.write_bytes(image_bytes)
        key.write_text(bytes(range(32)).hex() + "\n", encoding="ascii")
        key.chmod(0o600)

        run(
            signer_command(
                arguments.signer,
                image,
                key,
                artifact_a,
                public_a,
            ),
            True,
        )
        run(
            signer_command(
                arguments.signer,
                image,
                key,
                artifact_b,
                public_b,
            ),
            True,
        )

        first_artifact = artifact_a.read_bytes()
        second_artifact = artifact_b.read_bytes()
        if(first_artifact != second_artifact):
            raise RuntimeError("Signer không deterministic với cùng input")
        if(public_a.read_bytes() != public_b.read_bytes()):
            raise RuntimeError("Public key output không deterministic")
        if(len(first_artifact) != HEADER_SIZE + len(image_bytes) + SIGNATURE_SIZE):
            raise RuntimeError("Sai kích thước FULL artifact")

        domain, format_version, image_size, image_version, _ = struct.unpack(
            "<16sIIII",
            first_artifact[:HEADER_SIZE],
        )
        if(domain != DOMAIN or
           format_version != 1 or
           image_size != len(image_bytes) or
           image_version != 0x01020304):
            raise RuntimeError("SignedHeader Python sai byte contract")

        run(
            [
                str(arguments.verifier),
                str(artifact_a),
                str(public_a),
            ],
            True,
        )

        tampered = bytearray(first_artifact)
        tampered[HEADER_SIZE + 100] ^= 0x01
        tampered_path = temp / "tampered.aegisfw"
        tampered_path.write_bytes(tampered)
        run(
            [
                str(arguments.verifier),
                str(tampered_path),
                str(public_a),
            ],
            False,
        )

        empty_image = temp / "empty.bin"
        empty_image.write_bytes(b"")
        run(
            signer_command(
                arguments.signer,
                empty_image,
                key,
                temp / "empty.aegisfw",
                temp / "empty_public.bin",
            ),
            False,
        )

        oversized_image = temp / "oversized.bin"
        oversized_image.write_bytes(b"\xA5" * (APPLICATION_SIZE + 1))
        run(
            signer_command(
                arguments.signer,
                oversized_image,
                key,
                temp / "oversized.aegisfw",
                temp / "oversized_public.bin",
            ),
            False,
        )

        key.chmod(0o644)
        run(
            signer_command(
                arguments.signer,
                image,
                key,
                temp / "unsafe_key.aegisfw",
                temp / "unsafe_public.bin",
            ),
            False,
        )

    print("PASS Python signer -> C verifier, deterministic và negative cases")
    return 0


if(__name__ == "__main__"):
    raise SystemExit(main())
