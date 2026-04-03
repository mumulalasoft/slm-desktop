#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${SLM_PORTAL_ARTIFACT_SMOKE_INNER:-}" ]] && command -v dbus-run-session >/dev/null 2>&1; then
  export SLM_PORTAL_ARTIFACT_SMOKE_INNER=1
  exec dbus-run-session -- "$0" "$@"
fi

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALLER="$ROOT_DIR/scripts/install-portal-user-service.sh"

if [[ ! -x "$INSTALLER" ]]; then
  echo "[portal-artifacts-smoke] missing executable installer: $INSTALLER" >&2
  exit 2
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

export XDG_CONFIG_HOME="$tmp/config"
export XDG_DATA_HOME="$tmp/data"
export SLM_PORTAL_SKIP_SYSTEMCTL=1
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME"

conf_dir="$XDG_CONFIG_HOME/xdg-desktop-portal"
default_conf="$conf_dir/portals.conf"
slm_conf="$conf_dir/slm-portals.conf"
portal_file="$XDG_DATA_HOME/xdg-desktop-portal/portals/slm.portal"
dbus_service="$XDG_DATA_HOME/dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service"
unit_file="$XDG_CONFIG_HOME/systemd/user/slm-portald.service"

assert_file() {
  local f="$1"
  if [[ ! -f "$f" ]]; then
    echo "[portal-artifacts-smoke] missing file: $f" >&2
    exit 1
  fi
}

assert_contains_file() {
  local f="$1"
  local pattern="$2"
  local msg="$3"
  if ! grep -Fq "$pattern" "$f"; then
    echo "[portal-artifacts-smoke] assert failed: $msg (missing '$pattern' in $f)" >&2
    exit 1
  fi
}

ini_get() {
  local file="$1"
  local key="$2"
  awk -F= -v key="$key" '
    BEGIN { in_preferred=0 }
    /^[[:space:]]*\[preferred\][[:space:]]*$/ { in_preferred=1; next }
    /^[[:space:]]*\[/ { in_preferred=0 }
    in_preferred {
      k=$1; v=$2;
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", k);
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v);
      if (k == key) { print v; exit 0 }
    }
  ' "$file"
}

BIN_PATH="/bin/true"
profile="${1:-beta}"

assert_eq() {
  local got="$1"
  local want="$2"
  local msg="$3"
  if [[ "$got" != "$want" ]]; then
    echo "[portal-artifacts-smoke] assert failed: $msg (got='$got' want='$want')" >&2
    exit 1
  fi
}

assert_not_contains() {
  local hay="$1"
  local needle="$2"
  local msg="$3"
  if [[ "$hay" == *"$needle"* ]]; then
    echo "[portal-artifacts-smoke] assert failed: $msg (unexpected '$needle' in '$hay')" >&2
    exit 1
  fi
}

# Install selected profile to validate promoted interfaces and artifacts.
SLM_PORTAL_SET_DEFAULT=1 "$INSTALLER" "$BIN_PATH" "$profile" >/dev/null

assert_file "$default_conf"
assert_file "$slm_conf"
assert_file "$portal_file"
assert_file "$dbus_service"
assert_file "$unit_file"

assert_contains_file "$dbus_service" "Name=org.freedesktop.impl.portal.desktop.slm" "dbus name"
assert_contains_file "$dbus_service" "Exec=$BIN_PATH" "dbus service exec path substitution"
assert_contains_file "$portal_file" "DBusName=org.freedesktop.impl.portal.desktop.slm" "portal dbus name"
assert_contains_file "$portal_file" "UseIn=SLM;" "portal UseIn"
assert_contains_file "$portal_file" "org.freedesktop.impl.portal.GlobalShortcuts" "portal globalshortcuts iface"
assert_contains_file "$portal_file" "org.freedesktop.impl.portal.InputCapture" "portal inputcapture iface"
assert_contains_file "$unit_file" "ExecStart=$BIN_PATH" "systemd unit exec path substitution"

case "$profile" in
  mvp)
    assert_eq "$(ini_get "$slm_conf" default)" "gtk" "mvp default"
    assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.ScreenCast)" "" "mvp screencast unset"
    ;;
  beta)
    assert_eq "$(ini_get "$slm_conf" default)" "gtk" "beta default"
    assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.ScreenCast)" "slm" "beta screencast"
    assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.InputCapture)" "slm" "beta inputcapture fallback"
    ;;
  production)
    assert_eq "$(ini_get "$slm_conf" default)" "slm" "production default"
    assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.ScreenCast)" "slm" "production screencast"
    assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.RemoteDesktop)" "slm" "production remotedesktop fallback"
    assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.Camera)" "slm" "production camera fallback"
    assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.Location)" "slm" "production location fallback"
    assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.InputCapture)" "slm" "production inputcapture fallback"
    ;;
  *)
    echo "[portal-artifacts-smoke] unknown profile: $profile" >&2
    exit 2
    ;;
esac

# Verify existing global portals.conf remains untouched when not forced.
cat >"$default_conf" <<'EOF'
[preferred]
default=wlr
EOF
SLM_PORTAL_SET_DEFAULT=0 "$INSTALLER" "$BIN_PATH" production >/dev/null
if [[ "$(ini_get "$default_conf" default)" != "wlr" ]]; then
  echo "[portal-artifacts-smoke] existing portals.conf should not be overwritten when not forced" >&2
  exit 1
fi
if [[ "$(ini_get "$slm_conf" default)" != "slm" ]]; then
  echo "[portal-artifacts-smoke] production slm-portals.conf should be updated to default=slm" >&2
  exit 1
fi

echo "[portal-artifacts-smoke] PASS"
