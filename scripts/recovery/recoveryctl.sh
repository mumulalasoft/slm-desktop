#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SERVICE="org.slm.Desktop.Recovery"
PATH_OBJ="/org/slm/Desktop/Recovery"
IFACE="org.slm.Desktop.Recovery"

SNAPSHOTS_DIR="${SLM_SNAPSHOTS_DIR:-/.snapshots}"
LOG_FILE="${SLM_RECOVERY_LOG:-/var/log/recovery.log}"

RESTORE_SCRIPT="${SLM_RESTORE_SNAPSHOT_SCRIPT:-$ROOT_DIR/recovery/restore-snapshot.sh}"
REINSTALL_SCRIPT="${SLM_REINSTALL_SYSTEM_SCRIPT:-$ROOT_DIR/recovery/reinstall-system.sh}"
REPAIR_BOOT_SCRIPT="${SLM_REPAIR_BOOT_SCRIPT:-$ROOT_DIR/recovery/repair-boot.sh}"
BOOT_GUARD_SCRIPT="${SLM_BOOTCOUNT_GUARD_SCRIPT:-$ROOT_DIR/recovery/bootcount-guard.sh}"
PRUNE_SCRIPT="${SLM_SNAPSHOT_PRUNE_SCRIPT:-$ROOT_DIR/recovery/prune-btrfs-snapshots.sh}"
GRUB_INTEGRATE_SCRIPT="${SLM_GRUB_INTEGRATE_SCRIPT:-$ROOT_DIR/recovery/install-grub-boot-failure-integration.sh}"

log() {
  local msg="[$(date -u +"%Y-%m-%dT%H:%M:%SZ")][recoveryctl] $*"
  echo "$msg"
  mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
  echo "$msg" >> "$LOG_FILE" 2>/dev/null || true
}

usage() {
  cat <<USAGE
Usage:
  $0 snapshot create [auto|manual] [reason]
  $0 snapshot list
  $0 snapshot prune [--keep-auto N] [--dry-run]
  $0 snapshot restore [snapshot-id] [--yes]
  $0 system reset [snapshot-id] [--yes]
  $0 boot repair [--efi-mount PATH] [--boot-id ID]
  $0 boot grub-integrate [--no-regenerate]
  $0 boot check
  $0 boot success [snapshot-id]
  $0 boot clear-state

Legacy daemon DBus controls (kept for compatibility):
  $0 {status|ping|force-safe|force-recovery|auto-recover|clear} [reason]
USAGE
}

require_root() {
  [[ "${EUID}" -eq 0 ]] || { echo "must run as root for this command" >&2; exit 1; }
}

sanitize_token() {
  local raw="$1"
  raw="${raw// /-}"
  raw="$(echo "$raw" | tr -cd '[:alnum:]._:-')"
  echo "$raw"
}

snapshot_create() {
  require_root
  local snap_type="${1:-auto}"
  local reason="${2:-manual}"
  [[ "$snap_type" == "auto" || "$snap_type" == "manual" ]] || {
    echo "snapshot type must be auto|manual" >&2
    exit 2
  }

  local ts safe_reason snap_name
  ts="$(date +'%Y%m%d-%H%M%S')"
  safe_reason="$(sanitize_token "$reason")"
  snap_name="${ts}-${snap_type}-${safe_reason}"

  mkdir -p "$SNAPSHOTS_DIR"
  btrfs subvolume snapshot -r / "$SNAPSHOTS_DIR/$snap_name"
  log "snapshot created: $snap_name"
  if [[ "$snap_type" == "auto" && "${SLM_SNAPSHOT_AUTO_PRUNE:-1}" == "1" && -x "$PRUNE_SCRIPT" ]]; then
    "$PRUNE_SCRIPT" --dir "$SNAPSHOTS_DIR" --keep-auto "${SLM_KEEP_AUTO_SNAPSHOTS:-10}" >/dev/null 2>&1 || true
  fi
  echo "$snap_name"
}

snapshot_list() {
  [[ -d "$SNAPSHOTS_DIR" ]] || { echo "no snapshot directory: $SNAPSHOTS_DIR"; return 0; }
  find "$SNAPSHOTS_DIR" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort
}

snapshot_restore() {
  require_root
  [[ -x "$RESTORE_SCRIPT" ]] || { echo "missing restore script: $RESTORE_SCRIPT" >&2; exit 1; }
  "$RESTORE_SCRIPT" "$@"
}

snapshot_prune() {
  require_root
  [[ -x "$PRUNE_SCRIPT" ]] || { echo "missing prune script: $PRUNE_SCRIPT" >&2; exit 1; }
  "$PRUNE_SCRIPT" --dir "$SNAPSHOTS_DIR" "$@"
}

system_reset() {
  require_root
  [[ -x "$REINSTALL_SCRIPT" ]] || { echo "missing reinstall script: $REINSTALL_SCRIPT" >&2; exit 1; }

  local args=()
  local first="${1:-}"
  if [[ -n "$first" && "$first" != "--yes" && "$first" != "-y" ]]; then
    args+=(--snapshot "$first")
    shift
  fi
  args+=("$@")
  "$REINSTALL_SCRIPT" "${args[@]}"
}

