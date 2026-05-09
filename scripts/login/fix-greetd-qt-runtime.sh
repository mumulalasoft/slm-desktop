#!/usr/bin/env bash
set -euo pipefail

# Fix greetd startup when SLM binaries require a newer Qt runtime than host.
#
# What this script does:
# 1) Copies Qt runtime to /opt/slm/qt6.10 (readable for greeter user)
# 2) Creates launcher wrappers with Qt env vars
# 3) Rewrites /etc/greetd/config.toml to use wrappers
# 4) Ensures greeter system user exists
# 5) Restarts greetd and prints recent logs
#
# Usage:
#   sudo bash scripts/login/fix-greetd-qt-runtime.sh
#   sudo SLM_QT_SRC=/home/<user>/Qt/6.10.2/gcc_64 bash scripts/login/fix-greetd-qt-runtime.sh

if [[ "${EUID}" -ne 0 ]]; then
  echo "[fix-greetd-qt-runtime][FAIL] run as root (sudo)." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_USER="${SUDO_USER:-${USER:-}}"
DEFAULT_QT_SRC="/home/${SRC_USER}/Qt/6.10.2/gcc_64"
QT_SRC="${SLM_QT_SRC:-$DEFAULT_QT_SRC}"
QT_DST="${SLM_QT_DST:-/opt/slm/qt6.10}"

GREETER_BIN="${SLM_GREETER_BIN:-/usr/local/bin/slm-greeter}"
BROKER_BIN="${SLM_BROKER_BIN:-/usr/libexec/slm-session-broker}"

GREETER_LAUNCHER="/usr/local/libexec/slm-greeter-launch"
BROKER_LAUNCHER="/usr/local/libexec/slm-session-broker-launch"
GREETD_CFG="/etc/greetd/config.toml"
LOG_DIR="/var/lib/greetd/logs"
GREETER_LOG="${LOG_DIR}/slm-greeter.log"
GREETER_SHELL="${SLM_GREETER_SHELL:-/bin/sh}"

echo "[fix-greetd-qt-runtime] qt-src=${QT_SRC}"
echo "[fix-greetd-qt-runtime] qt-dst=${QT_DST}"

if [[ ! -d "${QT_SRC}" ]]; then
  echo "[fix-greetd-qt-runtime][FAIL] Qt source dir not found: ${QT_SRC}" >&2
  echo "[fix-greetd-qt-runtime][HINT] set SLM_QT_SRC=/path/to/Qt/6.10.x/gcc_64" >&2
  exit 1
fi

if [[ ! -x "${GREETER_BIN}" ]]; then
  echo "[fix-greetd-qt-runtime][FAIL] greeter binary not found: ${GREETER_BIN}" >&2
  exit 1
fi
if [[ ! -x "${BROKER_BIN}" ]]; then
  echo "[fix-greetd-qt-runtime][FAIL] session broker binary not found: ${BROKER_BIN}" >&2
  exit 1
fi

echo "[fix-greetd-qt-runtime] syncing Qt runtime..."
mkdir -p "${QT_DST}"
rsync -a --delete "${QT_SRC}/" "${QT_DST}/"
chmod -R a+rX "${QT_DST}"

echo "[fix-greetd-qt-runtime] creating launchers..."
mkdir -p /usr/local/libexec
mkdir -p "${LOG_DIR}"

cat > "${GREETER_LAUNCHER}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
export LD_LIBRARY_PATH="${QT_DST}/lib:\${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${QT_DST}/plugins"
export QML2_IMPORT_PATH="${QT_DST}/qml"
export QT_LOGGING_RULES="\${QT_LOGGING_RULES:-qt.qpa.*=true;qt.qml.*=true}"
{
  echo "===== \$(date --iso-8601=seconds) slm-greeter-launch start ====="
  echo "uid=\$(id -u) gid=\$(id -g) user=\$(id -un)"
  echo "LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}"
  echo "QT_PLUGIN_PATH=\${QT_PLUGIN_PATH}"
  echo "QML2_IMPORT_PATH=\${QML2_IMPORT_PATH}"
  echo "argv=\$*"
} >> "${GREETER_LOG}"
exec "${GREETER_BIN}" "\$@" >> "${GREETER_LOG}" 2>&1
EOF
chmod 0755 "${GREETER_LAUNCHER}"

