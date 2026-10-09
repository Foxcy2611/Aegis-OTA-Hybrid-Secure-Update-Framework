#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import json
import subprocess
import sys
import tempfile


def main() -> int:
    root = pathlib.Path(sys.argv[1])
    sys.path.insert(0, str(root / "tools"))
    import make_delta

    with tempfile.TemporaryDirectory(prefix="aegis-tools-") as directory:
        work = pathlib.Path(directory)
        base = bytes((index * 7) & 0xFF for index in range(4096))
        target = bytearray(base)
        target[100:112] = b"AEGIS-DELTA!"
        base_path = work / "base.bin"
        target_path = work / "target.bin"
        key_path = work / "test.seed"
        full_path = work / "target.aegisfw"
        public_key_path = work / "release_public_key.bin"
        delta_path = work / "target.aegisdelta"
        transfer_prefix = work / "full-transfer"
        delta_transfer_prefix = work / "delta-transfer"
        metadata_a = work / "metadata-a.bin"
        metadata_b = work / "metadata-b.bin"
        release_manifest = work / "release-evidence.json"
        base_path.write_bytes(base)
        target_path.write_bytes(target)
        key_path.write_bytes(bytes(range(32)))
        key_path.chmod(0o600)

        subprocess.run([
            sys.executable, str(root / "tools" / "sign_firmware.py"),
            "--bin", str(target_path), "--version", "2",
            "--private-key", str(key_path), "--output", str(full_path),
            "--public-key-output", str(public_key_path),
        ], check=True)
        subprocess.run([
            sys.executable, str(root / "tools" / "make_delta.py"),
            "--base", str(base_path), "--base-version", "1",
            "--target-artifact", str(full_path),
            "--private-key", str(key_path), "--output", str(delta_path),
        ], check=True)
        package = delta_path.read_bytes()
        if(len(package) <= 400):
            raise RuntimeError("Delta package không chứa patch")
        reconstructed = make_delta.apply_patch(base, package[400:])
        if(reconstructed != bytes(target)):
            raise RuntimeError("Delta tool reconstruct sai target")
        subprocess.run([
            sys.executable, str(root / "tools" / "package_transfer.py"),
            "--artifact", str(full_path),
            "--operation-id", "1", "--transfer-id", "2",
            "--output-prefix", str(transfer_prefix),
        ], check=True)
        if(transfer_prefix.with_suffix(".manifest.bin").stat().st_size != 404):
            raise RuntimeError("Staging manifest sai kích thước")
        subprocess.run([
            sys.executable, str(root / "tools" / "package_transfer.py"),
            "--artifact", str(delta_path),
            "--operation-id", "3", "--transfer-id", "4",
            "--output-prefix", str(delta_transfer_prefix),
        ], check=True)
        if(delta_transfer_prefix.with_suffix(".manifest.bin").stat().st_size != 656):
            raise RuntimeError("Delta manifest sai kích thước")
        subprocess.run([
            sys.executable, str(root / "tools" / "make_factory_metadata.py"),
            "--artifact", str(full_path),
            "--public-key", str(public_key_path),
            "--output-a", str(metadata_a), "--output-b", str(metadata_b),
        ], check=True)
        if(metadata_a.stat().st_size != 1024 or metadata_b.stat().st_size != 1024):
            raise RuntimeError("Factory metadata sai kích thước page")
        subprocess.run([
            sys.executable,
            str(root / "tools" / "create_release_manifest.py"),
            "--release-version", "test-v2",
            "--artifact", str(full_path),
            "--artifact", str(delta_path),
            "--output", str(release_manifest),
        ], check=True)
        evidence = json.loads(release_manifest.read_text(encoding="utf-8"))
        if(len(evidence["artifacts"]) != 2 or
           any(len(item["sha512"]) != 128 for item in evidence["artifacts"])):
            raise RuntimeError("Release evidence sai artifact/hash")
    return 0


if(__name__ == "__main__"):
    raise SystemExit(main())
