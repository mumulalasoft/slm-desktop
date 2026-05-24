#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT_DIR/scripts/polkit/org.slm.desktop.fsd.policy"
DST="/usr/share/polkit-1/actions/org.slm.desktop.fsd.policy"

if [[ ! -f "$SRC" ]]; then
  echo "[install-polkit-fsd] policy source not found: $SRC" >&2
  exit 1
fi

echo "[install-polkit-fsd] installing policy to $DST (requires root)"
sudo install -m 0644 "$SRC" "$DST"
echo "[install-polkit-fsd] done"