cat > "${BROKER_LAUNCHER}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
export LD_LIBRARY_PATH="${QT_DST}/lib:\${LD_LIBRARY_PATH:-}"
export SLM_OFFICIAL_SESSION=1
export XDG_SESSION_TYPE=\${XDG_SESSION_TYPE:-wayland}
export XDG_CURRENT_DESKTOP=\${XDG_CURRENT_DESKTOP:-SLM}
export LANG=\${LANG:-C.UTF-8}
export LC_ALL=\${LC_ALL:-C.UTF-8}
if [[ -z "\${XDG_RUNTIME_DIR:-}" ]]; then
  export XDG_RUNTIME_DIR="/run/user/\$(id -u)"
fi
LOG_FILE="\${XDG_RUNTIME_DIR:-/tmp}/slm-session-broker.log"
if touch "\${LOG_FILE}" 2>/dev/null; then
  {
    echo "===== \$(date --iso-8601=seconds) slm-session-broker-launch start ====="
    echo "uid=\$(id -u) gid=\$(id -g) user=\$(id -un)"
    echo "LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}"
    echo "argv=\$*"
  } >> "\${LOG_FILE}" 2>&1 || true
  exec "${BROKER_BIN}" "\$@" >> "\${LOG_FILE}" 2>&1
fi
exec "${BROKER_BIN}" "\$@"
EOF
chmod 0755 "${BROKER_LAUNCHER}"

if ! command -v cage >/dev/null 2>&1; then
  echo "[fix-greetd-qt-runtime] installing cage..."
  if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y cage
  else
    echo "[fix-greetd-qt-runtime][FAIL] cage not found and apt-get unavailable." >&2
    exit 1
  fi
fi

ensure_greeter_user() {
  if ! id -u greeter >/dev/null 2>&1; then
    echo "[fix-greetd-qt-runtime] creating system user: greeter"
    useradd --system --home /var/lib/greetd --create-home --shell "${GREETER_SHELL}" greeter
  else
    local current_shell
    current_shell="$(getent passwd greeter | awk -F: '{print $7}')"
    if [[ "${current_shell}" == */nologin || "${current_shell}" == */false ]]; then
      echo "[fix-greetd-qt-runtime] updating greeter shell: ${current_shell} -> ${GREETER_SHELL}"
      usermod --shell "${GREETER_SHELL}" greeter
    fi
  fi

  # greetd may reject or fail PAM setup for greeter accounts with nologin shells.
  # Keep the account password-locked instead of relying on an invalid shell.
  passwd -l greeter >/dev/null 2>&1 || true
}

ensure_greeter_user
touch "${GREETER_LOG}"
chown -R greeter:greeter "${LOG_DIR}"
chmod 0750 "${LOG_DIR}"
chmod 0640 "${GREETER_LOG}"

GREETER_CAGE_LAUNCHER="/usr/local/libexec/slm-greeter-cage-launch"
CAGE_LOG="${LOG_DIR}/slm-greeter-cage.log"

cat > "${GREETER_CAGE_LAUNCHER}" <<'EOF'
#!/usr/bin/env bash
# Long-lived greetd default_session wrapper.
set -u
export LIBSEAT_BACKEND=logind
export WLR_RENDERER=pixman
_log="/var/lib/greetd/logs/slm-greeter-cage.log"
_ts() { date --iso-8601=seconds 2>/dev/null || date; }
_log_msg() { echo "$(_ts) cage-launch [pid=$$]: $*" >>"$_log"; }

