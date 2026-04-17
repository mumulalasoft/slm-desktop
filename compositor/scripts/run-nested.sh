#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_ROOT="$(cd "${ROOT_DIR}/.." && pwd)"
APP_BIN_DEFAULT="${PROJECT_ROOT}/build/appSlm_Desktop"
APP_BIN_ALT="${PROJECT_ROOT}/build/toppanel-Debug/appSlm_Desktop"
APP_BIN="${DS_SHELL_APP_BIN:-}"

BACKEND="${DS_WINDOWING_BACKEND:-kwin-wayland}"
SKIP_SHELL_AUTOLAUNCH="${DS_SKIP_SHELL_AUTOLAUNCH:-0}"
KWIN_PROFILE="${DS_KWIN_PROFILE:-0}"
SHELL_WINDOWED="${DS_SHELL_WINDOWED:-1}"
SHELL_WINDOW_SIZE="${DS_SHELL_WINDOW_SIZE:-1480x900}"

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --backend <kwin-wayland>                Backend mode (kwin-only)
  --host                                  Compatibility flag (ignored; host mode is always used)
  --skip-build                            Compatibility flag (ignored; wlroots compositor removed)
  --no-autolaunch                         Do not autolaunch Slm Desktop app
  --kwin-profile                          Enable KWin profile log (DS_KWIN_PROFILE=1)
  --window-size <WxH>                    App window size in windowed mode (default: ${SHELL_WINDOW_SIZE})
  --fullscreen                            Run app without --windowed
  -h, --help                              Show this help

Environment:
  DS_WINDOWING_BACKEND   Backend mode selection (default: kwin-wayland).
  DS_SHELL_APP_BIN       Explicit appSlm_Desktop path.
  DS_SKIP_SHELL_AUTOLAUNCH 1 to skip app autolaunch.
  DS_KWIN_PROFILE        1 to enable KWin backend profile logs.
  DS_SHELL_WINDOWED      1 to pass --windowed (default: 1).
  DS_SHELL_WINDOW_SIZE   Initial app size in windowed mode, format WxH.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --backend)
      [[ $# -ge 2 ]] || { echo "missing value for --backend"; exit 2; }
      BACKEND="$2"
      shift 2
      ;;
    --host)
      # kwin-only runner always uses host mode.
      :
      shift
      ;;
    --skip-build)
      # wlroots compositor path is removed in kwin-only mode.
      :
      shift
      ;;
    --no-autolaunch)
      SKIP_SHELL_AUTOLAUNCH=1
      shift
      ;;
    --kwin-profile)
      KWIN_PROFILE=1
      shift
      ;;
    --window-size)
      [[ $# -ge 2 ]] || { echo "missing value for --window-size"; exit 2; }
      SHELL_WINDOW_SIZE="$2"
      shift 2
      ;;
    --fullscreen)
      SHELL_WINDOWED=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1"
      usage
      exit 2
      ;;
  esac
done

if [[ ! "${SHELL_WINDOW_SIZE}" =~ ^[0-9]{2,5}[xX][0-9]{2,5}$ ]]; then
  echo "invalid --window-size '${SHELL_WINDOW_SIZE}' (expected WxH)"
  exit 2
fi

if [[ "${BACKEND}" != "kwin-wayland" ]]; then
  echo "invalid backend: ${BACKEND}"
  echo "valid value: kwin-wayland"
  exit 2
fi

if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  echo "[run-nested] kwin-only mode requires a Wayland session."
  echo "[run-nested] XDG_SESSION_TYPE='${XDG_SESSION_TYPE:-unset}' (expected: wayland)."
  exit 2
fi
if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "[run-nested] kwin-only mode requires host Wayland session."
  echo "[run-nested] WAYLAND_DISPLAY is not set. Start from a Wayland session or export WAYLAND_DISPLAY."
  exit 2
fi

LSBLK_LOG="${DS_LSBLK_LOG:-/tmp/desktopshell-lsblk.json}"
ENV_LOG="${DS_ENV_LOG:-/tmp/desktopshell-env.log}"
APP_LOG="${DS_APP_LOG:-${HOME}/.desktopshell-app.log}"

if [[ -z "${APP_BIN}" ]]; then
  if [[ -x "${APP_BIN_DEFAULT}" ]]; then
    APP_BIN="${APP_BIN_DEFAULT}"
  elif [[ -x "${APP_BIN_ALT}" ]]; then
    APP_BIN="${APP_BIN_ALT}"
  fi
fi

if command -v lsblk >/dev/null 2>&1; then
  lsblk -J -b -o PATH,SIZE,TYPE,MOUNTPOINTS,MOUNTPOINT,LABEL,RM,NAME,FSTYPE > "${LSBLK_LOG}" 2>/dev/null || true
else
  echo "lsblk not found" > "${LSBLK_LOG}"
fi

{
  echo "timestamp=$(date --iso-8601=seconds)"
  echo "pwd=${PWD}"
  echo "user=${USER:-}"
  echo "shell=${SHELL:-}"
  echo "backend=${BACKEND}"
  echo "run_mode=host"
  echo "skip_shell_autolaunch=${SKIP_SHELL_AUTOLAUNCH}"
  echo "DS_KWIN_PROFILE=${KWIN_PROFILE}"
  echo "windowed=${SHELL_WINDOWED}"
  echo "window_size=${SHELL_WINDOW_SIZE}"
} > "${ENV_LOG}"

cleanup() {
  if [[ -n "${APP_PID:-}" ]]; then
    kill "${APP_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT INT TERM

echo "Starting Slm Desktop directly on host Wayland with backend=${BACKEND}"
echo "Environment log: ${ENV_LOG}"
echo "Storage snapshot: ${LSBLK_LOG}"

if [[ "${SKIP_SHELL_AUTOLAUNCH}" == "1" ]]; then
  echo "--no-autolaunch active; nothing to run in kwin-only mode."
  exit 0
fi

if [[ -z "${APP_BIN}" || ! -x "${APP_BIN}" ]]; then
  echo "Slm Desktop app binary not found."
  echo "Set DS_SHELL_APP_BIN=/path/to/appSlm_Desktop"
  exit 1
fi

{
  echo "===== DesktopShell App Run $(date --iso-8601=seconds) ====="
  echo "backend=${BACKEND}"
  echo "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
  echo "windowed=${SHELL_WINDOWED}"
  echo "window_size=${SHELL_WINDOW_SIZE}"
} > "${APP_LOG}"

APP_ARGS=()
if [[ "${SHELL_WINDOWED}" == "1" ]]; then
  APP_ARGS+=(--windowed "--window-size=${SHELL_WINDOW_SIZE}")
fi

DS_WINDOWING_BACKEND="${BACKEND}" \
DS_KWIN_PROFILE="${KWIN_PROFILE}" \
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}" \
QT_QPA_PLATFORMTHEME="" \
XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-wayland}" \
"${APP_BIN}" "${APP_ARGS[@]}" >> "${APP_LOG}" 2>&1 &
APP_PID=$!

wait "${APP_PID}"
