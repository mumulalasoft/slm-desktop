#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_BIN="${1:-${DS_SHELL_APP_BIN:-}}"
if [[ -z "${APP_BIN}" ]]; then
  if [[ -x "${ROOT_DIR}/build/appSlm_Desktop" ]]; then
    APP_BIN="${ROOT_DIR}/build/appSlm_Desktop"
  elif [[ -x "${ROOT_DIR}/build/toppanel-Debug/appSlm_Desktop" ]]; then
    APP_BIN="${ROOT_DIR}/build/toppanel-Debug/appSlm_Desktop"
  fi
fi

if [[ -z "${APP_BIN}" || ! -x "${APP_BIN}" ]]; then
  echo "appSlm_Desktop binary not found. Set DS_SHELL_APP_BIN=/path/to/appSlm_Desktop" >&2
  exit 1
fi

TIMEOUT_SEC="${DS_SMOKE_TIMEOUT_SEC:-8}"
LOG_DIR="${DS_SMOKE_LOG_DIR:-/tmp}"
mkdir -p "${LOG_DIR}"

run_one() {
  local backend="$1"
  local log_file="${LOG_DIR}/slm-desktop-kwin-smoke-${backend}.log"
  echo "[smoke] backend=${backend} timeout=${TIMEOUT_SEC}s log=${log_file}"

  set +e
  timeout "${TIMEOUT_SEC}" \
    env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
        DS_WINDOWING_BACKEND="${backend}" \
        DS_WINDOWING_AUTOROLLBACK="${DS_WINDOWING_AUTOROLLBACK:-1}" \
        "${APP_BIN}" --windowed >"${log_file}" 2>&1
  local rc=$?
  set -e

  case "${rc}" in
    0|124)
      # 124 timeout => expected for smoke: process stayed alive during window.
      echo "[smoke] backend=${backend} ok rc=${rc}"
      ;;
    *)
      echo "[smoke] backend=${backend} FAILED rc=${rc}" >&2
      tail -n 120 "${log_file}" >&2 || true
      exit 1
      ;;
  esac
}

run_one "kwin-wayland"

echo "[smoke] kwin-wayland backend passed"
