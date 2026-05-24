#!/usr/bin/env bash
# scripts/dev/qemu-create-disk.sh — Siapkan disk qcow2 untuk VM development lokal.

set -euo pipefail

STATE_DIR="${SLM_QEMU_STATE_DIR:-$HOME/.local/state/slm-qemu}"
DISK_PATH="${SLM_QEMU_DISK:-$STATE_DIR/ubuntu-dev.qcow2}"
DISK_SIZE="${SLM_QEMU_DISK_SIZE:-64G}"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-create-disk.sh [--disk PATH] [--size SIZE] [--force]

Options:
  --disk PATH   Override path qcow2. Default: $DISK_PATH
  --size SIZE   Disk size for new image. Default: $DISK_SIZE
  --force       Recreate disk if file already exists
  --help        Show this help
EOF
}

FORCE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --disk)
            DISK_PATH="$2"
            shift 2
            ;;
        --size)
            DISK_SIZE="$2"
            shift 2
            ;;
        --force)
            FORCE=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-create-disk] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v qemu-img >/dev/null 2>&1; then
    echo "[qemu-create-disk] ERROR: qemu-img tidak ditemukan." >&2
    exit 1
fi

mkdir -p "$(dirname "$DISK_PATH")"

if [[ -e "$DISK_PATH" && "$FORCE" != "1" ]]; then
    echo "[qemu-create-disk] Disk sudah ada: $DISK_PATH"
    echo "[qemu-create-disk] Gunakan --force jika ingin recreate."
    exit 0
fi

if [[ -e "$DISK_PATH" && "$FORCE" == "1" ]]; then
    rm -f "$DISK_PATH"
fi

echo "[qemu-create-disk] Creating qcow2 disk"
echo "  path : $DISK_PATH"
echo "  size : $DISK_SIZE"
qemu-img create -f qcow2 "$DISK_PATH" "$DISK_SIZE" >/dev/null

echo "[qemu-create-disk] OK"
