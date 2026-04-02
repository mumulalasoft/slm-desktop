#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UNIT_SRC="$ROOT_DIR/scripts/systemd/slm-portald.service"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_DST="$UNIT_DIR/slm-portald.service"
XDP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/xdg-desktop-portal"
XDP_PORTAL_DIR="$XDP_DIR/portals"
XDP_PORTAL_FILE="$XDP_PORTAL_DIR/slm.portal"
XDP_CONF_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/xdg-desktop-portal"
XDP_CONF_FILE="$XDP_CONF_DIR/portals.conf"
XDP_DESKTOP_CONF_FILE="$XDP_CONF_DIR/slm-portals.conf"
DBUS_SERVICE_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/dbus-1/services"
DBUS_SERVICE_FILE="$DBUS_SERVICE_DIR/org.freedesktop.impl.portal.desktop.slm.service"
PORTAL_TEMPLATE="$ROOT_DIR/scripts/xdg-desktop-portal/slm.portal"
PORTAL_CONF_TEMPLATE_MVP="$ROOT_DIR/scripts/xdg-desktop-portal/portals.mvp.conf"
PORTAL_CONF_TEMPLATE_BETA="$ROOT_DIR/scripts/xdg-desktop-portal/portals.beta.conf"
PORTAL_CONF_TEMPLATE_PROD="$ROOT_DIR/scripts/xdg-desktop-portal/portals.production.conf"
PORTAL_CONF_TEMPLATE="$ROOT_DIR/scripts/xdg-desktop-portal/portals.conf"
DBUS_SERVICE_TEMPLATE="$ROOT_DIR/scripts/xdg-desktop-portal/org.freedesktop.impl.portal.desktop.slm.service"

BIN_PATH_DEFAULT="$ROOT_DIR/build/slm-portald"
BIN_PATH="${1:-$BIN_PATH_DEFAULT}"
PORTAL_PROFILE_RAW="${2:-${SLM_PORTAL_PROFILE:-mvp}}"
PORTAL_PROFILE="$(printf '%s' "$PORTAL_PROFILE_RAW" | tr '[:upper:]' '[:lower:]')"
SET_DEFAULT="${SLM_PORTAL_SET_DEFAULT:-0}"
SKIP_SYSTEMCTL="${SLM_PORTAL_SKIP_SYSTEMCTL:-0}"

resolve_portal_template() {
  case "$PORTAL_PROFILE" in
    mvp)
      printf '%s\n' "$PORTAL_CONF_TEMPLATE_MVP"
      ;;
    beta)
      printf '%s\n' "$PORTAL_CONF_TEMPLATE_BETA"
      ;;
    production|prod)
      printf '%s\n' "$PORTAL_CONF_TEMPLATE_PROD"
      ;;
    legacy)
      printf '%s\n' "$PORTAL_CONF_TEMPLATE"
      ;;
    *)
      echo "[install-portald] unknown portal profile: $PORTAL_PROFILE" >&2
      echo "[install-portald] use one of: mvp | beta | production | legacy" >&2
      exit 1
      ;;
  esac
}

PORTAL_PROFILE_TEMPLATE="$(resolve_portal_template)"

if [[ ! -x "$BIN_PATH" ]]; then
  echo "[install-portald] binary not executable: $BIN_PATH" >&2
  echo "[install-portald] build first: cmake --build build -j8" >&2
  exit 1
fi

mkdir -p "$UNIT_DIR"
sed "s#%h/Development/Qt/Slm_Desktop/build/slm-portald#$BIN_PATH#g" "$UNIT_SRC" > "$UNIT_DST"

mkdir -p "$XDP_PORTAL_DIR"
cp "$PORTAL_TEMPLATE" "$XDP_PORTAL_FILE"

mkdir -p "$XDP_CONF_DIR"
cp "$PORTAL_PROFILE_TEMPLATE" "$XDP_DESKTOP_CONF_FILE"
if [[ "$SET_DEFAULT" == "1" || ! -f "$XDP_CONF_FILE" ]]; then
  cp "$PORTAL_PROFILE_TEMPLATE" "$XDP_CONF_FILE"
fi

mkdir -p "$DBUS_SERVICE_DIR"
sed "s#%h/Development/Qt/Slm_Desktop/build/slm-portald#$BIN_PATH#g" "$DBUS_SERVICE_TEMPLATE" > "$DBUS_SERVICE_FILE"

if [[ "$SKIP_SYSTEMCTL" != "1" ]]; then
  systemctl --user daemon-reload
  systemctl --user enable --now slm-portald.service
  systemctl --user restart xdg-desktop-portal.service || true
else
  echo "[install-portald] skipping systemctl operations (SLM_PORTAL_SKIP_SYSTEMCTL=1)"
fi

echo "[install-portald] installed: $UNIT_DST"
echo "[install-portald] installed: $XDP_PORTAL_FILE"
echo "[install-portald] installed: $XDP_CONF_FILE"
echo "[install-portald] installed: $XDP_DESKTOP_CONF_FILE"
echo "[install-portald] installed: $DBUS_SERVICE_FILE"
echo "[install-portald] portal profile: $PORTAL_PROFILE"
echo "[install-portald] set default portals.conf: $SET_DEFAULT"
echo "[install-portald] skip systemctl: $SKIP_SYSTEMCTL"
echo "[install-portald] status:"
if [[ "$SKIP_SYSTEMCTL" != "1" ]]; then
  systemctl --user --no-pager --full status slm-portald.service || true
fi
