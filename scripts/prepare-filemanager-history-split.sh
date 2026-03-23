#!/usr/bin/env bash
set -euo pipefail

# Non-destructive helper to prepare a history-preserving split repository
# for slm-filemanager using git-filter-repo.
#
# Usage:
#   scripts/prepare-filemanager-history-split.sh [output-dir]
#
# Example:
#   scripts/prepare-filemanager-history-split.sh /tmp/slm-filemanager-repo
#
# Optional:
#   ALLOW_SNAPSHOT_FALLBACK=0 scripts/prepare-filemanager-history-split.sh ...
#   (force history-preserving mode only; fail if git-filter-repo is unavailable)

OUTPUT_DIR="${1:-/tmp/slm-filemanager-repo}"
ALLOW_SNAPSHOT_FALLBACK="${ALLOW_SNAPSHOT_FALLBACK:-1}"
WORK_DIR="$(mktemp -d /tmp/slm-filemanager-split.XXXXXX)"
PATHS_FILE="modules/slm-filemanager/docs/EXTRACTION_PATHS.txt"
STANDALONE_TEMPLATE_ROOT="modules/slm-filemanager"
HAS_HISTORY_PATHS=1
FORCE_SNAPSHOT_FALLBACK=0

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT

commit_scaffolding_if_dirty() {
  local out_dir="$1"
  if ! git -C "$out_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    return 0
  fi
  if [[ -z "$(git -C "$out_dir" status --porcelain)" ]]; then
    return 0
  fi
  git -C "$out_dir" config user.name "SLM Build Bot" || true
  git -C "$out_dir" config user.email "slm-buildbot@local" || true
  git -C "$out_dir" add .
  git -C "$out_dir" commit -q -m "Add standalone split scaffolding (build + CI)"
}

