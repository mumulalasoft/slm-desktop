#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"

echo "[smoke-login-recovery] build dir=${BUILD_DIR}"

if [[ ! -x "${BUILD_DIR}/slm-session-broker" ]]; then
  echo "[FAIL] missing ${BUILD_DIR}/slm-session-broker"
  exit 1
fi

if [[ ! -x "${BUILD_DIR}/slm-watchdog" ]]; then
  echo "[FAIL] missing ${BUILD_DIR}/slm-watchdog"
  exit 1
fi

if [[ ! -x "${BUILD_DIR}/slm-recoveryd" ]]; then
  echo "[WARN] missing ${BUILD_DIR}/slm-recoveryd (skip daemon smoke)"
fi

echo "[smoke-login-recovery] running unit tests"
ctest --test-dir "${BUILD_DIR}" -R "(slmconfigmanager_validation_test|slmsessionstate_io_test|sessionbroker_recovery_rollback_test|recoveryservice_contract_test)" --output-on-failure

echo "[smoke-login-recovery] session mode parser sanity"
"${BUILD_DIR}/slm-session-broker" --help >/dev/null
"${BUILD_DIR}/slm-session-broker" --mode safe --help >/dev/null || true

echo "[smoke-login-recovery] recovery partition pipeline smoke"
scripts/recovery/smoke-recovery-partition-pipeline.sh

echo "[smoke-login-recovery] done"
