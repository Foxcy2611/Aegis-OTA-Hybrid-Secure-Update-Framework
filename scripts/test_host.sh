#!/usr/bin/env bash

# Configure, build và chạy toàn bộ regression không cần phần cứng.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

run_suite(){
    local source_dir="$1"
    local build_dir="$2"

    cmake -S "${source_dir}" -B "${build_dir}" -G Ninja
    cmake --build "${build_dir}"
    ctest --test-dir "${build_dir}" --output-on-failure
}

cd "${PROJECT_DIR}"
run_suite test/Test_to_Phase/Phase3_Crypto build/tests/phase3
run_suite test/Test_to_Phase/Phase4_LoRa/host build/tests/phase4
run_suite test/Test_to_Phase/Phase5_8_Host build/tests/phase5-8
