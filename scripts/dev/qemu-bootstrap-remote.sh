#!/usr/bin/env bash
# scripts/dev/qemu-bootstrap-remote.sh — Jalankan bootstrap guest dari host via SSH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
BOOTSTRAP_SCRIPT="$SCRIPT_DIR/qemu-guest-bootstrap.sh"
REMOTE_SCRIPT="/tmp/qemu-guest-bootstrap.sh"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-bootstrap-remote.sh [options]

Options:
  --user USER         Guest username. Default: $SSH_USER
  --port PORT         Host forwarded SSH port. Default: $SSH_PORT
  --skip-apt          Pass through to guest bootstrap
  --skip-mount        Pass through to guest bootstrap
  --apt-only          Pass through to guest bootstrap
  --mount-only        Pass through to guest bootstrap
  --help              Show this help
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
        --skip-apt|--skip-mount|--apt-only|--mount-only)
            PASSTHRU_ARGS+=("$1")
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-bootstrap-remote] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

echo "[qemu-bootstrap-remote] Running guest bootstrap over SSH"
echo "  user       : $SSH_USER"
echo "  port       : $SSH_PORT"
echo "  script     : $BOOTSTRAP_SCRIPT"
echo ""

REMOTE_ARGS=()
for arg in "${PASSTHRU_ARGS[@]}"; do
    REMOTE_ARGS+=("$(printf '%q' "$arg")")
done

"$SCRIPT_DIR/qemu-ssh.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "true" >/dev/null

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$BOOTSTRAP_SCRIPT" \
    "$REMOTE_SCRIPT"

REMOTE_CMD="chmod +x '$REMOTE_SCRIPT' && sudo '$REMOTE_SCRIPT'"
for arg in "${PASSTHRU_ARGS[@]}"; do
    REMOTE_CMD+=" $(printf '%q' "$arg")"
done

exec "$SCRIPT_DIR/qemu-ssh.sh" \
    --tty \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$REMOTE_CMD"
