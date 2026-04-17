#!/usr/bin/env bash
set -euo pipefail

# Request next boot into recovery entry.
# Works best as root. Supports:
# - systemd-boot via bootctl set-oneshot
# - GRUB via grub-reboot

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DETECT_ENTRY_SCRIPT="$ROOT_DIR/recovery/detect-recovery-boot-entry.sh"

ENTRY_HINT="${1:-${SLM_RECOVERY_BOOT_ENTRY:-auto}}"
ENTRY_RESOLVED=""
DO_REBOOT="${SLM_RECOVERY_REBOOT_NOW:-0}"

log() { echo "[recovery-boot] $*"; }
fail() { echo "[recovery-boot][FAIL] $*" >&2; exit 1; }

resolve_entry() {
  local hint="$1"
  if [[ "$hint" != "auto" && -n "$hint" ]]; then
    echo "$hint"
    return 0
  fi

  if [[ ! -x "$DETECT_ENTRY_SCRIPT" ]]; then
    fail "auto entry requested but detector script missing: $DETECT_ENTRY_SCRIPT"
  fi

  local detected=""
  if ! detected="$("$DETECT_ENTRY_SCRIPT" "${SLM_RECOVERY_BOOT_ENTRY_HINT:-recovery}")"; then
    fail "failed to auto-detect recovery boot entry"
  fi
  detected="${detected//$'\n'/}"
  if [[ -z "$detected" ]]; then
    fail "auto-detect returned empty boot entry"
  fi
  echo "$detected"
}

if [[ "${EUID}" -ne 0 ]]; then
  fail "run as root (required to set bootloader next-entry)"
fi

ENTRY_RESOLVED="$(resolve_entry "$ENTRY_HINT")"
log "entry hint='$ENTRY_HINT' resolved='$ENTRY_RESOLVED'"

set_ok=0

if command -v bootctl >/dev/null 2>&1; then
  if bootctl --no-pager set-oneshot "$ENTRY_RESOLVED" >/dev/null 2>&1; then
    log "systemd-boot oneshot entry set: $ENTRY_RESOLVED"
    set_ok=1
  fi
fi

if [[ "$set_ok" -eq 0 ]] && command -v grub-reboot >/dev/null 2>&1; then
  if grub-reboot "$ENTRY_RESOLVED" >/dev/null 2>&1; then
    log "GRUB next entry set: $ENTRY_RESOLVED"
    set_ok=1
  fi
fi

if [[ "$set_ok" -eq 0 ]]; then
  fail "cannot set bootloader recovery entry; ensure bootctl or grub-reboot is available and entry '$ENTRY_RESOLVED' exists"
fi

if [[ "$DO_REBOOT" == "1" ]]; then
  log "rebooting now"
  systemctl reboot
else
  log "done (set SLM_RECOVERY_REBOOT_NOW=1 to reboot immediately)"
fi
