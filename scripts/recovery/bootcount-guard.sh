#!/usr/bin/env bash
set -euo pipefail

ACTION="${1:-check}"
ARG_SNAPSHOT="${2:-}"

BOOTCOUNT_FILE="${SLM_BOOTCOUNT_FILE:-/boot/bootcount}"
LAST_GOOD_FILE="${SLM_LAST_GOOD_FILE:-/boot/last_good_snapshot}"
LOG_FILE="${SLM_BOOT_RECOVERY_LOG:-/var/log/boot-recovery.log}"
THRESHOLD="${SLM_BOOTCOUNT_THRESHOLD:-3}"
ROOT_MOUNT="${SLM_ROOT_MOUNT:-/}"
GRUBENV_PATH="${SLM_GRUBENV_PATH:-/boot/grub/grubenv}"
RECOVERY_ATTEMPTS_FILE="${SLM_RECOVERY_ATTEMPTS_FILE:-/boot/recovery_attempts}"
RECOVERY_LOCK_FILE="${SLM_RECOVERY_LOCK_FILE:-/boot/recovery_auto.lock}"
AUTO_DISABLE_FILE="${SLM_RECOVERY_AUTO_DISABLE_FILE:-/boot/recovery.auto.disable}"
MAX_RECOVERY_ATTEMPTS="${SLM_MAX_RECOVERY_ATTEMPTS:-3}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REQUEST_RECOVERY_SCRIPT="${SLM_REQUEST_RECOVERY_SCRIPT:-$ROOT_DIR/recovery/request-bootloader-recovery.sh}"
RESTORE_SNAPSHOT_SCRIPT="${SLM_RESTORE_SNAPSHOT_SCRIPT:-$ROOT_DIR/recovery/restore-snapshot.sh}"

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][bootcount-guard] $*"
  echo "$msg"
  if [[ -n "$LOG_FILE" ]]; then
    mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
    echo "$msg" >> "$LOG_FILE" 2>/dev/null || true
  fi
}

die() {
  log "FAIL: $*"
  exit 1
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "must run as root"
  fi
}

read_count() {
  if [[ ! -f "$BOOTCOUNT_FILE" ]]; then
    echo 0
    return 0
  fi
  local raw
  raw="$(tr -dc '0-9' < "$BOOTCOUNT_FILE" || true)"
  if [[ -z "$raw" ]]; then
    echo 0
    return 0
  fi
  echo "$raw"
}

read_int_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo 0
    return 0
  fi
  local raw
  raw="$(tr -dc '0-9' < "$path" || true)"
  if [[ -z "$raw" ]]; then
    echo 0
    return 0
  fi
  echo "$raw"
}

write_count() {
  local next="$1"
  local tmp="${BOOTCOUNT_FILE}.tmp"
  printf '%s\n' "$next" > "$tmp"
  chmod 0644 "$tmp" || true
  mv -f "$tmp" "$BOOTCOUNT_FILE"
}

write_int_file() {
  local path="$1"
  local value="$2"
  local tmp="${path}.tmp"
  printf '%s\n' "$value" > "$tmp"
  chmod 0644 "$tmp" || true
  mv -f "$tmp" "$path"
}

sync_grub_env() {
  local key="$1"
  local value="$2"
  if command -v grub-editenv >/dev/null 2>&1 && [[ -f "$GRUBENV_PATH" ]]; then
    grub-editenv "$GRUBENV_PATH" set "${key}=${value}" >/dev/null 2>&1 || true
  fi
}

current_subvolume_name() {
  local opts
  opts="$(findmnt -no OPTIONS "$ROOT_MOUNT" 2>/dev/null || true)"
  if [[ "$opts" =~ subvol=([^,]+) ]]; then
    local subvol="${BASH_REMATCH[1]}"
    subvol="${subvol#/}"
    echo "$subvol"
    return 0
  fi
  echo "@root"
}

maybe_auto_rollback() {
  if [[ "${SLM_BOOT_AUTO_ROLLBACK:-0}" != "1" ]]; then
    return 0
  fi
  if [[ ! -f "$LAST_GOOD_FILE" ]]; then
    log "auto-rollback disabled: no last good file"
    return 0
  fi

  local snap
  snap="$(head -n1 "$LAST_GOOD_FILE" | tr -d '[:space:]')"
  if [[ -z "$snap" ]]; then
    log "auto-rollback disabled: last good snapshot empty"
    return 0
  fi
  if [[ ! -x "$RESTORE_SNAPSHOT_SCRIPT" ]]; then
    log "auto-rollback skipped: restore script missing"
    return 0
  fi

  log "attempting auto-rollback to snapshot: $snap"
  if "$RESTORE_SNAPSHOT_SCRIPT" --yes "$snap"; then
    log "auto-rollback succeeded"
  else
    log "auto-rollback failed"
  fi
}

