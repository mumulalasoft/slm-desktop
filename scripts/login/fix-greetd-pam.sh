#!/usr/bin/env bash
set -euo pipefail

# Fix greetd PAM stack on systems where pam_lastlog.so is unavailable.
# We avoid touching /etc/pam.d/login globally by providing greetd-specific PAM.

if [[ "${EUID}" -ne 0 ]]; then
  echo "[fix-greetd-pam][FAIL] run as root (sudo)." >&2
  exit 1
fi

backup_file() {
  local f="$1"
  if [[ -f "$f" ]]; then
    cp -a "$f" "${f}.bak.$(date +%Y%m%d-%H%M%S)"
  fi
}

echo "[fix-greetd-pam] writing /etc/pam.d/greetd and /etc/pam.d/greetd-greeter ..."
backup_file /etc/pam.d/greetd
backup_file /etc/pam.d/greetd-greeter

cat >/etc/pam.d/greetd <<'EOF'
#%PAM-1.0
auth       requisite    pam_nologin.so
auth       include      common-auth
account    include      common-account
password   include      common-password
session    required     pam_env.so readenv=1
session    required     pam_limits.so
session    required     pam_unix.so
session    optional     pam_systemd.so
session    optional     pam_gnome_keyring.so auto_start
EOF

cat >/etc/pam.d/greetd-greeter <<'EOF'
#%PAM-1.0
auth       include      common-auth
account    include      common-account
session    required     pam_unix.so
session    optional     pam_systemd.so
EOF

echo "[fix-greetd-pam] restarting greetd..."
systemctl reset-failed greetd || true
systemctl restart greetd

echo "[fix-greetd-pam] recent logs:"
journalctl -b -u greetd --no-pager -n 60 || true

echo "[fix-greetd-pam] done."
