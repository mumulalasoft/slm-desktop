#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SLM_TEST_BUILD_DIR:-${ROOT_DIR}/build}"
STRICT=0
PRETTY=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --strict)
      STRICT=1
      shift
      ;;
    --compact)
      PRETTY=0
      shift
      ;;
    --build-dir)
      if [[ $# -lt 2 ]]; then
        echo "[test-globalmenu] --build-dir requires a value" >&2
        exit 2
      fi
      BUILD_DIR="$2"
      shift 2
      ;;
    *)
      echo "[test-globalmenu] unknown arg: $1" >&2
      echo "usage: $0 [--strict] [--compact] [--build-dir <dir>]" >&2
      exit 2
      ;;
  esac
done

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "[test-globalmenu] build directory not found: ${BUILD_DIR}" >&2
  exit 2
fi

TOOL_PATH="${BUILD_DIR}/globalmenuctl"
if [[ ! -x "${TOOL_PATH}" ]]; then
  echo "[test-globalmenu] globalmenuctl binary not found in ${BUILD_DIR}; building target..."
  cmake --build "${BUILD_DIR}" --target globalmenuctl -j6
fi
if [[ ! -x "${TOOL_PATH}" ]]; then
  echo "[test-globalmenu] missing tool: ${TOOL_PATH}" >&2
  exit 2
fi

echo "[test-globalmenu] build_dir=${BUILD_DIR}"
echo "[test-globalmenu] strict=${STRICT} pretty=${PRETTY}"

FMT_FLAG=""
if [[ "${PRETTY}" == "1" ]]; then
  FMT_FLAG="--pretty"
fi

echo "[test-globalmenu] dump"
if [[ -n "${FMT_FLAG}" ]]; then
  "${TOOL_PATH}" dump "${FMT_FLAG}"
else
  "${TOOL_PATH}" dump
fi

echo "[test-globalmenu] healthcheck"
set +e
if [[ -n "${FMT_FLAG}" ]]; then
  "${TOOL_PATH}" healthcheck "${FMT_FLAG}"
else
  "${TOOL_PATH}" healthcheck
fi
RC=$?
set -e

if [[ "${RC}" -eq 0 ]]; then
  echo "[test-globalmenu] PASS (registrar/service available)"
  exit 0
fi

if [[ "${RC}" -eq 2 && "${STRICT}" -eq 0 ]]; then
  echo "[test-globalmenu] WARN: healthcheck returned 2 (no active registrar/service in this session)"
  echo "[test-globalmenu] PASS (non-strict mode)"
  exit 0
fi

echo "[test-globalmenu] FAIL rc=${RC}" >&2
exit "${RC}"
