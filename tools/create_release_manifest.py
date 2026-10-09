#!/usr/bin/env python3
"""Tạo JSON evidence gồm size/SHA-512 artifact và phiên bản toolchain."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import platform
import shutil
import subprocess


def _version(command: list[str]) -> str:
    executable = shutil.which(command[0])
    if(executable is None):
        return "not-found"
    result = subprocess.run(
        [executable, *command[1:]],
        check=False,
        capture_output=True,
        text=True,
    )
    text = (result.stdout or result.stderr).strip().splitlines()
    return text[0] if text else f"exit-{result.returncode}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-version", required=True)
    parser.add_argument(
        "--artifact",
        action="append",
        required=True,
        type=pathlib.Path,
        help="Lặp lại option này cho mỗi ELF/BIN/MAP/FULL/Delta cần lưu evidence",
    )
    parser.add_argument("--output", required=True, type=pathlib.Path)
    arguments = parser.parse_args()

    try:
        if(arguments.output in arguments.artifact):
            raise ValueError("Output manifest không được trùng artifact input")
        artifacts = []
        names: set[str] = set()
        for path in sorted(arguments.artifact, key=lambda item: item.name):
            if(path.name in names):
                raise ValueError(f"Trùng tên artifact trong evidence: {path.name}")
            data = path.read_bytes()
            if(not data):
                raise ValueError(f"Artifact rỗng: {path}")
            names.add(path.name)
            artifacts.append({
                "name": path.name,
                "size_bytes": len(data),
                "sha512": hashlib.sha512(data).hexdigest(),
            })

        manifest = {
            "schema_version": 1,
            "release_version": arguments.release_version,
            "artifacts": artifacts,
            "toolchain": {
                "host": platform.platform(),
                "python": platform.python_version(),
                "cmake": _version(["cmake", "--version"]),
                "ninja": _version(["ninja", "--version"]),
                "arm_gcc": _version(["arm-none-eabi-gcc", "--version"]),
                "openocd": _version(["openocd", "--version"]),
            },
        }
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"Release evidence: {arguments.output}")
        return 0
    except (OSError, ValueError) as error:
        parser.exit(2, f"create_release_manifest: error: {error}\n")


if(__name__ == "__main__"):
    raise SystemExit(main())
