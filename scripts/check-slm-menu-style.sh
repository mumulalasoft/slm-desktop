#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

ALLOWLIST_FILE="${SLM_MENU_STYLE_ALLOWLIST_FILE:-${ROOT_DIR}/config/lint/slm-menu-style-allowlist.txt}"
MENU_RE='^\s*(Menu|MenuItem|MenuSeparator)\s*\{'

declare -A ALLOWLIST=()
if [[ -f "${ALLOWLIST_FILE}" ]]; then
  while IFS= read -r line; do
    line="${line%%#*}"
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [[ -z "${line}" ]] && continue
    ALLOWLIST["${line}"]=1
  done < "${ALLOWLIST_FILE}"
fi

mapfile -t TARGETS < <(find Qml -type f -name '*.qml' | sort)
if [[ ${#TARGETS[@]} -eq 0 ]]; then
  echo "[check-slm-menu-style] no QML files found"
  exit 0
fi

FAILED=0
BASELINE=0

echo "[check-slm-menu-style] scanning ${#TARGETS[@]} files"
for file in "${TARGETS[@]}"; do
  matches="$(rg -n --pcre2 "${MENU_RE}" "${file}" || true)"
  [[ -z "${matches}" ]] && continue

  if [[ -n "${ALLOWLIST[$file]:-}" ]]; then
    count="$(printf '%s\n' "${matches}" | wc -l | tr -d ' ')"
    BASELINE=$((BASELINE + count))
    echo "[check-slm-menu-style][BASELINE] ${file}: ${count} bare menu controls"
    continue
  fi

  echo "[check-slm-menu-style][FAIL] ${file}: use SlmStyle menu controls"
  printf '%s\n' "${matches}" | sed 's/^/  - /'
  FAILED=1
done

if [[ ${FAILED} -ne 0 ]]; then
  echo "[check-slm-menu-style] FAILED: found bare Menu/MenuItem/MenuSeparator usage." >&2
  echo "[check-slm-menu-style] Use DSStyle.Menu, DSStyle.MenuItem, and DSStyle.MenuSeparator." >&2
  echo "[check-slm-menu-style] Existing legacy files must be explicitly listed in ${ALLOWLIST_FILE}." >&2
  exit 1
fi

if [[ ${BASELINE} -gt 0 ]]; then
  echo "[check-slm-menu-style] OK (${BASELINE} allowlisted baseline matches)"
else
  echo "[check-slm-menu-style] OK"
fi
