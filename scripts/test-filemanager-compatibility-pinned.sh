#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${1:-build/filemanager-compatibility}"
EXPECTED_VERSION="${SLM_FILEMANAGER_PINNED_VERSION:-0.1.0}"
INSTALL_PREFIX="${BUILD_ROOT}/slm-filemanager-install"
VERSION_FILE="${INSTALL_PREFIX}/lib/cmake/SlmFileManager/SlmFileManagerConfigVersion.cmake"
EXTERNAL_BUILD_DIR="${BUILD_ROOT}/external"

log() { printf '[filemanager-compat] %s\n' "$*"; }
die() { printf '[filemanager-compat][FAIL] %s\n' "$*" >&2; exit 1; }

log "running integration modes baseline"
"${ROOT_DIR}/scripts/test-filemanager-integration-modes.sh" "${BUILD_ROOT}"

[[ -f "${VERSION_FILE}" ]] || die "missing package version file: ${VERSION_FILE}"

if rg -n "set\\(PACKAGE_VERSION \\\"${EXPECTED_VERSION}\\\"\\)" "${VERSION_FILE}" >/dev/null 2>&1; then
  log "pinned package version ok: ${EXPECTED_VERSION}"
else
  log "version file content:"
  sed -n '1,120p' "${VERSION_FILE}" || true
  die "expected pinned version ${EXPECTED_VERSION} was not found"
fi

[[ -d "${EXTERNAL_BUILD_DIR}" ]] || die "missing external build dir: ${EXTERNAL_BUILD_DIR}"
if [[ -x "${EXTERNAL_BUILD_DIR}/fileoperationsservice_dbus_test" ]] \
  && [[ -x "${EXTERNAL_BUILD_DIR}/fileopctl_smoke_test" ]] \
  && [[ -x "${EXTERNAL_BUILD_DIR}/filemanagerapi_daemon_recovery_test" ]]; then
  log "run FileManager DBus stable gate against external package shell build"
  "${ROOT_DIR}/scripts/test-filemanager-dbus-gates.sh" --build-dir "${EXTERNAL_BUILD_DIR}" --stable
else
  log "external compatibility build does not include DBus gate binaries; skip external DBus gate"
  log "DBus stable gate is already enforced in regular filemanager-integration-modes CI"
fi

log "compatibility pinned check passed"
