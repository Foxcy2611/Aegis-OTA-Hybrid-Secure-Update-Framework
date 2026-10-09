#!/usr/bin/env python3
"""Verify input và nạp trusted factory bundle bằng OpenOCD/ST-Link."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess


def _tcl(path: pathlib.Path) -> str:
    return "{" + str(path.resolve()).replace("}", "\\}") + "}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bootloader", required=True, type=pathlib.Path)
    parser.add_argument("--application", required=True, type=pathlib.Path)
    parser.add_argument("--metadata-a", required=True, type=pathlib.Path)
    parser.add_argument("--metadata-b", required=True, type=pathlib.Path)
    parser.add_argument("--execute", action="store_true")
    arguments = parser.parse_args()
    try:
        expected = (
            (arguments.bootloader, 1, 16 * 1024),
            (arguments.application, 1, 46 * 1024),
            (arguments.metadata_a, 1024, 1024),
            (arguments.metadata_b, 1024, 1024),
        )
        for path, minimum, maximum in expected:
            size = path.stat().st_size
            if(size < minimum or size > maximum):
                raise ValueError(
                    f"{path} có size {size}, cần nằm trong "
                    f"{minimum}..{maximum} byte"
                )
        openocd = shutil.which("openocd")
        if(openocd is None):
            raise RuntimeError("Không tìm thấy openocd")
        commands = [
            f"flash write_image erase {_tcl(arguments.bootloader)} 0x08000000 bin",
            f"flash write_image erase {_tcl(arguments.application)} 0x08004000 bin",
            f"flash write_image erase {_tcl(arguments.metadata_a)} 0x0800F800 bin",
            f"flash write_image erase {_tcl(arguments.metadata_b)} 0x0800FC00 bin",
            f"verify_image {_tcl(arguments.bootloader)} 0x08000000 bin",
            f"verify_image {_tcl(arguments.application)} 0x08004000 bin",
            f"verify_image {_tcl(arguments.metadata_a)} 0x0800F800 bin",
            f"verify_image {_tcl(arguments.metadata_b)} 0x0800FC00 bin",
            "reset run",
            "shutdown",
        ]
        command = [
            openocd,
            "-f", "interface/stlink.cfg",
            "-f", "target/stm32f1x.cfg",
            "-c", "adapter speed 100",
            "-c", "reset_config srst_only srst_nogate connect_assert_srst",
            "-c", "init; reset halt",
        ]
        for item in commands:
            command.extend(("-c", item))
        print(" ".join(command))
        if(arguments.execute):
            subprocess.run(command, check=True)
        else:
            print("Dry-run: thêm --execute sau khi đã kiểm đúng board/bundle.")
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"recovery_tool: error: {error}\n")


if(__name__ == "__main__"):
    raise SystemExit(main())