_log_msg "===== start pid=$$ GREETD_SOCK=${GREETD_SOCK:-<unset>} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-<unset>} ====="

# Kill any orphaned cage from a previous greetd run before we start.
pkill -KILL -x cage 2>/dev/null || true
sleep 0.2

while true; do
    _log_msg "starting cage"
    # Note: we assume the caller has set up the correct launcher path.
    # In fix-greetd-qt-runtime.sh, we use the standard wrapper which
    # in turn calls slm-greeter-launch (which has the Qt env vars).
    cage -s -- "/usr/local/libexec/slm-greeter-launch" >>"$_log" 2>&1
    _rc=$?
    _log_msg "cage exited rc=$_rc"

    # Wait for the user session to be fully registered and its compositor to start.
    # We use a 3-second grace period because QEMU is slow.
    sleep 3
    _waited=3

    # Check for active user sessions.
    while true; do
        # 1. Check for kwin_wayland process (any user)
        if pgrep -x kwin_wayland >/dev/null 2>&1; then
            [ "$_waited" -eq 3 ] && _log_msg "kwin_wayland detected — holding off cage restart"
        # 2. Check for any active session for user 1000
        elif loginctl list-sessions --no-pager 2>/dev/null | grep -v -E "SESSION|greeter" | grep -q .; then
            [ "$_waited" -eq 3 ] && _log_msg "user session detected via loginctl — holding off cage restart"
        else
            # No session detected
            break
        fi
        
        _waited=$(( _waited + 1 ))
        sleep 1
    done

    if [ "$_waited" -gt 3 ]; then
        _log_msg "user session ended after ${_waited}s — restarting cage"
        sleep 1
    else
        _log_msg "no user session detected after grace period — restarting cage immediately"
        sleep 0.5
    fi
done
EOF
chmod 0755 "${GREETER_CAGE_LAUNCHER}"

echo "[fix-greetd-qt-runtime] writing ${GREETD_CFG}..."
mkdir -p /etc/greetd
cat > "${GREETD_CFG}" <<EOF
[terminal]
vt = 7

[default_session]
command = "${GREETER_CAGE_LAUNCHER}"
user = "greeter"
EOF

if [[ -x "${ROOT_DIR}/scripts/login/fix-greetd-pam.sh" ]]; then
  echo "[fix-greetd-qt-runtime] applying greetd PAM compatibility fix..."
  bash "${ROOT_DIR}/scripts/login/fix-greetd-pam.sh"
fi

echo "[fix-greetd-qt-runtime] ensure display-manager.service -> greetd.service"
GREETD_UNIT_PATH="$(systemctl show -p FragmentPath --value greetd.service 2>/dev/null || true)"
if [[ -z "${GREETD_UNIT_PATH}" || ! -f "${GREETD_UNIT_PATH}" ]]; then
  if [[ -f /lib/systemd/system/greetd.service ]]; then
    GREETD_UNIT_PATH="/lib/systemd/system/greetd.service"
  elif [[ -f /usr/lib/systemd/system/greetd.service ]]; then
    GREETD_UNIT_PATH="/usr/lib/systemd/system/greetd.service"
  else
    echo "[fix-greetd-qt-runtime][FAIL] greetd.service not found." >&2
    exit 1
  fi
fi
ln -sfn "${GREETD_UNIT_PATH}" /etc/systemd/system/display-manager.service
systemctl daemon-reload

echo "[fix-greetd-qt-runtime] restarting greetd..."
systemctl reset-failed greetd || true
systemctl enable --now greetd
systemctl restart greetd

echo "[fix-greetd-qt-runtime] recent greetd logs:"
journalctl -b -u greetd --no-pager -n 60 || true
echo "[fix-greetd-qt-runtime] greeter log: ${GREETER_LOG}"
echo "[fix-greetd-qt-runtime] broker  log: \$XDG_RUNTIME_DIR/slm-session-broker.log (or /tmp fallback)"

echo "[fix-greetd-qt-runtime] done."
