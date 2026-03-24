#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${SLM_PORTAL_SMOKE_INNER:-}" ]] && command -v dbus-run-session >/dev/null 2>&1; then
  export SLM_PORTAL_SMOKE_INNER=1
  exec dbus-run-session -- "$0" "$@"
fi

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALLER="$ROOT_DIR/scripts/install-portal-user-service.sh"

if [[ ! -x "$INSTALLER" ]]; then
  echo "[portal-profile-smoke] missing executable installer: $INSTALLER" >&2
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

assert_eq() {
  local got="$1"
  local want="$2"
  local msg="$3"
  if [[ "$got" != "$want" ]]; then
    echo "[portal-profile-smoke] assert failed: $msg (got='$got' want='$want')" >&2
    exit 1
  fi
}

assert_contains() {
  local hay="$1"
  local needle="$2"
  local msg="$3"
  if [[ "$hay" != *"$needle"* ]]; then
    echo "[portal-profile-smoke] assert failed: $msg (missing '$needle' in '$hay')" >&2
    exit 1
  fi
}

assert_not_contains() {
  local hay="$1"
  local needle="$2"
  local msg="$3"
  if [[ "$hay" == *"$needle"* ]]; then
    echo "[portal-profile-smoke] assert failed: $msg (unexpected '$needle' in '$hay')" >&2
    exit 1
  fi
}

run_install() {
  local profile="$1"
  SLM_PORTAL_SET_DEFAULT="${2:-0}" "$INSTALLER" /bin/true "$profile" >/dev/null
}

check_mvp() {
  run_install mvp 1
  assert_eq "$(ini_get "$slm_conf" default)" "gtk" "mvp slm-portals.conf default"
  assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.FileChooser)" "slm" "mvp filechooser"
  assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.Screenshot)" "slm" "mvp screenshot"
  assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.ScreenCast)" "" "mvp screencast unset"
}

check_beta() {
  run_install beta 1
  assert_eq "$(ini_get "$slm_conf" default)" "gtk" "beta slm-portals.conf default"
  assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.ScreenCast)" "slm" "beta screencast"
  assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.GlobalShortcuts)" "slm" "beta globalshortcuts"
  assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.InputCapture)" "slm" "beta inputcapture fallback"
}

check_production() {
  run_install production 1
  assert_eq "$(ini_get "$slm_conf" default)" "slm" "production slm-portals.conf default"
  assert_eq "$(ini_get "$slm_conf" org.freedesktop.impl.portal.ScreenCast)" "slm" "production screencast"
  assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.RemoteDesktop)" "slm" "production remotedesktop fallback"
  assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.Camera)" "slm" "production camera fallback"
  assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.Location)" "slm" "production location fallback"
  assert_not_contains "$(ini_get "$slm_conf" org.freedesktop.impl.portal.InputCapture)" "slm" "production inputcapture fallback"
}

selected_profile="${1:-all}"
case "$selected_profile" in
  mvp) check_mvp ;;
  beta) check_beta ;;
  production) check_production ;;
  all)
    check_mvp
    check_beta
    check_production
    ;;
  *)
    echo "[portal-profile-smoke] unknown profile: $selected_profile" >&2
    exit 2
    ;;
esac

# Existing default portals.conf must be respected unless forced.
mkdir -p "$conf_dir"
cat >"$default_conf" <<'EOF'
[preferred]
default=wlr
EOF
run_install beta 0
assert_eq "$(ini_get "$default_conf" default)" "wlr" "non-forced portals.conf should not be overwritten"
run_install production 1
assert_eq "$(ini_get "$default_conf" default)" "slm" "forced portals.conf overwrite"

# Ensure advertised interfaces can cover all slm mappings in beta/production profiles.
portal_file="$ROOT_DIR/scripts/xdg-desktop-portal/slm.portal"
interfaces="$(awk -F= '/^Interfaces=/{print $2}' "$portal_file")"
for conf in "$ROOT_DIR/scripts/xdg-desktop-portal/portals.beta.conf" \
            "$ROOT_DIR/scripts/xdg-desktop-portal/portals.production.conf"; do
  while IFS= read -r key; do
    [[ -z "$key" ]] && continue
    assert_contains "$interfaces" "$key" "slm.portal must advertise mapped interface ($key)"
  done < <(awk -F= '
    BEGIN { in_preferred=0 }
    /^[[:space:]]*\[preferred\][[:space:]]*$/ { in_preferred=1; next }
    /^[[:space:]]*\[/ { in_preferred=0 }
    in_preferred {
      k=$1; v=$2;
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", k);
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v);
      if (k != "default" && v == "slm") print k;
    }
  ' "$conf")
done

echo "[portal-profile-smoke] PASS"
