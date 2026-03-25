#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SLM_TEST_BUILD_DIR:-${ROOT_DIR}/build}"
TOOL_PATH="${BUILD_DIR}/globalmenuctl"
SAMPLES=12
INTERVAL_SEC="1.0"
MIN_UNIQUE=2
STRICT=0

usage() {
  cat <<'EOF'
usage: scripts/smoke-globalmenu-focus.sh [options]

Options:
  --samples <n>      Number of polling samples (default: 12)
  --interval <sec>   Poll interval seconds (default: 1.0)
  --min-unique <n>   Minimum distinct bindings expected in strict mode (default: 2)
  --strict           Fail when unique binding count < min-unique
  --build-dir <dir>  Build directory containing globalmenuctl
  --help             Show this help

Notes:
  - During script run, manually switch focus between GTK/Qt/KDE apps.
  - Binding key format: "<service>|<path>".
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --samples)
      SAMPLES="${2:-}"
      shift 2
      ;;
    --interval)
      INTERVAL_SEC="${2:-}"
      shift 2
      ;;
    --min-unique)
      MIN_UNIQUE="${2:-}"
      shift 2
      ;;
    --strict)
      STRICT=1
      shift
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "[smoke-globalmenu-focus] unknown arg: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if ! [[ "${SAMPLES}" =~ ^[0-9]+$ ]] || [[ "${SAMPLES}" -lt 1 ]]; then
  echo "[smoke-globalmenu-focus] invalid --samples: ${SAMPLES}" >&2
  exit 2
fi
if ! [[ "${MIN_UNIQUE}" =~ ^[0-9]+$ ]] || [[ "${MIN_UNIQUE}" -lt 1 ]]; then
  echo "[smoke-globalmenu-focus] invalid --min-unique: ${MIN_UNIQUE}" >&2
  exit 2
fi
if ! [[ "${INTERVAL_SEC}" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "[smoke-globalmenu-focus] invalid --interval: ${INTERVAL_SEC}" >&2
  exit 2
fi

if [[ ! -x "${TOOL_PATH}" ]]; then
  echo "[smoke-globalmenu-focus] globalmenuctl not found, building target..."
  cmake --build "${BUILD_DIR}" --target globalmenuctl -j6
fi
if [[ ! -x "${TOOL_PATH}" ]]; then
  echo "[smoke-globalmenu-focus] missing tool: ${TOOL_PATH}" >&2
  exit 2
fi

echo "[smoke-globalmenu-focus] build_dir=${BUILD_DIR}"
echo "[smoke-globalmenu-focus] samples=${SAMPLES} interval=${INTERVAL_SEC}s min_unique=${MIN_UNIQUE} strict=${STRICT}"
echo "[smoke-globalmenu-focus] action: switch focus between GTK/Qt/KDE windows now..."

declare -A UNIQUE=()
LAST_KEY=""
CHANGES=0

extract_field() {
  local json="$1"
  local key="$2"
  sed -n "s/.*\"${key}\":\"\\([^\"]*\\)\".*/\\1/p" <<< "${json}" | head -n1
}

for ((i=1; i<=SAMPLES; i++)); do
  OUT="$("${TOOL_PATH}" dump 2>/dev/null || true)"
  SERVICE="$(extract_field "${OUT}" service)"
  PATH_VAL="$(extract_field "${OUT}" path)"
  VALID="0"
  if grep -q '"valid":true' <<< "${OUT}"; then
    VALID="1"
  fi
  KEY="${SERVICE}|${PATH_VAL}"
  if [[ "${VALID}" == "1" && -n "${SERVICE}" && -n "${PATH_VAL}" ]]; then
    UNIQUE["${KEY}"]=1
  fi

  if [[ -n "${LAST_KEY}" && "${KEY}" != "${LAST_KEY}" ]]; then
    CHANGES=$((CHANGES + 1))
  fi
  LAST_KEY="${KEY}"

  echo "[sample ${i}/${SAMPLES}] valid=${VALID} key=${KEY}"
  sleep "${INTERVAL_SEC}"
done

UNIQUE_COUNT="${#UNIQUE[@]}"
echo "[smoke-globalmenu-focus] unique_bindings=${UNIQUE_COUNT} changes=${CHANGES}"
if [[ "${UNIQUE_COUNT}" -gt 0 ]]; then
  echo "[smoke-globalmenu-focus] bindings observed:"
  for K in "${!UNIQUE[@]}"; do
    echo "  - ${K}"
  done
fi

if [[ "${STRICT}" == "1" && "${UNIQUE_COUNT}" -lt "${MIN_UNIQUE}" ]]; then
  echo "[smoke-globalmenu-focus] FAIL: unique bindings ${UNIQUE_COUNT} < ${MIN_UNIQUE}" >&2
  exit 1
fi

if [[ "${UNIQUE_COUNT}" -eq 0 ]]; then
  echo "[smoke-globalmenu-focus] WARN: no valid DBusMenu binding observed."
else
  echo "[smoke-globalmenu-focus] PASS"
fi
