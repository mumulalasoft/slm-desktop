#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_QML_FILE="${ROOT_DIR}/Main.qml"
SMOKE_RUNTIME="${ROOT_DIR}/scripts/smoke-runtime.sh"

if [[ ! -f "${MAIN_QML_FILE}" ]]; then
  echo "[smoke-portal-chooser] missing file: ${MAIN_QML_FILE}" >&2
  exit 2
fi

if [[ ! -x "${SMOKE_RUNTIME}" ]]; then
  echo "[smoke-portal-chooser] missing runtime smoke script: ${SMOKE_RUNTIME}" >&2
  exit 2
fi

if [[ "${SLM_SMOKE_BUILD:-1}" == "1" ]]; then
  echo "[smoke-portal-chooser] building slm-desktop"
  cmake --build "${ROOT_DIR}/build" -j4 --target slm-desktop
fi

echo "[smoke-portal-chooser] running runtime smoke"
"${SMOKE_RUNTIME}" "${1:-}"

declare -a REQUIRED_PATTERNS=(
  "function portalChooserSelectRangeTo("
  "function portalChooserAddRangeTo("
  "function portalChooserToggleIndexSelection("
  "function portalChooserLoadUiPreferences("
  "function portalChooserSaveUiPreferences("
  "function portalChooserSaveWindowSize("
  "function portalChooserValidateSaveName("
  "function portalChooserSelectionCount("
  "function portalChooserInvertSelection("
  "function portalChooserFormatBytes("
  "function portalChooserNameColumnWidth("
  "function portalChooserClampColumns("
  "portalChooserPathEditMode"
  "sequence: \"Shift+Down\""
  "sequence: \"Shift+Up\""
  "sequence: \"Ctrl+Shift+Down\""
  "sequence: \"Ctrl+Shift+Up\""
  "sequence: \"Shift+Home\""
  "sequence: \"Shift+End\""
  "sequence: \"Ctrl+Shift+Home\""
  "sequence: \"Ctrl+Shift+End\""
  "sequence: \"Shift+PgDown\""
  "sequence: \"Shift+PgUp\""
  "sequence: \"Ctrl+Shift+PgDown\""
  "sequence: \"Ctrl+Shift+PgUp\""
  "sequence: \"Ctrl+A\""
  "sequence: \"Ctrl+I\""
  "sequence: \"Ctrl+Shift+A\""
  "sequence: \"Ctrl+D\""
  "sequence: \"Delete\""
  "sequence: \"Space\""
  "sequence: \"Ctrl+Space\""
  "sequence: \"Escape\""
  "root.portalChooserAddRangeTo(0)"
  "root.portalChooserAddRangeTo(total - 1)"
  "root.portalChooserAddRangeTo(cur + 1)"
  "root.portalChooserAddRangeTo(cur - 1)"
  "root.portalChooserSelectRangeTo(cur + root.portalChooserPageStep())"
  "root.portalChooserSelectRangeTo(cur - root.portalChooserPageStep())"
  "root.portalChooserAddRangeTo(cur + root.portalChooserPageStep())"
  "root.portalChooserAddRangeTo(cur - root.portalChooserPageStep())"
  "root.portalChooserToggleIndexSelection(cur)"
  "root.portalChooserToggleIndexSelection(cur, true)"
  "root.portalChooserToggleIndexSelection(index)"
  "root.portalChooserInvertSelection()"
  "if (root.portalChooserSelectFolders"
  "if (mouse && (mouse.modifiers & Qt.ControlModifier)) {"
  "root.portalChooserResetSelection()"
  "if (root.portalChooserOverwriteDialogVisible)"
  "portalChooserOverwriteDialogVisible = true"
  "portalChooserOverwriteAlwaysThisSession"
  "Always replace in this session"
  "Clear Selection"
  "Invert Selection"
  " selected • total "
  "Kind / Size"
  "Qt.SplitHCursor"
  "ToolTip.visible:"
  "mimeType"
  "onDoubleClicked:"
  "portalChooserValidationError"
  "if (mouse && (mouse.modifiers & Qt.ShiftModifier))"
  "if (mouse && (mouse.modifiers & Qt.ControlModifier))"
  "root.portalChooserSelectRangeTo(index)"
)

echo "[smoke-portal-chooser] validating Main.qml guards"
for pattern in "${REQUIRED_PATTERNS[@]}"; do
  if ! rg -n -F "${pattern}" "${MAIN_QML_FILE}" >/dev/null 2>&1; then
    echo "[smoke-portal-chooser] missing Main.qml guard pattern: ${pattern}" >&2
    exit 1
  fi
done

echo "[smoke-portal-chooser] PASS"
