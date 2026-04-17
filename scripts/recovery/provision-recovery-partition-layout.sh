#!/usr/bin/env bash
set -euo pipefail

RECOVERY_MOUNT="${1:-/recovery}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

die() { echo "[provision-recovery][FAIL] $*" >&2; exit 1; }
log() { echo "[provision-recovery] $*"; }

[[ "${EUID}" -eq 0 ]] || die "must run as root"
[[ -d "$RECOVERY_MOUNT" ]] || die "mount path missing: $RECOVERY_MOUNT"
mountpoint -q "$RECOVERY_MOUNT" || die "$RECOVERY_MOUNT is not mounted"

install -d -m 0755 "$RECOVERY_MOUNT/bin" "$RECOVERY_MOUNT/scripts" "$RECOVERY_MOUNT/ui"

install -m 0755 "$ROOT_DIR/scripts/recovery/restore-snapshot.sh" "$RECOVERY_MOUNT/scripts/restore-snapshot.sh"
install -m 0755 "$ROOT_DIR/scripts/recovery/reinstall-system.sh" "$RECOVERY_MOUNT/scripts/reinstall-system.sh"
install -m 0755 "$ROOT_DIR/scripts/recovery/repair-boot.sh" "$RECOVERY_MOUNT/scripts/repair-boot.sh"

log "recovery partition layout prepared under $RECOVERY_MOUNT"
