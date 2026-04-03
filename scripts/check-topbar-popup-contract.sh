#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

FAILED=0

require_pattern() {
  local file="$1"
  local pattern="$2"
  local name="$3"
  if ! rg -q --pcre2 "${pattern}" "${file}"; then
    echo "[lint-topbar-popup][FAIL] ${name}: ${file}" >&2
    FAILED=1
  fi
}

forbid_pattern() {
  local file="$1"
  local pattern="$2"
  local name="$3"
  if rg -q --pcre2 "${pattern}" "${file}"; then
    echo "[lint-topbar-popup][FAIL] ${name}: ${file}" >&2
    FAILED=1
  fi
}

echo "[lint-topbar-popup] checking global popup host contract"
require_pattern "Qml/components/overlay/PopupHostLayer.qml" "popup\\.parent\\s*=\\s*root" "popup parent must be PopupHostLayer root"
require_pattern "Qml/components/overlay/PopupHostLayer.qml" "popup\\.popupType\\s*=\\s*Popup\\.Window" "popup type must be Popup.Window"
require_pattern "Qml/components/overlay/PopupHostLayer.qml" "Popup\\.CloseOnEscape\\s*\\|\\s*Popup\\.CloseOnPressOutside" "popup close policy contract"

echo "[lint-topbar-popup] checking anchored popup/menu contract"
require_pattern "Qml/components/topbar/TopBarAnchoredPopup.qml" "property bool hostManagedPosition" "TopBarAnchoredPopup host-managed position property"
require_pattern "Qml/components/topbar/TopBarAnchoredPopup.qml" "when:\\s*!control\\.hostManagedPosition" "TopBarAnchoredPopup binding guard"
require_pattern "Qml/components/topbar/TopBarAnchoredMenu.qml" "property bool hostManagedPosition" "TopBarAnchoredMenu host-managed position property"
require_pattern "Qml/components/topbar/TopBarAnchoredMenu.qml" "when:\\s*!control\\.hostManagedPosition" "TopBarAnchoredMenu binding guard"

echo "[lint-topbar-popup] checking IndicatorMenu contract"
require_pattern "third_party/slm-style/qml/SlmStyle/IndicatorMenu.qml" "^import\\s+Slm_Desktop\\s*$" "IndicatorMenu must import Slm_Desktop"
require_pattern "third_party/slm-style/qml/SlmStyle/IndicatorMenu.qml" "property bool hostManagedPosition" "IndicatorMenu host-managed position property"
require_pattern "third_party/slm-style/qml/SlmStyle/IndicatorMenu.qml" "function togglePopup\\(\\)" "IndicatorMenu togglePopup API"
require_pattern "third_party/slm-style/qml/SlmStyle/IndicatorMenu.qml" "when:\\s*!control\\.hostManagedPosition" "IndicatorMenu binding guard"

echo "[lint-topbar-popup] checking applet trigger contract"
forbid_pattern "Qml/components/applet/NetworkApplet.qml" "networkMenu\\.open\\(" "Network applet must not call networkMenu.open() directly"
forbid_pattern "Qml/components/applet/SoundApplet.qml" "\\bmenu\\.open\\(" "Sound applet must not call menu.open() directly"
forbid_pattern "Qml/components/applet/BatteryApplet.qml" "batteryMenu\\.open\\(" "Battery applet must not call batteryMenu.open() directly"
forbid_pattern "Qml/components/applet/BluetoothApplet.qml" "bluetoothMenu\\.open\\(" "Bluetooth applet must not call bluetoothMenu.open() directly"
forbid_pattern "Qml/components/applet/NotificationApplet.qml" "\\bmenu\\.open\\(" "Notification applet must not call menu.open() directly"
forbid_pattern "Qml/components/applet/ClipboardApplet.qml" "\\bmenu\\.open\\(" "Clipboard applet must not call menu.open() directly"
forbid_pattern "Qml/components/applet/PrintJobApplet.qml" "\\bmenu\\.open\\(" "Print applet must not call menu.open() directly"
forbid_pattern "Qml/components/applet/ScreencastIndicator.qml" "\\bmenu\\.open\\(" "Screencast applet must not call menu.open() directly"
forbid_pattern "Qml/components/applet/InputCaptureIndicator.qml" "\\bmenu\\.open\\(" "InputCapture applet must not call menu.open() directly"

if [[ "${FAILED}" -ne 0 ]]; then
  cat >&2 <<'EOF'
[lint-topbar-popup] contract failed.
[lint-topbar-popup] Topbar popup positioning/stacking must stay centralized in PopupHostLayer + TopBarPopupController.
EOF
  exit 1
fi

echo "[lint-topbar-popup] OK"
