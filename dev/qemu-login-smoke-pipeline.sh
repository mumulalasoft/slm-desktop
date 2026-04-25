#!/usr/bin/env bash
# dev/qemu-login-smoke-pipeline.sh — Build, install runtime, verify, lalu opsional jalankan smoke login/session.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SESSION_USER="${SLM_QEMU_SESSION_USER:-$SSH_USER}"
REPO_DIR="${SLM_QEMU_GUEST_REPO_DIR:-/mnt/hostshare}"
BUILD_DIR="${SLM_QEMU_GUEST_BUILD_DIR:-/home/${SSH_USER}/.cache/slm-qemu/build/dev}"
JOBS="${SLM_QEMU_GUEST_JOBS:-$(nproc)}"
RUN_SMOKE=1
SMOKE_TIMEOUT="${SLM_QEMU_SESSION_SMOKE_TIMEOUT_SEC:-90}"
BUILD_ONLY=0

REMOTE_BUILD_SCRIPT="/tmp/qemu-guest-build.sh"
REMOTE_BOOTSTRAP_SCRIPT="/tmp/qemu-guest-bootstrap.sh"

RUNTIME_TARGETS=(
    slm-greeter
    slm-watchdog
    slm-recovery-app
    slm-session-broker
    appSlm_Desktop
    desktopd
    slm-svcmgrd
    slm-loggerd
    slm-portald
    slm-fileopsd
    slm-devicesd
    slm-clipboardd
    slm-polkit-agent
    slm-envd
    slm-recoveryd
)

usage() {
    cat <<EOF
Usage: bash dev/qemu-login-smoke-pipeline.sh [options]

Options:
  --user USER           SSH user guest. Default: $SSH_USER
  --port PORT           SSH port host->guest. Default: $SSH_PORT
  --session-user USER   Desktop session user. Default: $SESSION_USER
  --repo-dir PATH       Repo path in guest. Default: $REPO_DIR
  --build-dir PATH      Build dir in guest. Default: $BUILD_DIR
  --jobs N              Parallel build jobs. Default: $JOBS
  --timeout SEC         Timeout smoke phase. Default: $SMOKE_TIMEOUT
  --build-only          Lewati configure, langsung cmake --build
  --skip-smoke          Hanya build + install + verify
  --help                Show this help

Catatan:
  Smoke session butuh guest benar-benar masuk ke sesi greetd/autologin/test-login.
  Bila belum ada login sesudah install runtime, pakai --skip-smoke lalu reboot/login dulu.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)
            SSH_USER="$2"
            shift 2
            ;;
        --port)
            SSH_PORT="$2"
            shift 2
            ;;
        --session-user)
            SESSION_USER="$2"
            shift 2
            ;;
        --repo-dir)
            REPO_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --timeout)
            SMOKE_TIMEOUT="$2"
            shift 2
            ;;
        --build-only)
            BUILD_ONLY=1
            shift
            ;;
        --skip-smoke)
            RUN_SMOKE=0
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-login-smoke-pipeline] unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

echo "[qemu-login-smoke-pipeline] Preparing guest"
echo "  ssh user      : $SSH_USER"
echo "  session user  : $SESSION_USER"
echo "  repo dir      : $REPO_DIR"
echo "  build dir     : $BUILD_DIR"
echo "  jobs          : $JOBS"
echo "  build only    : $BUILD_ONLY"
echo "  run smoke     : $RUN_SMOKE"
echo ""

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$SCRIPT_DIR/qemu-guest-build.sh" \
    "$REMOTE_BUILD_SCRIPT"

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$SCRIPT_DIR/qemu-guest-bootstrap.sh" \
    "$REMOTE_BOOTSTRAP_SCRIPT"

REMOTE_CMD="chmod +x '$REMOTE_BUILD_SCRIPT' '$REMOTE_BOOTSTRAP_SCRIPT'"
REMOTE_CMD+=" && sudo '$REMOTE_BOOTSTRAP_SCRIPT' --mount-only"
if [[ "$BUILD_ONLY" == "1" ]]; then
    REMOTE_CMD+=" && '$REMOTE_BUILD_SCRIPT' --repo-dir $(printf '%q' "$REPO_DIR") --build-dir $(printf '%q' "$BUILD_DIR") --build-only"
else
    REMOTE_CMD+=" && '$REMOTE_BUILD_SCRIPT' --repo-dir $(printf '%q' "$REPO_DIR") --build-dir $(printf '%q' "$BUILD_DIR") --configure-only"
fi
REMOTE_CMD+=" && cmake --build $(printf '%q' "$BUILD_DIR") --target"
for target in "${RUNTIME_TARGETS[@]}"; do
    REMOTE_CMD+=" $(printf '%q' "$target")"
done
REMOTE_CMD+=" -j $(printf '%q' "$JOBS")"
REMOTE_CMD+=" && sudo SLM_TARGET_USER=$(printf '%q' "$SESSION_USER") bash $(printf '%q' "$REPO_DIR/scripts/login/install-slm-desktop-runtime.sh") $(printf '%q' "$BUILD_DIR")"
REMOTE_CMD+=" && sudo SLM_TARGET_USER=$(printf '%q' "$SESSION_USER") bash $(printf '%q' "$REPO_DIR/scripts/login/verify-slm-desktop-runtime.sh")"
REMOTE_CMD+=" && sudo bash $(printf '%q' "$REPO_DIR/scripts/login/verify-greetd-slm.sh")"

"$SCRIPT_DIR/qemu-ssh.sh" \
    --tty \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$REMOTE_CMD"

if [[ "$RUN_SMOKE" -eq 1 ]]; then
    exec "$SCRIPT_DIR/qemu-session-smoke-remote.sh" \
        --user "$SSH_USER" \
        --port "$SSH_PORT" \
        --session-user "$SESSION_USER" \
        --timeout "$SMOKE_TIMEOUT"
fi

echo "[qemu-login-smoke-pipeline] install+verify selesai"
echo "[qemu-login-smoke-pipeline] smoke dilewati (--skip-smoke)."
