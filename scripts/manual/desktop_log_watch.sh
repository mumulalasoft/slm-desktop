#!/usr/bin/env bash
set -euo pipefail

MODE="watch"
LOG_FILE=""
PATTERN='\[desktop-sync\]|\[desktop-add\]|\[desktop-render\]'

if [[ "${1:-}" == "--summary" ]]; then
  MODE="summary"
  LOG_FILE="${2:-}"
else
  LOG_FILE="${1:-}"
fi

print_summary_from_file() {
  local file="$1"
  local last_sync_mismatch
  local last_sync_state
  local last_add_created
  local last_add_placed
  local last_render
  local last_missing

  last_sync_mismatch="$(rg '\[desktop-sync\] mismatch' "$file" | tail -n 1 || true)"
  last_sync_state="$(rg '\[desktop-sync\] state' "$file" | tail -n 1 || true)"
  last_add_created="$(rg '\[desktop-add\] created' "$file" | tail -n 1 || true)"
  last_add_placed="$(rg '\[desktop-add\] placed' "$file" | tail -n 1 || true)"
  last_render="$(rg '\[desktop-render\]' "$file" | tail -n 1 || true)"
  last_missing="$(rg '\[desktop-sync\] missing first:' "$file" | tail -n 1 || true)"

  echo "Desktop Trace Summary"
  echo "Source      : $file"
  echo
  echo "LastSyncMismatch:"
  echo "  ${last_sync_mismatch:-"(none)"}"
  echo "LastSyncState:"
  echo "  ${last_sync_state:-"(none)"}"
  echo "LastSyncMissing:"
  echo "  ${last_missing:-"(none)"}"
  echo "LastAddCreated:"
  echo "  ${last_add_created:-"(none)"}"
  echo "LastAddPlaced:"
  echo "  ${last_add_placed:-"(none)"}"
  echo "LastRender:"
  echo "  ${last_render:-"(none)"}"
}

if [[ "${MODE}" == "summary" ]]; then
  if [[ -n "${LOG_FILE}" ]]; then
    if [[ ! -f "${LOG_FILE}" ]]; then
      echo "[ERR] Log file not found: ${LOG_FILE}" >&2
      exit 1
    fi
    print_summary_from_file "${LOG_FILE}"
    exit 0
  fi
  if command -v journalctl >/dev/null 2>&1; then
    tmp="$(mktemp)"
    journalctl --user -n 1200 -o cat > "${tmp}" || true
    print_summary_from_file "${tmp}"
    rm -f "${tmp}"
    exit 0
  fi
  echo "Usage: $0 --summary /path/to/logfile" >&2
  exit 1
fi

if [[ -n "${LOG_FILE}" ]]; then
  if [[ ! -f "${LOG_FILE}" ]]; then
    echo "[ERR] Log file not found: ${LOG_FILE}" >&2
    exit 1
  fi
  echo "Watching ${LOG_FILE} for desktop traces..."
  echo "Pattern: ${PATTERN}"
  echo
  tail -F "${LOG_FILE}" | rg --line-buffered -N "${PATTERN}"
  exit 0
fi

if command -v journalctl >/dev/null 2>&1; then
  echo "Watching journalctl (user scope) for desktop traces..."
  echo "Pattern: ${PATTERN}"
  echo
  journalctl --user -f -o cat | rg --line-buffered -N "${PATTERN}"
else
  echo "Usage: $0 /path/to/logfile"
  echo "journalctl not available; provide explicit log file"
  exit 1
fi
