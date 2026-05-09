#!/usr/bin/env bash
# scripts/dev/qemu-guest-build.sh — Configure/build Desktop Shell di Ubuntu guest.

set -euo pipefail

REPO_DIR="${SLM_QEMU_GUEST_REPO_DIR:-/mnt/hostshare}"
BUILD_DIR="${SLM_QEMU_GUEST_BUILD_DIR:-$HOME/.cache/slm-qemu/build/dev}"
GENERATOR="${SLM_QEMU_GUEST_GENERATOR:-Ninja}"
BUILD_TYPE="${SLM_QEMU_GUEST_BUILD_TYPE:-Debug}"
JOBS="${SLM_QEMU_GUEST_JOBS:-$(nproc)}"
CMAKE_PREFIX_PATH="${SLM_QEMU_GUEST_CMAKE_PREFIX_PATH:-}"
TARGETS=()

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-guest-build.sh [options]

Options:
  --repo-dir PATH    Repo path. Default: $REPO_DIR
  --build-dir PATH   Build directory. Default: $BUILD_DIR
  --generator NAME   CMake generator. Default: $GENERATOR
  --type NAME        CMAKE_BUILD_TYPE. Default: $BUILD_TYPE
  --target NAME      Build target (repeatable). Default: appSlm_Desktop
  --jobs N           Parallel build jobs. Default: $JOBS
  --cmake-prefix P   CMAKE_PREFIX_PATH passed at configure. Default: <unset>
  --configure-only   Only run cmake configure
  --build-only       Skip configure and only build
  --no-run           Compatibility no-op (this script never runs binaries)
  --help             Show this help
EOF
}

CONFIGURE_ONLY=0
BUILD_ONLY=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo-dir)
            REPO_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --generator)
            GENERATOR="$2"
            shift 2
            ;;
        --type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --target)
            TARGETS+=("$2")
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --cmake-prefix)
            CMAKE_PREFIX_PATH="$2"
            shift 2
            ;;
        --configure-only)
            CONFIGURE_ONLY=1
            shift
            ;;
        --build-only)
            BUILD_ONLY=1
            shift
            ;;
        --no-run)
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-guest-build] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=("${SLM_QEMU_GUEST_TARGET:-appSlm_Desktop}")
fi

if [[ "$CONFIGURE_ONLY" == "1" && "$BUILD_ONLY" == "1" ]]; then
    echo "[qemu-guest-build] ERROR: --configure-only dan --build-only tidak bisa dipakai bersamaan." >&2
    exit 1
fi

if [[ ! -d "$REPO_DIR" ]]; then
    echo "[qemu-guest-build] ERROR: repo dir tidak ditemukan: $REPO_DIR" >&2
    exit 1
fi

if [[ ! -f "$REPO_DIR/CMakeLists.txt" ]]; then
    echo "[qemu-guest-build] ERROR: $REPO_DIR tidak berisi CMakeLists.txt" >&2
    echo "  repo dir   : $REPO_DIR" >&2
    echo "  build dir  : $BUILD_DIR" >&2
    echo "  user       : $(id -un)" >&2
    if mountpoint -q "$REPO_DIR" 2>/dev/null; then
        echo "  mount      : mounted" >&2
    else
        echo "  mount      : not mounted" >&2
    fi
    echo "  hint       : pastikan hostshare ter-mount ke repo root, bukan ke subdir scripts/" >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"

if [[ "$BUILD_ONLY" != "1" ]]; then
    echo "[qemu-guest-build] Configuring"
    echo "  repo      : $REPO_DIR"
    echo "  build dir : $BUILD_DIR"
    echo "  generator : $GENERATOR"
    echo "  type      : $BUILD_TYPE"
    echo "  prefix    : ${CMAKE_PREFIX_PATH:-<unset>}"
    CONFIGURE_ARGS=(-S "$REPO_DIR" -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE")
    if [[ -n "$CMAKE_PREFIX_PATH" ]]; then
        CONFIGURE_ARGS+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
    fi
    cmake "${CONFIGURE_ARGS[@]}"
fi

if [[ "$CONFIGURE_ONLY" != "1" ]]; then
    echo "[qemu-guest-build] Building"
    echo "  targets : ${TARGETS[*]}"
    echo "  jobs    : $JOBS"
    BUILD_ARGS=(--build "$BUILD_DIR" -j "$JOBS")
    for t in "${TARGETS[@]}"; do
        BUILD_ARGS+=(--target "$t")
    done
    cmake "${BUILD_ARGS[@]}"
fi
