#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build/toppanel-Debug}"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "[filemanager-smoke] build dir not found: ${BUILD_DIR}" >&2
  echo "Usage: $0 [build_dir]" >&2
  exit 2
fi

REGEX='(fileoperationsmanager_test|fileoperationsservice_dbus_test|fileopctl_smoke_test|filemanagerapi_daemon_recovery_test|filemanagerapi_fileops_contract_test|filemanager_dialogs_smoke_test)'

echo "[filemanager-smoke] build_dir=${BUILD_DIR}"
echo "[filemanager-smoke] running in isolated DBus session"
dbus-run-session -- \
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "${REGEX}"
