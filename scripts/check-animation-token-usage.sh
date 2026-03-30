#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

MODE="${1:-all}"
if [[ "${MODE}" != "all" && "${MODE}" != "--all" && "${MODE}" != "staged" && "${MODE}" != "--staged" ]]; then
  echo "Usage: $0 [all|staged]" >&2
  exit 2
fi

collect_all_targets() {
  find Qml -type f -name '*.qml' \
    ! -path 'third_party/slm-style/qml/SlmStyle/Theme.qml' \
    ! -path 'Qml/vendor/*' \
    ! -path 'Qml/3rdparty/*' | sort
}

collect_staged_targets() {
  git diff --cached --name-only --diff-filter=ACMR -- '*.qml' | \
    rg '^Qml/' | \
    rg -v '^Qml/Theme\.qml$|^Qml/vendor/|^Qml/3rdparty/' | sort -u
}

if [[ "${MODE}" == "staged" || "${MODE}" == "--staged" ]]; then
  mapfile -t TARGETS < <(collect_staged_targets)
  if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "[lint-animation] no staged QML files to check"
    exit 0
  fi
else
  mapfile -t TARGETS < <(collect_all_targets)
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  echo "[lint-animation] no QML files found"
  exit 0
fi

FAILED=0

check() {
  local name="$1"
  local pattern="$2"
  echo "[lint-animation] check: ${name}"
  if rg -n --pcre2 "${pattern}" "${TARGETS[@]}"; then
    echo "[lint-animation] FAILED: ${name}" >&2
    FAILED=1
  fi
}

check "no raw easing enum usage in bindings" \
  "\\beasing\\.type\\s*:\\s*Easing\\.[A-Za-z_][A-Za-z0-9_]*\\b"

check "no hardcoded numeric duration literal" \
  "\\bduration\\s*:\\s*[0-9]+(?:\\.[0-9]+)?\\b"

if [[ "${FAILED}" -ne 0 ]]; then
  cat >&2 <<'EOF'
[lint-animation] animation lint failed.
[lint-animation] Use Theme duration/easing tokens (e.g. Theme.durationFast, Theme.easingDefault).
EOF
  exit 1
fi

echo "[lint-animation] OK"
