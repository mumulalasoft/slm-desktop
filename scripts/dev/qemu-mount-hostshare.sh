#!/usr/bin/env bash
# scripts/dev/qemu-mount-hostshare.sh — Mount repo hostshare ke guest QEMU.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"

SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
HOSTSHARE_PATH="${SLM_QEMU_HOSTSHARE_PATH:-/mnt/hostshare}"
MOUNT_TAG="${SLM_QEMU_HOSTSHARE_TAG:-hostshare}"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-mount-hostshare.sh [options]

Options:
  --user USER       Guest SSH user. Default: $SSH_USER
  --port PORT       Host forwarded SSH port. Default: $SSH_PORT
  --path PATH       Guest mount point. Default: $HOSTSHARE_PATH
  --tag TAG         QEMU 9p mount tag. Default: $MOUNT_TAG
  --help            Show this help

Env overrides:
  SLM_QEMU_SSH_USER, SLM_QEMU_SSH_PORT, SLM_QEMU_HOSTSHARE_PATH,
  SLM_QEMU_HOSTSHARE_TAG, SLM_QEMU_STATE_DIR
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
        --path)
            HOSTSHARE_PATH="$2"
            shift 2
            ;;
        --tag)
            MOUNT_TAG="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-mount-hostshare] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$HOSTSHARE_PATH" || "$HOSTSHARE_PATH" != /* ]]; then
    echo "[qemu-mount-hostshare] ERROR: --path harus absolute: $HOSTSHARE_PATH" >&2
    exit 1
fi

echo "[qemu-mount-hostshare] Mounting hostshare in guest"
echo "  user : $SSH_USER"
echo "  port : $SSH_PORT"
echo "  tag  : $MOUNT_TAG"
echo "  path : $HOSTSHARE_PATH"
echo ""

REMOTE_SCRIPT="/tmp/qemu-mount-hostshare.sh"
TMP_SCRIPT="$(mktemp -t qemu-mount-hostshare.XXXXXX.sh)"
cleanup() {
    rm -f "$TMP_SCRIPT"
}
trap cleanup EXIT

cat >"$TMP_SCRIPT" <<'GUEST_SCRIPT'
#!/usr/bin/env bash
set -u

mount_path="${1:-/mnt/hostshare}"
mount_tag="${2:-hostshare}"

echo "[guest] mount hostshare diagnostic"
echo "  user       : $(id -un)"
echo "  uid        : $(id -u)"
echo "  kernel     : $(uname -r)"
echo "  mount tag  : $mount_tag"
echo "  mount path : $mount_path"
echo ""

echo "[guest] checking 9p kernel support"
if grep -qw 9p /proc/filesystems; then
    echo "  /proc/filesystems: 9p available"
else
    echo "  /proc/filesystems: 9p not listed, trying modprobe"
    modprobe 9p 2>/dev/null || true
    modprobe 9pnet 2>/dev/null || true
    modprobe 9pnet_virtio 2>/dev/null || true
fi

if grep -qw 9p /proc/filesystems; then
    echo "  result: 9p available"
else
    echo "  result: 9p still unavailable"
fi
echo ""

echo "[guest] visible 9p mount tags"
tag_found=0
while IFS= read -r tag_file; do
    tag_value="$(cat "$tag_file" 2>/dev/null || true)"
    echo "  $tag_file: $tag_value"
    if [[ "$tag_value" == "$mount_tag" ]]; then
        tag_found=1
    fi
done < <(find /sys/bus/virtio/drivers/9pnet_virtio -name mount_tag -print 2>/dev/null)
if [[ "$tag_found" == "0" ]]; then
    echo "  WARN: mount tag '$mount_tag' tidak terlihat di guest."
    echo "        Pastikan VM dijalankan dengan scripts/dev/qemu-run.sh yang membawa -virtfs mount_tag=$mount_tag."
fi
echo ""

mkdir -p "$mount_path"
if mountpoint -q "$mount_path"; then
    echo "[guest] already mounted"
    findmnt "$mount_path" || true
    exit 0
fi

echo "[guest] mounting"
set +e
mount -t 9p -o trans=virtio,version=9p2000.L,msize=262144,access=any "$mount_tag" "$mount_path"
rc=$?
set -e
if [[ "$rc" -ne 0 ]]; then
    echo "[guest] ERROR: mount failed rc=$rc"
    echo ""
    echo "[guest] recent kernel messages"
    dmesg 2>/dev/null | tail -40 || true
    echo ""
    echo "[guest] diagnosis hints"
    echo "  - 'unknown filesystem type 9p': kernel module 9p/9pnet_virtio belum tersedia."
    echo "  - 'No such device' atau tag tidak terlihat: QEMU belum expose -virtfs hostshare; restart VM via qemu-run.sh."
    echo "  - permission sudo gagal: jalankan script ini dan masukkan password sudo guest."
    exit "$rc"
fi

echo "[guest] mounted: $mount_tag -> $mount_path"
findmnt "$mount_path" || true
GUEST_SCRIPT
chmod +x "$TMP_SCRIPT"

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$TMP_SCRIPT" \
    "$REMOTE_SCRIPT"

HOSTSHARE_PATH_Q="$(printf '%q' "$HOSTSHARE_PATH")"
MOUNT_TAG_Q="$(printf '%q' "$MOUNT_TAG")"
REMOTE_CMD="chmod +x '$REMOTE_SCRIPT' && sudo '$REMOTE_SCRIPT' $HOSTSHARE_PATH_Q $MOUNT_TAG_Q"

exec "$SCRIPT_DIR/qemu-ssh.sh" \
    --tty \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$REMOTE_CMD"
