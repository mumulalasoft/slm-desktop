#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"
MIN_LABELED_TESTS="${SLM_SECRET_CONSENT_MIN_TESTS:-6}"
STRICT_NAMES="${SLM_SECRET_CONSENT_STRICT_NAMES:-1}"
EXPECTED_TESTS="${SLM_SECRET_CONSENT_EXPECTED_TESTS:-portal_secret_integration_test,portal_consent_dialog_copy_contract_test,portal_consent_dialog_behavior_test,portal_dialog_bridge_payload_test,portal_secret_consent_contract_snapshot_test,settingsapp_secret_consent_test}"
SKIP_BUILD="${SLM_SECRET_CONSENT_SKIP_BUILD:-0}"

if [[ "${SKIP_BUILD}" != "0" && "${SKIP_BUILD}" != "1" ]]; then
  echo "[secret-consent] invalid SLM_SECRET_CONSENT_SKIP_BUILD='${SKIP_BUILD}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${SKIP_BUILD}" == "1" ]]; then
  echo "[secret-consent] skip configure/build (SLM_SECRET_CONSENT_SKIP_BUILD=1)"
else
  echo "[secret-consent] configure: ${BUILD_DIR}"
  cmake -S . -B "${BUILD_DIR}"

  echo "[secret-consent] build target: secret_consent_test_suite"
  cmake --build "${BUILD_DIR}" --target secret_consent_test_suite -j"$(nproc)"
fi

echo "[secret-consent] run label: secret-consent"
if ! [[ "${MIN_LABELED_TESTS}" =~ ^[0-9]+$ ]]; then
  echo "[secret-consent] invalid SLM_SECRET_CONSENT_MIN_TESTS='${MIN_LABELED_TESTS}' (expected integer)" >&2
  exit 2
fi
if [[ "${MIN_LABELED_TESTS}" -lt 1 ]]; then
  echo "[secret-consent] invalid SLM_SECRET_CONSENT_MIN_TESTS='${MIN_LABELED_TESTS}' (must be >= 1)" >&2
  exit 2
fi

LABEL_LIST_OUTPUT="$(ctest --test-dir "${BUILD_DIR}" -N -L secret-consent 2>/dev/null || true)"
LABELED_COUNT="$(printf '%s\n' "${LABEL_LIST_OUTPUT}" | sed -n 's/^Total Tests: //p' | tail -n1)"
if [[ -z "${LABELED_COUNT}" || ! "${LABELED_COUNT}" =~ ^[0-9]+$ ]]; then
  echo "[secret-consent] failed to determine labeled test count for label 'secret-consent'" >&2
  exit 2
fi
if [[ "${LABELED_COUNT}" -lt "${MIN_LABELED_TESTS}" ]]; then
  echo "[secret-consent] expected at least ${MIN_LABELED_TESTS} tests with label 'secret-consent', found ${LABELED_COUNT}" >&2
  exit 2
fi
echo "[secret-consent] labeled tests: ${LABELED_COUNT} (min required: ${MIN_LABELED_TESTS})"

if [[ "${STRICT_NAMES}" != "0" && "${STRICT_NAMES}" != "1" ]]; then
  echo "[secret-consent] invalid SLM_SECRET_CONSENT_STRICT_NAMES='${STRICT_NAMES}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${STRICT_NAMES}" == "1" ]]; then
  mapfile -t expected_arr < <(printf '%s\n' "${EXPECTED_TESTS}" | tr ',' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | sed '/^$/d')
  if [[ "${#expected_arr[@]}" -eq 0 ]]; then
    echo "[secret-consent] strict mode enabled but expected test list is empty" >&2
    exit 2
  fi

  mapfile -t actual_arr < <(printf '%s\n' "${LABEL_LIST_OUTPUT}" | sed -En 's/^[[:space:]]*Test[[:space:]]*#[[:space:]]*[0-9]+:[[:space:]]*(.+)$/\1/p')
  if [[ "${#actual_arr[@]}" -eq 0 ]]; then
    echo "[secret-consent] strict mode enabled but no labeled test names were discovered" >&2
    exit 2
  fi

  declare -A expected_set=()
  declare -A actual_set=()
  for t in "${expected_arr[@]}"; do
    expected_set["$t"]=1
  done
  for t in "${actual_arr[@]}"; do
    actual_set["$t"]=1
  done

  for t in "${expected_arr[@]}"; do
    if [[ -z "${actual_set[$t]:-}" ]]; then
      echo "[secret-consent] strict mode: missing expected test '${t}'" >&2
      exit 2
    fi
  done
  for t in "${actual_arr[@]}"; do
    if [[ -z "${expected_set[$t]:-}" ]]; then
      echo "[secret-consent] strict mode: unexpected labeled test '${t}'" >&2
      echo "[secret-consent] adjust SLM_SECRET_CONSENT_EXPECTED_TESTS or set SLM_SECRET_CONSENT_STRICT_NAMES=0" >&2
      exit 2
    fi
  done
  echo "[secret-consent] strict name contract: OK (${#actual_arr[@]} tests)"
fi

ctest --test-dir "${BUILD_DIR}" -L secret-consent --output-on-failure
