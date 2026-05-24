#!/usr/bin/env bash
# scripts/dev/desktop-run-dev.sh — Launcher desktop shell untuk sesi development.
#
# Cara pakai:
#   bash scripts/dev/desktop-run-dev.sh [--no-fileopsd] [extra args untuk slm-desktop]
#
# Contoh:
#   bash scripts/dev/desktop-run-dev.sh
#   bash scripts/dev/desktop-run-dev.sh --no-fileopsd
#   QT_LOGGING_RULES="slm.filemanager.*=true" bash scripts/dev/desktop-run-dev.sh

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Load workspace environment
SLM_ENV_QUIET=1 source "$SCRIPT_DIR/workspace.env"

# ── Parse args ────────────────────────────────────────────────────────────────
START_FILEOPSD=1
EXTRA_ARGS=()
for arg in "$@"; do
    case "$arg" in
        --no-fileopsd) START_FILEOPSD=0 ;;
        *) EXTRA_ARGS+=("$arg") ;;
    esac
done

# ── Resolve desktop binary ────────────────────────────────────────────────────
DESKTOP_BINARY="$SLM_DESKTOP_BUILD_DIR/slm-desktop"
if [[ ! -x "$DESKTOP_BINARY" ]]; then
    echo "[desktop-run-dev] ERROR: binary desktop tidak ditemukan. Build dulu:"
    echo "  cmake --build $SLM_DESKTOP_BUILD_DIR --target slm-desktop"
    exit 1
fi

# ── Start fileopsd lokal bila diminta ─────────────────────────────────────────
FILEOPSD_PID=""
if [[ "$START_FILEOPSD" == "1" ]]; then
    if [[ -x "${SLM_FILEOPSD_BINARY:-}" ]]; then
        if ! pgrep -f "slm-fileopsd" > /dev/null 2>&1; then
            echo "[desktop-run-dev] Starting local fileopsd: $SLM_FILEOPSD_BINARY"
            QT_LOGGING_RULES="slm.fileops*=true" \
                "$SLM_FILEOPSD_BINARY" 2>&1 | sed 's/^/[fileopsd] /' &
            FILEOPSD_PID=$!
            sleep 0.3  # tunggu DBus registration
        else
            echo "[desktop-run-dev] fileopsd sudah running, skip start."
        fi
    else
        echo "[desktop-run-dev] WARN: SLM_FILEOPSD_BINARY tidak ditemukan, skip fileopsd."
    fi
fi

# ── Cleanup on exit ───────────────────────────────────────────────────────────
cleanup() {
    if [[ -n "$FILEOPSD_PID" ]]; then
        echo "[desktop-run-dev] Stopping fileopsd ($FILEOPSD_PID)..."
        kill "$FILEOPSD_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ── Log file ──────────────────────────────────────────────────────────────────
LOG_FILE="/tmp/slm-dev-$(date +%Y%m%d-%H%M%S).log"

echo "[desktop-run-dev] Desktop binary : $DESKTOP_BINARY"
echo "[desktop-run-dev] Log file       : $LOG_FILE"
echo "[desktop-run-dev] QT_LOGGING_RULES: ${QT_LOGGING_RULES:-<not set>}"
echo ""

# ── Run desktop shell ─────────────────────────────────────────────────────────
exec "$DESKTOP_BINARY" \
    "${EXTRA_ARGS[@]}" \
    2>&1 | tee "$LOG_FILE"
