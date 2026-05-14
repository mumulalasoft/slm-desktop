#!/usr/bin/env bash
# scripts/dev/qemu-setup-ssh-key.sh
# Bootstrap SSH public-key auth to QEMU guest once, then all tooling can be non-interactive.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"

SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
STATE_DIR="$(qemu_dev_state_dir)"
KNOWN_HOSTS="$(qemu_dev_known_hosts)"
IDENTITY_FILE="$(qemu_dev_identity_file)"
PUBLIC_KEY_FILE="${IDENTITY_FILE}.pub"
TIMEOUT_SEC="${SLM_QEMU_SSH_BOOTSTRAP_TIMEOUT_SEC:-90}"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-setup-ssh-key.sh [options]

Options:
  --user USER           SSH user (default: $SSH_USER)
  --port PORT           SSH port (default: $SSH_PORT)
  --identity-file PATH  Private key path (default: $IDENTITY_FILE)
  --timeout SEC         Wait timeout for sshd (default: $TIMEOUT_SEC)
  --help                Show help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user) SSH_USER="$2"; shift 2 ;;
        --port) SSH_PORT="$2"; shift 2 ;;
        --identity-file) IDENTITY_FILE="$2"; PUBLIC_KEY_FILE="${2}.pub"; shift 2 ;;
        --timeout) TIMEOUT_SEC="$2"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "[qemu-setup-ssh-key] unknown arg: $1" >&2; usage >&2; exit 1 ;;
    esac
done

mkdir -p "$STATE_DIR"

if [[ ! -f "$IDENTITY_FILE" ]]; then
    echo "[qemu-setup-ssh-key] generating key: $IDENTITY_FILE"
    ssh-keygen -t ed25519 -N "" -f "$IDENTITY_FILE" >/dev/null
fi
if [[ ! -f "$PUBLIC_KEY_FILE" ]]; then
    echo "[qemu-setup-ssh-key] missing public key: $PUBLIC_KEY_FILE" >&2
    exit 1
fi

echo "[qemu-setup-ssh-key] waiting guest ssh on port $SSH_PORT..."
if ! qemu_dev_wait_ssh "$SSH_PORT" "$TIMEOUT_SEC"; then
    echo "[qemu-setup-ssh-key] ERROR: guest ssh not ready" >&2
    exit 1
fi

TARGET="${SSH_USER}@127.0.0.1"
BASE_OPTS=(
    -F /dev/null
    -o StrictHostKeyChecking=accept-new
    -o "UserKnownHostsFile=$KNOWN_HOSTS"
    -o IdentitiesOnly=yes
    -i "$IDENTITY_FILE"
    -p "$SSH_PORT"
)

if ssh "${BASE_OPTS[@]}" -o BatchMode=yes "$TARGET" "echo key-auth-ok" >/dev/null 2>&1; then
    echo "[qemu-setup-ssh-key] key auth already working"
    exit 0
fi

if [[ ! -e /dev/tty ]]; then
    echo "[qemu-setup-ssh-key] ERROR: key auth failed and no interactive tty for password login" >&2
    exit 1
fi

if ! command -v ssh-copy-id >/dev/null 2>&1; then
    echo "[qemu-setup-ssh-key] ERROR: ssh-copy-id not found on host" >&2
    exit 1
fi

echo "[qemu-setup-ssh-key] key auth failed, falling back to one-time password login..."
echo "[qemu-setup-ssh-key] enter guest password when prompted."
ssh-copy-id \
    -f \
    -i "$PUBLIC_KEY_FILE" \
    -o StrictHostKeyChecking=accept-new \
    -o "UserKnownHostsFile=$KNOWN_HOSTS" \
    -o PreferredAuthentications=publickey,password,keyboard-interactive \
    -o PubkeyAuthentication=yes \
    -o IdentitiesOnly=yes \
    -p "$SSH_PORT" \
    "$TARGET" </dev/tty >/dev/tty

ssh "${BASE_OPTS[@]}" -o BatchMode=yes "$TARGET" "echo key-auth-ok" >/dev/null
echo "[qemu-setup-ssh-key] success: non-interactive key auth is now enabled"
