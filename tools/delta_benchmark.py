#!/usr/bin/env python3
"""So sánh kích thước FULL và patch COPY/ADD trên một hoặc nhiều cặp BIN."""

from __future__ import annotations

import argparse
import pathlib

from make_delta import make_patch


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pairs", nargs="+", help="Mỗi cặp có dạng old.bin:new.bin")
    arguments = parser.parse_args()
    print("base,target,full_bytes,patch_bytes,reduction_percent")
    for pair in arguments.pairs:
        try:
            old_name, new_name = pair.split(":", 1)
            old_path = pathlib.Path(old_name)
            new_path = pathlib.Path(new_name)
            old = old_path.read_bytes()
            new = new_path.read_bytes()
            patch = make_patch(old, new)
            reduction = 100.0 * (1.0 - len(patch) / len(new)) if new else 0.0
            print(
                f"{old_path},{new_path},{len(new)},{len(patch)},"
                f"{reduction:.2f}"
            )
        except (OSError, ValueError) as error:
            parser.exit(2, f"delta_benchmark: error: {error}\n")
    return 0


if(__name__ == "__main__"):
    raise SystemExit(main())
