#!/usr/bin/env bash
# run-harness.sh — build (if needed) and launch the SLM installer dev harness.
#
# Usage: scripts/installer/run-harness.sh <screen>
#   screen ∈ { disk-select, summary, welcome }
#
# The harness is a tiny Qt 6 process bundled with the SlmInstaller QML
# module, so screens render exactly as Calamares would render them
# without needing a VM, a .deb, or Calamares itself.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${SLM_INSTALLER_BUILD_DIR:-${ROOT_DIR}/build_installer}"
HARNESS_BIN="${BUILD_DIR}/src/installer/dev-harness/slm-installer-harness"

usage() {
  cat <<EOF
Usage: $(basename "$0") <screen>

Available screens:
  disk-select   §2.3 disk selection
  summary       §2.5 confirmation
  welcome       §2.1 welcome card

Environment:
  SLM_INSTALLER_BUILD_DIR   override build directory (default: ./build_installer)
EOF
}

if [[ $# -ne 1 || "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 64
fi

SCREEN="$1"

case "${SCREEN}" in
  disk-select|summary|welcome) ;;
  *)
    echo "[run-harness] unknown screen: ${SCREEN}" >&2
    usage >&2
    exit 64
    ;;
esac

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "[run-harness] configuring ${BUILD_DIR}"
  cmake -B "${BUILD_DIR}" -S "${ROOT_DIR}" -DSLM_BUILD_INSTALLER=ON >/dev/null
fi

echo "[run-harness] building slm-installer-harness"
cmake --build "${BUILD_DIR}" --target slm-installer-harness -j"$(nproc)"

if [[ ! -x "${HARNESS_BIN}" ]]; then
  echo "[run-harness] harness binary not found at ${HARNESS_BIN}" >&2
  exit 1
fi

echo "[run-harness] launching ${SCREEN}"
exec "${HARNESS_BIN}" "${SCREEN}"
