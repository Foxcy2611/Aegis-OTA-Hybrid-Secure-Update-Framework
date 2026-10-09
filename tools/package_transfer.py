#!/usr/bin/env python3
"""Đóng .aegisfw/.aegisdelta thành manifest + data để ESP32 OTA sender dùng."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import pathlib
import struct


CHUNK_SIZE = 224
STAGING_MANIFEST_SIZE = 404
DELTA_MANIFEST_SIZE = 656
FIRMWARE_DOMAIN = b"AEGIS-F103-FW-V1"
DELTA_DOMAIN = b"AEGIS-F103-DELTA-V1\0"


def _id(value: str) -> int:
    result = int(value, 0)
    if(not 0 < result <= 0xFFFFFFFFFFFFFFFF):
        raise argparse.ArgumentTypeError("ID phải nằm trong 1..2^64-1")
    return result


def _full(artifact: bytes, operation_id: int, transfer_id: int) -> tuple[bytes, bytes]:
    if(len(artifact) < 97):
        raise ValueError("FULL artifact quá ngắn")
    header = artifact[:32]
    image_size = struct.unpack_from("<I", header, 20)[0]
    if(header[:16] != FIRMWARE_DOMAIN or
       struct.unpack_from("<I", header, 16)[0] != 1 or
       image_size == 0 or image_size > 46 * 1024 or
       len(artifact) != 32 + image_size + 64):
        raise ValueError("FULL artifact sai image_size")
    image = artifact[32:32 + image_size]
    if((binascii.crc32(image) & 0xFFFFFFFF) !=
       struct.unpack_from("<I", header, 28)[0]):
        raise ValueError("FULL artifact sai image CRC32")
    signature = artifact[-64:]
    artifact_id = hashlib.sha512(artifact).digest()[:8]
    fragment_count = (image_size + CHUNK_SIZE - 1) // CHUNK_SIZE
    prefix = struct.pack(
        "<IIBBHQQ8s32s64sIHH",
        0x53544731, 1, 1, 0, CHUNK_SIZE,
        operation_id, transfer_id, artifact_id,
        header, signature, image_size, fragment_count, 0,
    )
    if(len(prefix) != 140):
        raise RuntimeError("Staging prefix không đúng 140 byte")
    manifest = (
        prefix
        + struct.pack("<II", binascii.crc32(prefix) & 0xFFFFFFFF, 0x48445231)
        + b"\xFF" * 108
        + b"\xFF" * 144
        + struct.pack("<I", 0xFFFFFFFF)
    )
    if(len(manifest) != STAGING_MANIFEST_SIZE):
        raise RuntimeError("Staging manifest không đúng 404 byte")
    return manifest, image


def _delta(package: bytes, operation_id: int, transfer_id: int) -> tuple[bytes, bytes]:
    if(len(package) <= 400):
        raise ValueError("Delta package quá ngắn")
    immutable_and_signature = package[:400]
    patch = package[400:]
    if(package[:20] != DELTA_DOMAIN or
       struct.unpack_from("<I", package, 20)[0] != 1 or
       struct.unpack_from("<I", package, 24)[0] != 1):
        raise ValueError("Delta package sai domain/format/algorithm")
    declared_patch_size = struct.unpack_from("<I", package, 268)[0]
    if(declared_patch_size != len(patch)):
        raise ValueError("Delta package sai patch_size")
    if(hashlib.sha512(patch).digest() != package[272:336]):
        raise ValueError("Delta package sai patch SHA-512")
    fragment_count = (len(patch) + CHUNK_SIZE - 1) // CHUNK_SIZE
    control = struct.pack(
        "<QQHH",
        operation_id,
        transfer_id,
        CHUNK_SIZE,
        fragment_count,
    )
    crc_region = immutable_and_signature + control
    manifest = (
        crc_region
        + struct.pack("<I", binascii.crc32(crc_region) & 0xFFFFFFFF)
        + struct.pack("<III", 0x44484431, 0xFFFFFFFF, 0xFFFFFFFF)
        + b"\xFF" * 76
        + b"\xFF" * 144
    )
    if(len(manifest) != DELTA_MANIFEST_SIZE):
        raise RuntimeError("Delta manifest không đúng 656 byte")
    return manifest, patch


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True, type=pathlib.Path)
    parser.add_argument("--operation-id", required=True, type=_id)
    parser.add_argument("--transfer-id", required=True, type=_id)
    parser.add_argument("--output-prefix", required=True, type=pathlib.Path)
    arguments = parser.parse_args()
    try:
        artifact = arguments.artifact.read_bytes()
        if(arguments.artifact.suffix == ".aegisfw"):
            manifest, data = _full(
                artifact, arguments.operation_id, arguments.transfer_id
            )
        elif(arguments.artifact.suffix == ".aegisdelta"):
            manifest, data = _delta(
                artifact, arguments.operation_id, arguments.transfer_id
            )
        else:
            raise ValueError("Artifact phải có đuôi .aegisfw hoặc .aegisdelta")
        manifest_path = arguments.output_prefix.with_suffix(".manifest.bin")
        data_path = arguments.output_prefix.with_suffix(".data.bin")
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_bytes(manifest)
        data_path.write_bytes(data)
        print(f"Manifest: {manifest_path} ({len(manifest)} byte)")
        print(f"Data: {data_path} ({len(data)} byte)")
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(2, f"package_transfer: error: {error}\n")


if(__name__ == "__main__"):
    raise SystemExit(main())
