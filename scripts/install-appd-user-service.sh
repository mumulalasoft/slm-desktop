#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_SRC="$ROOT_DIR/scripts/systemd/slm-appd.service"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_DST="$UNIT_DIR/slm-appd.service"

BIN_PATH_DEFAULT="$ROOT_DIR/build/desktop-appd"
BIN_PATH="${1:-$BIN_PATH_DEFAULT}"
BIN_PATH="$(readlink -f "$BIN_PATH")"

if [[ ! -x "$BIN_PATH" ]]; then
  echo "[install-appd] binary not executable: $BIN_PATH" >&2
  echo "[install-appd] build first: cmake --build build --target desktop-appd -j8" >&2
  exit 1
fi

mkdir -p "$UNIT_DIR"

sed "s#%h/Development/Qt/Desktop_Shell/build/desktop-appd#$BIN_PATH#g" "$UNIT_SRC" > "$UNIT_DST"

systemctl --user daemon-reload
systemctl --user enable --now slm-appd.service

echo "[install-appd] installed: $UNIT_DST"
echo "[install-appd] status:"
systemctl --user --no-pager --full status slm-appd.service || true
