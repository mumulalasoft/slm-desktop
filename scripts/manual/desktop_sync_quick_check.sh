#!/usr/bin/env bash
set -euo pipefail

DESKTOP_DIR="${HOME}/Desktop"
APP_CFG_DIRS=(
  "${HOME}/.config/slm-desktop"
  "${HOME}/.config/Slm_Desktop"
)
LOG_FILE="${1:-}"

abs_desktop="$(cd "${DESKTOP_DIR}" 2>/dev/null && pwd || true)"
if [[ -z "${abs_desktop}" ]]; then
  echo "[ERR] Desktop directory not found: ${DESKTOP_DIR}"
  exit 1
fi

sha1_hash="$(printf '%s' "${abs_desktop}" | sha1sum | awk '{print $1}')"

echo "Desktop Sync Quick Check"
echo "Time       : $(date '+%Y-%m-%d %H:%M:%S %Z')"
echo "Desktop    : ${abs_desktop}"
echo "DesktopSHA : ${sha1_hash}"

desktop_count="$(find "${abs_desktop}" -mindepth 1 -maxdepth 1 \
  ! -name '.desktop_shell_slot_map.json' \
  ! -name '.desktop_shell_shortcut_order' | wc -l | tr -d ' ')"

echo "DesktopCnt : ${desktop_count}"

slot_file=""
for cfg in "${APP_CFG_DIRS[@]}"; do
  candidate="${cfg}/slotmaps/${sha1_hash}.json"
  if [[ -f "${candidate}" ]]; then
    slot_file="${candidate}"
    break
  fi
done

if [[ -n "${slot_file}" ]]; then
  slot_count="$(jq 'keys | length' "${slot_file}" 2>/dev/null || echo "?")"
  echo "SlotMap    : ${slot_file}"
  echo "SlotMapCnt : ${slot_count}"
else
  echo "SlotMap    : (not found in known config paths)"
fi

echo
if [[ -n "${LOG_FILE}" ]]; then
  if [[ ! -f "${LOG_FILE}" ]]; then
    echo "[WARN] Log file not found: ${LOG_FILE}"
    exit 0
  fi
  echo "Log file   : ${LOG_FILE}"
  echo
  echo "Recent [desktop-sync] lines:"
  rg "\\[desktop-sync\\]" "${LOG_FILE}" | tail -n 20 || true
  echo
  echo "Recent [desktop-add] lines:"
  rg "\\[desktop-add\\]" "${LOG_FILE}" | tail -n 20 || true
  echo
  last_mismatch="$(rg "\\[desktop-sync\\] mismatch" "${LOG_FILE}" | tail -n 1 || true)"
  if [[ -n "${last_mismatch}" ]]; then
    echo "LastMismatch: ${last_mismatch}"
  else
    echo "LastMismatch: (none found in log)"
  fi
else
  echo "Tip: pass a log file to parse desktop sync/add traces:"
  echo "  scripts/manual/desktop_sync_quick_check.sh /path/to/shell.log"
fi
