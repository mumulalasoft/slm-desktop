#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_SRC="$ROOT_DIR/scripts/systemd/slm-polkit-agent.service"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_DST="$UNIT_DIR/slm-polkit-agent.service"

BIN_PATH_DEFAULT="$ROOT_DIR/build/slm-polkit-agent"
BIN_PATH="${1:-$BIN_PATH_DEFAULT}"
BIN_PATH="$(readlink -f "$BIN_PATH")"

if [[ ! -x "$BIN_PATH" ]]; then
  echo "[install-polkit-agent] binary not executable: $BIN_PATH" >&2
  echo "[install-polkit-agent] build first: cmake --build build -j8 --target slm-polkit-agent" >&2
  exit 1
fi

mkdir -p "$UNIT_DIR"
sed "s#%h/Development/Qt/Slm_Desktop/build/slm-polkit-agent#$BIN_PATH#g" "$UNIT_SRC" > "$UNIT_DST"

systemctl --user daemon-reload
systemctl --user enable --now slm-polkit-agent.service

echo "[install-polkit-agent] installed: $UNIT_DST"
systemctl --user --no-pager --full status slm-polkit-agent.service || true
