#!/usr/bin/env bash
set -euo pipefail

TARGET_USER="${SUDO_USER:-${SLM_TARGET_USER:-}}"

fail=0
warn=0

ok()   { echo "[OK] $*"; }
ng()   { echo "[FAIL] $*"; fail=$((fail + 1)); }
warnf(){ echo "[WARN] $*"; warn=$((warn + 1)); }

check_file() {
  local f="$1"
  [[ -f "$f" ]] && ok "file $f" || ng "missing file $f"
}

check_exec() {
  local f="$1"
  [[ -x "$f" ]] && ok "bin $f" || ng "missing executable $f"
}

echo "[verify-slm-runtime] start"

check_exec "/usr/local/bin/slm-greeter"
check_exec "/usr/local/bin/slm-watchdog"
check_exec "/usr/local/bin/slm-recovery-app"
check_exec "/usr/libexec/slm-session-broker"
check_exec "/usr/local/libexec/slm/recovery/request-bootloader-recovery.sh"
check_exec "/usr/local/libexec/slm/recovery/detect-recovery-boot-entry.sh"
check_exec "/usr/local/libexec/slm/recovery/install-recovery-boot-entry.sh"
check_exec "/usr/local/libexec/slm/recovery/build-recovery-partition-image.sh"

for opt in /usr/local/bin/slm-shell /usr/local/bin/desktopd /usr/local/bin/slm-svcmgrd \
           /usr/local/bin/slm-loggerd /usr/local/bin/slm-portald /usr/local/bin/slm-fileopsd \
           /usr/local/bin/slm-devicesd /usr/local/bin/slm-clipboardd /usr/local/bin/slm-envd \
           /usr/local/bin/slm-polkit-agent /usr/local/bin/slm-recoveryd; do
  [[ -x "$opt" ]] && ok "optional bin $opt" || warnf "optional bin missing $opt"
done

check_file "/usr/share/wayland-sessions/slm.desktop"
if [[ -f "/usr/share/wayland-sessions/slm.desktop" ]]; then
  if grep -q "^Exec=/usr/libexec/slm-session-broker" /usr/share/wayland-sessions/slm.desktop; then
    ok "session entry uses slm-session-broker"
  else
    ng "session entry not wired to /usr/libexec/slm-session-broker"
  fi
fi

if [[ -n "$TARGET_USER" ]]; then
  USER_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
  if [[ -n "$USER_HOME" ]]; then
    for unit in slm-desktopd.service slm-portald.service slm-fileopsd.service \
                slm-devicesd.service slm-clipboardd.service slm-polkit-agent.service slm-envd.service \
                slm-recoveryd.service; do
      if [[ -f "$USER_HOME/.config/systemd/user/$unit" ]]; then
        ok "user unit installed $unit"
      else
        warnf "user unit missing $unit"
      fi
    done
  else
    warnf "cannot resolve home for TARGET_USER=$TARGET_USER"
  fi
else
  warnf "TARGET_USER not set; skip user unit checks"
fi

echo "[verify-slm-runtime] summary fail=${fail} warn=${warn}"
if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
