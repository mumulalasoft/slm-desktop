#!/usr/bin/env bash
# scripts/dev/qemu-build-remote.sh — Build project di guest via SSH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
GUEST_BUILD_SCRIPT="$SCRIPT_DIR/qemu-guest-build.sh"
REMOTE_BUILD_SCRIPT="/tmp/qemu-guest-build.sh"
REMOTE_BOOTSTRAP_SCRIPT="/tmp/qemu-guest-bootstrap.sh"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-build-remote.sh [options] [-- guest-build args]

Options:
  --user USER         Guest username. Default: $SSH_USER
  --port PORT         Host forwarded SSH port. Default: $SSH_PORT
  --help              Show this help

Everything after -- is passed to scripts/dev/qemu-guest-build.sh inside the guest.
EOF
}

PASSTHRU_ARGS=()
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
        --help|-h)
            usage
            exit 0
            ;;
        --)
            shift
            PASSTHRU_ARGS=("$@")
            break
            ;;
        *)
            PASSTHRU_ARGS+=("$1")
            shift
            ;;
    esac
done

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$GUEST_BUILD_SCRIPT" \
    "$REMOTE_BUILD_SCRIPT"

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$SCRIPT_DIR/qemu-guest-bootstrap.sh" \
    "$REMOTE_BOOTSTRAP_SCRIPT"

REMOTE_CMD="chmod +x '$REMOTE_BUILD_SCRIPT' '$REMOTE_BOOTSTRAP_SCRIPT' && sudo '$REMOTE_BOOTSTRAP_SCRIPT' --mount-only && '$REMOTE_BUILD_SCRIPT'"
for arg in "${PASSTHRU_ARGS[@]}"; do
    REMOTE_CMD+=" $(printf '%q' "$arg")"
done

exec "$SCRIPT_DIR/qemu-ssh.sh" \
    --tty \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$REMOTE_CMD"
