#!/usr/bin/env bash
# scripts/dev/qemu-quick.sh — One-command dev workflow: start VM headless, wait SSH,
# optionally mount hostshare and/atau run remote build.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"
SLM_ENV_QUIET=1 source "$SCRIPT_DIR/workspace.env"

SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
MOUNT_HOSTSHARE=1
RUN_BUILD=0
SNAPSHOT=0
EXTRA_RUN_ARGS=()

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-quick.sh [options]

Jalankan VM headless, tunggu SSH siap, lalu mount hostshare. Semua flag
tambahan diteruskan ke qemu-run.sh.

Options:
  --no-mount        Jangan mount hostshare setelah VM siap
  --build           Jalankan qemu-build-remote.sh setelah VM siap
  --snapshot        Mode snapshot: disk writes dibuang saat VM berhenti
  --ssh-port PORT   SSH port. Default: $SSH_PORT
  --ssh-user USER   SSH user. Default: $SSH_USER
  --help            Tampilkan help ini
  [qemu-run opts]   Semua opsi lain diteruskan ke qemu-run.sh

Contoh:
  bash scripts/dev/qemu-quick.sh
  bash scripts/dev/qemu-quick.sh --snapshot --build
  bash scripts/dev/qemu-quick.sh --memory 4096 --cpus 2
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-mount)
            MOUNT_HOSTSHARE=0
            shift
            ;;
        --build)
            RUN_BUILD=1
            shift
            ;;
        --snapshot)
            SNAPSHOT=1
            shift
            ;;
        --ssh-port)
            SSH_PORT="$2"
            shift 2
            ;;
        --ssh-user)
            SSH_USER="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            EXTRA_RUN_ARGS+=("$1")
            shift
            ;;
    esac
done

RUN_ARGS=(
    --headless
    --wait-ssh
    --ssh-port "$SSH_PORT"
    --ssh-user "$SSH_USER"
)
if [[ "$SNAPSHOT" == "1" ]]; then
    RUN_ARGS+=(--snapshot)
fi
RUN_ARGS+=("${EXTRA_RUN_ARGS[@]}")

echo "[qemu-quick] Starting VM..."
bash "$SCRIPT_DIR/qemu-run.sh" "${RUN_ARGS[@]}"

if [[ "$MOUNT_HOSTSHARE" == "1" ]]; then
    echo ""
    echo "[qemu-quick] Mounting hostshare..."
    bash "$SCRIPT_DIR/qemu-mount-hostshare.sh" \
        --user "$SSH_USER" \
        --port "$SSH_PORT"
fi

if [[ "$RUN_BUILD" == "1" ]]; then
    echo ""
    echo "[qemu-quick] Running remote build..."
    bash "$SCRIPT_DIR/qemu-build-remote.sh" \
        --user "$SSH_USER" \
        --port "$SSH_PORT"
fi

echo ""
echo "[qemu-quick] Done. Connect:"
echo "  ssh -p $SSH_PORT $SSH_USER@127.0.0.1"
echo "  bash scripts/dev/qemu-ssh.sh"
