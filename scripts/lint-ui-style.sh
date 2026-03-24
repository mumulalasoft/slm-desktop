#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

declare -a TARGETS=(
  "Main.qml"
  "Qml/DesktopScene.qml"
)

while IFS= read -r file; do
  TARGETS+=("${file}")
done < <(find Qml/components -type f -name "*.qml" | sort)

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  echo "[lint-ui-style] no QML targets found"
  exit 0
fi

FAILED=0

check() {
  local name="$1"
  local pattern="$2"
  echo "[lint-ui-style] check: ${name}"
  if rg -n --pcre2 "${pattern}" "${TARGETS[@]}"; then
    echo "[lint-ui-style] FAILED: ${name}" >&2
    FAILED=1
  fi
}

check "no hex color literal in component assignments" \
  "\\b(color|border\\.color|selectionColor|selectedTextColor)\\s*:\\s*\"#[0-9a-fA-F]{3,8}\""

check "no hex color return literal" \
  "return\\s+\"#[0-9a-fA-F]{3,8}\""

check "no numeric radius literal except 0" \
  "\\bradius\\s*:\\s*(?!0(?:\\.0+)?\\b)[0-9]+(?:\\.[0-9]+)?\\b"

check "no numeric border width literal" \
  "\\bborder\\.width\\s*:\\s*[0-9]+\\b"

check "no numeric opacity literal" \
  "\\bopacity\\s*:\\s*0\\.[0-9]+\\b"

check "no numeric font pixel size literal" \
  "\\bfont\\.pixelSize\\s*:\\s*[0-9]+(?:\\.[0-9]+)?\\b"

check "no direct font.bold usage" \
  "\\bfont\\.bold\\s*:"

check "no direct font.weight literal usage" \
  "\\bfont\\.weight\\s*:\\s*(?:Font\\.[A-Za-z]+|[0-9]+)\\b"

check "no numeric lineHeight literal" \
  "\\blineHeight\\s*:\\s*[0-9]+(?:\\.[0-9]+)?\\b"

# ── Print framework anti-leak checks ─────────────────────────────────────────
# Ensure no CUPS/IPP backend terminology appears in user-visible QML.

check "no CUPS in UI text/binding" \
  "\\bCUPS\\b"

check "no IPP in UI text/binding" \
  "\\bIPP\\b"

check "no 'scheduler' in UI text" \
  "\\bscheduler\\b"

check "no legacy isCupsAvailable property binding" \
  "\\bisCupsAvailable\\b"

check "no legacy cupsAvailabilityChanged signal" \
  "\\bcupsAvailabilityChanged\\b"

check "no IPP duplex value 'one-sided' in UI" \
  "value\\s*:\\s*\"one-sided\""

check "no IPP duplex value 'two-sided-long-edge' in UI" \
  "value\\s*:\\s*\"two-sided-long-edge\""

check "no IPP duplex value 'two-sided-short-edge' in UI" \
  "value\\s*:\\s*\"two-sided-short-edge\""

check "no IPP color value 'monochrome' in UI" \
  "value\\s*:\\s*\"monochrome\""

if [[ "${FAILED}" -ne 0 ]]; then
  echo "[lint-ui-style] style lint failed. Promote constants to Qml/Theme.qml tokens." >&2
  exit 1
fi

echo "[lint-ui-style] OK"
