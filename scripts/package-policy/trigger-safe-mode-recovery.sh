#!/usr/bin/env bash
set -euo pipefail

TARGET_USER="${SLM_DESKTOP_USER:-}"
if [[ -n "${1:-}" && "${1:-}" != "--" ]]; then
  TARGET_USER="$1"
fi
if [[ -z "$TARGET_USER" && -n "${SUDO_USER:-}" ]]; then
  TARGET_USER="$SUDO_USER"
fi
if [[ -z "$TARGET_USER" ]]; then
  TARGET_USER="${USER:-}"
fi

if ! getent passwd "$TARGET_USER" >/dev/null 2>&1; then
  echo "trigger-safe-mode-recovery: unknown user '$TARGET_USER'" >&2
  exit 2
fi

TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
if [[ -z "$TARGET_HOME" || ! -d "$TARGET_HOME" ]]; then
  echo "trigger-safe-mode-recovery: home dir not found for '$TARGET_USER'" >&2
  exit 2
fi

CFG_DIR="$TARGET_HOME/.config/slm-desktop"
STATE_FILE="$CFG_DIR/state.json"
ACTIVE_CFG="$CFG_DIR/config.json"
PREV_CFG="$CFG_DIR/config.prev.json"
SAFE_CFG="$CFG_DIR/config.safe.json"
mkdir -p "$CFG_DIR"

rollback_source="none"
if [[ -f "$SAFE_CFG" ]]; then
  cp -f "$SAFE_CFG" "$ACTIVE_CFG"
  rollback_source="safe"
elif [[ -f "$PREV_CFG" ]]; then
  cp -f "$PREV_CFG" "$ACTIVE_CFG"
  rollback_source="previous"
fi

now_iso="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
reason="package-policy-health-failed"

if command -v jq >/dev/null 2>&1 && [[ -f "$STATE_FILE" ]]; then
  tmp="$(mktemp)"
  jq \
    --arg reason "$reason" \
    --arg now "$now_iso" \
    --arg mode "safe" \
    '.safe_mode_forced = true
     | .config_pending = false
     | .recovery_reason = $reason
     | .last_boot_status = "recovery-requested"
     | .last_mode = $mode
     | .last_updated = $now' \
    "$STATE_FILE" > "$tmp" 2>/dev/null || true
  if [[ -s "$tmp" ]]; then
    mv "$tmp" "$STATE_FILE"
  else
    rm -f "$tmp"
  fi
fi

if [[ ! -f "$STATE_FILE" ]]; then
  cat > "$STATE_FILE" <<JSON
{
  "crash_count": 0,
  "last_mode": "safe",
  "last_good_snapshot": "",
  "safe_mode_forced": true,
  "config_pending": false,
  "recovery_reason": "$reason",
  "last_boot_status": "recovery-requested",
  "last_updated": "$now_iso"
}
JSON
fi

if [[ "$EUID" -eq 0 ]]; then
  chown -R "$TARGET_USER":"$TARGET_USER" "$CFG_DIR" 2>/dev/null || true
fi

echo "trigger-safe-mode-recovery: user=$TARGET_USER rollback_source=$rollback_source state=$STATE_FILE"
