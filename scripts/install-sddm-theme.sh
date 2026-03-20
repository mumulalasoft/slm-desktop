#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_NATIVE="$ROOT_DIR/sddm/theme/SLM"
SRC_LIGHT="$ROOT_DIR/sddm/theme/Light"
SRC_DARK="$ROOT_DIR/sddm/theme/Dark"
DST_BASE="/usr/share/sddm/themes"
DST_NATIVE="$DST_BASE/SLM"
DST_LIGHT="$DST_BASE/SLM-Light"
DST_DARK="$DST_BASE/SLM-Dark"

THEME_VARIANT="${1:-slm}"
case "${THEME_VARIANT,,}" in
  slm|native)
    SELECTED="SLM"
    ;;
  light)
    SELECTED="SLM-Light"
    ;;
  dark)
    SELECTED="SLM-Dark"
    ;;
  *)
    echo "[install-sddm-theme] unknown variant: $THEME_VARIANT" >&2
    echo "[install-sddm-theme] use: slm | light | dark" >&2
    exit 1
    ;;
esac

echo "[install-sddm-theme] installing themes to $DST_BASE"
sudo mkdir -p "$DST_BASE"
sudo rm -rf "$DST_NATIVE" "$DST_LIGHT" "$DST_DARK"
sudo cp -a "$SRC_NATIVE" "$DST_NATIVE"
sudo cp -a "$SRC_LIGHT" "$DST_LIGHT"
sudo cp -a "$SRC_DARK" "$DST_DARK"

echo "[install-sddm-theme] setting current theme: $SELECTED"
sudo mkdir -p /etc/sddm.conf.d
sudo tee /etc/sddm.conf.d/10-slm-theme.conf >/dev/null <<EOC
[Theme]
Current=$SELECTED
EOC

echo "[install-sddm-theme] done"
echo "[install-sddm-theme] restart service: sudo systemctl restart sddm"
