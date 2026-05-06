#!/usr/bin/env bash
# setup-slm-session-manager.sh — Install SLM direct-PAM session manager.
#
# What this does:
#   1. Write /etc/pam.d/slm (for slm-greeter direct PAM path)
#   2. Write /etc/pam.d/greetd with pam_systemd.so required (if greetd is kept)
#   3. Install slm-session-check diagnostic tool
#   4. Install slm-greeter.service systemd unit
#   5. Optionally enable the service
#
# Usage:
#   sudo bash scripts/login/setup-slm-session-manager.sh [--enable]
#
# --enable   Disable greetd and enable slm-greeter.service as the DM.
#            Without this flag only files are installed; nothing is enabled.

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "[setup-slm][FAIL] run as root (sudo)." >&2
  exit 1
fi

ENABLE_SERVICE=0
for arg in "$@"; do
  [[ "$arg" == "--enable" ]] && ENABLE_SERVICE=1
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

backup_file() {
  local f="$1"
  [[ -f "$f" ]] && cp -a "$f" "${f}.bak.$(date +%Y%m%d-%H%M%S)" || true
}

remove_stale_slm_initial_session() {
  local cfg="/etc/greetd/config.toml"
  [[ -f "$cfg" ]] || return 0

  local tmp
  tmp="$(mktemp)"
  awk '
    /^\[[^]]+\]/ {
      if (in_initial) {
        if (!remove_block) {
          printf "%s", block
        }
        in_initial = 0
        block = ""
        remove_block = 0
      }
      if ($0 == "[initial_session]") {
        in_initial = 1
        block = $0 "\n"
        next
      }
    }
    in_initial {
      block = block $0 "\n"
      if ($0 ~ /slm-session-broker/) {
        remove_block = 1
      }
      next
    }
    { print }
    END {
      if (in_initial && !remove_block) {
        printf "%s", block
      }
    }
  ' "$cfg" >"$tmp"

  if ! cmp -s "$cfg" "$tmp"; then
    backup_file "$cfg"
    install -m644 "$tmp" "$cfg"
    echo "[setup-slm][OK] removed stale [initial_session] slm-session-broker autostart"
  fi
  rm -f "$tmp"
}

echo "[setup-slm] Writing PAM service files..."

backup_file /etc/pam.d/slm
cat >/etc/pam.d/slm <<'EOF'
#%PAM-1.0
# SLM direct PAM authentication service.
# pam_systemd.so MUST be required so that logind creates a local graphical
# session on seat0. Making it optional causes kwin_wayland to fail ("Failed
# to open drm node") because the session has no seat assigned.
auth       requisite    pam_nologin.so
auth       include      common-auth
account    include      common-account
password   include      common-password
session    required     pam_env.so readenv=1
session    required     pam_limits.so
session    required     pam_unix.so
session    optional     pam_loginuid.so
session    required     pam_systemd.so
EOF
echo "[setup-slm][OK] /etc/pam.d/slm"

# Fix greetd PAM file if greetd is installed.
if [[ -f /etc/pam.d/greetd ]]; then
  backup_file /etc/pam.d/greetd
  cat >/etc/pam.d/greetd <<'EOF'
#%PAM-1.0
auth       requisite    pam_nologin.so
auth       include      common-auth
account    include      common-account
password   include      common-password
session    required     pam_env.so readenv=1
session    required     pam_limits.so
session    required     pam_unix.so
session    required     pam_systemd.so
EOF
  echo "[setup-slm][OK] /etc/pam.d/greetd (pam_systemd.so is now required)"
  systemctl reset-failed greetd.service 2>/dev/null || true
  systemctl restart greetd.service 2>/dev/null || true
fi

remove_stale_slm_initial_session

echo "[setup-slm] Installing slm-session-check..."
install -m755 "$ROOT_DIR/scripts/login/slm-session-check.sh" /usr/local/bin/slm-session-check
echo "[setup-slm][OK] /usr/local/bin/slm-session-check"

echo "[setup-slm] Installing slm-greeter.service..."
install -m644 "$ROOT_DIR/scripts/systemd/system/slm-greeter.service" \
              /etc/systemd/system/slm-greeter.service
echo "[setup-slm][OK] /etc/systemd/system/slm-greeter.service"

systemctl daemon-reload

if [[ "$ENABLE_SERVICE" == "1" ]]; then
  echo "[setup-slm] Disabling greetd and enabling slm-greeter.service..."
  systemctl disable --now greetd.service 2>/dev/null || true
  ln -sfn /etc/systemd/system/slm-greeter.service \
          /etc/systemd/system/display-manager.service
  systemctl enable --now slm-greeter.service
  echo "[setup-slm][OK] slm-greeter.service enabled as display manager"
else
  echo "[setup-slm] Files installed. To enable as DM:"
  echo "   sudo bash $0 --enable"
fi

echo ""
echo "[setup-slm] Quick verification:"
echo "   sudo cat /etc/pam.d/slm"
echo "   sudo cat /etc/pam.d/greetd"
echo "   slm-session-check    (run inside a logged-in session)"
echo ""
echo "[setup-slm] Rollback:"
echo "   sudo systemctl disable slm-greeter.service"
echo "   sudo systemctl enable --now greetd.service"
echo "   sudo cp /etc/pam.d/slm.bak.<timestamp> /etc/pam.d/slm"
echo "[setup-slm] done."
