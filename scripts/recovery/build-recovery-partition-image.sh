#!/usr/bin/env bash
set -euo pipefail

# Build a minimal recovery partition artifact (SquashFS rootfs + boot entry templates).
# This is a non-destructive builder script; it does not write bootloader config directly.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${SLM_RECOVERY_BUILD_DIR:-$ROOT_DIR/build/toppanel-Debug}"
OUT_DIR="${SLM_RECOVERY_OUT_DIR:-$ROOT_DIR/build/recovery-partition}"
WORK_DIR=""
IMAGE_NAME="${SLM_RECOVERY_IMAGE_NAME:-slm-recovery-rootfs.squashfs}"
KERNEL_PATH="${SLM_RECOVERY_KERNEL_PATH:-/vmlinuz-linux}"
INITRD_PATH="${SLM_RECOVERY_INITRD_PATH:-/initramfs-linux.img}"
APPEND_CMDLINE="${SLM_RECOVERY_APPEND_CMDLINE:-quiet systemd.unit=multi-user.target slm.recovery=1}"

log() { echo "[build-recovery-partition] $*"; }
die() { echo "[build-recovery-partition][FAIL] $*" >&2; exit 1; }

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --build-dir PATH      Build directory containing slm-recovery-app/slm-recoveryd
  --out-dir PATH        Output directory for image + manifest
  --work-dir PATH       Working directory (default: mktemp)
  --image-name NAME     Output image file name (default: $IMAGE_NAME)
  --kernel PATH         Kernel path for boot entry template (default: $KERNEL_PATH)
  --initrd PATH         Initrd path for boot entry template (default: $INITRD_PATH)
  --append-cmdline STR  Extra kernel command line (default set for recovery)
  --help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --out-dir) OUT_DIR="$2"; shift 2 ;;
    --work-dir) WORK_DIR="$2"; shift 2 ;;
    --image-name) IMAGE_NAME="$2"; shift 2 ;;
    --kernel) KERNEL_PATH="$2"; shift 2 ;;
    --initrd) INITRD_PATH="$2"; shift 2 ;;
    --append-cmdline) APPEND_CMDLINE="$2"; shift 2 ;;
    --help|-h) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

command -v mksquashfs >/dev/null 2>&1 || die "mksquashfs not found (install squashfs-tools)"
command -v sha256sum >/dev/null 2>&1 || die "sha256sum not found"

if [[ ! -d "$BUILD_DIR" ]]; then
  die "build directory not found: $BUILD_DIR"
fi

mkdir -p "$OUT_DIR"
if [[ -z "$WORK_DIR" ]]; then
  WORK_DIR="$(mktemp -d)"
else
  mkdir -p "$WORK_DIR"
fi

STAGE_DIR="$WORK_DIR/stage"
ROOTFS_DIR="$STAGE_DIR/rootfs"
mkdir -p "$ROOTFS_DIR"/{usr/bin,usr/libexec,etc/systemd/system,var/log,proc,sys,dev,tmp}
chmod 1777 "$ROOTFS_DIR/tmp"

BINARIES_INSTALLED=()
copy_binary() {
  local name="$1"
  local src_build="$BUILD_DIR/$name"
  local src_system="/usr/local/bin/$name"
  local dst="$ROOTFS_DIR/usr/bin/$name"

  if [[ -x "$src_build" ]]; then
    install -Dm755 "$src_build" "$dst"
    BINARIES_INSTALLED+=("$name:build")
    return 0
  fi
  if [[ -x "$src_system" ]]; then
    install -Dm755 "$src_system" "$dst"
    BINARIES_INSTALLED+=("$name:system")
    return 0
  fi
  return 1
}

copy_binary "slm-recovery-app" || die "required binary missing: slm-recovery-app"
copy_binary "slm-recoveryd" || die "required binary missing: slm-recoveryd"
copy_binary "slm-shell" || true
copy_binary "desktopd" || true

cat > "$ROOTFS_DIR/usr/libexec/slm-recovery-session.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
export QT_QUICK_BACKEND=software
export LIBGL_ALWAYS_SOFTWARE=1
export SLM_RECOVERY_MODE=1
exec /usr/bin/slm-recovery-app
EOF
chmod +x "$ROOTFS_DIR/usr/libexec/slm-recovery-session.sh"

cat > "$ROOTFS_DIR/etc/systemd/system/slm-recovery-session.service" <<'EOF'
[Unit]
Description=SLM Recovery Session
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/libexec/slm-recovery-session.sh
Restart=on-failure
RestartSec=1

[Install]
WantedBy=multi-user.target
EOF

IMAGE_PATH="$OUT_DIR/$IMAGE_NAME"
rm -f "$IMAGE_PATH"
mksquashfs "$ROOTFS_DIR" "$IMAGE_PATH" -noappend -comp xz -all-root >/dev/null

IMAGE_SHA="$(sha256sum "$IMAGE_PATH" | awk '{print $1}')"
CREATED_AT="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

MANIFEST_PATH="$OUT_DIR/recovery-partition.manifest.json"
{
  echo "{"
  echo "  \"format\": \"slm-recovery-partition/v1\","
  echo "  \"createdAt\": \"$CREATED_AT\","
  echo "  \"buildDir\": \"${BUILD_DIR//\"/\\\"}\","
  echo "  \"image\": {"
  echo "    \"path\": \"${IMAGE_PATH//\"/\\\"}\","
  echo "    \"sha256\": \"$IMAGE_SHA\""
  echo "  },"
  echo "  \"binaries\": ["
  for i in "${!BINARIES_INSTALLED[@]}"; do
    sep=","
    [[ "$i" -eq $((${#BINARIES_INSTALLED[@]} - 1)) ]] && sep=""
    item="${BINARIES_INSTALLED[$i]}"
    name="${item%%:*}"
    source="${item##*:}"
    echo "    {\"name\":\"$name\",\"source\":\"$source\"}$sep"
  done
  echo "  ]"
  echo "}"
} > "$MANIFEST_PATH"

cat > "$OUT_DIR/slm-recovery-systemd-boot.conf.template" <<EOF
title   SLM Recovery
linux   $KERNEL_PATH
initrd  $INITRD_PATH
options root=LABEL=SLM_RECOVERY rootfstype=squashfs ro $APPEND_CMDLINE
EOF

cat > "$OUT_DIR/slm-recovery-grub.cfg.template" <<EOF
menuentry 'SLM Recovery' {
    linux $KERNEL_PATH root=LABEL=SLM_RECOVERY rootfstype=squashfs ro $APPEND_CMDLINE
    initrd $INITRD_PATH
}
EOF

log "image:    $IMAGE_PATH"
log "manifest: $MANIFEST_PATH"
log "boot templates:"
log "  - $OUT_DIR/slm-recovery-systemd-boot.conf.template"
log "  - $OUT_DIR/slm-recovery-grub.cfg.template"

if [[ "${SLM_RECOVERY_KEEP_WORKDIR:-0}" != "1" ]]; then
  rm -rf "$WORK_DIR"
else
  log "workdir kept: $WORK_DIR"
fi
