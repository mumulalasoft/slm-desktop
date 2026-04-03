#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESKTOPD_BIN_DEFAULT="$ROOT_DIR/build/desktopd"
DESKTOPD_BIN="${1:-$DESKTOPD_BIN_DEFAULT}"
DESKTOPD_BIN="$(readlink -f "$DESKTOPD_BIN")"

if [[ ! -x "$DESKTOPD_BIN" ]]; then
  echo "[install-globalsearch] desktopd binary not executable: $DESKTOPD_BIN" >&2
  echo "[install-globalsearch] build first: cmake --build build -j8 --target desktopd" >&2
  exit 1
fi

echo "[install-globalsearch] GlobalSearchService is hosted by slm-desktopd."
echo "[install-globalsearch] Installing/starting slm-desktopd service..."
"$ROOT_DIR/scripts/install-desktopd-user-service.sh" "$DESKTOPD_BIN"

echo "[install-globalsearch] verifying DBus name: org.slm.Desktop.Search.v1"
if busctl --user --list | awk '{print $1}' | grep -qx "org.slm.Desktop.Search.v1"; then
  echo "[install-globalsearch] DBus service is active."
else
  echo "[install-globalsearch] DBus name not visible yet. Checking desktopd status..." >&2
  systemctl --user --no-pager --full status slm-desktopd.service || true
  exit 1
fi

echo "[install-globalsearch] quick probe GetActiveSearchProfile:"
busctl --user call \
  org.slm.Desktop.Search.v1 \
  /org/slm/Desktop/Search \
  org.slm.Desktop.Search.v1 \
  GetActiveSearchProfile

echo "[install-globalsearch] done."
