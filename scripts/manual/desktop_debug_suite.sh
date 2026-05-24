#!/usr/bin/env bash
set -euo pipefail

LOG_FILE="${1:-}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

CHECKLIST="${ROOT_DIR}/scripts/manual/desktop_add_smoke_checklist.sh"
QUICK="${ROOT_DIR}/scripts/manual/desktop_sync_quick_check.sh"
DOCTOR="${ROOT_DIR}/scripts/manual/desktop_health_doctor.sh"
WATCH="${ROOT_DIR}/scripts/manual/desktop_log_watch.sh"

run_step() {
  local title="$1"
  shift
  echo
  echo "== ${title} =="
  "$@"
}

echo "Desktop Debug Suite"
echo "Root: ${ROOT_DIR}"
if [[ -n "${LOG_FILE}" ]]; then
  echo "Log : ${LOG_FILE}"
fi

run_step "Smoke Checklist" "${CHECKLIST}"

if [[ -n "${LOG_FILE}" ]]; then
  run_step "Quick Check" "${QUICK}" "${LOG_FILE}"
else
  run_step "Quick Check" "${QUICK}"
fi

if [[ -n "${LOG_FILE}" ]]; then
  run_step "Health Doctor" "${DOCTOR}" "${LOG_FILE}" || true
  run_step "Trace Summary" "${WATCH}" --summary "${LOG_FILE}" || true
else
  run_step "Health Doctor" "${DOCTOR}" || true
  run_step "Trace Summary" "${WATCH}" --summary || true
fi

echo
echo "Suite finished."
if [[ -n "${LOG_FILE}" ]]; then
  echo "Live watch: ${WATCH} ${LOG_FILE}"
else
  echo "Live watch: ${WATCH}"
fi
