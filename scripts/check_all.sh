#!/usr/bin/env bash

# Gate phần mềm tổng: hai firmware, board-test compile và mọi host regression.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

"${SCRIPT_DIR}/build_firmware.sh" all

cd "${PROJECT_DIR}"
cmake --build build/debug --target aegis_phase4_lora_board_test
cmake --build build/release --target aegis_phase4_lora_board_test
cmake --build build/debug --target size disasm

"${SCRIPT_DIR}/test_host.sh"

echo "Software gate PASS. Hardware/RF gates vẫn phải chạy trên mạch thật."
