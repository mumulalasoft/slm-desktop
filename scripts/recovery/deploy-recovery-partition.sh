#!/usr/bin/env bash
set -euo pipefail

# Deploy recovery image to a dedicated partition and optionally install boot entry.
# This is destructive for target device.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${SLM_RECOVERY_OUT_DIR:-$ROOT_DIR/build/recovery-partition}"
IMAGE_PATH="${SLM_RECOVERY_IMAGE_PATH:-$ARTIFACT_DIR/slm-recovery-rootfs.squashfs}"
TARGET_DEVICE="${SLM_RECOVERY_TARGET_DEVICE:-}"
INSTALL_BOOT_ENTRY="${SLM_RECOVERY_INSTALL_BOOT_ENTRY:-1}"
ENTRY_ID="${SLM_RECOVERY_ENTRY_ID:-slm-recovery}"
AUTO_YES="${SLM_RECOVERY_YES:-0}"

INSTALL_ENTRY_SCRIPT="$ROOT_DIR/scripts/recovery/install-recovery-boot-entry.sh"

log() { echo "[deploy-recovery-partition] $*"; }
die() { echo "[deploy-recovery-partition][FAIL] $*" >&2; exit 1; }

usage() {
  cat <<EOF
Usage: $0 --device /dev/<partition> [options]

Options:
  --device DEV            Target recovery partition block device (required)
  --image PATH            Recovery SquashFS image (default: $IMAGE_PATH)
  --artifact-dir PATH     Artifact dir with boot templates (default: $ARTIFACT_DIR)
  --entry-id ID           Boot entry id (default: $ENTRY_ID)
  --skip-boot-entry       Do not install/update boot entry
  --yes                   Non-interactive destructive confirmation
  --help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --device) TARGET_DEVICE="$2"; shift 2 ;;
    --image) IMAGE_PATH="$2"; shift 2 ;;
    --artifact-dir) ARTIFACT_DIR="$2"; shift 2 ;;
    --entry-id) ENTRY_ID="$2"; shift 2 ;;
    --skip-boot-entry) INSTALL_BOOT_ENTRY="0"; shift ;;
    --yes) AUTO_YES="1"; shift ;;
    --help|-h) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

if [[ "${EUID}" -ne 0 ]]; then
  die "run as root"
fi
[[ -n "$TARGET_DEVICE" ]] || die "missing --device"
[[ -b "$TARGET_DEVICE" ]] || die "target is not a block device: $TARGET_DEVICE"
[[ -f "$IMAGE_PATH" ]] || die "image not found: $IMAGE_PATH"
[[ -x "$INSTALL_ENTRY_SCRIPT" ]] || die "install boot entry script missing: $INSTALL_ENTRY_SCRIPT"

if mount | grep -q "^$TARGET_DEVICE "; then
  die "target device is mounted, unmount first: $TARGET_DEVICE"
fi

if [[ "$AUTO_YES" != "1" ]]; then
  echo
  echo "WARNING: this will overwrite data on $TARGET_DEVICE"
  echo "Image: $IMAGE_PATH"
  echo
  read -r -p "Type YES to continue: " confirm
  [[ "$confirm" == "YES" ]] || die "aborted"
fi

log "writing image to $TARGET_DEVICE ..."
dd if="$IMAGE_PATH" of="$TARGET_DEVICE" bs=4M conv=fsync status=progress
sync

partuuid="$(blkid -s PARTUUID -o value "$TARGET_DEVICE" 2>/dev/null || true)"
uuid="$(blkid -s UUID -o value "$TARGET_DEVICE" 2>/dev/null || true)"

ROOT_SPEC=""
if [[ -n "$partuuid" ]]; then
  ROOT_SPEC="PARTUUID=$partuuid"
elif [[ -n "$uuid" ]]; then
  ROOT_SPEC="UUID=$uuid"
else
  die "failed to detect PARTUUID/UUID after deploy ($TARGET_DEVICE)"
fi

log "resolved recovery root spec: $ROOT_SPEC"

if [[ "$INSTALL_BOOT_ENTRY" == "1" ]]; then
  "$INSTALL_ENTRY_SCRIPT" \
    --artifact-dir "$ARTIFACT_DIR" \
    --entry-id "$ENTRY_ID" \
    --root-spec "$ROOT_SPEC"
fi

log "done"
