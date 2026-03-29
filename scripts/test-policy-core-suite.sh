#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"
MIN_LABELED_TESTS="${SLM_POLICY_CORE_MIN_TESTS:-1}"
STRICT_NAMES="${SLM_POLICY_CORE_STRICT_NAMES:-1}"
EXPECTED_TESTS="${SLM_POLICY_CORE_EXPECTED_TESTS:-settingsapp_policy_core_test}"
SKIP_BUILD="${SLM_POLICY_CORE_SKIP_BUILD:-0}"

if [[ "${SKIP_BUILD}" != "0" && "${SKIP_BUILD}" != "1" ]]; then
  echo "[policy-core] invalid SLM_POLICY_CORE_SKIP_BUILD='${SKIP_BUILD}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${SKIP_BUILD}" == "1" ]]; then
  echo "[policy-core] skip configure/build (SLM_POLICY_CORE_SKIP_BUILD=1)"
else
  echo "[policy-core] configure: ${BUILD_DIR}"
  cmake -S . -B "${BUILD_DIR}"

  echo "[policy-core] build target: policy_core_test_suite"
  cmake --build "${BUILD_DIR}" --target policy_core_test_suite -j"$(nproc)"
fi

echo "[policy-core] run label: policy-core"
if ! [[ "${MIN_LABELED_TESTS}" =~ ^[0-9]+$ ]]; then
  echo "[policy-core] invalid SLM_POLICY_CORE_MIN_TESTS='${MIN_LABELED_TESTS}' (expected integer)" >&2
  exit 2
fi
if [[ "${MIN_LABELED_TESTS}" -lt 1 ]]; then
  echo "[policy-core] invalid SLM_POLICY_CORE_MIN_TESTS='${MIN_LABELED_TESTS}' (must be >= 1)" >&2
  exit 2
fi

LABEL_LIST_OUTPUT="$(ctest --test-dir "${BUILD_DIR}" -N -L policy-core 2>/dev/null || true)"
LABELED_COUNT="$(printf '%s\n' "${LABEL_LIST_OUTPUT}" | sed -n 's/^Total Tests: //p' | tail -n1)"
if [[ -z "${LABELED_COUNT}" || ! "${LABELED_COUNT}" =~ ^[0-9]+$ ]]; then
  echo "[policy-core] failed to determine labeled test count for label 'policy-core'" >&2
  exit 2
fi
if [[ "${LABELED_COUNT}" -lt "${MIN_LABELED_TESTS}" ]]; then
  echo "[policy-core] expected at least ${MIN_LABELED_TESTS} tests with label 'policy-core', found ${LABELED_COUNT}" >&2
  exit 2
fi
echo "[policy-core] labeled tests: ${LABELED_COUNT} (min required: ${MIN_LABELED_TESTS})"

if [[ "${STRICT_NAMES}" != "0" && "${STRICT_NAMES}" != "1" ]]; then
  echo "[policy-core] invalid SLM_POLICY_CORE_STRICT_NAMES='${STRICT_NAMES}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${STRICT_NAMES}" == "1" ]]; then
  mapfile -t expected_arr < <(printf '%s\n' "${EXPECTED_TESTS}" | tr ',' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | sed '/^$/d')
  if [[ "${#expected_arr[@]}" -eq 0 ]]; then
    echo "[policy-core] strict mode enabled but expected test list is empty" >&2
    exit 2
  fi

  mapfile -t actual_arr < <(printf '%s\n' "${LABEL_LIST_OUTPUT}" | sed -En 's/^[[:space:]]*Test[[:space:]]*#[[:space:]]*[0-9]+:[[:space:]]*(.+)$/\1/p')
  if [[ "${#actual_arr[@]}" -eq 0 ]]; then
    echo "[policy-core] strict mode enabled but no labeled test names were discovered" >&2
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
      echo "[policy-core] strict mode: missing expected test '${t}'" >&2
      exit 2
    fi
  done
  for t in "${actual_arr[@]}"; do
    if [[ -z "${expected_set[$t]:-}" ]]; then
      echo "[policy-core] strict mode: unexpected labeled test '${t}'" >&2
      echo "[policy-core] adjust SLM_POLICY_CORE_EXPECTED_TESTS or set SLM_POLICY_CORE_STRICT_NAMES=0" >&2
      exit 2
    fi
  done
  echo "[policy-core] strict name contract: OK (${#actual_arr[@]} tests)"
fi

ctest --test-dir "${BUILD_DIR}" -L policy-core --output-on-failure
