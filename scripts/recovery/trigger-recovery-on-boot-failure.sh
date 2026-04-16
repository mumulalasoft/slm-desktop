#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GUARD_SCRIPT="${SLM_BOOTCOUNT_GUARD_SCRIPT:-$ROOT_DIR/recovery/bootcount-guard.sh}"
LOG_FILE="${SLM_BOOT_RECOVERY_LOG:-/var/log/boot-recovery.log}"

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][trigger-recovery] $*"
  echo "$msg"
  mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
  echo "$msg" >> "$LOG_FILE" 2>/dev/null || true
}

die() {
  log "FAIL: $*"
  exit 1
}

[[ "${EUID}" -eq 0 ]] || die "must run as root"
[[ -x "$GUARD_SCRIPT" ]] || die "bootcount guard not found: $GUARD_SCRIPT"

set +e
"$GUARD_SCRIPT" check-and-trigger
rc=$?
set -e

if [[ "$rc" -eq 100 || "$rc" -eq 101 ]]; then
  log "recovery requested by boot failure policy (rc=$rc); rebooting"
  systemctl reboot
  exit 0
fi

if [[ "$rc" -eq 110 ]]; then
  log "auto recovery disabled by manual override; continuing normal boot"
  exit 0
fi

if [[ "$rc" -eq 111 ]]; then
  log "auto recovery locked due to too many attempts; continuing normal boot"
  exit 0
fi

if [[ "$rc" -ne 0 ]]; then
  die "bootcount guard returned rc=$rc"
fi

log "no recovery action needed"
