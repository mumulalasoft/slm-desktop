#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

mapfile -t TARGETS < <(find Qml -type f -name '*.qml' | sort)
if [[ ${#TARGETS[@]} -eq 0 ]]; then
  echo "[check-slm-style] no QML files found"
  exit 0
fi

CONTROL_RE='^\s*(Button|CheckBox|ComboBox|TextField|SpinBox|Slider|Switch|TabBar|TabButton|Menu|MenuItem|ToolButton|DialogButtonBox|ProgressBar|RadioButton|Label)\s*\{'
FAILED=0

echo "[check-slm-style] scanning ${#TARGETS[@]} files"
for file in "${TARGETS[@]}"; do
  if ! rg -q '^\s*import\s+QtQuick\.Controls' "$file"; then
    continue
  fi

  if rg -n --pcre2 "$CONTROL_RE" "$file" >/dev/null; then
    if ! rg -q '^\s*import\s+SlmStyle(?:\s+as\s+[A-Za-z_][A-Za-z0-9_]*)?\s*$' "$file"; then
      echo "[check-slm-style][WARN] ${file}: bare QtQuick.Controls types without SlmStyle import"
    fi
    echo "[check-slm-style][FAIL] ${file}"
    rg -n --pcre2 "$CONTROL_RE" "$file" | sed 's/^/  - /'
    FAILED=1
  fi
done

if [[ ${FAILED} -ne 0 ]]; then
  echo "[check-slm-style] FAILED: found non-SlmStyle control usages" >&2
  echo "[check-slm-style] use DSStyle.<Control> or set strict exceptions explicitly" >&2
  exit 1
fi

echo "[check-slm-style] OK"
