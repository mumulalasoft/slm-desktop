#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

MODE="${1:-all}"
if [[ "${MODE}" != "all" && "${MODE}" != "--all" && "${MODE}" != "staged" && "${MODE}" != "--staged" ]]; then
  echo "Usage: $0 [all|staged]" >&2
  exit 2
fi

ALLOWLIST_FILE="${SLM_STYLE_USAGE_ALLOWLIST_FILE:-${ROOT_DIR}/config/lint/slm-style-usage-allowlist.txt}"
USE_ALLOWLIST="${SLM_STYLE_USAGE_USE_ALLOWLIST:-1}"
declare -A ALLOWLIST=()

if [[ "${USE_ALLOWLIST}" == "1" && -f "${ALLOWLIST_FILE}" ]]; then
  while IFS= read -r row; do
    row="${row%%#*}"
    row="$(echo "${row}" | xargs)"
    [[ -z "${row}" ]] && continue
    ALLOWLIST["${row}"]=1
  done < "${ALLOWLIST_FILE}"
fi

collect_all_targets() {
  for file in Main.qml FastMain.qml; do
    [[ -f "${file}" ]] && printf '%s\n' "${file}"
  done
  find Qml src/apps modules -type f -name '*.qml' \
    ! -path 'third_party/*' \
    ! -path 'modules/slm-desktop-style/*' \
    ! -path 'modules/slm-filemanager/*' \
    ! -path '*/build/*' | sort
}

collect_staged_targets() {
  git diff --cached --name-only --diff-filter=ACMR -- '*.qml' | \
    rg '^(Main\.qml|FastMain\.qml|Qml|src/apps|modules)/' | \
    rg -v '^third_party/|^modules/slm-desktop-style/|^modules/slm-filemanager/|/build/' | sort -u
}

if [[ "${MODE}" == "staged" || "${MODE}" == "--staged" ]]; then
  mapfile -t TARGETS < <(collect_staged_targets)
  if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "[check-slm-style] no staged QML files to check"
    exit 0
  fi
else
  mapfile -t TARGETS < <(collect_all_targets)
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  echo "[check-slm-style] no QML files found"
  exit 0
fi

STYLE_TYPES='Button|CheckBox|ComboBox|TextField|TextArea|SpinBox|Slider|Switch|TabBar|TabButton|Menu|MenuItem|MenuSeparator|ToolButton|ToolBar|DialogButtonBox|ProgressBar|RadioButton|Label|TypeLabel|Pane|Frame|GroupBox|ScrollView|ScrollBar|BusyIndicator|Popup|Dialog|Drawer|PageIndicator|RoundButton|DatePicker|TimePicker|DateTimePicker|ToolTip|Breadcrumb|IndicatorMenu|AppDialog|AppPopup|PopupSurface|WindowDialogSurface|Card|IndicatorSectionLabel|IndicatorSectionRow|SearchField'
BARE_CONTROL_RE="^\\s*(${STYLE_TYPES})\\s*\\{"
FAILED=0
BASELINE=0

echo "[check-slm-style] scanning ${#TARGETS[@]} files"
for file in "${TARGETS[@]}"; do
  matches="$(rg -n --pcre2 "${BARE_CONTROL_RE}" "${file}" || true)"

  while IFS= read -r alias; do
    [[ -z "${alias}" ]] && continue
    alias_matches="$(rg -n --pcre2 "^\\s*${alias}\\.(${STYLE_TYPES})\\s*\\{" "${file}" || true)"
    if [[ -n "${alias_matches}" ]]; then
      if [[ -n "${matches}" ]]; then
        matches+=$'\n'
      fi
      matches+="${alias_matches}"
    fi
  done < <(rg --pcre2 -o -r '$1' '^\s*import\s+QtQuick\.Controls(?:\.[A-Za-z0-9_]+)?(?:\s+[0-9.]+)?\s+as\s+([A-Za-z_][A-Za-z0-9_]*)\s*$' "${file}" || true)

  [[ -z "${matches}" ]] && continue

  if [[ "${USE_ALLOWLIST}" == "1" && -n "${ALLOWLIST[$file]:-}" ]]; then
    count="$(printf '%s\n' "${matches}" | wc -l | tr -d ' ')"
    BASELINE=$((BASELINE + count))
    echo "[check-slm-style][BASELINE] ${file}: ${count} legacy bare controls"
    continue
  fi

  if ! rg -q '^\s*import\s+SlmStyle(?:\s+as\s+[A-Za-z_][A-Za-z0-9_]*)?\s*$' "${file}"; then
    echo "[check-slm-style][WARN] ${file}: missing SlmStyle import"
  fi

  echo "[check-slm-style][FAIL] ${file}: use SlmStyle controls"
  printf '%s\n' "${matches}" | sed 's/^/  - /'
  FAILED=1
done

if [[ ${FAILED} -ne 0 ]]; then
  echo "[check-slm-style] FAILED: found non-SlmStyle control usages" >&2
  echo "[check-slm-style] Use DSStyle.<Control> (or another SlmStyle alias) instead of bare QtQuick.Controls types." >&2
  echo "[check-slm-style] Existing legacy files must be explicitly listed in ${ALLOWLIST_FILE}." >&2
  exit 1
fi

if [[ ${BASELINE} -gt 0 ]]; then
  echo "[check-slm-style] OK (${BASELINE} allowlisted baseline matches)"
else
  echo "[check-slm-style] OK"
fi
