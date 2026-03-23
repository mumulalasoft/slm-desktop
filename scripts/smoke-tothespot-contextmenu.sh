#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QML_FILE="${ROOT_DIR}/Qml/components/tothespot/TothespotOverlay.qml"
MAIN_QML_FILE="${ROOT_DIR}/Main.qml"
CONTROLLER_JS_FILE="${ROOT_DIR}/Qml/components/shell/TothespotController.js"
SMOKE_RUNTIME="${ROOT_DIR}/scripts/smoke-runtime.sh"

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
  echo "[smoke-tothespot] building appSlm_Desktop"
  cmake --build "${ROOT_DIR}/build" -j4 --target appSlm_Desktop
fi

echo "[smoke-tothespot] running runtime smoke"
"${SMOKE_RUNTIME}" "${1:-}"

declare -a REQUIRED_PATTERNS=(
  "function openContextMenuForIndexGlobal("
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
  if ! rg -n -F "${pattern}" "${QML_FILE}" >/dev/null 2>&1; then
    echo "[smoke-tothespot] missing guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-tothespot] validating Main.qml guards"
for pattern in "${REQUIRED_MAIN_PATTERNS[@]}"; do
  if ! rg -n -F "${pattern}" "${MAIN_QML_FILE}" >/dev/null 2>&1; then
    echo "[smoke-tothespot] missing Main.qml guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-tothespot] validating TothespotController guards"
for pattern in "${REQUIRED_CONTROLLER_PATTERNS[@]}"; do
  if ! rg -n -F "${pattern}" "${CONTROLLER_JS_FILE}" >/dev/null 2>&1; then
    echo "[smoke-tothespot] missing TothespotController guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-tothespot] PASS"
