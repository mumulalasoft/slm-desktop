#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build/toppanel-Debug"
MODE="stable"

usage() {
  cat <<'EOF'
Usage:
  test-filemanager-dbus-gates.sh [--build-dir <dir>] [--stable|--strict]

Modes:
  --stable  Run default FileManager DBus gates:
            fileoperationsservice_dbus_test
            fileopctl_smoke_test
            filemanagerapi_daemon_recovery_test
  --strict  Run strict stress gate:
            filemanagerapi_daemon_recovery_strict_test
            (exports SLM_RUN_STRICT_FILEOPS_RECOVERY=1)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --stable)
      MODE="stable"
      shift
      ;;
    --strict)
      MODE="strict"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[filemanager-dbus-gates] unknown arg: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "[filemanager-dbus-gates] build dir not found: $BUILD_DIR" >&2
  exit 2
fi

if [[ "$MODE" == "strict" ]]; then
  REGEX='filemanagerapi_daemon_recovery_strict_test'
  echo "[filemanager-dbus-gates] mode=strict build_dir=$BUILD_DIR"
  SLM_RUN_STRICT_FILEOPS_RECOVERY=1 \
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  dbus-run-session -- \
    ctest --test-dir "$BUILD_DIR" -R "$REGEX" --output-on-failure
  exit 0
fi

REGEX='(fileoperationsservice_dbus_test|fileopctl_smoke_test|filemanagerapi_daemon_recovery_test)'
echo "[filemanager-dbus-gates] mode=stable build_dir=$BUILD_DIR"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
dbus-run-session -- \
  ctest --test-dir "$BUILD_DIR" -R "$REGEX" --output-on-failure

