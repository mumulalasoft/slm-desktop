#!/usr/bin/env bash
set -euo pipefail

# Install SLM Desktop runtime (not only greetd):
# - install core binaries from build dir
# - install slm desktop session entry
# - install systemd --user units for core daemons
# - optional: setup greetd as display manager

if [[ "${EUID}" -ne 0 ]]; then
  echo "[install-slm-runtime][FAIL] run as root (sudo)." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build/toppanel-Debug}"
TARGET_USER="${SUDO_USER:-${SLM_TARGET_USER:-}}"
INSTALL_GREETD="${SLM_INSTALL_GREETD:-1}"
ENABLE_USER_UNITS="${SLM_ENABLE_USER_UNITS:-1}"
QEMU_COMPOSITOR="${SLM_QEMU_COMPOSITOR:-1}"  # 1 = write headless compositor config when inside QEMU

BIN_DIR="/usr/local/bin"
LIBEXEC_DIR="/usr/libexec"
SLM_LIBEXEC_DIR="/usr/local/libexec/slm/recovery"
SESSION_DIR="/usr/share/wayland-sessions"
APPLICATIONS_DIR="/usr/share/applications"
SETTINGS_MODULES_DIR="/usr/lib/settings/modules"
SETTINGS_COMPONENTS_DIR="/usr/lib/settings/components"
POLKIT_ACTIONS_DIR="/usr/share/polkit-1/actions"

echo "[install-slm-runtime] root=$ROOT_DIR"
echo "[install-slm-runtime] build=$BUILD_DIR"
echo "[install-slm-runtime] target-user=${TARGET_USER:-<none>}"

must_install_bin() {
  local src="$1"
  local dst="$2"
  if [[ ! -x "$src" ]]; then
    echo "[install-slm-runtime][FAIL] missing executable: $src" >&2
    exit 1
  fi
  install -Dm755 "$src" "$dst"
  echo "[install-slm-runtime][OK] $dst"
}

install_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -x "$src" ]]; then
    install -Dm755 "$src" "$dst"
    echo "[install-slm-runtime][OK] $dst"
  else
    echo "[install-slm-runtime][WARN] optional binary not found: $src"
  fi
}

must_install_bin "$BUILD_DIR/slm-greeter" "$BIN_DIR/slm-greeter"
must_install_bin "$BUILD_DIR/slm-watchdog" "$BIN_DIR/slm-watchdog"
must_install_bin "$BUILD_DIR/slm-recovery-app" "$BIN_DIR/slm-recovery-app"
must_install_bin "$BUILD_DIR/slm-session-broker" "$LIBEXEC_DIR/slm-session-broker"
ln -sfn "$LIBEXEC_DIR/slm-session-broker" "$BIN_DIR/slm-session-broker"
echo "[install-slm-runtime][OK] $BIN_DIR/slm-session-broker -> $LIBEXEC_DIR/slm-session-broker"

install_if_exists "$BUILD_DIR/slm-desktop" "$BIN_DIR/slm-shell"
install_if_exists "$BUILD_DIR/desktopd" "$BIN_DIR/desktopd"
install_if_exists "$BUILD_DIR/slm-svcmgrd" "$BIN_DIR/slm-svcmgrd"
install_if_exists "$BUILD_DIR/slm-loggerd" "$BIN_DIR/slm-loggerd"
install_if_exists "$BUILD_DIR/slm-portald" "$BIN_DIR/slm-portald"
install_if_exists "$BUILD_DIR/slm-fileopsd" "$BIN_DIR/slm-fileopsd"
install_if_exists "$BUILD_DIR/slm-sharingd" "$BIN_DIR/slm-sharingd"
install_if_exists "$BUILD_DIR/slm-devicesd" "$BIN_DIR/slm-devicesd"
install_if_exists "$BUILD_DIR/slm-clipboardd" "$BIN_DIR/slm-clipboardd"
install_if_exists "$BUILD_DIR/slm-polkit-agent" "$BIN_DIR/slm-polkit-agent"
install_if_exists "$BUILD_DIR/slm-envd" "$BIN_DIR/slm-envd"
install_if_exists "$BUILD_DIR/slm-envd-helper" "$BIN_DIR/slm-envd-helper"
install_if_exists "$BUILD_DIR/slm-recoveryd" "$BIN_DIR/slm-recoveryd"
install_if_exists "$BUILD_DIR/slm-settings" "$BIN_DIR/slm-settings"
install_if_exists "$BUILD_DIR/slm-settingsd" "$BIN_DIR/slm-settingsd"
install_if_exists "$BUILD_DIR/desktop-contextd" "$BIN_DIR/desktop-contextd"
install_if_exists "$BUILD_DIR/slm-filemanager" "$BIN_DIR/slm-filemanager"

