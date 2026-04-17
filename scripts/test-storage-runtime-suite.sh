#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"
MIN_LABELED_TESTS="${SLM_STORAGE_SMOKE_MIN_TESTS:-1}"
STRICT_NAMES="${SLM_STORAGE_SMOKE_STRICT_NAMES:-1}"
EXPECTED_TESTS="${SLM_STORAGE_SMOKE_EXPECTED_TESTS:-storage_runtime_smoke_test}"
SKIP_BUILD="${SLM_STORAGE_SMOKE_SKIP_BUILD:-0}"
HARDWARE_MODE="${SLM_STORAGE_RUNTIME_HARDWARE_MODE:-optional}"
REQUIRE_SERVICE_RESTART="${SLM_STORAGE_SMOKE_REQUIRE_SERVICE_RESTART:-0}"
DEVICESD_SERVICE="${SLM_STORAGE_DEVICESD_SERVICE:-slm-devicesd.service}"

if [[ "${SKIP_BUILD}" != "0" && "${SKIP_BUILD}" != "1" ]]; then
  echo "[storage-smoke] invalid SLM_STORAGE_SMOKE_SKIP_BUILD='${SKIP_BUILD}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${HARDWARE_MODE}" != "required" && "${HARDWARE_MODE}" != "optional" ]]; then
  echo "[storage-smoke] invalid SLM_STORAGE_RUNTIME_HARDWARE_MODE='${HARDWARE_MODE}' (expected required|optional)" >&2
  exit 2
fi
if [[ "${REQUIRE_SERVICE_RESTART}" != "0" && "${REQUIRE_SERVICE_RESTART}" != "1" ]]; then
  echo "[storage-smoke] invalid SLM_STORAGE_SMOKE_REQUIRE_SERVICE_RESTART='${REQUIRE_SERVICE_RESTART}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${SKIP_BUILD}" == "1" ]]; then
  echo "[storage-smoke] skip configure/build (SLM_STORAGE_SMOKE_SKIP_BUILD=1)"
else
  echo "[storage-smoke] configure: ${BUILD_DIR}"
  cmake -S . -B "${BUILD_DIR}"

  echo "[storage-smoke] build target: storage_runtime_smoke_test"
  cmake --build "${BUILD_DIR}" --target storage_runtime_smoke_test -j"$(nproc)"
fi

if ! [[ "${MIN_LABELED_TESTS}" =~ ^[0-9]+$ ]]; then
  echo "[storage-smoke] invalid SLM_STORAGE_SMOKE_MIN_TESTS='${MIN_LABELED_TESTS}' (expected integer)" >&2
  exit 2
fi
if [[ "${MIN_LABELED_TESTS}" -lt 1 ]]; then
  echo "[storage-smoke] invalid SLM_STORAGE_SMOKE_MIN_TESTS='${MIN_LABELED_TESTS}' (must be >= 1)" >&2
  exit 2
fi

echo "[storage-smoke] run label: storage-smoke"
LABEL_LIST_OUTPUT="$(ctest --test-dir "${BUILD_DIR}" -N -L storage-smoke 2>/dev/null || true)"
LABELED_COUNT="$(printf '%s\n' "${LABEL_LIST_OUTPUT}" | sed -n 's/^Total Tests: //p' | tail -n1)"
if [[ -z "${LABELED_COUNT}" || ! "${LABELED_COUNT}" =~ ^[0-9]+$ ]]; then
  echo "[storage-smoke] failed to determine labeled test count for label 'storage-smoke'" >&2
  exit 2
fi
if [[ "${LABELED_COUNT}" -lt "${MIN_LABELED_TESTS}" ]]; then
  echo "[storage-smoke] expected at least ${MIN_LABELED_TESTS} tests with label 'storage-smoke', found ${LABELED_COUNT}" >&2
  exit 2
fi
echo "[storage-smoke] labeled tests: ${LABELED_COUNT} (min required: ${MIN_LABELED_TESTS})"

