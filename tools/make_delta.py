#!/usr/bin/env python3
"""Tạo Aegis Delta COPY/ADD streaming từ base BIN và signed FULL target."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import struct

from monocypher_host import ph_check, ph_sign, read_private_seed


DELTA_DOMAIN = b"AEGIS-F103-DELTA-V1\0"
DELTA_FORMAT_VERSION = 1
DELTA_ALGORITHM_COPY_ADD_V1 = 1
APPLICATION_SIZE = 46 * 1024
SIGNED_HEADER_SIZE = 32
SIGNATURE_SIZE = 64
FIRMWARE_DOMAIN = b"AEGIS-F103-FW-V1"
FIRMWARE_FORMAT_VERSION = 1
OP_COPY = 0x01
OP_ADD = 0x02
OP_END = 0xFF
MAX_RECORD_LENGTH = 0xFFFF


def _records(opcode: int, offset: int, data: bytes | int) -> bytes:
    output = bytearray()
    if(opcode == OP_COPY):
        length = int(data)
        while(length):
            count = min(length, MAX_RECORD_LENGTH)
            output += struct.pack("<BIH", OP_COPY, offset, count)
            offset += count
            length -= count
    else:
        raw = bytes(data)
        while(raw):
            part, raw = raw[:MAX_RECORD_LENGTH], raw[MAX_RECORD_LENGTH:]
            output += struct.pack("<BH", OP_ADD, len(part)) + part
    return bytes(output)


def make_patch(base: bytes, target: bytes) -> bytes:
    """Format đơn giản: COPY prefix/suffix giống nhau, ADD phần giữa."""
    prefix = 0
    common_limit = min(len(base), len(target))
    while(prefix < common_limit and base[prefix] == target[prefix]):
        prefix += 1

    suffix = 0
    while(suffix < (common_limit - prefix) and
          base[len(base) - 1 - suffix] == target[len(target) - 1 - suffix]):
        suffix += 1

    patch = bytearray()
    if(prefix):
        patch += _records(OP_COPY, 0, prefix)
    middle = target[prefix:len(target) - suffix if suffix else len(target)]
    if(middle):
        patch += _records(OP_ADD, 0, middle)
    if(suffix):
        patch += _records(OP_COPY, len(base) - suffix, suffix)
    patch.append(OP_END)
    return bytes(patch)


def apply_patch(base: bytes, patch: bytes) -> bytes:
    output = bytearray()
    cursor = 0
    while(cursor < len(patch)):
        opcode = patch[cursor]
        cursor += 1
        if(opcode == OP_END):
            if(cursor != len(patch)):
                raise ValueError("Có byte thừa sau END")
            return bytes(output)
        if(opcode == OP_COPY):
            if(cursor + 6 > len(patch)):
                raise ValueError("COPY bị cắt ngắn")
            offset, length = struct.unpack_from("<IH", patch, cursor)
            cursor += 6
            if(length == 0 or offset + length > len(base)):
                raise ValueError("COPY vượt base")
            output += base[offset:offset + length]
        elif(opcode == OP_ADD):
            if(cursor + 2 > len(patch)):
                raise ValueError("ADD bị cắt ngắn")
            (length,) = struct.unpack_from("<H", patch, cursor)
            cursor += 2
            if(length == 0 or cursor + length > len(patch)):
                raise ValueError("ADD vượt patch")
            output += patch[cursor:cursor + length]
            cursor += length
        else:
            raise ValueError(f"Opcode không hỗ trợ: 0x{opcode:02X}")
    raise ValueError("Patch thiếu END")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, type=pathlib.Path)
    parser.add_argument("--base-version", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--target-artifact", required=True, type=pathlib.Path)
    parser.add_argument("--private-key", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    arguments = parser.parse_args()

    try:
        base = arguments.base.read_bytes()
        full = arguments.target_artifact.read_bytes()
        if(not base or len(base) > APPLICATION_SIZE):
            raise ValueError("Base phải nằm trong 1..46 KB")
        if(len(full) < SIGNED_HEADER_SIZE + 1 + SIGNATURE_SIZE):
            raise ValueError("FULL artifact quá ngắn")
        target_header = full[:SIGNED_HEADER_SIZE]
        if(target_header[:16] != FIRMWARE_DOMAIN or
           struct.unpack_from("<I", target_header, 16)[0] !=
               FIRMWARE_FORMAT_VERSION):
            raise ValueError("FULL target sai domain/format")
        target_size = struct.unpack_from("<I", target_header, 20)[0]
        expected_size = SIGNED_HEADER_SIZE + target_size + SIGNATURE_SIZE
        if(target_size == 0 or target_size > APPLICATION_SIZE or
           len(full) != expected_size):
            raise ValueError("FULL artifact sai image_size")
        target = full[SIGNED_HEADER_SIZE:SIGNED_HEADER_SIZE + target_size]
        target_signature = full[-SIGNATURE_SIZE:]
        patch = make_patch(base, target)
        if(apply_patch(base, patch) != target):
            raise RuntimeError("Self-apply Delta không tạo đúng target")

        base_hash = hashlib.sha512(base).digest()
        target_hash = hashlib.sha512(target).digest()
        patch_hash = hashlib.sha512(patch).digest()
        artifact_id = hashlib.sha512(
            base_hash + target_hash + patch_hash
        ).digest()[:8]
        base_version = arguments.base_version
        if(not 0 < base_version <= 0xFFFFFFFF):
            raise ValueError("Base version phải nằm trong 1..0xFFFFFFFF")
        target_version = struct.unpack_from("<I", target_header, 24)[0]
        if(target_version <= base_version):
            raise ValueError(
                "Target version phải lớn hơn base version để chống downgrade"
            )
        body = struct.pack(
            "<20sII8sII64s32s64s64sI64s",
            DELTA_DOMAIN,
            DELTA_FORMAT_VERSION,
            DELTA_ALGORITHM_COPY_ADD_V1,
            artifact_id,
            base_version,
            len(base),
            base_hash,
            target_header,
            target_hash,
            target_signature,
            len(patch),
            patch_hash,
        )
        if(len(body) != 336):
            raise RuntimeError("Delta immutable body không đúng 336 byte")
        seed = read_private_seed(arguments.private_key)
        signature, public_key = ph_sign(hashlib.sha512(body).digest(), seed)
        if(not ph_check(hashlib.sha512(body).digest(), signature, public_key)):
            raise RuntimeError("Self-check chữ ký Delta thất bại")
        if(not ph_check(
            hashlib.sha512(target_header + target).digest(),
            target_signature,
            public_key,
        )):
            raise ValueError(
                "Target FULL không được ký bởi cùng release key"
            )

        output = body + signature + patch
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_bytes(output)
        print(
            f"DELTA: {arguments.output} ({len(patch)} byte patch, "
            f"{len(output)} byte package, target version {target_version})"
        )
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(2, f"make_delta: error: {error}\n")


if(__name__ == "__main__"):
    raise SystemExit(main())
