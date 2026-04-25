#!/usr/bin/env bash
# dev/qemu-scp.sh — Helper SCP ke Ubuntu guest via port forwarding QEMU.

set -euo pipefail

STATE_DIR="${SLM_QEMU_STATE_DIR:-$HOME/.local/state/slm-qemu}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
KNOWN_HOSTS="$STATE_DIR/known_hosts"

usage() {
    cat <<EOF
Usage: bash dev/qemu-scp.sh [--user USER] [--port PORT] <src> <dst>

Defaults:
  user : $SSH_USER
  port : $SSH_PORT
EOF
}

POSITIONAL=()
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
            POSITIONAL+=("$1")
            shift
            ;;
    esac
done

if [[ "${#POSITIONAL[@]}" -ne 2 ]]; then
    usage >&2
    exit 1
fi

mkdir -p "$STATE_DIR"

exec scp \
    -o StrictHostKeyChecking=accept-new \
    -o UserKnownHostsFile="$KNOWN_HOSTS" \
    -P "$SSH_PORT" \
    "${POSITIONAL[0]}" \
    "$SSH_USER@127.0.0.1:${POSITIONAL[1]}"
