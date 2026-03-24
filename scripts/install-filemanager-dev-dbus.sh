#!/usr/bin/env bash
# scripts/install-filemanager-dev-dbus.sh
#
# Install DBus service activation file untuk dev build filemanager lokal.
# Diperlukan agar DBus bisa auto-start binary lokal saat shell memanggil
# org.slm.Desktop.FileManager1 untuk pertama kali.
#
# Cara pakai:
#   bash scripts/install-filemanager-dev-dbus.sh
#   bash scripts/install-filemanager-dev-dbus.sh --binary /custom/path/slm-filemanager
#
# Untuk uninstall:
#   bash scripts/install-filemanager-dev-dbus.sh --uninstall

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Load workspace env jika tersedia
ENV_FILE="$SCRIPT_DIR/../dev/workspace.env"
if [[ -f "$ENV_FILE" ]]; then
    SLM_ENV_QUIET=1 source "$ENV_FILE"
fi

DBUS_SERVICES_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/dbus-1/services"
SERVICE_NAME="org.slm.Desktop.FileManager1"
SERVICE_FILE="$DBUS_SERVICES_DIR/${SERVICE_NAME}.service"
UNINSTALL=0

# Binary default: dari env atau build dir konvensional
FM_BINARY="${SLM_FILEMANAGER_BINARY:-${SLM_FILEMANAGER_BUILD_DIR:-}/slm-filemanager}"

for arg in "$@"; do
    case "$arg" in
        --binary=*) FM_BINARY="${arg#--binary=}" ;;
        --binary)   shift; FM_BINARY="$1" ;;
        --uninstall) UNINSTALL=1 ;;
    esac
done

if [[ "$UNINSTALL" == "1" ]]; then
    if [[ -f "$SERVICE_FILE" ]]; then
        rm "$SERVICE_FILE"
        echo "[install-dbus] Removed: $SERVICE_FILE"
    else
        echo "[install-dbus] Service file tidak ada: $SERVICE_FILE"
    fi
    exit 0
fi

# Cari binary bila belum ditemukan
if [[ -z "$FM_BINARY" || ! -x "$FM_BINARY" ]]; then
    # Coba cari di build dirs umum
    for candidate in \
        "$HOME/Development/Qt/Desktop_Shell/build/slm-filemanager" \
        "$SCRIPT_DIR/../build/slm-filemanager" \
        "$SCRIPT_DIR/../build/dev/slm-filemanager" \
        "$(which slm-filemanager 2>/dev/null || true)"; do
        if [[ -x "$candidate" ]]; then
            FM_BINARY="$candidate"
            break
        fi
    done
fi

if [[ -z "$FM_BINARY" || ! -x "$FM_BINARY" ]]; then
    echo "[install-dbus] ERROR: binary slm-filemanager tidak ditemukan."
    echo "  Set SLM_FILEMANAGER_BINARY atau gunakan --binary /path/to/slm-filemanager"
    exit 1
fi

echo "[install-dbus] Installing DBus activation:"
echo "  Service : $SERVICE_NAME"
echo "  Binary  : $FM_BINARY"
echo "  File    : $SERVICE_FILE"

mkdir -p "$DBUS_SERVICES_DIR"

cat > "$SERVICE_FILE" <<EOF
[D-BUS Service]
Name=$SERVICE_NAME
Exec=$FM_BINARY --dbus-activate
EOF

echo "[install-dbus] OK. Reload DBus dengan: systemctl --user reload dbus.service"
echo "  atau restart sesi."
