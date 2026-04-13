#!/usr/bin/env bash
set -euo pipefail

METHOD="${1:-status}"
ARG="${2:-}"

SERVICE="org.slm.Desktop.Recovery"
PATH_OBJ="/org/slm/Desktop/Recovery"
IFACE="org.slm.Desktop.Recovery"

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

if command -v qdbus >/dev/null 2>&1; then
  case "$METHOD" in
    status)
      call_qdbus "GetStatus"
      ;;
    ping)
      call_qdbus "Ping"
      ;;
    force-safe)
      call_qdbus "RequestSafeMode" "${ARG:-manual-force-safe}"
      ;;
    force-recovery)
      call_qdbus "RequestRecoveryPartition" "${ARG:-manual-force-recovery-partition}"
      ;;
    auto-recover)
      call_qdbus "TriggerAutoRecovery" "${ARG:-manual-auto-recovery}"
      ;;
    clear)
      call_qdbus "ClearRecoveryFlags"
      ;;
    *)
      echo "usage: $0 {status|ping|force-safe|force-recovery|auto-recover|clear} [reason]" >&2
      exit 2
      ;;
  esac
else
  case "$METHOD" in
    status) call_dbus_send "GetStatus" ;;
    ping) call_dbus_send "Ping" ;;
    force-safe) call_dbus_send "RequestSafeMode" "${ARG:-manual-force-safe}" ;;
    force-recovery) call_dbus_send "RequestRecoveryPartition" "${ARG:-manual-force-recovery-partition}" ;;
    auto-recover) call_dbus_send "TriggerAutoRecovery" "${ARG:-manual-auto-recovery}" ;;
    clear) call_dbus_send "ClearRecoveryFlags" ;;
    *)
      echo "usage: $0 {status|ping|force-safe|force-recovery|auto-recover|clear} [reason]" >&2
      exit 2
      ;;
  esac
fi
