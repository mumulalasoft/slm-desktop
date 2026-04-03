#!/usr/bin/env bash
set -euo pipefail

# Build/install slm-filemanager package from external repo, then configure
# Slm_Desktop in external package mode.
#
# Usage:
#   scripts/bootstrap-external-filemanager-package.sh [repo-url] [branch] [work-root]
#
# Example:
#   scripts/bootstrap-external-filemanager-package.sh \
#     https://github.com/mumulalasoft/slm-filemanager.git main build/external-filemanager

REPO_URL="${1:-https://github.com/mumulalasoft/slm-filemanager.git}"
BRANCH="${2:-main}"
WORK_ROOT="${3:-build/external-filemanager}"
SKIP_SHELL_BUILD="${SLM_EXTERNAL_FM_SKIP_SHELL:-0}"

SRC_DIR="${WORK_ROOT}/src/slm-filemanager"
PKG_BUILD_DIR="${WORK_ROOT}/build/slm-filemanager"
PKG_INSTALL_DIR="${WORK_ROOT}/install/slm-filemanager"
SHELL_BUILD_DIR="${WORK_ROOT}/build/slm-desktop-external"

log() { printf '[external-fm] %s\n' "$*"; }

mkdir -p "${WORK_ROOT}/src" "${WORK_ROOT}/build" "${WORK_ROOT}/install"

if [[ -d "${SRC_DIR}/.git" ]]; then
  log "update existing slm-filemanager checkout"
  git -C "${SRC_DIR}" fetch origin
  git -C "${SRC_DIR}" checkout "${BRANCH}"
  git -C "${SRC_DIR}" pull --ff-only origin "${BRANCH}"
else
  log "clone slm-filemanager (${REPO_URL})"
  git clone --branch "${BRANCH}" --single-branch "${REPO_URL}" "${SRC_DIR}"
fi

log "configure slm-filemanager package"
cmake -S "${SRC_DIR}" -B "${PKG_BUILD_DIR}" -G Ninja -DSLM_FILEMANAGER_BUILD_TESTS=OFF

log "build slm-filemanager core"
cmake --build "${PKG_BUILD_DIR}" --target slm-filemanager-core -j"$(nproc)"

log "install slm-filemanager package"
cmake --install "${PKG_BUILD_DIR}" --prefix "${PKG_INSTALL_DIR}"

if [[ "${SKIP_SHELL_BUILD}" == "1" ]]; then
  log "skip Slm_Desktop external build (SLM_EXTERNAL_FM_SKIP_SHELL=1)"
  log "done (package only)"
  log "package prefix: ${PKG_INSTALL_DIR}"
  exit 0
fi

log "configure Slm_Desktop with external SlmFileManager package"
set +e
cmake -S . -B "${SHELL_BUILD_DIR}" -G Ninja \
  -DSLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=ON \
  -DCMAKE_PREFIX_PATH="${PKG_INSTALL_DIR}"
CONFIGURE_RC=$?
set -e
if [[ ${CONFIGURE_RC} -ne 0 ]]; then
  log "Slm_Desktop external configure failed."
  log "common prerequisite on Debian/Ubuntu/elementary: sudo apt install qt6-shadertools-dev"
  log "package build/install already succeeded."
  exit ${CONFIGURE_RC}
fi

log "build Slm_Desktop app target (slm-desktop)"
cmake --build "${SHELL_BUILD_DIR}" --target slm-desktop -j"$(nproc)"

log "done"
log "package prefix: ${PKG_INSTALL_DIR}"
log "shell build dir: ${SHELL_BUILD_DIR}"
