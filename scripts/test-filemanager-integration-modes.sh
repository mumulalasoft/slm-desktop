#!/usr/bin/env bash
set -euo pipefail

BUILD_ROOT="${1:-build/filemanager-integration}"
GENERATOR="${CMAKE_GENERATOR:-Ninja}"
INSTALL_PREFIX="${BUILD_ROOT}/slm-filemanager-install"
REMOTE_EXTERNAL_REPO="${SLM_FILEMANAGER_EXTERNAL_REPO:-}"
REMOTE_EXTERNAL_BRANCH="${SLM_FILEMANAGER_EXTERNAL_BRANCH:-main}"
REMOTE_EXTERNAL_ROOT="${SLM_FILEMANAGER_EXTERNAL_ROOT:-${BUILD_ROOT}/remote-external}"

log() { printf '[filemanager-integration] %s\n' "$*"; }

mkdir -p "$BUILD_ROOT"

IN_TREE_BUILD_DIR="${BUILD_ROOT}/in-tree"
STANDALONE_BUILD_DIR="${BUILD_ROOT}/standalone"
EXTERNAL_BUILD_DIR="${BUILD_ROOT}/external"
REMOTE_EXTERNAL_BUILD_DIR="${BUILD_ROOT}/external-remote"

qt6_dir="${SLM_QT6_DIR:-}"
qt_cache_source="${SLM_QT_CACHE_SOURCE:-build/toppanel-Debug/CMakeCache.txt}"
qt_cache_args=()
if [[ -z "$qt6_dir" && -f "$qt_cache_source" ]]; then
  qt6_dir="$(awk -F= '/^Qt6_DIR:/{print $2; exit}' "$qt_cache_source" || true)"
fi
if [[ -f "$qt_cache_source" ]]; then
  while IFS= read -r line; do
    key="${line%%:*}"
    value="${line#*=}"
    [[ -z "$key" || -z "$value" ]] && continue
    qt_cache_args+=("-D${key}=${value}")
  done < <(awk -F= '/^Qt6[^:]*_DIR:PATH=/{print $1 "=" $2}' "$qt_cache_source")
fi
qt_prefix=""
if [[ -n "$qt6_dir" ]]; then
  qt_prefix="$(cd "$(dirname "$qt6_dir")/../.." && pwd)"
fi

log "configure in-tree shell build"
in_tree_args=(-S . -B "$IN_TREE_BUILD_DIR" -G "$GENERATOR")
if [[ -n "$qt6_dir" ]]; then
  in_tree_args+=("-DQt6_DIR=${qt6_dir}")
fi
if [[ -n "$qt_prefix" ]]; then
  in_tree_args+=("-DCMAKE_PREFIX_PATH=${qt_prefix}")
fi
if [[ "${#qt_cache_args[@]}" -gt 0 ]]; then
  in_tree_args+=("${qt_cache_args[@]}")
fi
cmake "${in_tree_args[@]}"

log "build in-tree filemanager profile targets"
cmake --build "$IN_TREE_BUILD_DIR" --target slm-filemanager-core filemanager_test_suite -j"$(nproc)"

if [[ -x scripts/check-filemanager-extraction-gate.sh ]]; then
  log "run extraction gate on in-tree build"
  scripts/check-filemanager-extraction-gate.sh "$IN_TREE_BUILD_DIR"
fi

log "configure standalone slm-filemanager staging"
cmake -S modules/slm-filemanager -B "$STANDALONE_BUILD_DIR" -G "$GENERATOR"
cmake --build "$STANDALONE_BUILD_DIR" --target slm-filemanager-core -j"$(nproc)"
cmake --install "$STANDALONE_BUILD_DIR" --prefix "$INSTALL_PREFIX"

ext_qt6_dir="$qt6_dir"
if [[ -f "${IN_TREE_BUILD_DIR}/CMakeCache.txt" ]]; then
  ext_qt6_dir="$(awk -F= '/^Qt6_DIR:/{print $2; exit}' "${IN_TREE_BUILD_DIR}/CMakeCache.txt" || true)"
fi

configure_external_shell() {
  local prefix_path="$1"
  local build_dir="$2"

  local external_args=(
    -S . -B "$build_dir"
    -G "$GENERATOR"
    -DSLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=ON
  )
  if [[ -n "$ext_qt6_dir" ]]; then
    external_args+=("-DQt6_DIR=${ext_qt6_dir}")
  fi
  if [[ -n "$qt_prefix" ]]; then
    external_args+=("-DCMAKE_PREFIX_PATH=${prefix_path};${qt_prefix}")
  else
    external_args+=("-DCMAKE_PREFIX_PATH=${prefix_path}")
  fi
  if [[ "${#qt_cache_args[@]}" -gt 0 ]]; then
    external_args+=("${qt_cache_args[@]}")
  fi
  cmake "${external_args[@]}"
}

log "configure shell build using external SlmFileManager package (local staging)"
configure_external_shell "$INSTALL_PREFIX" "$EXTERNAL_BUILD_DIR"

log "build appDesktop_Shell in external package mode (local staging)"
cmake --build "$EXTERNAL_BUILD_DIR" --target appDesktop_Shell -j"$(nproc)"

if [[ -n "$REMOTE_EXTERNAL_REPO" ]]; then
  log "prepare remote external FileManager package"
  SLM_EXTERNAL_FM_SKIP_SHELL=1 \
    scripts/bootstrap-external-filemanager-package.sh \
      "$REMOTE_EXTERNAL_REPO" \
      "$REMOTE_EXTERNAL_BRANCH" \
      "$REMOTE_EXTERNAL_ROOT"
  REMOTE_PREFIX="${REMOTE_EXTERNAL_ROOT}/install/slm-filemanager"

  log "configure shell build using external SlmFileManager package (remote repo)"
  configure_external_shell "$REMOTE_PREFIX" "$REMOTE_EXTERNAL_BUILD_DIR"

  log "build appDesktop_Shell in external package mode (remote repo)"
  cmake --build "$REMOTE_EXTERNAL_BUILD_DIR" --target appDesktop_Shell -j"$(nproc)"
fi

log "integration mode checks passed"
