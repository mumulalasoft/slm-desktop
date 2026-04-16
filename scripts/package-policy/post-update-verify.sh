#!/usr/bin/env bash
set -euo pipefail

STATE_DIR_DEFAULT="/var/lib/slm-package-policy"
STATE_DIR="${SLM_PACKAGE_POLICY_STATE_DIR:-$STATE_DIR_DEFAULT}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRUNE_SCRIPT="${SLM_SNAPSHOT_PRUNE_SCRIPT:-$ROOT_DIR/recovery/prune-btrfs-snapshots.sh}"
SNAPSHOT_ID=""

ensure_state_dir() {
  local target="$1"
  if mkdir -p "$target" 2>/dev/null; then
    echo "$target"
    return 0
  fi

  local fallback="${HOME:-/tmp}/.local/state/slm-package-policy"
  if ! mkdir -p "$fallback" 2>/dev/null; then
    fallback="/tmp/slm-package-policy-state"
    mkdir -p "$fallback"
  fi
  echo "$fallback"
}

STATE_DIR="$(ensure_state_dir "$STATE_DIR")"
REPORT_DIR="$STATE_DIR/health"
mkdir -p "$REPORT_DIR"
REPORT_FILE="$REPORT_DIR/post-update-verify-$(date -u +%Y%m%dT%H%M%SZ)-$$.log"

log() {
  echo "$*" | tee -a "$REPORT_FILE" >/dev/null
}

fail() {
  log "[verify][fail] $*"
  return 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --snapshot-id) SNAPSHOT_ID="${2:-}"; shift 2 ;;
    *) shift ;;
  esac
done

fail_count=0

log "[verify] snapshot_id=${SNAPSHOT_ID:-none}"

root_fs="$(findmnt -no FSTYPE / 2>/dev/null || true)"
if [[ "$root_fs" != "btrfs" ]]; then
  log "[verify][warn] root fs is not btrfs: ${root_fs:-unknown}"
else
  if [[ ! -d /.snapshots ]]; then
    fail "btrfs root but /.snapshots is missing"
    fail_count=$((fail_count + 1))
  fi

  if ! find /.snapshots -mindepth 1 -maxdepth 1 -type d | grep -q .; then
    fail "no snapshots found in /.snapshots"
    fail_count=$((fail_count + 1))
  fi

  if [[ -x "$PRUNE_SCRIPT" && "${SLM_AUTO_PRUNE_AFTER_VERIFY:-1}" == "1" ]]; then
    if "$PRUNE_SCRIPT" --keep-auto "${SLM_KEEP_AUTO_SNAPSHOTS:-10}" >> "$REPORT_FILE" 2>&1; then
      log "[verify][action] snapshot retention executed"
    else
      fail "snapshot retention failed"
      fail_count=$((fail_count + 1))
    fi
  fi
fi

for required in /usr/local/lib/slm-recovery/restore-snapshot.sh \
                /usr/local/lib/slm-recovery/repair-boot.sh \
                /usr/local/lib/slm-recovery/bootcount-guard.sh; do
  if [[ ! -x "$required" ]]; then
    log "[verify][warn] missing recovery runtime script: $required"
  fi
done

if [[ "$fail_count" -gt 0 ]]; then
  log "[verify][result] failed count=$fail_count"
  echo "post-update verification failed ($fail_count); report: $REPORT_FILE" >&2
  exit 1
fi

log "[verify][result] ok"
exit 0
