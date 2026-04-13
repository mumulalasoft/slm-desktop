#!/usr/bin/env bash
set -euo pipefail

# Request next boot into recovery entry.
# Works best as root. Supports:
# - systemd-boot via bootctl set-oneshot
# - GRUB via grub-reboot

ENTRY_HINT="${1:-recovery}"
DO_REBOOT="${SLM_RECOVERY_REBOOT_NOW:-0}"

log() { echo "[recovery-boot] $*"; }
fail() { echo "[recovery-boot][FAIL] $*" >&2; exit 1; }

if [[ "${EUID}" -ne 0 ]]; then
  fail "run as root (required to set bootloader next-entry)"
fi

set_ok=0

if command -v bootctl >/dev/null 2>&1; then
  if bootctl --no-pager set-oneshot "$ENTRY_HINT" >/dev/null 2>&1; then
    log "systemd-boot oneshot entry set: $ENTRY_HINT"
    set_ok=1
  fi
fi

if [[ "$set_ok" -eq 0 ]] && command -v grub-reboot >/dev/null 2>&1; then
  if grub-reboot "$ENTRY_HINT" >/dev/null 2>&1; then
    log "GRUB next entry set: $ENTRY_HINT"
    set_ok=1
  fi
fi

if [[ "$set_ok" -eq 0 ]]; then
  fail "cannot set bootloader recovery entry; ensure bootctl or grub-reboot is available and entry '$ENTRY_HINT' exists"
fi

if [[ "$DO_REBOOT" == "1" ]]; then
  log "rebooting now"
  systemctl reboot
else
  log "done (set SLM_RECOVERY_REBOOT_NOW=1 to reboot immediately)"
fi
