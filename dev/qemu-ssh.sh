#!/usr/bin/env bash
# dev/qemu-ssh.sh — Helper SSH ke Ubuntu guest via port forwarding QEMU.

set -euo pipefail

STATE_DIR="${SLM_QEMU_STATE_DIR:-$HOME/.local/state/slm-qemu}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
KNOWN_HOSTS="$STATE_DIR/known_hosts"

usage() {
    cat <<EOF
Usage: bash dev/qemu-ssh.sh [--user USER] [--port PORT] [--tty] [ssh args...]

Defaults:
  user : $SSH_USER
  port : $SSH_PORT
EOF
}

EXTRA_ARGS=()
FORCE_TTY=0
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
        --tty|-t)
            FORCE_TTY=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

mkdir -p "$STATE_DIR"

SSH_ARGS=()
if [[ "$FORCE_TTY" == "1" ]]; then
    SSH_ARGS+=(-tt)
fi

exec ssh \
    "${SSH_ARGS[@]}" \
    -o StrictHostKeyChecking=accept-new \
    -o UserKnownHostsFile="$KNOWN_HOSTS" \
    -p "$SSH_PORT" \
    "$SSH_USER@127.0.0.1" \
    "${EXTRA_ARGS[@]}"