setup_samba_usershares() {
  local target_user="$1"
  if ! command -v net >/dev/null 2>&1 && ! [[ -x "$BUILD_DIR/slm-sharingd" ]]; then
    return 0
  fi
  groupadd -f sambashare || true
  mkdir -p /var/lib/samba/usershares
  chown root:sambashare /var/lib/samba/usershares || true
  chmod 1770 /var/lib/samba/usershares || true
  if [[ -n "$target_user" ]]; then
    usermod -aG sambashare "$target_user" || true
  fi
  if [[ -f /etc/samba/smb.conf ]] && ! grep -q '^[[:space:]]*usershare max shares' /etc/samba/smb.conf; then
    cp /etc/samba/smb.conf "/etc/samba/smb.conf.slm-bak.$(date +%s)" || true
    sed -i '/^\[global\]/a\   usershare max shares = 100\n   usershare path = /var/lib/samba/usershares\n   usershare allow guests = yes\n   usershare owner only = no' /etc/samba/smb.conf || true
  fi
  systemctl enable --now smbd.service nmbd.service >/dev/null 2>&1 || true
  if [[ "$QEMU_COMPOSITOR" == "1" ]]; then
    # qemu-smoke installs while the user manager is already alive; it will not
    # pick up new supplementary groups until reboot. Keep dev smoke usable.
    chmod 1777 /var/lib/samba/usershares || true
  fi
  echo "[install-slm-runtime][OK] samba usershare prerequisites prepared"
}

setup_samba_usershares "$TARGET_USER"

