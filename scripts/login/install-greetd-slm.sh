#!/usr/bin/env bash
set -euo pipefail

# Install/activate greetd as the only display manager for SLM.
# This script is idempotent and intentionally opinionated.

if [[ "${EUID}" -ne 0 ]]; then
  echo "[install-greetd-slm][FAIL] run as root (sudo)." >&2
  exit 1
fi

echo "[install-greetd-slm] ensuring greetd is installed..."
if ! command -v greetd >/dev/null 2>&1; then
  if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y greetd
  else
    echo "[install-greetd-slm][FAIL] greetd not found and no apt-get available." >&2
    exit 1
  fi
fi

echo "[install-greetd-slm] ensuring cage is installed..."
if ! command -v cage >/dev/null 2>&1; then
  if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y cage
  else
    echo "[install-greetd-slm][FAIL] cage not found and no apt-get available." >&2
    echo "[install-greetd-slm][HINT] install cage manually, then rerun." >&2
    exit 1
  fi
fi

GREETER_BIN="$(command -v slm-greeter || true)"
if [[ -z "$GREETER_BIN" ]]; then
  echo "[install-greetd-slm][FAIL] slm-greeter not found on PATH. Install SLM runtime first." >&2
  exit 1
fi

GREETER_CAGE_LAUNCHER="/usr/local/libexec/slm-greeter-cage-launch"
GREETER_LAUNCHER="/usr/local/libexec/slm-greeter-greetd-launch"
BROKER_LAUNCHER="/usr/local/libexec/slm-session-broker-launch"
GREETER_LOG_DIR="/var/lib/greetd/logs"
GREETER_LOG="${GREETER_LOG_DIR}/slm-greeter.log"
CAGE_LOG="${GREETER_LOG_DIR}/slm-greeter-cage.log"
BROKER_LOG="/tmp/slm-session-broker-launch.log"
GREETER_SHELL="${SLM_GREETER_SHELL:-/bin/sh}"

ensure_greeter_user() {
  if ! id -u greeter >/dev/null 2>&1; then
    echo "[install-greetd-slm] creating system user: greeter"
    useradd --system --home /var/lib/greetd --create-home \
      --shell "${GREETER_SHELL}" greeter
  else
    local current_shell
    current_shell="$(getent passwd greeter | awk -F: '{print $7}')"
    if [[ "${current_shell}" == */nologin || "${current_shell}" == */false ]]; then
      echo "[install-greetd-slm] updating greeter shell: ${current_shell} -> ${GREETER_SHELL}"
      usermod --shell "${GREETER_SHELL}" greeter
    fi
  fi

  # The greeter user needs a valid shell for greetd/PAM session setup on Debian,
  # but it must remain non-login-capable through password authentication.
  passwd -l greeter >/dev/null 2>&1 || true
}

ensure_greeter_user

echo "[install-greetd-slm] preparing greeter launcher and logs..."
install -d -m0755 /usr/local/libexec
install -d -m0750 "$GREETER_LOG_DIR"
touch "$GREETER_LOG"
touch "$CAGE_LOG"
chown -R greeter:greeter /var/lib/greetd "$GREETER_LOG_DIR"
chmod 0640 "$GREETER_LOG"
chmod 0640 "$CAGE_LOG"
cat >"$GREETER_LAUNCHER" <<EOF
#!/usr/bin/env bash
# slm-greeter-greetd-launch — run slm-greeter with retry inside an existing cage session.
# Exit 0 = greeter requested logout/session-start (normal).
# Exit non-0 after MAX_RETRIES = persistent failure, cage should exit.
export LIBSEAT_BACKEND=logind
export QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=software
export LIBGL_ALWAYS_SOFTWARE=1

_MAX_RETRIES=5
_attempt=0
_log="${GREETER_LOG}"

_ts() { date --iso-8601=seconds 2>/dev/null || date; }

while [ "\$_attempt" -lt "\$_MAX_RETRIES" ]; do
  _attempt=\$((_attempt + 1))
  {
    echo "===== \$(_ts) slm-greeter-launch attempt \$_attempt/\$_MAX_RETRIES ====="
    echo "  GREETER_BIN=${GREETER_BIN}"
    echo "  GREETD_SOCK=\${GREETD_SOCK:-<unset>}"
    echo "  XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-<unset>}"
    echo "  QT_QPA_PLATFORM=\${QT_QPA_PLATFORM:-<unset>}"
    echo "  WLR_RENDERER=\${WLR_RENDERER:-<unset>}"
  } >>"\$_log" 2>&1

  "${GREETER_BIN}" >>"\$_log" 2>&1
  _rc=\$?

  {
    echo "  greeter exited: code=\$_rc attempt=\$_attempt"
  } >>"\$_log" 2>&1

  # Exit 0 = normal (login succeeded / session handed off).
  if [ "\$_rc" -eq 0 ]; then
    exit 0
  fi

  if [ "\$_attempt" -lt "\$_MAX_RETRIES" ]; then
    echo "  retrying in 1s..." >>"\$_log" 2>&1
    sleep 1
  fi