if [[ "${STRICT_NAMES}" != "0" && "${STRICT_NAMES}" != "1" ]]; then
  echo "[storage-smoke] invalid SLM_STORAGE_SMOKE_STRICT_NAMES='${STRICT_NAMES}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${STRICT_NAMES}" == "1" ]]; then
  mapfile -t expected_arr < <(printf '%s\n' "${EXPECTED_TESTS}" | tr ',' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | sed '/^$/d')
  if [[ "${#expected_arr[@]}" -eq 0 ]]; then
    echo "[storage-smoke] strict mode enabled but expected test list is empty" >&2
    exit 2
  fi

  mapfile -t actual_arr < <(printf '%s\n' "${LABEL_LIST_OUTPUT}" | sed -En 's/^[[:space:]]*Test[[:space:]]*#[[:space:]]*[0-9]+:[[:space:]]*(.+)$/\1/p')
  if [[ "${#actual_arr[@]}" -eq 0 ]]; then
    echo "[storage-smoke] strict mode enabled but no labeled test names were discovered" >&2
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
      echo "[storage-smoke] strict mode: missing expected test '${t}'" >&2
      exit 2
    fi
  done
  for t in "${actual_arr[@]}"; do
    if [[ -z "${expected_set[$t]:-}" ]]; then
      echo "[storage-smoke] strict mode: unexpected labeled test '${t}'" >&2
      echo "[storage-smoke] adjust SLM_STORAGE_SMOKE_EXPECTED_TESTS or set SLM_STORAGE_SMOKE_STRICT_NAMES=0" >&2
      exit 2
    fi
  done
  echo "[storage-smoke] strict name contract: OK (${#actual_arr[@]} tests)"
fi

run_required_checks() {
  SLM_STORAGE_SMOKE_REQUIRE_SERVICE=1 \
  SLM_STORAGE_SMOKE_REQUIRE_MULTI_PARTITION=1 \
  SLM_STORAGE_SMOKE_REQUIRE_ENCRYPTED=1 \
  SLM_STORAGE_SMOKE_REQUIRE_POLICY_PERSISTENCE=1 \
  ctest --test-dir "${BUILD_DIR}" -L storage-smoke --output-on-failure
}

run_required_checks_phase() {
  local phase="${1}"
  local state_file="${2}"
  SLM_STORAGE_SMOKE_REQUIRE_SERVICE=1 \
  SLM_STORAGE_SMOKE_REQUIRE_MULTI_PARTITION=1 \
  SLM_STORAGE_SMOKE_REQUIRE_ENCRYPTED=1 \
  SLM_STORAGE_SMOKE_REQUIRE_POLICY_PERSISTENCE=1 \
  SLM_STORAGE_SMOKE_POLICY_PHASE="${phase}" \
  SLM_STORAGE_SMOKE_POLICY_STATE_FILE="${state_file}" \
  ctest --test-dir "${BUILD_DIR}" -L storage-smoke --output-on-failure
}

if [[ "${HARDWARE_MODE}" == "required" ]]; then
  echo "[storage-smoke] hardware checks: required"
  run_required_checks

  if [[ "${REQUIRE_SERVICE_RESTART}" == "1" ]]; then
    echo "[storage-smoke] service restart verification: required (${DEVICESD_SERVICE})"
    if ! command -v systemctl >/dev/null 2>&1; then
      echo "[storage-smoke] systemctl not available for restart verification" >&2
      exit 2
    fi
    if ! systemctl --user list-unit-files --type=service --no-pager | rg -q "^${DEVICESD_SERVICE}\\b"; then
      echo "[storage-smoke] required service '${DEVICESD_SERVICE}' is not installed for --user" >&2
      exit 2
    fi
    systemctl --user restart "${DEVICESD_SERVICE}"
    if ! systemctl --user is-active --quiet "${DEVICESD_SERVICE}"; then
      echo "[storage-smoke] service '${DEVICESD_SERVICE}' is not active after restart" >&2
      systemctl --user --no-pager --full status "${DEVICESD_SERVICE}" || true
      exit 2
    fi

    state_file="/tmp/slm-storage-policy-state-${RANDOM}-$$.json"
    echo "[storage-smoke] phased persistence prepare before restart (state=${state_file})"
    run_required_checks_phase "prepare" "${state_file}"

    echo "[storage-smoke] restarting ${DEVICESD_SERVICE} for phased persistence verify"
    systemctl --user restart "${DEVICESD_SERVICE}"
    if ! systemctl --user is-active --quiet "${DEVICESD_SERVICE}"; then
      echo "[storage-smoke] service '${DEVICESD_SERVICE}' is not active after restart (phase verify)" >&2
      systemctl --user --no-pager --full status "${DEVICESD_SERVICE}" || true
      exit 2
    fi
    echo "[storage-smoke] phased persistence verify after restart (state=${state_file})"
    run_required_checks_phase "verify" "${state_file}"
    rm -f "${state_file}" || true
  fi
else
  echo "[storage-smoke] hardware checks: optional"
  ctest --test-dir "${BUILD_DIR}" -L storage-smoke --output-on-failure
fi
