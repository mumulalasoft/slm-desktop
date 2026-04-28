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

if [[ -x "$BUILD_DIR/slm-desktop" ]]; then
  install_if_exists "$BUILD_DIR/slm-desktop" "$BIN_DIR/slm-shell"
else
  install_if_exists "$BUILD_DIR/appSlm_Desktop" "$BIN_DIR/slm-shell"
fi
install_if_exists "$BUILD_DIR/desktopd" "$BIN_DIR/desktopd"
install_if_exists "$BUILD_DIR/slm-svcmgrd" "$BIN_DIR/slm-svcmgrd"
install_if_exists "$BUILD_DIR/slm-loggerd" "$BIN_DIR/slm-loggerd"
install_if_exists "$BUILD_DIR/slm-portald" "$BIN_DIR/slm-portald"
install_if_exists "$BUILD_DIR/slm-fileopsd" "$BIN_DIR/slm-fileopsd"
install_if_exists "$BUILD_DIR/slm-devicesd" "$BIN_DIR/slm-devicesd"
install_if_exists "$BUILD_DIR/slm-clipboardd" "$BIN_DIR/slm-clipboardd"
install_if_exists "$BUILD_DIR/slm-polkit-agent" "$BIN_DIR/slm-polkit-agent"
install_if_exists "$BUILD_DIR/slm-envd" "$BIN_DIR/slm-envd"
install_if_exists "$BUILD_DIR/slm-recoveryd" "$BIN_DIR/slm-recoveryd"

install -Dm644 "$ROOT_DIR/sessions/slm.desktop" "$SESSION_DIR/slm.desktop"
echo "[install-slm-runtime][OK] $SESSION_DIR/slm.desktop"

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
    echo "[install-slm-runtime][OK] user unit: $dst"
  }

  install_user_unit "$ROOT_DIR/scripts/systemd/slm-desktopd.service" "slm-desktopd.service" "$BIN_DIR/desktopd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-portald.service" "slm-portald.service" "$BIN_DIR/slm-portald"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-fileopsd.service" "slm-fileopsd.service" "$BIN_DIR/slm-fileopsd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-devicesd.service" "slm-devicesd.service" "$BIN_DIR/slm-devicesd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-clipboardd.service" "slm-clipboardd.service" "$BIN_DIR/slm-clipboardd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-polkit-agent.service" "slm-polkit-agent.service" "$BIN_DIR/slm-polkit-agent"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-envd.service" "slm-envd.service" "$BIN_DIR/slm-envd"
  install_user_unit "$ROOT_DIR/scripts/systemd/slm-recoveryd.service" "slm-recoveryd.service" "$BIN_DIR/slm-recoveryd"

  if [[ "$ENABLE_USER_UNITS" == "1" ]]; then
    if runuser -u "$TARGET_USER" -- systemctl --user daemon-reload >/dev/null 2>&1; then
      runuser -u "$TARGET_USER" -- systemctl --user enable --now \
        slm-desktopd.service slm-portald.service slm-fileopsd.service \
        slm-devicesd.service slm-clipboardd.service slm-polkit-agent.service slm-envd.service \
        slm-recoveryd.service || true
      echo "[install-slm-runtime][OK] attempted to enable user units"
    else
      echo "[install-slm-runtime][WARN] cannot access user systemd manager now; units installed but not started"
      echo "[install-slm-runtime][HINT] login as ${TARGET_USER} and run: systemctl --user daemon-reload && systemctl --user enable --now slm-desktopd.service slm-portald.service slm-fileopsd.service slm-devicesd.service slm-clipboardd.service slm-polkit-agent.service slm-envd.service slm-recoveryd.service"
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