done

echo "===== \$(_ts) slm-greeter-launch: gave up after \$_MAX_RETRIES attempts =====" >>"\$_log" 2>&1
exit 1
EOF
chmod 0755 "$GREETER_LAUNCHER"
cat >"$BROKER_LAUNCHER" <<'EOF'
#!/usr/bin/env bash
set -u
export SLM_OFFICIAL_SESSION=1
export XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-wayland}
export XDG_CURRENT_DESKTOP=${XDG_CURRENT_DESKTOP:-SLM}
export LANG=${LANG:-C.UTF-8}
export LC_ALL=${LC_ALL:-C.UTF-8}
if [[ -z "${XDG_RUNTIME_DIR:-}" ]]; then
  export XDG_RUNTIME_DIR="/run/user/$(id -u)"
fi
log="/tmp/slm-session-broker-launch.log"
{
  echo "===== $(date --iso-8601=seconds 2>/dev/null || date) slm-session-broker-launch start ====="
  echo "uid=$(id -u) gid=$(id -g) user=$(id -un)"
  echo "argv=$*"
  echo "XDG_SESSION_ID=${XDG_SESSION_ID:-<unset>}"
  echo "XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-<unset>}"
  echo "XDG_SEAT=${XDG_SEAT:-<unset>}"
  echo "XDG_VTNR=${XDG_VTNR:-<unset>}"
  echo "PATH=${PATH:-<unset>}"
  if command -v loginctl >/dev/null 2>&1 && [[ -n "${XDG_SESSION_ID:-}" ]]; then
    loginctl show-session "${XDG_SESSION_ID}" --no-pager 2>&1 || true
  fi
  ldd /usr/libexec/slm-session-broker 2>&1 | grep -E 'not found|libQt6Core|libicu' || true
} >>"$log" 2>&1
exec /usr/libexec/slm-session-broker "$@" >>"$log" 2>&1
EOF
chmod 0755 "$BROKER_LAUNCHER"
cat >"$GREETER_CAGE_LAUNCHER" <<EOF
#!/usr/bin/env bash
# slm-greeter-cage-launch — start cage compositor and run greeter inside it.
export LIBSEAT_BACKEND=logind
export WLR_RENDERER=pixman

_log="${CAGE_LOG}"
_ts() { date --iso-8601=seconds 2>/dev/null || date; }

{
  echo "===== \$(_ts) slm-greeter-cage-launch start ====="
  echo "  XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-<unset>}"
  echo "  GREETD_SOCK=\${GREETD_SOCK:-<unset>}"
  echo "  WLR_RENDERER=\${WLR_RENDERER:-<unset>}"
} >>"\$_log" 2>&1

exec cage -s -- "${GREETER_LAUNCHER}" >>"\$_log" 2>&1
EOF
chmod 0755 "$GREETER_CAGE_LAUNCHER"

echo "[install-greetd-slm] disabling competing display managers..."
for dm in gdm gdm3 sddm lightdm lxdm; do
  if systemctl list-unit-files | grep -q "^${dm}\\.service"; then
    systemctl disable --now "${dm}.service" || true
  fi
done

echo "[install-greetd-slm] writing /etc/greetd/config.toml..."
mkdir -p /etc/greetd
cat >/etc/greetd/config.toml <<'EOF'
[terminal]
vt = 1

[default_session]
# Run SLM greeter inside cage (required for Qt/QML greeter).
command = "__SLM_GREETER_CAGE_LAUNCHER__"
user = "greeter"
EOF

sed -i "s#__SLM_GREETER_CAGE_LAUNCHER__#${GREETER_CAGE_LAUNCHER//\//\\/}#g" /etc/greetd/config.toml

echo "[install-greetd-slm] setting display-manager.service -> greetd.service ..."
GREETD_UNIT_PATH="$(systemctl show -p FragmentPath --value greetd.service 2>/dev/null || true)"
if [[ -z "$GREETD_UNIT_PATH" || ! -f "$GREETD_UNIT_PATH" ]]; then
  if [[ -f /lib/systemd/system/greetd.service ]]; then
    GREETD_UNIT_PATH="/lib/systemd/system/greetd.service"
  elif [[ -f /usr/lib/systemd/system/greetd.service ]]; then
    GREETD_UNIT_PATH="/usr/lib/systemd/system/greetd.service"
  else
    echo "[install-greetd-slm][FAIL] cannot locate greetd.service unit file." >&2
    exit 1
  fi
fi
ln -sfn "$GREETD_UNIT_PATH" /etc/systemd/system/display-manager.service
systemctl daemon-reload

echo "[install-greetd-slm] enabling greetd..."
systemctl enable --now greetd.service
systemctl reset-failed greetd.service || true
systemctl restart greetd.service || true

echo "[install-greetd-slm] done."
echo "Logs:"
echo "  journalctl -b -u greetd --no-pager -n 200"
echo "  cat ${GREETER_LOG}"
echo "  cat ${CAGE_LOG}"
