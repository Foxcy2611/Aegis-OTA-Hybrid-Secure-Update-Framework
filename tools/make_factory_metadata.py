#!/usr/bin/env python3
"""Tạo hai page Metadata factory từ FULL artifact đã ký."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import pathlib
import struct

from monocypher_host import ph_check, read_public_key


DOMAIN = b"AEGIS-F103-FW-V1"
FORMAT_VERSION = 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True, type=pathlib.Path)
    parser.add_argument("--public-key", required=True, type=pathlib.Path)
    parser.add_argument("--output-a", required=True, type=pathlib.Path)
    parser.add_argument("--output-b", required=True, type=pathlib.Path)
    arguments = parser.parse_args()
    try:
        artifact = arguments.artifact.read_bytes()
        if(len(artifact) < 97):
            raise ValueError("FULL artifact quá ngắn")
        header = artifact[:32]
        if(header[:16] != DOMAIN or
           struct.unpack_from("<I", header, 16)[0] != FORMAT_VERSION):
            raise ValueError("FULL artifact sai domain/format")
        image_size = struct.unpack_from("<I", header, 20)[0]
        if(image_size == 0 or image_size > 46 * 1024 or
           len(artifact) != 32 + image_size + 64):
            raise ValueError("FULL artifact sai image_size")
        image = artifact[32:32 + image_size]
        image_crc32 = struct.unpack_from("<I", header, 28)[0]
        if((binascii.crc32(image) & 0xFFFFFFFF) != image_crc32):
            raise ValueError("FULL artifact sai image CRC32")
        signature = artifact[-64:]
        public_key = read_public_key(arguments.public_key)
        if(not ph_check(
            hashlib.sha512(header + image).digest(),
            signature,
            public_key,
        )):
            raise ValueError("FULL artifact sai chữ ký Ed25519ph")
        body = struct.pack(
            "<IIIBBBBQ32s64s",
            0x41454749, 2, 1,
            0, 0, 0, 0,
            0,
            header,
            signature,
        )
        if(len(body) != 120):
            raise RuntimeError("Metadata body không đúng 120 byte")
        record = body + struct.pack(
            "<II", binascii.crc32(body) & 0xFFFFFFFF, 0x434D4954
        )
        page_a = record + b"\xFF" * (1024 - len(record))
        page_b = b"\xFF" * 1024
        arguments.output_a.parent.mkdir(parents=True, exist_ok=True)
        arguments.output_b.parent.mkdir(parents=True, exist_ok=True)
        arguments.output_a.write_bytes(page_a)
        arguments.output_b.write_bytes(page_b)
        print(f"Metadata A: {arguments.output_a}")
        print(f"Metadata B: {arguments.output_b}")
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(2, f"make_factory_metadata: error: {error}\n")


if(__name__ == "__main__"):
    raise SystemExit(main())
