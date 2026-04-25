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

echo "[install-greetd-slm] ensuring seatd is installed..."
if ! command -v seatd >/dev/null 2>&1; then
  if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y seatd
  else
    echo "[install-greetd-slm][FAIL] seatd not found and no apt-get available." >&2
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
GREETER_LOG_DIR="/var/lib/greetd/logs"
GREETER_LOG="${GREETER_LOG_DIR}/slm-greeter.log"
CAGE_LOG="${GREETER_LOG_DIR}/slm-greeter-cage.log"

if ! id -u greeter >/dev/null 2>&1; then
  echo "[install-greetd-slm] creating system user: greeter"
  useradd --system --home /var/lib/greetd --create-home \
    --shell /usr/sbin/nologin greeter
fi

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
set -euo pipefail
export LIBSEAT_BACKEND=logind
export QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=software
export LIBGL_ALWAYS_SOFTWARE=1
{
  echo "===== \$(date --iso-8601=seconds) slm-greeter-launch start ====="
  echo "GREETER_BIN=${GREETER_BIN}"
  echo "GREETD_SOCK=\${GREETD_SOCK:-}"
  echo "XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-}"
  echo "QT_QPA_PLATFORM=\${QT_QPA_PLATFORM:-}"
  echo "QT_QUICK_BACKEND=\${QT_QUICK_BACKEND:-}"
  echo "LIBGL_ALWAYS_SOFTWARE=\${LIBGL_ALWAYS_SOFTWARE:-}"
  exec "${GREETER_BIN}"
} >>"${GREETER_LOG}" 2>&1
EOF
chmod 0755 "$GREETER_LAUNCHER"
cat >"$GREETER_CAGE_LAUNCHER" <<EOF
#!/usr/bin/env bash
set -euo pipefail
export LIBSEAT_BACKEND=logind
export WLR_RENDERER=pixman
{
  echo "===== \$(date --iso-8601=seconds) slm-greeter-cage-launch start ====="
  echo "XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-}"
  echo "GREETD_SOCK=\${GREETD_SOCK:-}"
  echo "WLR_RENDERER=\${WLR_RENDERER:-}"
  exec cage -s -- "${GREETER_LAUNCHER}"
} >>"${CAGE_LOG}" 2>&1
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

SEATD_BIN="$(command -v seatd || true)"
if [[ -n "$SEATD_BIN" ]]; then
  echo "[install-greetd-slm] configuring seatd socket group -> greeter"
  mkdir -p /etc/systemd/system/seatd.service.d
  cat >/etc/systemd/system/seatd.service.d/slm-greeter.conf <<EOF
[Service]
ExecStart=
ExecStart=${SEATD_BIN} -g greeter
EOF
  systemctl daemon-reload
fi

echo "[install-greetd-slm] enabling seatd..."
systemctl enable --now seatd.service
systemctl restart seatd.service

SEATD_GROUP=""
if [[ -S /run/seatd.sock ]]; then
  SEATD_GROUP="$(stat -c '%G' /run/seatd.sock 2>/dev/null || true)"
fi
if [[ -z "$SEATD_GROUP" || "$SEATD_GROUP" == "UNKNOWN" ]]; then
  if getent group video >/dev/null 2>&1; then
    SEATD_GROUP="video"
  elif getent group seat >/dev/null 2>&1; then
    SEATD_GROUP="seat"
  fi
fi
if [[ -n "$SEATD_GROUP" ]]; then
  echo "[install-greetd-slm] ensuring greeter is in group: $SEATD_GROUP"
  usermod -aG "$SEATD_GROUP" greeter
else
  echo "[install-greetd-slm][WARN] could not determine seatd socket group; greeter access may fail"
fi

echo "[install-greetd-slm] enabling greetd..."
systemctl enable --now greetd.service
systemctl reset-failed greetd.service || true
systemctl restart greetd.service || true

echo "[install-greetd-slm] done."
echo "Logs:"
echo "  journalctl -b -u greetd --no-pager -n 200"
echo "  cat ${GREETER_LOG}"
echo "  cat ${CAGE_LOG}"
