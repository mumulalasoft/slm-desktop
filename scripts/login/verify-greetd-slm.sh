#!/usr/bin/env bash
set -euo pipefail

echo "[verify-greetd-slm] start"

fail=0
warn=0

check_file() {
  local path="$1"
  if [[ -f "$path" ]]; then
    echo "[OK] file: $path"
  else
    echo "[FAIL] missing file: $path"
    fail=$((fail + 1))
  fi
}

check_cmd() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "[OK] command: $cmd"
  else
    echo "[WARN] command missing: $cmd"
    warn=$((warn + 1))
  fi
}

check_file "/usr/share/wayland-sessions/slm.desktop"
check_file "/etc/greetd/config.toml"

check_cmd "greetd"
check_cmd "slm-greeter"
check_cmd "slm-watchdog"
check_cmd "slm-recovery-app"
if [[ -x /usr/libexec/slm-session-broker ]]; then
  echo "[OK] binary: /usr/libexec/slm-session-broker"
else
  echo "[FAIL] missing executable: /usr/libexec/slm-session-broker"
  fail=$((fail + 1))
fi

if getent passwd greeter >/dev/null 2>&1; then
  greeter_shell="$(getent passwd greeter | awk -F: '{print $7}')"
  if [[ "$greeter_shell" == */nologin || "$greeter_shell" == */false ]]; then
    echo "[FAIL] greeter user has non-login shell: $greeter_shell"
    echo "[HINT] run: sudo usermod --shell /bin/sh greeter && sudo passwd -l greeter"
    fail=$((fail + 1))
  else
    echo "[OK] greeter user shell is PAM-compatible: $greeter_shell"
  fi
else
  echo "[FAIL] missing greeter user"
  fail=$((fail + 1))
fi

if [[ -L /etc/systemd/system/display-manager.service ]]; then
  target="$(readlink -f /etc/systemd/system/display-manager.service || true)"
  if [[ "$target" == *"/greetd.service" ]]; then
    echo "[OK] display-manager.service points to greetd.service"
  else
    echo "[WARN] display-manager.service points elsewhere: $target"
    warn=$((warn + 1))
  fi
else
  echo "[WARN] /etc/systemd/system/display-manager.service is not a symlink"
  warn=$((warn + 1))
fi

if [[ -f /etc/greetd/config.toml ]]; then
  if grep -q '^\[default_session\]' /etc/greetd/config.toml; then
    echo "[OK] greetd config has default_session"
  else
    echo "[FAIL] greetd config missing [default_session]"
    fail=$((fail + 1))
  fi

  if grep -Eq '^command = ".*slm-greeter-cage-launch"' /etc/greetd/config.toml; then
    echo "[OK] greetd default_session launches cage wrapper for greeter"
  elif grep -Eq '^command = "env LIBSEAT_BACKEND=logind cage -s -- .*slm-greeter' /etc/greetd/config.toml; then
    echo "[WARN] greetd default_session launches slm-greeter directly via cage"
    warn=$((warn + 1))
  else
    echo "[FAIL] greetd default_session is not wired to slm-greeter via cage"
    fail=$((fail + 1))
  fi

  if grep -q '^\[initial_session\]' /etc/greetd/config.toml; then
    echo "[FAIL] greetd config still contains [initial_session] and may bypass greeter"
    fail=$((fail + 1))
  else
    echo "[OK] greetd config does not bypass greeter with initial_session"
  fi
fi

if systemctl list-unit-files >/dev/null 2>&1; then
  if systemctl is-enabled greetd.service >/dev/null 2>&1; then
    echo "[OK] greetd enabled"
  else
    echo "[FAIL] greetd not enabled"
    fail=$((fail + 1))
  fi

  if systemctl is-active greetd.service >/dev/null 2>&1; then
    echo "[OK] greetd active"
  else
    echo "[WARN] greetd not active right now"
    warn=$((warn + 1))
  fi

  for dm in gdm.service gdm3.service sddm.service lightdm.service; do
    if systemctl list-unit-files | grep -q "^${dm}"; then
      if systemctl is-enabled "${dm}" >/dev/null 2>&1; then
        echo "[WARN] competing DM still enabled: ${dm}"
        warn=$((warn + 1))
      else
        echo "[OK] competing DM disabled: ${dm}"
      fi
    fi
  done
else
  echo "[WARN] systemctl unavailable/unreachable; skip service-state checks"
  warn=$((warn + 1))
fi

if [[ -f /usr/share/wayland-sessions/slm.desktop ]]; then
  if grep -q "^Exec=/usr/libexec/slm-session-broker" /usr/share/wayland-sessions/slm.desktop; then
    echo "[OK] session broker wired in slm.desktop"
  else
    echo "[FAIL] slm.desktop Exec is not /usr/libexec/slm-session-broker"
    fail=$((fail + 1))
  fi
else
  echo "[WARN] slm.desktop not installed system-wide yet"
  warn=$((warn + 1))
fi

if [[ "${fail}" -eq 0 ]]; then
  if [[ "${warn}" -gt 0 ]]; then
    echo "[verify-greetd-slm] PASS with warnings"
  else
    echo "[verify-greetd-slm] PASS"
  fi
else
  echo "[verify-greetd-slm] FAIL"
fi

echo "[verify-greetd-slm] summary fail=${fail} warn=${warn}"
if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
