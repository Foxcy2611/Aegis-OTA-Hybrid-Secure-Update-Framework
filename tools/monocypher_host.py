#!/usr/bin/env python3
"""Binding host tối thiểu cho Ed25519ph từ Monocypher 4.0.3 đã pin."""

from __future__ import annotations

import ctypes
import os
import pathlib
import shutil
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
VENDOR = ROOT / "libraries" / "Third_Party" / "Monocypher"
_LOADED_LIBRARY: ctypes.CDLL | None = None
_LIBRARY_DIRECTORY: tempfile.TemporaryDirectory[str] | None = None


def _build_library() -> pathlib.Path:
    global _LIBRARY_DIRECTORY

    sources = [
        VENDOR / "monocypher.c",
        VENDOR / "monocypher-ed25519.c",
    ]
    compiler = shutil.which("cc")
    if compiler is None:
        raise RuntimeError(
            "Không tìm thấy host C compiler 'cc' để nạp Monocypher"
        )

    # Không dùng tên cache đoán được trong /tmp: signer đang giữ private key,
    # nên chỉ nạp shared library vừa build trong private temporary directory.
    _LIBRARY_DIRECTORY = tempfile.TemporaryDirectory(
        prefix="aegis-monocypher-"
    )
    output = (
        pathlib.Path(_LIBRARY_DIRECTORY.name) / "monocypher_host.so"
    )
    subprocess.run(
        [
            compiler,
            "-O2",
            "-fPIC",
            "-shared",
            *(str(source) for source in sources),
            "-I",
            str(VENDOR),
            "-o",
            str(output),
        ],
        check=True,
    )
    return output


def _library() -> ctypes.CDLL:
    global _LOADED_LIBRARY

    if(_LOADED_LIBRARY is not None):
        return _LOADED_LIBRARY

    library = ctypes.CDLL(str(_build_library()))
    byte_pointer = ctypes.POINTER(ctypes.c_ubyte)

    library.crypto_ed25519_key_pair.argtypes = [
        byte_pointer,
        byte_pointer,
        byte_pointer,
    ]
    library.crypto_ed25519_key_pair.restype = None
    library.crypto_ed25519_ph_sign.argtypes = [
        byte_pointer,
        byte_pointer,
        byte_pointer,
    ]
    library.crypto_ed25519_ph_sign.restype = None
    library.crypto_ed25519_ph_check.argtypes = [
        byte_pointer,
        byte_pointer,
        byte_pointer,
    ]
    library.crypto_ed25519_ph_check.restype = ctypes.c_int
    library.crypto_wipe.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    library.crypto_wipe.restype = None

    _LOADED_LIBRARY = library
    return library


def _array(data: bytes):
    return (ctypes.c_ubyte * len(data)).from_buffer_copy(data)


def ph_sign(message_hash: bytes, seed: bytes) -> tuple[bytes, bytes]:
    """Ký prehash 64 byte; xóa seed/secret C buffer trước khi trả về."""
    if(len(message_hash) != 64):
        raise ValueError("Prehash Ed25519ph phải đúng 64 byte")
    if(len(seed) != 32):
        raise ValueError("Seed Ed25519 phải đúng 32 byte")

    library = _library()
    mutable_seed = _array(seed)
    secret_key = (ctypes.c_ubyte * 64)()
    public_key = (ctypes.c_ubyte * 32)()
    signature = (ctypes.c_ubyte * 64)()
    prehash = _array(message_hash)

    try:
        library.crypto_ed25519_key_pair(
            secret_key,
            public_key,
            mutable_seed,
        )
        library.crypto_ed25519_ph_sign(
            signature,
            secret_key,
            prehash,
        )
        return bytes(signature), bytes(public_key)
    finally:
        library.crypto_wipe(mutable_seed, ctypes.sizeof(mutable_seed))
        library.crypto_wipe(secret_key, ctypes.sizeof(secret_key))
        library.crypto_wipe(prehash, ctypes.sizeof(prehash))


def ph_check(
    message_hash: bytes,
    signature: bytes,
    public_key: bytes,
) -> bool:
    if(len(message_hash) != 64 or
       len(signature) != 64 or
       len(public_key) != 32):
        return False

    return _library().crypto_ed25519_ph_check(
        _array(signature),
        _array(public_key),
        _array(message_hash),
    ) == 0


def read_private_seed(path: pathlib.Path) -> bytes:
    """Đọc seed raw/hex và từ chối file có quyền group/other trên POSIX."""
    file_mode = path.stat().st_mode
    if(os.name == "posix" and (file_mode & 0o077) != 0):
        raise ValueError(
            "File private key phải chỉ owner được đọc/ghi "
            "(ví dụ: chmod 600 key_file)"
        )

    raw = path.read_bytes()
    if(len(raw) == 32):
        return raw

    try:
        decoded = bytes.fromhex(raw.decode("ascii").strip())
    except (UnicodeDecodeError, ValueError) as error:
        raise ValueError(
            f"{path} không phải seed Ed25519 raw/hex hợp lệ"
        ) from error

    if(len(decoded) != 32):
        raise ValueError("Private signing seed phải chứa đúng 32 byte")

    return decoded


def read_public_key(path: pathlib.Path) -> bytes:
    """Đọc public key Ed25519 dạng raw 32 byte hoặc 64 ký tự hex."""
    raw = path.read_bytes()
    if(len(raw) == 32):
        return raw

    try:
        decoded = bytes.fromhex(raw.decode("ascii").strip())
    except (UnicodeDecodeError, ValueError) as error:
        raise ValueError(
            f"{path} không phải public key Ed25519 raw/hex hợp lệ"
        ) from error

    if(len(decoded) != 32):
        raise ValueError("Public key Ed25519 phải chứa đúng 32 byte")
    return decoded
