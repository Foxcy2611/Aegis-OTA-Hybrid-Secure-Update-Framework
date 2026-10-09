#!/usr/bin/env bash

# Configure và build firmware STM32 bằng preset canonical.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
PROFILE="${1:-all}"

build_profile(){
    local profile="$1"

    cmake --preset "${profile}"
    cmake --build --preset "${profile}"
}

cd "${PROJECT_DIR}"

case "${PROFILE}" in
    debug|release)
        build_profile "${PROFILE}"
        ;;
    all)
        build_profile debug
        build_profile release
        ;;
    *)
        echo "Cách dùng: $0 [debug|release|all]" >&2
        exit 2
        ;;
esac
