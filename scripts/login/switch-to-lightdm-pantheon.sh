#!/usr/bin/env bash
set -euo pipefail

# Switch display manager back to LightDM and prefer Pantheon session.
# Usage:
#   bash scripts/login/switch-to-lightdm-pantheon.sh

if [[ "${EUID}" -ne 0 ]]; then
  SUDO="sudo"
else
  SUDO=""
fi

echo "[switch-lightdm] detecting lightdm unit..."
LIGHTDM_UNIT=""
if [[ -f /lib/systemd/system/lightdm.service ]]; then
  LIGHTDM_UNIT="/lib/systemd/system/lightdm.service"
elif [[ -f /usr/lib/systemd/system/lightdm.service ]]; then
  LIGHTDM_UNIT="/usr/lib/systemd/system/lightdm.service"
fi

if [[ -z "${LIGHTDM_UNIT}" ]]; then
  echo "[switch-lightdm][FAIL] lightdm.service not found"
  exit 1
fi

echo "[switch-lightdm] lightdm unit: ${LIGHTDM_UNIT}"
echo "[switch-lightdm] configuring display-manager.service -> lightdm..."
${SUDO} ln -sfn "${LIGHTDM_UNIT}" /etc/systemd/system/display-manager.service

echo "[switch-lightdm] writing default session preference..."
${SUDO} mkdir -p /etc/lightdm/lightdm.conf.d
${SUDO} tee /etc/lightdm/lightdm.conf.d/90-slm-default-session.conf >/dev/null <<'EOF'
[Seat:*]
user-session=pantheon
greeter-session=lightdm-gtk-greeter
EOF

echo "[switch-lightdm] disabling greetd..."
${SUDO} systemctl disable --now greetd.service || true

echo "[switch-lightdm] enabling/restarting lightdm..."
${SUDO} systemctl daemon-reload
${SUDO} systemctl enable lightdm.service
${SUDO} systemctl restart lightdm.service

echo "[switch-lightdm] verification:"
${SUDO} systemctl is-enabled lightdm.service || true
${SUDO} systemctl is-active lightdm.service || true
readlink -f /etc/systemd/system/display-manager.service || true
ls -1 /usr/share/xsessions/pantheon.desktop /usr/share/wayland-sessions/pantheon-wayland.desktop 2>/dev/null || true

echo "[switch-lightdm] done."
