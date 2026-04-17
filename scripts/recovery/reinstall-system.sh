#!/usr/bin/env bash
set -euo pipefail

SNAPSHOT_ID=""
ASSUME_YES=0
LOG_FILE="${SLM_RECOVERY_LOG:-/var/log/recovery.log}"
TOP_MOUNT=""

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][reinstall-system] $*"
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
Usage: $0 [--yes] [--snapshot SNAPSHOT_ID]

Reset @root from selected snapshot while preserving @home.
If --snapshot is omitted, /boot/last_good_snapshot is used.
USAGE
}

require_root() {
  [[ "${EUID}" -eq 0 ]] || die "must run as root"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --yes|-y) ASSUME_YES=1; shift ;;
      --snapshot) SNAPSHOT_ID="$2"; shift 2 ;;
      --help|-h) usage; exit 0 ;;
      *) die "unknown argument: $1" ;;
    esac
  done
}

resolve_snapshot_id() {
  if [[ -n "$SNAPSHOT_ID" ]]; then
    return 0
  fi
  if [[ -f /boot/last_good_snapshot ]]; then
    SNAPSHOT_ID="$(head -n1 /boot/last_good_snapshot | tr -d '[:space:]')"
  fi
  [[ -n "$SNAPSHOT_ID" ]] || die "snapshot id is required (or set /boot/last_good_snapshot)"
}

confirm() {
  (( ASSUME_YES == 1 )) && return 0
  cat >&2 <<CONFIRM
This operation will replace @root using snapshot "$SNAPSHOT_ID".
@home will be preserved.
Continue? [y/N]
CONFIRM
  read -r ans
  [[ "$ans" == "y" || "$ans" == "Y" ]] || die "operation cancelled"
}

mount_top_level() {
  local root_dev
  root_dev="$(findmnt -no SOURCE / | sed -E 's/\[[^]]+\]$//')"
  [[ -n "$root_dev" ]] || die "cannot resolve root btrfs device"

  TOP_MOUNT="$(mktemp -d /tmp/slm-reinstall-root.XXXXXX)"
  mount -t btrfs -o subvolid=5 "$root_dev" "$TOP_MOUNT"
  log "mounted top-level btrfs at $TOP_MOUNT"
}

cleanup() {
  if [[ -n "$TOP_MOUNT" && -d "$TOP_MOUNT" ]]; then
    umount "$TOP_MOUNT" 2>/dev/null || true
    rmdir "$TOP_MOUNT" 2>/dev/null || true
  fi
}

main() {
  trap cleanup EXIT
  parse_args "$@"
  require_root
  resolve_snapshot_id
  confirm
  mount_top_level

  local root_subvol="$TOP_MOUNT/@root"
  local home_subvol="$TOP_MOUNT/@home"
  local snapshots_dir="$TOP_MOUNT/@snapshots"
  local source_snapshot="$snapshots_dir/$SNAPSHOT_ID"

  [[ -d "$home_subvol" ]] || die "@home missing, abort"
  [[ -d "$root_subvol" ]] || die "@root missing, abort"
  [[ -d "$source_snapshot" ]] || die "snapshot not found: $source_snapshot"

  local ts
  ts="$(date +"%Y%m%d-%H%M%S")"
  log "creating safety backup of @root -> @snapshots/pre-reset-$ts"
  btrfs subvolume snapshot -r "$root_subvol" "$snapshots_dir/pre-reset-$ts"

  local next_root="$TOP_MOUNT/@root.next-$ts"
  log "creating next root subvolume from snapshot $SNAPSHOT_ID -> $next_root"
  btrfs subvolume snapshot "$source_snapshot" "$next_root"

  local new_id
  new_id="$(btrfs subvolume show "$next_root" | awk '/Subvolume ID:/ {print $3; exit}')"
  [[ -n "$new_id" ]] || die "cannot resolve new @root subvolume id"

  btrfs subvolume set-default "$new_id" "$TOP_MOUNT"
  sync

  log "system reset staged; next boot uses subvolume id=$new_id path=@root.next-$ts; reboot required"
  echo "System reset staged to @root.next-$ts. Reboot required."
}

main "$@"
