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

if ! id -u greeter >/dev/null 2>&1; then
  echo "[install-greetd-slm] creating system user: greeter"
  useradd --system --home /var/lib/greetd --create-home \
    --shell /usr/sbin/nologin greeter
fi

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
command = "cage -s -- __SLM_GREETER_BIN__"
user = "greeter"

[initial_session]
command = "/usr/libexec/slm-session-broker --mode normal"
user = "greeter"
EOF

sed -i "s#__SLM_GREETER_BIN__#${GREETER_BIN//\//\\/}#g" /etc/greetd/config.toml

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

echo "[install-greetd-slm] done."
echo "Next: reboot and run scripts/login/verify-greetd-slm.sh"
