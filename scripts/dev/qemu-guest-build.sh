#!/usr/bin/env bash
# scripts/dev/qemu-guest-build.sh — Configure/build Desktop Shell di Ubuntu guest.

set -euo pipefail

REPO_DIR="${SLM_QEMU_GUEST_REPO_DIR:-/mnt/hostshare}"
BUILD_DIR="${SLM_QEMU_GUEST_BUILD_DIR:-$HOME/.cache/slm-qemu/build/dev}"
GENERATOR="${SLM_QEMU_GUEST_GENERATOR:-Ninja}"
BUILD_TYPE="${SLM_QEMU_GUEST_BUILD_TYPE:-Debug}"
TARGET="${SLM_QEMU_GUEST_TARGET:-appSlm_Desktop}"
JOBS="${SLM_QEMU_GUEST_JOBS:-$(nproc)}"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-guest-build.sh [options]

Options:
  --repo-dir PATH    Repo path in guest. Default: $REPO_DIR
  --build-dir PATH   Build directory. Default: $BUILD_DIR
  --generator NAME   CMake generator. Default: $GENERATOR
  --type NAME        CMAKE_BUILD_TYPE. Default: $BUILD_TYPE
  --target NAME      Build target. Default: $TARGET
  --jobs N           Parallel build jobs. Default: $JOBS
  --configure-only   Only run cmake configure
  --build-only       Skip configure and only build
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
            TARGET="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
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
    cmake -S "$REPO_DIR" -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

if [[ "$CONFIGURE_ONLY" != "1" ]]; then
    echo "[qemu-guest-build] Building"
    echo "  target : $TARGET"
    echo "  jobs   : $JOBS"
    cmake --build "$BUILD_DIR" --target "$TARGET" -j "$JOBS"
fi