if [[ -d "$ROOT_DIR/src/apps/settings/modules" ]]; then
  install -d -m0755 "$SETTINGS_MODULES_DIR"
  rm -rf "$SETTINGS_MODULES_DIR"/*
  cp -a "$ROOT_DIR/src/apps/settings/modules/." "$SETTINGS_MODULES_DIR/"
  echo "[install-slm-runtime][OK] settings modules installed: $SETTINGS_MODULES_DIR"
else
  echo "[install-slm-runtime][WARN] settings modules source not found: $ROOT_DIR/src/apps/settings/modules"
fi

if [[ -d "$ROOT_DIR/Qml/apps/settings/components" ]]; then
  install -d -m0755 "$SETTINGS_COMPONENTS_DIR"
  rm -rf "$SETTINGS_COMPONENTS_DIR"/*
  cp -a "$ROOT_DIR/Qml/apps/settings/components/." "$SETTINGS_COMPONENTS_DIR/"
  echo "[install-slm-runtime][OK] settings components installed: $SETTINGS_COMPONENTS_DIR"
else
  echo "[install-slm-runtime][WARN] settings components source not found: $ROOT_DIR/Qml/apps/settings/components"
fi

install -Dm644 "$ROOT_DIR/sessions/slm.desktop" "$SESSION_DIR/slm.desktop"
echo "[install-slm-runtime][OK] $SESSION_DIR/slm.desktop"
install -Dm644 "$ROOT_DIR/sessions/slm-shell.desktop" "$APPLICATIONS_DIR/slm-shell.desktop"
echo "[install-slm-runtime][OK] $APPLICATIONS_DIR/slm-shell.desktop"
if [[ -f "$ROOT_DIR/apps/desktop-shell/slm-filemanager.desktop" ]]; then
  install -Dm644 "$ROOT_DIR/apps/desktop-shell/slm-filemanager.desktop" \
    "$APPLICATIONS_DIR/slm-filemanager.desktop"
  echo "[install-slm-runtime][OK] $APPLICATIONS_DIR/slm-filemanager.desktop"
else
  echo "[install-slm-runtime][WARN] missing desktop entry: $ROOT_DIR/apps/desktop-shell/slm-filemanager.desktop"
fi

install -d -m0755 "$POLKIT_ACTIONS_DIR"
for policy in org.slm.desktop.devices.policy \
              org.slm.desktop.foldersharing.policy \
              org.slm.settings.policy; do
  src="$ROOT_DIR/scripts/polkit/$policy"
  if [[ -f "$src" ]]; then
    install -Dm644 "$src" "$POLKIT_ACTIONS_DIR/$policy"
    echo "[install-slm-runtime][OK] $POLKIT_ACTIONS_DIR/$policy"
  else
    echo "[install-slm-runtime][WARN] polkit policy missing: $src"
  fi
done

install -d "$SLM_LIBEXEC_DIR"
install -m755 "$ROOT_DIR/scripts/recovery/request-bootloader-recovery.sh" \
  "$SLM_LIBEXEC_DIR/request-bootloader-recovery.sh"
install -m755 "$ROOT_DIR/scripts/recovery/detect-recovery-boot-entry.sh" \
  "$SLM_LIBEXEC_DIR/detect-recovery-boot-entry.sh"
install -m755 "$ROOT_DIR/scripts/recovery/install-recovery-boot-entry.sh" \
  "$SLM_LIBEXEC_DIR/install-recovery-boot-entry.sh"
install -m755 "$ROOT_DIR/scripts/recovery/build-recovery-partition-image.sh" \
  "$SLM_LIBEXEC_DIR/build-recovery-partition-image.sh"
install -m755 "$ROOT_DIR/scripts/recovery/deploy-recovery-partition.sh" \
  "$SLM_LIBEXEC_DIR/deploy-recovery-partition.sh"
install -m755 "$ROOT_DIR/scripts/recovery/smoke-recovery-partition-pipeline.sh" \
  "$SLM_LIBEXEC_DIR/smoke-recovery-partition-pipeline.sh"
echo "[install-slm-runtime][OK] recovery helpers installed at $SLM_LIBEXEC_DIR"

if [[ -n "$TARGET_USER" ]]; then
  USER_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
  if [[ -z "$USER_HOME" ]]; then
    echo "[install-slm-runtime][FAIL] cannot resolve home for user: $TARGET_USER" >&2
    exit 1
  fi
  UNIT_DIR="$USER_HOME/.config/systemd/user"
  mkdir -p "$UNIT_DIR"
  chown "$TARGET_USER:$TARGET_USER" "$USER_HOME/.config" "$USER_HOME/.config/systemd" "$UNIT_DIR" 2>/dev/null || true

  install_user_unit() {
    local template="$1"
    local unit_name="$2"
    local replacement="$3"
    local dst="$UNIT_DIR/$unit_name"
    sed "s#%h/Development/Qt/Desktop_Shell/build/[A-Za-z0-9._-]*#$replacement#g" "$template" > "$dst"
    chown "$TARGET_USER:$TARGET_USER" "$dst" 2>/dev/null || true
    if [[ "$ENABLE_USER_UNITS" == "1" ]]; then
      mkdir -p "$UNIT_DIR/default.target.wants"
      ln -sfn "../$unit_name" "$UNIT_DIR/default.target.wants/$unit_name"
      chown -h "$TARGET_USER:$TARGET_USER" "$UNIT_DIR/default.target.wants/$unit_name" 2>/dev/null || true
      chown "$TARGET_USER:$TARGET_USER" "$UNIT_DIR/default.target.wants" 2>/dev/null || true
    fi
    echo "[install-slm-runtime][OK] user unit: $dst"
  }

  install_user_unit "$ROOT_DIR/scripts/systemd/slm-desktopd.service" "slm-desktopd.service" "$BIN_DIR/desktopd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-portald.service" "slm-portald.service" "$BIN_DIR/slm-portald"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-fileopsd.service" "slm-fileopsd.service" "$BIN_DIR/slm-fileopsd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-sharingd.service" "slm-sharingd.service" "$BIN_DIR/slm-sharingd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-devicesd.service" "slm-devicesd.service" "$BIN_DIR/slm-devicesd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-clipboardd.service" "slm-clipboardd.service" "$BIN_DIR/slm-clipboardd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-polkit-agent.service" "slm-polkit-agent.service" "$BIN_DIR/slm-polkit-agent"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-envd.service" "slm-envd.service" "$BIN_DIR/slm-envd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-recoveryd.service" "slm-recoveryd.service" "$BIN_DIR/slm-recoveryd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-settingsd.service" "slm-settingsd.service" "$BIN_DIR/slm-settingsd"

  if [[ "$ENABLE_USER_UNITS" == "1" ]]; then
    if runuser -u "$TARGET_USER" -- systemctl --user daemon-reload >/dev/null 2>&1; then
      runuser -u "$TARGET_USER" -- systemctl --user enable --now \
        slm-desktopd.service slm-portald.service slm-fileopsd.service slm-sharingd.service \
        slm-devicesd.service slm-clipboardd.service slm-polkit-agent.service slm-envd.service \
        slm-recoveryd.service slm-settingsd.service || true
      echo "[install-slm-runtime][OK] attempted to enable user units"
    else
      echo "[install-slm-runtime][WARN] cannot access user systemd manager now; units installed but not started"
      echo "[install-slm-runtime][HINT] login as ${TARGET_USER} and run: systemctl --user daemon-reload && systemctl --user enable --now slm-desktopd.service slm-portald.service slm-fileopsd.service slm-sharingd.service slm-devicesd.service slm-clipboardd.service slm-polkit-agent.service slm-envd.service slm-recoveryd.service slm-settingsd.service"
    fi
  fi
else
  echo "[install-slm-runtime][WARN] TARGET_USER not set; skipping user service install"
fi

if [[ "$INSTALL_GREETD" == "1" ]]; then
  bash "$ROOT_DIR/scripts/login/install-greetd-slm.sh"
else
  echo "[install-slm-runtime] skip greetd setup (SLM_INSTALL_GREETD=$INSTALL_GREETD)"
fi

# Always install slm-session-check and PAM/service files (does not enable by default).
bash "$ROOT_DIR/scripts/login/setup-slm-session-manager.sh"

# KWin-only mode: do not install a non-KWin fallback config.  If QEMU has
# no DRM device, leave the failure visible so the session broker can record the
# exact KWin/logind reason in state and last-crash.json.
if [[ "$QEMU_COMPOSITOR" == "1" && -n "$TARGET_USER" ]]; then
  VIRT="$(systemd-detect-virt 2>/dev/null || true)"
  HAS_DRM=0
  if ls /dev/dri/card* >/dev/null 2>&1; then HAS_DRM=1; fi

  if [[ "$VIRT" == "qemu" || "$VIRT" == "kvm" ]] && [[ "$HAS_DRM" == "0" ]]; then
    echo "[install-slm-runtime][WARN] QEMU/KVM has no /dev/dri/card*; KWin needs a DRM-capable GPU"
    echo "[install-slm-runtime][HINT] use virtio-gpu/virgl or a real DRM device; non-KWin fallback is disabled"
  elif [[ "$HAS_DRM" == "0" ]]; then
    echo "[install-slm-runtime][WARN] no /dev/dri/card* found (virt=$VIRT); KWin may fail to open DRM"
  fi
fi

echo "[install-slm-runtime] done"
echo "[install-slm-runtime] verify:"
echo "  sudo bash $ROOT_DIR/scripts/login/verify-slm-desktop-runtime.sh"
