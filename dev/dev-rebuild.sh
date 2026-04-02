#!/usr/bin/env bash
# dev/dev-rebuild.sh — Hot rebuild: filemanager core → relink desktop shell.
#
# Cara pakai:
#   bash dev/dev-rebuild.sh              # rebuild keduanya
#   bash dev/dev-rebuild.sh --fm-only    # hanya rebuild filemanager
#   bash dev/dev-rebuild.sh --shell-only # hanya relink desktop shell
#   bash dev/dev-rebuild.sh --jobs 8     # override jumlah parallel jobs

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SLM_ENV_QUIET=1 source "$SCRIPT_DIR/workspace.env"

JOBS="${JOBS:-$(nproc)}"
BUILD_FM=1
BUILD_SHELL=1

for arg in "$@"; do
    case "$arg" in
        --fm-only)    BUILD_SHELL=0 ;;
        --shell-only) BUILD_FM=0 ;;
        --jobs)       shift; JOBS="$1" ;;
        --jobs=*)     JOBS="${arg#--jobs=}" ;;
    esac
done

# ── Build filemanager core ────────────────────────────────────────────────────
if [[ "$BUILD_FM" == "1" ]]; then
    if [[ ! -d "$SLM_FILEMANAGER_BUILD_DIR" ]]; then
        echo "[dev-rebuild] Build dir filemanager tidak ada: $SLM_FILEMANAGER_BUILD_DIR"
        echo "  Jalankan setup awal dulu:"
        echo "  cmake -B $SLM_FILEMANAGER_BUILD_DIR -S $SLM_FILEMANAGER_SOURCE_DIR -DCMAKE_BUILD_TYPE=Debug"
        exit 1
    fi
    echo "[dev-rebuild] Building filemanager core (jobs=$JOBS)..."
    cmake --build "$SLM_FILEMANAGER_BUILD_DIR" \
        --target slm-filemanager-core \
        -j "$JOBS"
    echo "[dev-rebuild] Filemanager core OK."
fi

# ── Relink desktop shell ──────────────────────────────────────────────────────
if [[ "$BUILD_SHELL" == "1" ]]; then
    if [[ ! -d "$SLM_DESKTOP_BUILD_DIR" ]]; then
        echo "[dev-rebuild] Build dir desktop tidak ada: $SLM_DESKTOP_BUILD_DIR"
        echo "  Jalankan setup awal dulu:"
        echo "  cmake -B $SLM_DESKTOP_BUILD_DIR -S . -DCMAKE_BUILD_TYPE=Debug -DSLM_USE_LOCAL_FILEMANAGER=ON"
        exit 1
    fi
    echo "[dev-rebuild] Relinking desktop shell (jobs=$JOBS)..."
    cmake --build "$SLM_DESKTOP_BUILD_DIR" \
        --target appSlm_Desktop \
        -j "$JOBS"
    echo "[dev-rebuild] Desktop shell OK."
fi

echo ""
echo "[dev-rebuild] Selesai. Restart slm-desktop untuk memuat perubahan."
if pgrep -f "appSlm_Desktop" > /dev/null 2>&1; then
    echo "  Hint: pkill appSlm_Desktop && bash dev/run-dev.sh"
fi
