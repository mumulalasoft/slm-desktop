#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_BUILD_DIR="${ROOT_DIR}/build/toppanel-Debug"

usage() {
  cat <<EOF
Usage:
  $0 [build_dir]
  $0 unlock [build_dir] [ctest_regex]
  $0 filemanager-gates [build_dir] [stable|strict]
  $0 filemanager-smoke [build_dir]

Modes:
  default    Run lint + fileops suite + full ctest suite.
  unlock     Run unlock-related DBus test flow (uses secure password prompt helper).
  filemanager-gates  Run FileManager DBus gate subset via unified script.
  filemanager-smoke  Run FileManager smoke+regression subset in isolated DBus session.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "${1:-}" == "unlock" ]]; then
  if [[ "${2:-}" == "-h" || "${2:-}" == "--help" ]]; then
    usage
    echo
    "${ROOT_DIR}/scripts/test-session-unlock.sh" --help
    exit 0
  fi
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  UNLOCK_REGEX="${3:-sessionstateservice_dbus_test}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 unlock [build_dir] [ctest_regex]" >&2
    exit 2
  fi
  echo "[test] mode=unlock build_dir=${BUILD_DIR} regex=${UNLOCK_REGEX}"
  exec "${ROOT_DIR}/scripts/test-session-unlock.sh" \
    --build-dir "${BUILD_DIR}" \
    --regex "${UNLOCK_REGEX}"
fi

if [[ "${1:-}" == "filemanager-gates" ]]; then
  if [[ "${2:-}" == "-h" || "${2:-}" == "--help" ]]; then
    usage
    echo
    "${ROOT_DIR}/scripts/test-filemanager-dbus-gates.sh" --help
    exit 0
  fi
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  GATE_MODE="${3:-stable}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 filemanager-gates [build_dir] [stable|strict]" >&2
    exit 2
  fi
  if [[ "${GATE_MODE}" != "stable" && "${GATE_MODE}" != "strict" ]]; then
    echo "Invalid gate mode: ${GATE_MODE} (expected stable|strict)" >&2
    exit 2
  fi
  echo "[test] mode=filemanager-gates build_dir=${BUILD_DIR} gate_mode=${GATE_MODE}"
  exec "${ROOT_DIR}/scripts/test-filemanager-dbus-gates.sh" \
    --build-dir "${BUILD_DIR}" \
    "--${GATE_MODE}"
fi

if [[ "${1:-}" == "filemanager-smoke" ]]; then
  if [[ "${2:-}" == "-h" || "${2:-}" == "--help" ]]; then
    usage
    echo
    "${ROOT_DIR}/scripts/test-filemanager-smoke-regression.sh" --help || true
    exit 0
  fi
  BUILD_DIR="${2:-${DEFAULT_BUILD_DIR}}"
  if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    echo "Usage: $0 filemanager-smoke [build_dir]" >&2
    exit 2
  fi
  echo "[test] mode=filemanager-smoke build_dir=${BUILD_DIR}"
  exec "${ROOT_DIR}/scripts/test-filemanager-smoke-regression.sh" "${BUILD_DIR}"
fi

BUILD_DIR="${1:-${DEFAULT_BUILD_DIR}}"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "Build directory not found: ${BUILD_DIR}" >&2
  echo "Usage: $0 [build_dir]" >&2
  exit 2
fi

echo "[test] build_dir=${BUILD_DIR}"
FILEOPS_REGEX="${SLM_TEST_FILEOPS_REGEX:-^(fileoperationsmanager_test|fileoperationsservice_dbus_test|fileopctl_smoke_test|filemanagerapi_daemon_recovery_test)$}"
SKIP_UI_LINT="${SLM_TEST_SKIP_UI_LINT:-0}"
SKIP_CAP_MATRIX_LINT="${SLM_TEST_SKIP_CAPABILITY_MATRIX_LINT:-0}"

if [[ "${SKIP_UI_LINT}" != "1" ]]; then
  echo "[test] running UI style lint"
  "${ROOT_DIR}/scripts/lint-ui-style.sh"
fi

if [[ "${SKIP_CAP_MATRIX_LINT}" != "1" ]]; then
  echo "[test] running capability matrix lint"
  "${ROOT_DIR}/scripts/check-capability-matrix.sh"
fi

echo "[test] running file operations contract suite: ${FILEOPS_REGEX}"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "${FILEOPS_REGEX}"

echo "[test] running full suite"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
ctest --test-dir "${BUILD_DIR}" --output-on-failure

SOAK_ENABLED="${SLM_TEST_ENABLE_SOAK:-0}"
if [[ "${SOAK_ENABLED}" == "1" ]]; then
  SOAK_MINUTES="${SLM_TEST_SOAK_MINUTES:-5}"
  SOAK_REGEX="${SLM_TEST_SOAK_REGEX:-^(filemanagerapi_daemon_recovery_test)$}"
  SOAK_QPA="${SLM_TEST_SOAK_QPA_PLATFORM:-${QT_QPA_PLATFORM:-offscreen}}"

  if ! [[ "${SOAK_MINUTES}" =~ ^[0-9]+$ ]] || [[ "${SOAK_MINUTES}" -lt 1 ]]; then
    echo "[test] invalid SLM_TEST_SOAK_MINUTES='${SOAK_MINUTES}' (expected integer >= 1)" >&2
    exit 2
  fi

  SOAK_SECONDS=$((SOAK_MINUTES * 60))
  START_TS="$(date +%s)"
  DEADLINE_TS=$((START_TS + SOAK_SECONDS))
  ITER=0

  echo "[test] soak mode enabled"
  echo "[test] soak_minutes=${SOAK_MINUTES} soak_regex=${SOAK_REGEX} qpa=${SOAK_QPA}"

  while [[ "$(date +%s)" -lt "${DEADLINE_TS}" ]]; do
    ITER=$((ITER + 1))
    NOW_TS="$(date +%s)"
    REMAIN=$((DEADLINE_TS - NOW_TS))
    if [[ "${REMAIN}" -lt 0 ]]; then
      REMAIN=0
    fi
    echo "[test][soak] iteration=${ITER} remaining_sec=${REMAIN}"
    QT_QPA_PLATFORM="${SOAK_QPA}" \
    ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "${SOAK_REGEX}"
  done

  END_TS="$(date +%s)"
  ELAPSED=$((END_TS - START_TS))
  echo "[test] soak completed elapsed_sec=${ELAPSED} iterations=${ITER}"
fi
