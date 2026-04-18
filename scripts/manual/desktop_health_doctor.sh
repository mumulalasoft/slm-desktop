#!/usr/bin/env bash
set -euo pipefail

LOG_FILE="${1:-}"
DESKTOP_DIR="${HOME}/Desktop"
CFG_ROOT="${HOME}/.config/slm-desktop"

failures=0
warns=0

say() { printf "%s\n" "$*"; }
pass() { say "[PASS] $*"; }
warn() { say "[WARN] $*"; warns=$((warns + 1)); }
fail() { say "[FAIL] $*"; failures=$((failures + 1)); }

if [[ ! -d "${DESKTOP_DIR}" ]]; then
  fail "Desktop directory missing: ${DESKTOP_DIR}"
  say
  say "Result: FAIL (${failures} failure, ${warns} warning)"
  exit 2
fi

abs_desktop="$(cd "${DESKTOP_DIR}" && pwd)"
sha1_hash="$(printf '%s' "${abs_desktop}" | sha1sum | awk '{print $1}')"
slot_file=""
for candidate_root in \
  "${HOME}/.config/slm-desktop" \
  "${HOME}/.config/Slm_Desktop" \
  "${CFG_ROOT}"; do
  candidate="${candidate_root}/slotmaps/${sha1_hash}.json"
  if [[ -f "${candidate}" ]]; then
    slot_file="${candidate}"
    break
  fi
done
if [[ -z "${slot_file}" ]]; then
  found="$(find "${HOME}/.config" -maxdepth 4 -type f -path "*/slotmaps/${sha1_hash}.json" 2>/dev/null | head -n 1 || true)"
  if [[ -n "${found}" ]]; then
    slot_file="${found}"
  fi
fi

desktop_count="$(find "${abs_desktop}" -mindepth 1 -maxdepth 1 \
  ! -name '.desktop_shell_slot_map.json' \
  ! -name '.desktop_shell_shortcut_order' | wc -l | tr -d ' ')"

say "Desktop Health Doctor"
say "Time        : $(date '+%Y-%m-%d %H:%M:%S %Z')"
say "DesktopPath : ${abs_desktop}"
say "DesktopCnt  : ${desktop_count}"

if [[ -n "${slot_file}" && -f "${slot_file}" ]]; then
  if command -v jq >/dev/null 2>&1; then
    slot_count="$(jq 'keys | length' "${slot_file}" 2>/dev/null || echo '?')"
  else
    slot_count="$(python3 - <<'PY' "$slot_file"
import json,sys
try:
    print(len(json.load(open(sys.argv[1]))))
except Exception:
    print('?')
PY
)"
  fi
  say "SlotMapPath : ${slot_file}"
  say "SlotMapCnt  : ${slot_count}"
  if [[ "${slot_count}" != "?" ]] && [[ "${slot_count}" -lt "${desktop_count}" ]]; then
    warn "slot map count is smaller than desktop items"
  else
    pass "slot map file present"
  fi
else
  warn "slot map file not found for desktop hash ${sha1_hash}"
fi

parse_log_line_field() {
  local line="$1"
  local key="$2"
  printf '%s' "$line" | sed -n "s/.*${key}=\([^ ]*\).*/\1/p"
}

if [[ -n "${LOG_FILE}" ]]; then
  if [[ ! -f "${LOG_FILE}" ]]; then
    fail "log file not found: ${LOG_FILE}"
  else
    say "LogFile     : ${LOG_FILE}"

    last_sync_mismatch="$(rg '\[desktop-sync\] mismatch' "${LOG_FILE}" | tail -n 1 || true)"
    last_render="$(rg '\[desktop-render\]' "${LOG_FILE}" | tail -n 1 || true)"

    if [[ -n "${last_sync_mismatch}" ]]; then
      missing="$(parse_log_line_field "${last_sync_mismatch}" "missing")"
      excess="$(parse_log_line_field "${last_sync_mismatch}" "excess")"
      missing="${missing:-0}"
      excess="${excess:-0}"
      say "LastSyncMis : ${last_sync_mismatch}"
      if [[ "${missing}" =~ ^[0-9]+$ ]] && [[ "${excess}" =~ ^[0-9]+$ ]]; then
        if [[ "${missing}" -gt 0 || "${excess}" -gt 0 ]]; then
          fail "desktop-sync mismatch detected (missing=${missing}, excess=${excess})"
        else
          pass "desktop-sync mismatch line reports zero delta"
        fi
      else
        warn "unable to parse missing/excess from mismatch line"
      fi
    else
      pass "no desktop-sync mismatch line found"
    fi

    if [[ -n "${last_render}" ]]; then
      say "LastRender  : ${last_render}"
      model="$(parse_log_line_field "${last_render}" "model")"
      placed="$(parse_log_line_field "${last_render}" "placed")"
      cell00="$(parse_log_line_field "${last_render}" "cell00")"
      if [[ "${model}" =~ ^[0-9]+$ ]] && [[ "${placed}" =~ ^[0-9]+$ ]]; then
        if [[ "${model}" -ne "${placed}" ]]; then
          fail "render mismatch model(${model}) != placed(${placed})"
        else
          pass "render count model == placed (${model})"
        fi
      else
        warn "unable to parse model/placed from desktop-render line"
      fi
      if [[ "${cell00}" == "false" ]]; then
        warn "latest render shows cell00=false (verify row0/col0 occupancy scenario)"
      fi
    else
      warn "no desktop-render line found in log"
    fi
  fi
else
  warn "no log file provided, log-based checks skipped"
fi

say
if [[ "${failures}" -gt 0 ]]; then
  say "Result: FAIL (${failures} failure, ${warns} warning)"
  exit 2
fi
if [[ "${warns}" -gt 0 ]]; then
  say "Result: WARN (0 failure, ${warns} warning)"
  exit 1
fi
say "Result: PASS"
