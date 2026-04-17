#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"

FILEOPSD_BIN_DEFAULT="$ROOT_DIR/build/slm-fileopsd"
DEVICESD_BIN_DEFAULT="$ROOT_DIR/build/slm-devicesd"
PORTALD_BIN_DEFAULT="$ROOT_DIR/build/slm-portald"
FILEOPSD_BIN="${1:-$FILEOPSD_BIN_DEFAULT}"
DEVICESD_BIN="${2:-$DEVICESD_BIN_DEFAULT}"
PORTALD_BIN="${3:-$PORTALD_BIN_DEFAULT}"
FILEOPSD_BIN="$(readlink -f "$FILEOPSD_BIN")"
DEVICESD_BIN="$(readlink -f "$DEVICESD_BIN")"
PORTALD_BIN="$(readlink -f "$PORTALD_BIN")"

if [[ ! -x "$FILEOPSD_BIN" ]]; then
  echo "[install-split] binary not executable: $FILEOPSD_BIN" >&2
  echo "[install-split] build first: cmake --build build -j8" >&2
  exit 1
fi

if [[ ! -x "$DEVICESD_BIN" ]]; then
  echo "[install-split] binary not executable: $DEVICESD_BIN" >&2
  echo "[install-split] build first: cmake --build build -j8" >&2
  exit 1
fi

if [[ ! -x "$PORTALD_BIN" ]]; then
  echo "[install-split] binary not executable: $PORTALD_BIN" >&2
  echo "[install-split] build first: cmake --build build -j8" >&2
  exit 1
fi

mkdir -p "$UNIT_DIR"

sed "s#%h/Development/Qt/Slm_Desktop/build/slm-fileopsd#$FILEOPSD_BIN#g" \
  "$ROOT_DIR/scripts/systemd/slm-fileopsd.service" > "$UNIT_DIR/slm-fileopsd.service"
sed "s#%h/Development/Qt/Slm_Desktop/build/slm-devicesd#$DEVICESD_BIN#g" \
  "$ROOT_DIR/scripts/systemd/slm-devicesd.service" > "$UNIT_DIR/slm-devicesd.service"
sed "s#%h/Development/Qt/Slm_Desktop/build/slm-portald#$PORTALD_BIN#g" \
  "$ROOT_DIR/scripts/systemd/slm-portald.service" > "$UNIT_DIR/slm-portald.service"

# Ensure desktopd does not claim the same DBus names when split daemons are enabled.
mkdir -p "$UNIT_DIR/slm-desktopd.service.d"
cat > "$UNIT_DIR/slm-desktopd.service.d/split-services.conf" <<'EOF'
[Service]
Environment=SLM_DESKTOPD_DISABLE_FILEOPS=1
Environment=SLM_DESKTOPD_DISABLE_DEVICES=1
EOF

systemctl --user daemon-reload
systemctl --user enable --now slm-fileopsd.service
systemctl --user enable --now slm-devicesd.service
systemctl --user enable --now slm-portald.service
if systemctl --user is-enabled slm-desktopd.service >/dev/null 2>&1; then
  systemctl --user restart slm-desktopd.service
fi

echo "[install-split] installed:"
echo "  - $UNIT_DIR/slm-fileopsd.service"
echo "  - $UNIT_DIR/slm-devicesd.service"
echo "  - $UNIT_DIR/slm-portald.service"
echo "  - $UNIT_DIR/slm-desktopd.service.d/split-services.conf"
echo "[install-split] status:"
systemctl --user --no-pager --full status slm-fileopsd.service || true
systemctl --user --no-pager --full status slm-devicesd.service || true
systemctl --user --no-pager --full status slm-portald.service || true
