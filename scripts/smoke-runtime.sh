#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SLM_SMOKE_BUILD_DIR:-${ROOT_DIR}/build}"
APP_BIN=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        echo "[smoke-runtime] --build-dir requires a value" >&2
        exit 2
      fi
      BUILD_DIR="$2"
      shift 2
      ;;
    --app-bin)
      if [[ $# -lt 2 ]]; then
        echo "[smoke-runtime] --app-bin requires a value" >&2
        exit 2
      fi
      APP_BIN="$2"
      shift 2
      ;;
    *)
      # Backward compatible: treat first positional arg as app binary path.
      if [[ -z "${APP_BIN}" ]]; then
        APP_BIN="$1"
      else
        echo "[smoke-runtime] unknown arg: $1" >&2
        echo "usage: $0 [--build-dir <dir>] [--app-bin <path>] [legacy_app_bin]" >&2
        exit 2
      fi
      shift
      ;;
  esac
done

APP_BIN_DEFAULT="${BUILD_DIR}/appSlm_Desktop"
APP_BIN_ALT="${BUILD_DIR}/toppanel-Debug/appSlm_Desktop"

if [[ -z "${APP_BIN}" ]]; then
  if [[ -x "${APP_BIN_DEFAULT}" ]]; then
    APP_BIN="${APP_BIN_DEFAULT}"
  elif [[ -x "${APP_BIN_ALT}" ]]; then
    APP_BIN="${APP_BIN_ALT}"
  fi
fi

if [[ -z "${APP_BIN}" || ! -x "${APP_BIN}" ]]; then
  echo "[smoke-runtime] app binary not found." >&2
  echo "[smoke-runtime] pass binary path as arg1 or build appSlm_Desktop first." >&2
  exit 2
fi

TIMEOUT_SEC="${SLM_SMOKE_TIMEOUT_SEC:-12}"
LOG_FILE="${SLM_SMOKE_LOG_FILE:-/tmp/slm-smoke-runtime.log}"
QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

echo "[smoke-runtime] bin=${APP_BIN}"
echo "[smoke-runtime] timeout=${TIMEOUT_SEC}s"
echo "[smoke-runtime] qpa=${QPA_PLATFORM}"
echo "[smoke-runtime] log=${LOG_FILE}"

set +e
timeout "${TIMEOUT_SEC}" \
  env QT_QPA_PLATFORM="${QPA_PLATFORM}" \
      DS_WINDOWING_BACKEND="${DS_WINDOWING_BACKEND:-kwin-wayland}" \
      "${APP_BIN}" --windowed --window-size=1280x800 >"${LOG_FILE}" 2>&1
rc=$?
set -e

# timeout(1) returns 124 when the process is terminated by timeout.
if [[ "${rc}" != "0" && "${rc}" != "124" ]]; then
  echo "[smoke-runtime] app exited early with rc=${rc}" >&2
  sed -n '1,200p' "${LOG_FILE}" >&2 || true
  exit 1
fi

if rg -n -i \
  -e "QQmlApplicationEngine failed to load component" \
  -e "Cannot assign to non-existent property" \
  -e "Binding loop detected" \
  -e "Type .* unavailable" \
  -e "Segmentation fault" \
  -e "ASSERT" \
  "${LOG_FILE}" >/dev/null 2>&1; then
  echo "[smoke-runtime] detected critical runtime pattern in log" >&2
  rg -n -i \
    -e "QQmlApplicationEngine failed to load component" \
    -e "Cannot assign to non-existent property" \
    -e "Binding loop detected" \
    -e "Type .* unavailable" \
    -e "Segmentation fault" \
    -e "ASSERT" \
    "${LOG_FILE}" >&2 || true
  exit 1
fi

echo "[smoke-runtime] PASS"
