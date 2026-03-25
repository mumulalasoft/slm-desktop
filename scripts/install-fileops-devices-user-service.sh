#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SPLIT_INSTALLER="$ROOT_DIR/scripts/install-split-fileops-devices-user-services.sh"

if [[ ! -x "$SPLIT_INSTALLER" ]]; then
  echo "[install-fileops-devices] missing installer: $SPLIT_INSTALLER" >&2
  exit 1
fi

echo "[install-fileops-devices] desktopd embed mode is deprecated."
echo "[install-fileops-devices] installing split services (slm-fileopsd + slm-devicesd + slm-portald)..."
if [[ $# -ge 2 ]]; then
  "$SPLIT_INSTALLER" "$1" "$2"
elif [[ $# -eq 1 ]]; then
  "$SPLIT_INSTALLER" "$1"
else
  "$SPLIT_INSTALLER"
fi

echo "[install-fileops-devices] done"
