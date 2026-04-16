#!/usr/bin/env bash
set -euo pipefail

DISK=""
ASSUME_YES=0
ESP_SIZE_MIB="${SLM_ESP_SIZE_MIB:-512}"
RECOVERY_SIZE_MIB="${SLM_RECOVERY_SIZE_MIB:-1024}"

usage() {
  cat <<USAGE
Usage: $0 --disk /dev/sdX [--yes]

Partition disk for hybrid recovery layout (UEFI):
  1) EFI (FAT32)      ${ESP_SIZE_MIB}MiB
  2) RECOVERY (ext4)  ${RECOVERY_SIZE_MIB}MiB
  3) SYSTEM (btrfs)   rest of disk

WARNING: This destroys existing partition table on target disk.
USAGE
}

die() { echo "[partition-hybrid][FAIL] $*" >&2; exit 1; }
log() { echo "[partition-hybrid] $*"; }

confirm() {
  (( ASSUME_YES == 1 )) && return 0
  echo "Destructive operation on disk: $DISK" >&2
  echo "Type EXACTLY 'ERASE $DISK' to continue:" >&2
  read -r answer
  [[ "$answer" == "ERASE $DISK" ]] || die "confirmation mismatch"
}

part_path() {
  local disk="$1"
  local n="$2"
  if [[ "$disk" =~ nvme[0-9]+n[0-9]+$ || "$disk" =~ mmcblk[0-9]+$ ]]; then
    echo "${disk}p${n}"
  else
    echo "${disk}${n}"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --disk) DISK="$2"; shift 2 ;;
    --yes|-y) ASSUME_YES=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ -n "$DISK" ]] || die "--disk is required"
[[ -b "$DISK" ]] || die "not a block device: $DISK"
[[ "${EUID}" -eq 0 ]] || die "must run as root"
command -v sgdisk >/dev/null 2>&1 || die "sgdisk not found"

confirm

local_end_esp="$((1 + ESP_SIZE_MIB))MiB"
local_end_recovery="$((1 + ESP_SIZE_MIB + RECOVERY_SIZE_MIB))MiB"

log "wiping GPT on $DISK"
sgdisk --zap-all "$DISK"

log "creating EFI partition"
sgdisk -n 1:1MiB:"$local_end_esp" -t 1:EF00 -c 1:EFI "$DISK"

log "creating RECOVERY partition"
sgdisk -n 2:"$local_end_esp":"$local_end_recovery" -t 2:8300 -c 2:RECOVERY "$DISK"

log "creating SYSTEM partition"
sgdisk -n 3:"$local_end_recovery":0 -t 3:8300 -c 3:SYSTEM "$DISK"

partprobe "$DISK" || true

ESP_PART="$(part_path "$DISK" 1)"
REC_PART="$(part_path "$DISK" 2)"
SYS_PART="$(part_path "$DISK" 3)"

log "partitioning complete"
log "ESP:      $ESP_PART"
log "RECOVERY: $REC_PART"
log "SYSTEM:   $SYS_PART"
