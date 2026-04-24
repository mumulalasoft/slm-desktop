#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

MODE="${1:-all}"
if [[ "${MODE}" != "all" && "${MODE}" != "--all" && "${MODE}" != "staged" && "${MODE}" != "--staged" ]]; then
  echo "Usage: $0 [all|staged]" >&2
  exit 2
fi

ANIMATION_ALLOWLIST="${ROOT_DIR}/config/lint/animation-allowlist.txt"

collect_all_targets() {
  local excludes=()
  if [[ -f "${ANIMATION_ALLOWLIST}" ]]; then
    while IFS= read -r row; do
      row="${row%%#*}"; row="$(echo "${row}" | xargs)"
      [[ -z "${row}" ]] && continue
      excludes+=("${row}")
    done < "${ANIMATION_ALLOWLIST}"
  fi

  find Qml src/apps -type f -name '*.qml' \
    ! -path 'third_party/slm-style/qml/SlmStyle/Theme.qml' \
    ! -name 'MotionController.qml' \
    ! -path 'Qml/vendor/*' \
    ! -path 'Qml/3rdparty/*' | sort | while IFS= read -r f; do
      local skip=0
      for ex in "${excludes[@]}"; do
        [[ "${f}" == "${ex}" ]] && skip=1 && break
      done
      [[ "${skip}" -eq 0 ]] && echo "${f}"
  done
}

collect_staged_targets() {
  git diff --cached --name-only --diff-filter=ACMR -- '*.qml' | \
    rg '^(Qml|src/apps)/' | \
    rg -v '^Qml/Theme\.qml$|^Qml/MotionController\.qml$|^Qml/vendor/|^Qml/3rdparty/' | sort -u
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
WARNED=0

check() {
  local name="$1"
  local pattern="$2"
  echo "[lint-animation] check: ${name}"
  if rg -n --pcre2 "${pattern}" "${TARGETS[@]}"; then
    echo "[lint-animation] FAILED: ${name}" >&2
    FAILED=1
  fi
}

# warn() — prints matches but does NOT set FAILED (non-blocking advisory).
warn() {
  local name="$1"
  local pattern="$2"
  echo "[lint-animation] warn: ${name}"
  local output
  output="$(rg -n --pcre2 "${pattern}" "${TARGETS[@]}" || true)"
  if [[ -n "${output}" ]]; then
    printf '%s\n' "${output}"
    echo "[lint-animation] WARNING: ${name}" >&2
    WARNED=1
  fi
}

check "no raw easing enum usage in bindings" \
  "\\beasing\\.type\\s*:\\s*Easing\\.[A-Za-z_][A-Za-z0-9_]*\\b"

check "no hardcoded numeric duration literal" \
  "\\bduration\\s*:\\s*[0-9]+(?:\\.[0-9]+)?\\b"

# ── Anti-jitter rules (WARNINGS — not CI-blocking) ───────────────────────────
# Animating layout properties triggers relayout on every frame and can cause
# jitter. Prefer transform properties (x, y, scale, opacity).
# These are WARNINGS because some surfaces (dock, file manager) legitimately
# animate dimensions. Add layer.enabled: MotionController.effectsEnabled to
# cache the subtree and mitigate the cost.

warn "Behavior on width may cause layout jitter — prefer scale or x/y" \
  "\\bBehavior\\s+on\\s+width\\b"

warn "Behavior on height may cause layout jitter — prefer scale or x/y" \
  "\\bBehavior\\s+on\\s+height\\b"

if [[ "${FAILED}" -ne 0 ]]; then
  cat >&2 <<'EOF'
[lint-animation] animation lint failed.
[lint-animation] Rules:
[lint-animation]   duration   → Theme.durationMicro / durationSm / durationMd / durationNormal / durationLg / durationXl
[lint-animation]   easing     → Theme.easingStandard / easingDecelerate / easingAccelerate / easingLight / easingSpring
[lint-animation]   preset     → MotionController.preset("enter").duration / .easing
[lint-animation]   jitter     → animate transform properties (x, y, scale, opacity) — prefer NOT width/height
EOF
  exit 1
fi

if [[ "${WARNED}" -ne 0 ]]; then
  echo "[lint-animation] OK (with warnings — see above)"
else
  echo "[lint-animation] OK"
fi
