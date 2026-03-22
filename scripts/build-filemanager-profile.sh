#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"

cmake -S . -B "$BUILD_DIR"

TARGET_HELP_OUTPUT="$(cmake --build "$BUILD_DIR" --target help || true)"
if [[ "$TARGET_HELP_OUTPUT" == *"filemanager_test_suite"* ]]; then
  cmake --build "$BUILD_DIR" --target \
    slm-filemanager-core \
    filemanager_test_suite \
    -j"$(nproc)"
else
  cmake --build "$BUILD_DIR" --target \
    slm-filemanager-core \
    fileoperationsmanager_test \
    fileoperationsservice_dbus_test \
    fileopctl_smoke_test \
    filemanagerapi_daemon_recovery_test \
    filemanagerapi_fileops_contract_test \
    filemanager_dialogs_smoke_test \
    -j"$(nproc)"
fi

echo "[filemanager-profile] build complete in $BUILD_DIR"
