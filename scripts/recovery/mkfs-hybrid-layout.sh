#!/usr/bin/env bash
set -euo pipefail

ESP_PART=""
RECOVERY_PART=""
SYSTEM_PART=""
ASSUME_YES=0

usage() {
  cat <<USAGE
Usage: $0 --esp /dev/XXX --recovery /dev/YYY --system /dev/ZZZ [--yes]

Formats hybrid recovery partitions and prepares Btrfs subvolumes:
  @root, @home, @var, @snapshots
USAGE
}

die() { echo "[mkfs-hybrid][FAIL] $*" >&2; exit 1; }
log() { echo "[mkfs-hybrid] $*"; }

confirm() {
  (( ASSUME_YES == 1 )) && return 0
  echo "This will format:" >&2
  echo "  ESP:      $ESP_PART" >&2
  echo "  RECOVERY: $RECOVERY_PART" >&2
  echo "  SYSTEM:   $SYSTEM_PART" >&2
  echo "Type 'FORMAT' to continue:" >&2
  read -r ans
  [[ "$ans" == "FORMAT" ]] || die "operation cancelled"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --esp) ESP_PART="$2"; shift 2 ;;
    --recovery) RECOVERY_PART="$2"; shift 2 ;;
    --system) SYSTEM_PART="$2"; shift 2 ;;
    --yes|-y) ASSUME_YES=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ -n "$ESP_PART" && -n "$RECOVERY_PART" && -n "$SYSTEM_PART" ]] || die "all partition args are required"
[[ "${EUID}" -eq 0 ]] || die "must run as root"

confirm

mkfs.fat -F 32 -n EFI "$ESP_PART"
mkfs.ext4 -F -L RECOVERY "$RECOVERY_PART"
mkfs.btrfs -f -L SLMROOT "$SYSTEM_PART"

workdir="$(mktemp -d /tmp/slm-btrfs-layout.XXXXXX)"
trap 'umount "$workdir" 2>/dev/null || true; rmdir "$workdir" 2>/dev/null || true' EXIT

mount "$SYSTEM_PART" "$workdir"
btrfs subvolume create "$workdir/@root"
btrfs subvolume create "$workdir/@home"
btrfs subvolume create "$workdir/@var"
btrfs subvolume create "$workdir/@snapshots"
umount "$workdir"

root_uuid="$(blkid -s UUID -o value "$SYSTEM_PART")"
esp_uuid="$(blkid -s UUID -o value "$ESP_PART")"
rec_uuid="$(blkid -s UUID -o value "$RECOVERY_PART")"

cat <<FSTAB

# /etc/fstab (hybrid recovery baseline)
UUID=$root_uuid  /            btrfs  subvol=@root,compress=zstd,noatime,space_cache=v2  0  0
UUID=$root_uuid  /home        btrfs  subvol=@home,compress=zstd,noatime,space_cache=v2  0  0
UUID=$root_uuid  /var         btrfs  subvol=@var,compress=zstd,noatime,space_cache=v2   0  0
UUID=$root_uuid  /.snapshots  btrfs  subvol=@snapshots,compress=zstd,noatime,space_cache=v2 0 0
UUID=$esp_uuid   /boot/efi    vfat   umask=0077                                      0  2
UUID=$rec_uuid   /recovery    ext4   defaults,noatime                                 0  2

FSTAB

log "format and subvolume initialization complete"
