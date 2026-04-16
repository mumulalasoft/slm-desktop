#!/usr/bin/env bash
set -euo pipefail

SNAP_DIR="${SLM_SNAPSHOTS_DIR:-/.snapshots}"
KEEP_AUTO="${SLM_KEEP_AUTO_SNAPSHOTS:-10}"
DRY_RUN=0
LOG_FILE="${SLM_RECOVERY_LOG:-/var/log/recovery.log}"

usage() {
  cat <<USAGE
Usage: $0 [--dir PATH] [--keep-auto N] [--dry-run]

Retention policy:
- keep newest N automatic snapshots matching: YYYYMMDD-HHMMSS-auto-*
- keep all manual snapshots (YYYYMMDD-HHMMSS-manual-*)
USAGE
}

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][prune-snapshots] $*"
  echo "$msg"
  mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
  echo "$msg" >> "$LOG_FILE" 2>/dev/null || true
}

die() {
  log "FAIL: $*"
  exit 1
}

require_root() {
  [[ "${EUID}" -eq 0 ]] || die "must run as root"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dir) SNAP_DIR="$2"; shift 2 ;;
    --keep-auto) KEEP_AUTO="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ "$KEEP_AUTO" =~ ^[0-9]+$ ]] || die "--keep-auto must be integer"
require_root

if [[ ! -d "$SNAP_DIR" ]]; then
  log "snapshot directory missing, nothing to prune: $SNAP_DIR"
  exit 0
fi

mapfile -t auto_snapshots < <(
  find "$SNAP_DIR" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' \
  | grep -E '^[0-9]{8}-[0-9]{6}-auto-' \
  | sort
)

auto_count="${#auto_snapshots[@]}"
if (( auto_count <= KEEP_AUTO )); then
  log "retention ok: auto_count=$auto_count keep_auto=$KEEP_AUTO"
  exit 0
fi

delete_count=$((auto_count - KEEP_AUTO))
log "pruning auto snapshots: total=$auto_count keep=$KEEP_AUTO delete=$delete_count"

for ((i=0; i<delete_count; i++)); do
  name="${auto_snapshots[$i]}"
  path="$SNAP_DIR/$name"
  [[ -d "$path" ]] || continue

  if (( DRY_RUN == 1 )); then
    log "dry-run delete: $path"
    continue
  fi

  if btrfs subvolume delete "$path" >/dev/null 2>&1; then
    log "deleted subvolume snapshot: $path"
  else
    rm -rf --one-file-system "$path"
    log "deleted directory snapshot fallback: $path"
  fi
done

log "prune completed"
