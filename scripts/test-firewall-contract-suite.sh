#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/toppanel-Debug}"
SKIP_BUILD="${SLM_FIREWALL_CONTRACT_SKIP_BUILD:-0}"

if [[ "${SKIP_BUILD}" != "0" && "${SKIP_BUILD}" != "1" ]]; then
  echo "[firewall-contract] invalid SLM_FIREWALL_CONTRACT_SKIP_BUILD='${SKIP_BUILD}' (expected 0|1)" >&2
  exit 2
fi

if [[ "${SKIP_BUILD}" == "0" ]]; then
  cmake -S . -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --target \
    firewallservice_dbus_contract_test \
    firewall_policyengine_contract_test \
    firewall_nftables_adapter_test \
    firewall_identity_mapping_contract_test \
    -j"$(nproc)"
fi

ctest --test-dir "${BUILD_DIR}" -R '^firewall_.*_test$' --output-on-failure
