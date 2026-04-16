#!/usr/bin/env bash
set -euo pipefail

EFI_MOUNT="${SLM_EFI_MOUNT:-/boot/efi}"
BOOT_ID="${SLM_BOOTLOADER_ID:-SLM}"
ROOT_MOUNT="${SLM_ROOT_MOUNT:-/}"
LOG_FILE="${SLM_RECOVERY_LOG:-/var/log/recovery.log}"

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][repair-boot] $*"
  echo "$msg"
  mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
  echo "$msg" >> "$LOG_FILE" 2>/dev/null || true
}

die() {
  log "FAIL: $*"
  exit 1
}

usage() {
  cat <<USAGE
Usage: $0 [--efi-mount PATH] [--boot-id ID]

Repair UEFI bootloader using GRUB tools.
USAGE
}

require_root() {
  [[ "${EUID}" -eq 0 ]] || die "must run as root"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --efi-mount) EFI_MOUNT="$2"; shift 2 ;;
      --boot-id) BOOT_ID="$2"; shift 2 ;;
      --help|-h) usage; exit 0 ;;
      *) die "unknown argument: $1" ;;
    esac
  done
}

ensure_efi_mounted() {
  mkdir -p "$EFI_MOUNT"
  if mountpoint -q "$EFI_MOUNT"; then
    return 0
  fi

  local efi_dev=""
  efi_dev="$(blkid -L EFI 2>/dev/null || true)"
  if [[ -z "$efi_dev" ]]; then
    die "EFI partition is not mounted and label EFI was not found"
  fi

  mount "$efi_dev" "$EFI_MOUNT"
  log "mounted EFI from $efi_dev to $EFI_MOUNT"
}

regenerate_grub_cfg() {
  if command -v grub-mkconfig >/dev/null 2>&1; then
    local cfg
    if [[ -d /boot/grub ]]; then
      cfg="/boot/grub/grub.cfg"
    elif [[ -d /boot/grub2 ]]; then
      cfg="/boot/grub2/grub.cfg"
    else
      mkdir -p /boot/grub
      cfg="/boot/grub/grub.cfg"
    fi
    grub-mkconfig -o "$cfg"
    log "grub config regenerated: $cfg"
    return 0
  fi

  if command -v update-grub >/dev/null 2>&1; then
    update-grub
    log "grub config regenerated via update-grub"
    return 0
  fi

  die "no grub config generator found (grub-mkconfig/update-grub)"
}

main() {
  parse_args "$@"
  require_root

  command -v grub-install >/dev/null 2>&1 || die "grub-install not found"

  ensure_efi_mounted

  local root_dev
  root_dev="$(findmnt -no SOURCE "$ROOT_MOUNT" | sed -E 's/\[[^]]+\]$//')"
  [[ -n "$root_dev" ]] || die "cannot resolve root device"

  log "reinstalling grub: root_dev=$root_dev efi_mount=$EFI_MOUNT boot_id=$BOOT_ID"
  grub-install --target=x86_64-efi --efi-directory="$EFI_MOUNT" --bootloader-id="$BOOT_ID" --recheck

  regenerate_grub_cfg
  sync
  log "bootloader repair completed"
}

main "$@"
