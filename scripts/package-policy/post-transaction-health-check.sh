#!/usr/bin/env bash
set -euo pipefail

STATE_DIR_DEFAULT="/var/lib/slm-package-policy"
STATE_DIR="${SLM_PACKAGE_POLICY_STATE_DIR:-$STATE_DIR_DEFAULT}"
MAPPING_FILE="${SLM_PACKAGE_POLICY_MAPPING_FILE:-/etc/slm/package-policy/package-mappings/debian.yaml}"
WRAPPER_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISABLE_REPOS_SCRIPT="${SLM_PACKAGE_POLICY_DISABLE_REPOS_SCRIPT:-$WRAPPER_ROOT/disable-external-repos.sh}"
SAFE_MODE_SCRIPT="${SLM_PACKAGE_POLICY_SAFE_MODE_SCRIPT:-$WRAPPER_ROOT/trigger-safe-mode-recovery.sh}"

SNAP_ID=""
if [[ "${1:-}" == "--snapshot-id" ]]; then
  SNAP_ID="${2:-}"
fi

ensure_state_dir() {
  local target="$1"
  if [[ -d "$target" ]]; then
    echo "$target"
    return 0
  fi
  local fallback="${HOME:-/tmp}/.local/state/slm-package-policy"
  mkdir -p "$fallback"
  echo "$fallback"
}

STATE_DIR="$(ensure_state_dir "$STATE_DIR")"
REPORT_DIR="$STATE_DIR/health"
mkdir -p "$REPORT_DIR"
REPORT_FILE="$REPORT_DIR/health-$(date -u +%Y%m%dT%H%M%SZ)-$$.log"

fail_count=0

log() {
  echo "$*" | tee -a "$REPORT_FILE" >/dev/null
}

check_pkg_installed() {
  local pkg="$1"
  dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"
}

log "[health] snapshot_id=${SNAP_ID:-none}"

if command -v dpkg >/dev/null 2>&1; then
  if ! dpkg --audit >/dev/null 2>&1; then
    log "[health][warn] dpkg audit reports issues"
    fail_count=$((fail_count + 1))
  fi
fi

if command -v apt-get >/dev/null 2>&1; then
  if ! apt-get -s check >/dev/null 2>&1; then
    log "[health][warn] apt dependency check failed"
    fail_count=$((fail_count + 1))
  fi
fi

if [[ -f "$MAPPING_FILE" ]]; then
  current_cap=""
  cap_ok=0
  cap_has_pkg=0

  while IFS= read -r line; do
    trimmed="$(echo "$line" | sed 's/^\s*//;s/\s*$//')"
    [[ -z "$trimmed" ]] && continue
    [[ "$trimmed" == \#* ]] && continue

    if [[ "$line" != " "* && "$trimmed" == *: ]]; then
      if [[ -n "$current_cap" && "$cap_has_pkg" -eq 1 && "$cap_ok" -eq 0 ]]; then
        log "[health][fail] capability missing package provider: $current_cap"
        fail_count=$((fail_count + 1))
      fi
      current_cap="${trimmed%:}"
      cap_ok=0
      cap_has_pkg=0
      continue
    fi

    if [[ "$trimmed" == -* ]]; then
      pkg="$(echo "$trimmed" | sed 's/^-\s*//;s/#.*$//;s/\s*$//' | tr '[:upper:]' '[:lower:]')"
      [[ -z "$pkg" ]] && continue
      cap_has_pkg=1
      if check_pkg_installed "$pkg"; then
        cap_ok=1
      fi
    fi
  done < "$MAPPING_FILE"

  if [[ -n "$current_cap" && "$cap_has_pkg" -eq 1 && "$cap_ok" -eq 0 ]]; then
    log "[health][fail] capability missing package provider: $current_cap"
    fail_count=$((fail_count + 1))
  fi
fi

if [[ "$fail_count" -gt 0 ]]; then
  log "[health][result] failed count=$fail_count"
  if [[ "${SLM_PACKAGE_POLICY_AUTO_ROLLBACK_SAFE_MODE:-1}" == "1" ]]; then
    if [[ -x "$SAFE_MODE_SCRIPT" ]]; then
      if "$SAFE_MODE_SCRIPT" >> "$REPORT_FILE" 2>&1; then
        log "[health][action] rollback + safe mode forced"
      else
        log "[health][action] failed to force safe mode"
      fi
    else
      log "[health][action] safe mode script missing: $SAFE_MODE_SCRIPT"
    fi
  fi
  if [[ "${SLM_PACKAGE_POLICY_AUTO_DISABLE_EXTERNAL_REPOS:-1}" == "1" ]]; then
    if [[ -x "$DISABLE_REPOS_SCRIPT" ]]; then
      if "$DISABLE_REPOS_SCRIPT" >> "$REPORT_FILE" 2>&1; then
        log "[health][action] external repositories disabled"
      else
        log "[health][action] failed to disable external repositories"
      fi
    else
      log "[health][action] disable repo script missing: $DISABLE_REPOS_SCRIPT"
    fi
  fi
  echo "slm-package-policy: post-transaction health check failed ($fail_count)." >&2
  echo "slm-package-policy: review report: $REPORT_FILE" >&2
  echo "slm-package-policy: run recovery helper: scripts/package-policy/recover-last-snapshot.sh" >&2
  exit 1
fi

log "[health][result] ok"
exit 0
