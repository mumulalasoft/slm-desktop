#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_SRC="$ROOT_DIR/scripts/systemd/slm-clipboardd.service"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_DST="$UNIT_DIR/slm-clipboardd.service"

BIN_PATH_DEFAULT="$ROOT_DIR/build/slm-clipboardd"
BIN_PATH="${1:-$BIN_PATH_DEFAULT}"
BIN_PATH="$(readlink -f "$BIN_PATH")"

if [[ ! -x "$BIN_PATH" ]]; then
  echo "[install-clipboardd] binary not executable: $BIN_PATH" >&2
  echo "[install-clipboardd] build first: cmake --build build -j8 --target slm-clipboardd" >&2
  exit 1
fi

mkdir -p "$UNIT_DIR"
sed "s#%h/Development/Qt/Slm_Desktop/build/slm-clipboardd#$BIN_PATH#g" "$UNIT_SRC" > "$UNIT_DST"

systemctl --user daemon-reload
systemctl --user enable --now slm-clipboardd.service

echo "[install-clipboardd] installed: $UNIT_DST"
systemctl --user --no-pager --full status slm-clipboardd.service || true
