#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SLM_TEST_BUILD_DIR:-${ROOT_DIR}/build}"
MODE="quick"
APP_BIN_ARG=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --full)
      MODE="full"
      shift
      ;;
    --smoke-only)
      MODE="smoke-only"
      shift
      ;;
    --build-dir)
      if [[ $# -lt 2 ]]; then
        echo "[test-tothespot] --build-dir requires a value" >&2
        exit 2
      fi
      BUILD_DIR="$2"
      shift 2
      ;;
    --app-bin)
      if [[ $# -lt 2 ]]; then
        echo "[test-tothespot] --app-bin requires a value" >&2
        exit 2
      fi
      APP_BIN_ARG="$2"
      shift 2
      ;;
    *)
      echo "[test-tothespot] unknown arg: $1" >&2
      echo "usage: $0 [--full|--smoke-only] [--build-dir <dir>] [--app-bin <path>]" >&2
      exit 2
      ;;
  esac
done

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "[test-tothespot] build directory not found: ${BUILD_DIR}" >&2
  exit 2
fi

SMOKE_SCRIPT="${ROOT_DIR}/scripts/smoke-tothespot-contextmenu.sh"
if [[ ! -x "${SMOKE_SCRIPT}" ]]; then
  echo "[test-tothespot] missing smoke script: ${SMOKE_SCRIPT}" >&2
  exit 2
fi

echo "[test-tothespot] mode=${MODE}"
echo "[test-tothespot] build_dir=${BUILD_DIR}"

if [[ -n "${APP_BIN_ARG}" ]]; then
  "${SMOKE_SCRIPT}" "${APP_BIN_ARG}"
else
  "${SMOKE_SCRIPT}"
fi

if [[ "${MODE}" == "smoke-only" ]]; then
  echo "[test-tothespot] PASS (smoke-only)"
  exit 0
fi

CTEST_REGEX_QUICK="${SLM_TEST_TOTHESPOT_REGEX:-^(globalsearchservice_dbus_test|appmodel_scoring_weights_test|appmodel_xbel_fallback_test|filemanagerapi_fileops_contract_test|tothespot_contextmenuhelper_test|tothespot_texthighlighter_test)$}"
echo "[test-tothespot] running focused tests: ${CTEST_REGEX_QUICK}"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "${CTEST_REGEX_QUICK}"

if [[ "${MODE}" == "full" ]]; then
  echo "[test-tothespot] running full test suite"
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

echo "[test-tothespot] PASS"