materialize_standalone_layout() {
  local out_dir="$1"

  mkdir -p "$out_dir/cmake" "$out_dir/tests" "$out_dir/scripts" "$out_dir/docs" "$out_dir/.github/workflows"

  if [[ -f "$STANDALONE_TEMPLATE_ROOT/cmake/Sources.cmake" ]]; then
    sed 's#${SLM_FM_MONOREPO_ROOT}/##g' \
      "$STANDALONE_TEMPLATE_ROOT/cmake/Sources.cmake" \
      | grep -v 'src/core/execution/appexecutiongate.cpp' \
      | grep -v 'src/core/execution/appruntimeregistry.cpp' \
      > "$out_dir/cmake/Sources.cmake"
  fi

  if [[ -f "$STANDALONE_TEMPLATE_ROOT/cmake/SlmFileManagerConfig.cmake.in" ]]; then
    cp -f "$STANDALONE_TEMPLATE_ROOT/cmake/SlmFileManagerConfig.cmake.in" \
      "$out_dir/cmake/SlmFileManagerConfig.cmake.in"
  fi

  if [[ -f "$STANDALONE_TEMPLATE_ROOT/scripts/ci-smoke.sh" ]]; then
    cp -f "$STANDALONE_TEMPLATE_ROOT/scripts/ci-smoke.sh" "$out_dir/scripts/ci-smoke.sh"
    chmod +x "$out_dir/scripts/ci-smoke.sh"
  fi

  if [[ -f "$STANDALONE_TEMPLATE_ROOT/.github/workflows/ci.yml" ]]; then
    cp -f "$STANDALONE_TEMPLATE_ROOT/.github/workflows/ci.yml" "$out_dir/.github/workflows/ci.yml"
  fi

  if [[ -f "$STANDALONE_TEMPLATE_ROOT/docs/MIGRATION.md" ]]; then
    cp -f "$STANDALONE_TEMPLATE_ROOT/docs/MIGRATION.md" "$out_dir/docs/MIGRATION.md"
  fi

  cat > "$out_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)

project(slm-filemanager LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/Sources.cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus Sql Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-unix-2.0)

add_library(slm-filemanager-core STATIC
    ${SLM_FILEMANAGER_CORE_SOURCES}
    ${SLM_FILEMANAGER_CORE_HEADERS}
)

add_library(SlmFileManager::Core ALIAS slm-filemanager-core)

target_include_directories(slm-filemanager-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/apps/filemanager/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/slm-filemanager>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apps/filemanager/include
)

target_link_libraries(slm-filemanager-core
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Concurrent
        PkgConfig::GIO
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/SlmFileManagerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SlmFileManager"
)

install(TARGETS slm-filemanager-core
    EXPORT SlmFileManagerTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/apps/filemanager/include/filemanagerapi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/apps/filemanager/include/filemanagermodel.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/apps/filemanager/include/filemanagermodelfactory.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/slm-filemanager"
)

install(EXPORT SlmFileManagerTargets
    FILE SlmFileManagerTargets.cmake
    NAMESPACE SlmFileManager::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SlmFileManager"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SlmFileManager"
)

option(SLM_FILEMANAGER_BUILD_TESTS "Build slm-filemanager standalone tests" OFF)
if(SLM_FILEMANAGER_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
EOF

  cat > "$out_dir/tests/CMakeLists.txt" <<'EOF'
# Placeholder for standalone test migration.
message(STATUS "slm-filemanager standalone tests are not migrated yet.")
EOF

  for md in README.md ROADMAP.md MILESTONES.md; do
    if [[ -f "$STANDALONE_TEMPLATE_ROOT/$md" ]]; then
      cp -f "$STANDALONE_TEMPLATE_ROOT/$md" "$out_dir/$md"
    fi
  done
}

if [[ ! -f "$PATHS_FILE" ]]; then
  echo "[filemanager-split] missing path manifest: $PATHS_FILE" >&2
  exit 1
fi

rm -rf "$OUTPUT_DIR"

# Preflight: history-preserving split only works if path manifest exists in git history.
while IFS= read -r path; do
  [[ -z "$path" ]] && continue
  if ! git cat-file -e "HEAD:$path" >/dev/null 2>&1; then
    HAS_HISTORY_PATHS=0
    break
  fi
done < "$PATHS_FILE"

if [[ "$HAS_HISTORY_PATHS" != "1" ]] && [[ -x "scripts/check-filemanager-split-readiness.sh" ]]; then
  scripts/check-filemanager-split-readiness.sh "$PATHS_FILE" || true
fi

if ! command -v git-filter-repo >/dev/null 2>&1; then
  if [[ "$HAS_HISTORY_PATHS" != "1" ]]; then
    if [[ "$ALLOW_SNAPSHOT_FALLBACK" != "1" ]]; then
      echo "[filemanager-split] manifest paths are not present in HEAD history; cannot perform history-preserving split." >&2
      exit 1
    fi
    FORCE_SNAPSHOT_FALLBACK=1
  fi
  if [[ "$FORCE_SNAPSHOT_FALLBACK" != "1" ]] && git fast-export --help >/dev/null 2>&1; then
    echo "[filemanager-split] git-filter-repo not found; using git fast-export/import fallback (history preserved)."
    echo "[filemanager-split] cloning current repo to temporary workspace..."
    git clone --no-local . "$WORK_DIR/repo"
    pushd "$WORK_DIR/repo" >/dev/null
    CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    PATH_SPECS=()
    while IFS= read -r path; do
      [[ -z "$path" ]] && continue
      PATH_SPECS+=("$path")
    done < "$OLDPWD/$PATHS_FILE"
    mkdir -p "$OUTPUT_DIR"
    pushd "$OUTPUT_DIR" >/dev/null
    git init -q
    popd >/dev/null
    git fast-export --tag-of-filtered-object=drop --all -- "${PATH_SPECS[@]}" | (cd "$OUTPUT_DIR" && git fast-import --quiet)
    if git -C "$OUTPUT_DIR" show-ref --verify --quiet "refs/heads/${CURRENT_BRANCH}"; then
      git -C "$OUTPUT_DIR" checkout -q "${CURRENT_BRANCH}"
    elif git -C "$OUTPUT_DIR" show-ref --verify --quiet "refs/heads/main"; then
      git -C "$OUTPUT_DIR" checkout -q main
    elif git -C "$OUTPUT_DIR" show-ref --verify --quiet "refs/heads/master"; then
      git -C "$OUTPUT_DIR" checkout -q master
    fi
    git -C "$OUTPUT_DIR" gc --prune=now >/dev/null 2>&1 || true
    popd >/dev/null
    materialize_standalone_layout "$OUTPUT_DIR"
    commit_scaffolding_if_dirty "$OUTPUT_DIR"
    echo "[filemanager-split] history-preserving fallback repository prepared at: $OUTPUT_DIR"
    exit 0
  fi

  if [[ "$ALLOW_SNAPSHOT_FALLBACK" != "1" ]]; then
    echo "[filemanager-split] no history-preserving splitter available (git-filter-repo/fast-export)." >&2
    exit 1
  fi

  if [[ "$FORCE_SNAPSHOT_FALLBACK" == "1" ]]; then
    echo "[filemanager-split] manifest paths are not present in committed HEAD; creating snapshot fallback (no history)."
  else
    echo "[filemanager-split] no history-preserving splitter available; creating snapshot fallback (no history)."
  fi
  mkdir -p "$OUTPUT_DIR"
  while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    if [[ -e "$path" ]]; then
      mkdir -p "$OUTPUT_DIR/$(dirname "$path")"
      cp -a "$path" "$OUTPUT_DIR/$path"
    fi
  done < "$PATHS_FILE"

  pushd "$OUTPUT_DIR" >/dev/null
  git init -q
  git config user.name "SLM Build Bot"
  git config user.email "slm-buildbot@local"
  materialize_standalone_layout "$OUTPUT_DIR"
  git add .
  git commit -q -m "Initial slm-filemanager snapshot split (fallback, no history)"
  popd >/dev/null

  echo "[filemanager-split] snapshot repository prepared at: $OUTPUT_DIR"
  if [[ "$FORCE_SNAPSHOT_FALLBACK" == "1" ]]; then
    echo "[filemanager-split] commit filemanager paths first, then rerun for history-preserving split."
  else
    echo "[filemanager-split] install git-filter-repo and rerun for full history-preserving split."
  fi
  exit 0
fi

if [[ "$HAS_HISTORY_PATHS" != "1" || "$FORCE_SNAPSHOT_FALLBACK" == "1" ]]; then
  if [[ "$ALLOW_SNAPSHOT_FALLBACK" != "1" ]]; then
    echo "[filemanager-split] manifest paths are not present in HEAD history; cannot perform history-preserving split." >&2
    exit 1
  fi
  echo "[filemanager-split] manifest paths are not present in HEAD history; using snapshot fallback."
  mkdir -p "$OUTPUT_DIR"
  while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    if [[ -e "$path" ]]; then
      mkdir -p "$OUTPUT_DIR/$(dirname "$path")"
      cp -a "$path" "$OUTPUT_DIR/$path"
    fi
  done < "$PATHS_FILE"

  pushd "$OUTPUT_DIR" >/dev/null
  git init -q
  git config user.name "SLM Build Bot"
  git config user.email "slm-buildbot@local"
  materialize_standalone_layout "$OUTPUT_DIR"
  git add .
  git commit -q -m "Initial slm-filemanager snapshot split (fallback, no history)"
  popd >/dev/null

  echo "[filemanager-split] snapshot repository prepared at: $OUTPUT_DIR"
  echo "[filemanager-split] after filemanager paths are committed, rerun for history-preserving split."
  exit 0
fi

echo "[filemanager-split] cloning current repo to temporary workspace..."
git clone --no-local . "$WORK_DIR/repo"

pushd "$WORK_DIR/repo" >/dev/null

FILTER_ARGS=()
while IFS= read -r path; do
  [[ -z "$path" ]] && continue
  FILTER_ARGS+=(--path "$path")
done < "$OLDPWD/$PATHS_FILE"

echo "[filemanager-split] filtering history by manifest paths..."
git filter-repo "${FILTER_ARGS[@]}" --force

mkdir -p "$OUTPUT_DIR"
cp -a . "$OUTPUT_DIR/"
popd >/dev/null

materialize_standalone_layout "$OUTPUT_DIR"
commit_scaffolding_if_dirty "$OUTPUT_DIR"

echo "[filemanager-split] split repository prepared at: $OUTPUT_DIR"
echo "[filemanager-split] next recommended step:"
echo "  cd $OUTPUT_DIR && git remote add origin <new-slm-filemanager-repo-url> && git push -u origin HEAD"