boot_repair() {
  require_root
  [[ -x "$REPAIR_BOOT_SCRIPT" ]] || { echo "missing boot repair script: $REPAIR_BOOT_SCRIPT" >&2; exit 1; }
  "$REPAIR_BOOT_SCRIPT" "$@"
}

boot_check() {
  require_root
  [[ -x "$BOOT_GUARD_SCRIPT" ]] || { echo "missing boot guard script: $BOOT_GUARD_SCRIPT" >&2; exit 1; }
  "$BOOT_GUARD_SCRIPT" check
}

boot_success() {
  require_root
  [[ -x "$BOOT_GUARD_SCRIPT" ]] || { echo "missing boot guard script: $BOOT_GUARD_SCRIPT" >&2; exit 1; }
  "$BOOT_GUARD_SCRIPT" success "${1:-}"
}

boot_clear_state() {
  require_root
  [[ -x "$BOOT_GUARD_SCRIPT" ]] || { echo "missing boot guard script: $BOOT_GUARD_SCRIPT" >&2; exit 1; }
  "$BOOT_GUARD_SCRIPT" clear-recovery-state
}

boot_grub_integrate() {
  require_root
  [[ -x "$GRUB_INTEGRATE_SCRIPT" ]] || { echo "missing GRUB integration script: $GRUB_INTEGRATE_SCRIPT" >&2; exit 1; }
  "$GRUB_INTEGRATE_SCRIPT" "$@"
}

call_qdbus() {
  local method="$1"
  shift || true
  qdbus "$SERVICE" "$PATH_OBJ" "$IFACE.$method" "$@"
}

call_dbus_send() {
  local method="$1"
  shift || true

  case "$method" in
    Ping|GetStatus|ClearRecoveryFlags)
      dbus-send --session --print-reply --dest="$SERVICE" "$PATH_OBJ" "$IFACE.$method"
      ;;
    RequestSafeMode|RequestRecoveryPartition)
      local arg="${1:-manual}"
      dbus-send --session --print-reply --dest="$SERVICE" "$PATH_OBJ" "$IFACE.$method" string:"$arg"
      ;;
    TriggerAutoRecovery)
      local arg="${1:-manual-trigger}"
      dbus-send --session --print-reply --dest="$SERVICE" "$PATH_OBJ" "$IFACE.$method" string:"$arg"
      ;;
    *)
      echo "unsupported method for dbus-send fallback: $method" >&2
      return 2
      ;;
  esac
}

legacy_dbus_dispatch() {
  local method="$1"
  local arg="${2:-}"
  if command -v qdbus >/dev/null 2>&1; then
    case "$method" in
      status) call_qdbus "GetStatus" ;;
      ping) call_qdbus "Ping" ;;
      force-safe) call_qdbus "RequestSafeMode" "${arg:-manual-force-safe}" ;;
      force-recovery) call_qdbus "RequestRecoveryPartition" "${arg:-manual-force-recovery-partition}" ;;
      auto-recover) call_qdbus "TriggerAutoRecovery" "${arg:-manual-auto-recovery}" ;;
      clear) call_qdbus "ClearRecoveryFlags" ;;
      *) return 2 ;;
    esac
  else
    case "$method" in
      status) call_dbus_send "GetStatus" ;;
      ping) call_dbus_send "Ping" ;;
      force-safe) call_dbus_send "RequestSafeMode" "${arg:-manual-force-safe}" ;;
      force-recovery) call_dbus_send "RequestRecoveryPartition" "${arg:-manual-force-recovery-partition}" ;;
      auto-recover) call_dbus_send "TriggerAutoRecovery" "${arg:-manual-auto-recovery}" ;;
      clear) call_dbus_send "ClearRecoveryFlags" ;;
      *) return 2 ;;
    esac
  fi
}

main() {
  local cmd="${1:-}"
  [[ -n "$cmd" ]] || { usage; exit 2; }

  case "$cmd" in
    snapshot)
      local sub="${2:-}"
      case "$sub" in
        create) shift 2; snapshot_create "${1:-auto}" "${2:-manual}" ;;
        list) snapshot_list ;;
        prune) shift 2; snapshot_prune "$@" ;;
        restore) shift 2; snapshot_restore "$@" ;;
        *) usage; exit 2 ;;
      esac
      ;;
    system)
      local sub="${2:-}"
      case "$sub" in
        reset) shift 2; system_reset "$@" ;;
        *) usage; exit 2 ;;
      esac
      ;;
    boot)
      local sub="${2:-}"
      case "$sub" in
        repair) shift 2; boot_repair "$@" ;;
        grub-integrate) shift 2; boot_grub_integrate "$@" ;;
        check) boot_check ;;
        success) boot_success "${3:-}" ;;
        clear-state) boot_clear_state ;;
        *) usage; exit 2 ;;
      esac
      ;;
    status|ping|force-safe|force-recovery|auto-recover|clear)
      legacy_dbus_dispatch "$cmd" "${2:-}" || { usage; exit 2; }
      ;;
    *)
      usage
      exit 2
      ;;
  esac
}

main "$@"
