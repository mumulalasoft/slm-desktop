#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LIB_DIR="/usr/local/lib/slm-recovery"
UNIT_DIR="/etc/systemd/system"

SCRIPTS=(
  bootcount-guard.sh
  trigger-recovery-on-boot-failure.sh
  restore-snapshot.sh
  prune-btrfs-snapshots.sh
  repair-boot.sh
  reinstall-system.sh
  request-bootloader-recovery.sh
  detect-recovery-boot-entry.sh
  install-grub-boot-failure-integration.sh
)

ASSETS=(
  grub-recovery-snippet.cfg
)

UNITS=(
  slm-boot-attempt.service
  slm-boot-failure-check.service
  boot-success.service
  slm-snapshot-retention.service
  slm-snapshot-retention.timer
)

log() { echo "[install-system-recovery] $*"; }
die() { echo "[install-system-recovery][FAIL] $*" >&2; exit 1; }

[[ "${EUID}" -eq 0 ]] || die "must run as root"

mkdir -p "$LIB_DIR" "$UNIT_DIR"

for s in "${SCRIPTS[@]}"; do
  src="$ROOT_DIR/scripts/recovery/$s"
  [[ -f "$src" ]] || die "missing script: $src"
  install -m 0755 "$src" "$LIB_DIR/$s"
  log "installed script: $LIB_DIR/$s"
done

for a in "${ASSETS[@]}"; do
  src="$ROOT_DIR/scripts/recovery/$a"
  [[ -f "$src" ]] || die "missing asset: $src"
  install -m 0644 "$src" "$LIB_DIR/$a"
  log "installed asset: $LIB_DIR/$a"
done

for u in "${UNITS[@]}"; do
  src="$ROOT_DIR/scripts/systemd/system/$u"
  [[ -f "$src" ]] || die "missing unit: $src"
  install -m 0644 "$src" "$UNIT_DIR/$u"
  log "installed unit: $UNIT_DIR/$u"
done

systemctl daemon-reload
systemctl enable \
  slm-boot-attempt.service \
  slm-boot-failure-check.service \
  boot-success.service \
  slm-snapshot-retention.timer

log "installation complete"
log "start immediately (optional): systemctl start slm-boot-attempt.service slm-boot-failure-check.service"
