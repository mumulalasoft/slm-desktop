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
    echo "[fix-greetd-pam] removed stale [initial_session] slm-session-broker autostart"
  fi
  rm -f "$tmp"
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
# pam_systemd.so MUST be required (not optional) so that logind creates a
# local graphical session with seat0.  Without it kwin_wayland cannot open DRM.
session    required     pam_systemd.so
EOF

# Also write /etc/pam.d/slm for the direct-PAM (no-greetd) path.
backup_file /etc/pam.d/slm
cat >/etc/pam.d/slm <<'EOF'
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

cat >/etc/pam.d/greetd-greeter <<'EOF'
#%PAM-1.0
auth       include      common-auth
account    include      common-account
session    required     pam_unix.so
session    required     pam_systemd.so
EOF

remove_stale_slm_initial_session

echo "[fix-greetd-pam] restarting greetd..."
systemctl reset-failed greetd || true
systemctl restart greetd

echo "[fix-greetd-pam] recent logs:"
journalctl -b -u greetd --no-pager -n 60 || true

echo "[fix-greetd-pam] done."
