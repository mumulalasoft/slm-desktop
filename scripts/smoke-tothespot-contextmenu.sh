#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SLM_SMOKE_BUILD_DIR:-${ROOT_DIR}/build}"
APP_BIN_ARG=""
QML_FILE="${ROOT_DIR}/Qml/components/tothespot/TothespotOverlay.qml"
MAIN_QML_FILE="${ROOT_DIR}/Main.qml"
CONTROLLER_JS_FILE="${ROOT_DIR}/Qml/components/shell/TothespotController.js"
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
        echo "[smoke-tothespot] --build-dir requires a value" >&2
        exit 2
      fi
      BUILD_DIR="$2"
      shift 2
      ;;
    --app-bin)
      if [[ $# -lt 2 ]]; then
        echo "[smoke-tothespot] --app-bin requires a value" >&2
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
        echo "[smoke-tothespot] unknown arg: $1" >&2
        echo "usage: $0 [--build-dir <dir>] [--app-bin <path>] [legacy_app_bin]" >&2
        exit 2
      fi
      shift
      ;;
  esac
done

if [[ ! -f "${QML_FILE}" ]]; then
  echo "[smoke-tothespot] missing file: ${QML_FILE}" >&2
  exit 2
fi
if [[ ! -f "${MAIN_QML_FILE}" ]]; then
  echo "[smoke-tothespot] missing file: ${MAIN_QML_FILE}" >&2
  exit 2
fi
if [[ ! -f "${CONTROLLER_JS_FILE}" ]]; then
  echo "[smoke-tothespot] missing file: ${CONTROLLER_JS_FILE}" >&2
  exit 2
fi

if [[ ! -x "${SMOKE_RUNTIME}" ]]; then
  echo "[smoke-tothespot] missing runtime smoke script: ${SMOKE_RUNTIME}" >&2
  exit 2
fi

if [[ "${SLM_SMOKE_BUILD:-1}" == "1" ]]; then
  echo "[smoke-tothespot] building slm-desktop"
  cmake --build "${BUILD_DIR}" -j4 --target slm-desktop
fi

echo "[smoke-tothespot] running runtime smoke"
if [[ -n "${APP_BIN_ARG}" ]]; then
  "${SMOKE_RUNTIME}" --build-dir "${BUILD_DIR}" --app-bin "${APP_BIN_ARG}"
else
  "${SMOKE_RUNTIME}" --build-dir "${BUILD_DIR}"
fi

declare -a REQUIRED_PATTERNS=(
  "rowMouse.mapToGlobal("
  "function moveContextMenuSelection("
  "function moveSelectionByGroup("
  "function triggerContextMenuAction("
  "function highlightQueryText("
  "text: root.highlightQueryText(name, root.queryText)"
  "textFormat: Text.StyledText"
  "section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart"
  "if (event.key === Qt.Key_C"
  "if (event.key === Qt.Key_K"
  "if (event.key === Qt.Key_Tab"
  "if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9)"
  "if (event.modifiers & Qt.ShiftModifier)"
  "if (event.modifiers & Qt.AltModifier)"
  "if (event.key === Qt.Key_L && (event.modifiers & Qt.ControlModifier))"
  "onContentYChanged: root.closeContextMenu()"
  "if ((Date.now() - root.contextMenuOpenedAtMs) < 120)"
  "acceptedButtons: Qt.LeftButton"
)

declare -a REQUIRED_MAIN_PATTERNS=(
  "property int tothespotQueryGeneration: 0"
  "property int tothespotAppliedGeneration: -1"
  "interval: 150"
  "TothespotController.activateResult(root, tothespotResultsModel, indexValue)"
)

declare -a REQUIRED_CONTROLLER_PATTERNS=(
  "TothespotService.activateResult(rid, {"
  "\"action\": action"
  "if (act === \"copyPath\" || act === \"copyName\" || act === \"copyUri\") {"
)

echo "[smoke-tothespot] validating QML guards"
for pattern in "${REQUIRED_PATTERNS[@]}"; do
  if ! find_fixed_pattern "${pattern}" "${QML_FILE}"; then
    echo "[smoke-tothespot] missing guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-tothespot] validating Main.qml guards"
for pattern in "${REQUIRED_MAIN_PATTERNS[@]}"; do
  if ! find_fixed_pattern "${pattern}" "${MAIN_QML_FILE}"; then
    echo "[smoke-tothespot] missing Main.qml guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-tothespot] validating TothespotController guards"
for pattern in "${REQUIRED_CONTROLLER_PATTERNS[@]}"; do
  if ! find_fixed_pattern "${pattern}" "${CONTROLLER_JS_FILE}"; then
    echo "[smoke-tothespot] missing TothespotController guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-tothespot] PASS"
