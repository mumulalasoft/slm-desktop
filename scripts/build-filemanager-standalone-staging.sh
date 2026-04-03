#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/slm-filemanager}"

cmake -S modules/slm-filemanager -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target slm-filemanager-core -j"$(nproc)"

echo "[slm-filemanager-staging] build complete in $BUILD_DIR"
