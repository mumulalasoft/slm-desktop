#!/usr/bin/env bash
set -euo pipefail

SNAPSHOT_ID=""
ASSUME_YES=0
ROOT_MOUNT="${SLM_ROOT_MOUNT:-/}"
SNAPSHOTS_DIR="${SLM_SNAPSHOTS_DIR:-/.snapshots}"
LOG_FILE="${SLM_RECOVERY_LOG:-/var/log/recovery.log}"

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][restore-snapshot] $*"
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
Usage: $0 [--yes] [snapshot-id]

Set Btrfs default subvolume to selected snapshot.
Reboot is required afterwards.

Options:
  --yes          Skip confirmation prompt
  --help         Show this help
USAGE
}

require_root() {
  [[ "${EUID}" -eq 0 ]] || die "must run as root"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --yes|-y) ASSUME_YES=1; shift ;;
      --help|-h) usage; exit 0 ;;
      *)
        if [[ -z "$SNAPSHOT_ID" ]]; then
          SNAPSHOT_ID="$1"
          shift
        else
          die "unexpected argument: $1"
        fi
        ;;
    esac
  done
}

ensure_btrfs_root() {
  local fs
  fs="$(findmnt -no FSTYPE "$ROOT_MOUNT" 2>/dev/null || true)"
  [[ "$fs" == "btrfs" ]] || die "root mount is not btrfs: $ROOT_MOUNT"
}

list_snapshots() {
  [[ -d "$SNAPSHOTS_DIR" ]] || die "snapshot directory missing: $SNAPSHOTS_DIR"
  find "$SNAPSHOTS_DIR" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort
}

choose_snapshot_interactive() {
  local snaps
  mapfile -t snaps < <(list_snapshots)
  (( ${#snaps[@]} > 0 )) || die "no snapshots available"

  echo "Available snapshots:" >&2
  local i
  for i in "${!snaps[@]}"; do
    printf '  %2d) %s\n' "$((i + 1))" "${snaps[$i]}" >&2
  done
  printf 'Select snapshot number: ' >&2
  read -r idx
  [[ "$idx" =~ ^[0-9]+$ ]] || die "invalid selection"
  (( idx >= 1 && idx <= ${#snaps[@]} )) || die "selection out of range"
  SNAPSHOT_ID="${snaps[$((idx - 1))]}"
}

confirm() {
  (( ASSUME_YES == 1 )) && return 0
  printf 'Set default subvolume to "%s" and require reboot? [y/N]: ' "$SNAPSHOT_ID" >&2
  read -r ans
  [[ "$ans" == "y" || "$ans" == "Y" ]] || die "operation cancelled"
}

main() {
  parse_args "$@"
  require_root
  ensure_btrfs_root

  if [[ -z "$SNAPSHOT_ID" ]]; then
    choose_snapshot_interactive
  fi

  local snapshot_path="$SNAPSHOTS_DIR/$SNAPSHOT_ID"
  [[ -d "$snapshot_path" ]] || die "snapshot not found: $snapshot_path"

  confirm

  local subvol_id
  subvol_id="$(btrfs subvolume show "$snapshot_path" | awk '/Subvolume ID:/ {print $3; exit}')"
  [[ -n "$subvol_id" ]] || die "cannot resolve subvolume id for: $snapshot_path"

  log "setting btrfs default subvolume id=$subvol_id snapshot=$SNAPSHOT_ID"
  btrfs subvolume set-default "$subvol_id" "$ROOT_MOUNT"
  sync
  log "restore configured successfully; reboot required"
  echo "Reboot required to boot into snapshot: $SNAPSHOT_ID"
}

main "$@"
