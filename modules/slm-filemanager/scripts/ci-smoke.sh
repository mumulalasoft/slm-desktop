#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build/ci}"
INSTALL_PREFIX="${2:-${BUILD_DIR}/install}"
if [[ "${BUILD_DIR}" != /* ]]; then
  BUILD_DIR="$(pwd)/${BUILD_DIR}"
fi
if [[ "${INSTALL_PREFIX}" != /* ]]; then
  INSTALL_PREFIX="$(pwd)/${INSTALL_PREFIX}"
fi
CONSUMER_DIR="${BUILD_DIR}/consumer-smoke"

log() { printf '[slm-filemanager-ci] %s\n' "$*"; }

log "configure standalone package"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja -DSLM_FILEMANAGER_BUILD_TESTS=ON

log "build core target"
cmake --build "${BUILD_DIR}" --target slm-filemanager-core -j"$(nproc)"

if [[ -f "${BUILD_DIR}/CTestTestfile.cmake" ]]; then
  log "list standalone tests (placeholder-safe)"
  ctest --test-dir "${BUILD_DIR}" -N || true
fi

log "install package"
cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"

log "package-configure smoke via find_package(SlmFileManager)"
mkdir -p "${CONSUMER_DIR}"
cat > "${CONSUMER_DIR}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(slm_filemanager_package_smoke LANGUAGES CXX)
find_package(SlmFileManager CONFIG REQUIRED)
add_library(pkg_smoke INTERFACE)
target_link_libraries(pkg_smoke INTERFACE SlmFileManager::Core)
EOF
cmake -S "${CONSUMER_DIR}" -B "${CONSUMER_DIR}/build" -G Ninja \
  -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}"

log "ci smoke passed"