kernel_panic_marker_present() {
  if [[ -d /sys/fs/pstore ]] && find /sys/fs/pstore -maxdepth 1 -type f | grep -q .; then
    return 0
  fi
  if command -v journalctl >/dev/null 2>&1; then
    if journalctl -b -1 -k --no-pager 2>/dev/null \
      | grep -Eiq 'kernel panic|panic - not syncing|Oops:'; then
      return 0
    fi
  fi
  return 1
}

auto_recovery_disabled() {
  [[ "${SLM_DISABLE_AUTO_RECOVERY:-0}" == "1" ]] && return 0
  [[ -f "$AUTO_DISABLE_FILE" ]] && return 0
  [[ "${slm_disable_auto_recovery:-0}" == "1" ]] && return 0
  return 1
}

trigger_recovery() {
  if [[ ! -x "$REQUEST_RECOVERY_SCRIPT" ]]; then
    die "missing recovery trigger script: $REQUEST_RECOVERY_SCRIPT"
  fi

  if auto_recovery_disabled; then
    log "auto recovery disabled by override"
    return 110
  fi

  if [[ -f "$RECOVERY_LOCK_FILE" ]]; then
    log "auto recovery locked by $RECOVERY_LOCK_FILE"
    return 111
  fi

  local attempts
  attempts="$(read_int_file "$RECOVERY_ATTEMPTS_FILE")"
  if (( attempts >= MAX_RECOVERY_ATTEMPTS )); then
    log "max recovery attempts exceeded: attempts=$attempts max=$MAX_RECOVERY_ATTEMPTS"
    printf 'locked at %s attempts\n' "$attempts" > "$RECOVERY_LOCK_FILE"
    sync_grub_env "slm_force_recovery" "0"
    return 111
  fi

  local next_attempts=$((attempts + 1))
  write_int_file "$RECOVERY_ATTEMPTS_FILE" "$next_attempts"
  sync_grub_env "slm_recovery_attempts" "$next_attempts"

  maybe_auto_rollback
  if "$REQUEST_RECOVERY_SCRIPT" auto; then
    log "recovery boot entry requested (attempt=$next_attempts)"
    sync_grub_env "slm_force_recovery" "1"
    return 0
  fi

  die "failed to request recovery boot entry"
}

increment() {
  require_root
  local now
  now="$(read_count)"
  local next=$((now + 1))
  write_count "$next"
  sync_grub_env "slm_bootcount" "$next"
  log "bootcount incremented: $now -> $next"
}

success() {
  require_root
  write_count 0
  write_int_file "$RECOVERY_ATTEMPTS_FILE" 0
  rm -f "$RECOVERY_LOCK_FILE"
  sync_grub_env "slm_bootcount" "0"
  sync_grub_env "slm_recovery_attempts" "0"
  sync_grub_env "slm_force_recovery" "0"

  local subvol
  if [[ -n "$ARG_SNAPSHOT" ]]; then
    subvol="$ARG_SNAPSHOT"
  else
    subvol="$(current_subvolume_name)"
  fi
  printf '%s\n' "$subvol" > "$LAST_GOOD_FILE"
  chmod 0644 "$LAST_GOOD_FILE" || true
  log "boot success marked, bootcount reset, last_good_snapshot=$subvol"
}

check() {
  require_root
  local now
  now="$(read_count)"
  log "bootcount check: current=$now threshold=$THRESHOLD"

  if kernel_panic_marker_present; then
    log "kernel panic marker detected from previous boot"
    trigger_recovery
    return 101
  fi

  if (( now >= THRESHOLD )); then
    log "threshold exceeded; triggering recovery"
    trigger_recovery
    return 100
  fi
  return 0
}

check_and_trigger() {
  check || return "$?"
  return 0
}

case "$ACTION" in
  increment) increment ;;
  success) success ;;
  check) check ;;
  check-and-trigger) check_and_trigger ;;
  clear-recovery-state)
    require_root
    write_int_file "$RECOVERY_ATTEMPTS_FILE" 0
    rm -f "$RECOVERY_LOCK_FILE"
    sync_grub_env "slm_recovery_attempts" "0"
    sync_grub_env "slm_force_recovery" "0"
    log "recovery state cleared"
    ;;
  *)
    echo "usage: $0 {increment|success|check|check-and-trigger|clear-recovery-state} [snapshot-id]" >&2
    exit 2
    ;;
esac
