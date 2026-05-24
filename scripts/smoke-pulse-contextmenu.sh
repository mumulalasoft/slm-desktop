#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SLM_SMOKE_BUILD_DIR:-${ROOT_DIR}/build}"
APP_BIN_ARG=""
QML_FILE="${ROOT_DIR}/Qml/components/pulse/PulseOverlay.qml"
MAIN_QML_FILE="${ROOT_DIR}/Main.qml"
CONTROLLER_JS_FILE="${ROOT_DIR}/Qml/components/shell/PulseController.js"
SMOKE_RUNTIME="${ROOT_DIR}/scripts/smoke-runtime.sh"

find_fixed_pattern() {
  local pattern="$1"
  local file="$2"
  if command -v rg >/dev/null 2>&1; then
    rg -n -F "${pattern}" "${file}" >/dev/null 2>&1
    return $?
  fi
  grep -n -F -- "${pattern}" "${file}" >/dev/null 2>&1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        echo "[smoke-pulse] --build-dir requires a value" >&2
        exit 2
      fi
      BUILD_DIR="$2"
      shift 2
      ;;
    --app-bin)
      if [[ $# -lt 2 ]]; then
        echo "[smoke-pulse] --app-bin requires a value" >&2
        exit 2
      fi
      APP_BIN_ARG="$2"
      shift 2
      ;;
    *)
      # Backward compatible: first positional arg as app binary.
      if [[ -z "${APP_BIN_ARG}" ]]; then
        APP_BIN_ARG="$1"
      else
        echo "[smoke-pulse] unknown arg: $1" >&2
        echo "usage: $0 [--build-dir <dir>] [--app-bin <path>] [legacy_app_bin]" >&2
        exit 2
      fi
      shift
      ;;
  esac
done

if [[ ! -f "${QML_FILE}" ]]; then
  echo "[smoke-pulse] missing file: ${QML_FILE}" >&2
  exit 2
fi
if [[ ! -f "${MAIN_QML_FILE}" ]]; then
  echo "[smoke-pulse] missing file: ${MAIN_QML_FILE}" >&2
  exit 2
fi
if [[ ! -f "${CONTROLLER_JS_FILE}" ]]; then
  echo "[smoke-pulse] missing file: ${CONTROLLER_JS_FILE}" >&2
  exit 2
fi

if [[ ! -x "${SMOKE_RUNTIME}" ]]; then
  echo "[smoke-pulse] missing runtime smoke script: ${SMOKE_RUNTIME}" >&2
  exit 2
fi

if [[ "${SLM_SMOKE_BUILD:-1}" == "1" ]]; then
  echo "[smoke-pulse] building slm-desktop"
  cmake --build "${BUILD_DIR}" -j4 --target slm-desktop
fi

echo "[smoke-pulse] running runtime smoke"
if [[ -n "${APP_BIN_ARG}" ]]; then
  "${SMOKE_RUNTIME}" --build-dir "${BUILD_DIR}" --app-bin "${APP_BIN_ARG}"
else
  "${SMOKE_RUNTIME}" --build-dir "${BUILD_DIR}"
fi

declare -a REQUIRED_PATTERNS=(
  "property string mode: \"idle\" // idle | search | command"
  "if (trimmed.charAt(0) === \">\" || trimmed.charAt(0) === \"/\")"
  "PulseComp.PulseTopResult {"
  "PulseComp.PulseActionRow {"
  "PulseComp.PulseListSection {"
  "titleText: \"Apps\""
  "titleText: \"Files\""
  "PulseComp.PulseCommandPreview {"
  "PulseComp.PulseGridSection {"
  "titleText: commandMode ? \"Commands\" : \"System / Commands\""
  "if (c === \"open apphub\" || c.indexOf(\"open apphub \") === 0)"
  "if (event.key === Qt.Key_Tab"
  "if (event.key === Qt.Key_Right)"
  "if (event.key === Qt.Key_Up || event.key === Qt.Key_Down)"
  "if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)"
  "if (event.key === Qt.Key_Escape)"
)

declare -a REQUIRED_MAIN_PATTERNS=(
  "property int pulseQueryGeneration: 0"
  "property int pulseAppliedGeneration: -1"
  "interval: 150"
  "PulseController.activateResult(root, pulseResultsModel, indexValue)"
)

declare -a REQUIRED_CONTROLLER_PATTERNS=(
  "PulseService.activateResult(rid, {"
  "\"action\": action"
  "if (act === \"copyPath\" || act === \"copyName\" || act === \"copyUri\") {"
  "if (act === \"pinToAppDeck\") {"
)

echo "[smoke-pulse] validating QML guards"
for pattern in "${REQUIRED_PATTERNS[@]}"; do
  if ! find_fixed_pattern "${pattern}" "${QML_FILE}"; then
    echo "[smoke-pulse] missing guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-pulse] validating Main.qml guards"
for pattern in "${REQUIRED_MAIN_PATTERNS[@]}"; do
  if ! find_fixed_pattern "${pattern}" "${MAIN_QML_FILE}"; then
    echo "[smoke-pulse] missing Main.qml guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-pulse] validating PulseController guards"
for pattern in "${REQUIRED_CONTROLLER_PATTERNS[@]}"; do
  if ! find_fixed_pattern "${pattern}" "${CONTROLLER_JS_FILE}"; then
    echo "[smoke-pulse] missing PulseController guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-pulse] PASS"
