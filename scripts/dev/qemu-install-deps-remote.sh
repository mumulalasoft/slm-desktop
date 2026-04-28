#!/usr/bin/env bash
# scripts/dev/qemu-install-deps-remote.sh — Install dependency dev di guest via SSH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
BOOTSTRAP_SCRIPT="$SCRIPT_DIR/qemu-guest-bootstrap.sh"
REMOTE_SCRIPT="/tmp/qemu-guest-bootstrap.sh"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-install-deps-remote.sh [options]

Options:
  --user USER         Guest username. Default: $SSH_USER
  --port PORT         Host forwarded SSH port. Default: $SSH_PORT
  --help              Show this help
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
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-install-deps-remote] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$BOOTSTRAP_SCRIPT" \
    "$REMOTE_SCRIPT"

"$SCRIPT_DIR/qemu-ssh.sh" \
    --tty \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "chmod +x '$REMOTE_SCRIPT' && sudo '$REMOTE_SCRIPT' --apt-only"
