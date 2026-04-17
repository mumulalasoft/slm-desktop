#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_SRC="$ROOT_DIR/scripts/systemd/slm-recoveryd.service"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_DST="$UNIT_DIR/slm-recoveryd.service"

BIN_PATH_DEFAULT="$ROOT_DIR/build/slm-recoveryd"
BIN_PATH="${1:-$BIN_PATH_DEFAULT}"
BIN_PATH="$(readlink -f "$BIN_PATH")"

if [[ ! -x "$BIN_PATH" ]]; then
  echo "[install-recoveryd] binary not executable: $BIN_PATH" >&2
  echo "[install-recoveryd] build first: cmake --build build --target slm-recoveryd -j8" >&2
  exit 1
fi

mkdir -p "$UNIT_DIR"

sed "s#/usr/local/bin/slm-recoveryd#$BIN_PATH#g" "$UNIT_SRC" > "$UNIT_DST"

systemctl --user daemon-reload
systemctl --user enable --now slm-recoveryd.service

echo "[install-recoveryd] installed: $UNIT_DST"
echo "[install-recoveryd] status:"
systemctl --user --no-pager --full status slm-recoveryd.service || true
