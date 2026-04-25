#!/usr/bin/env bash
# dev/qemu-run.sh — Jalankan Ubuntu VM untuk development Desktop Shell.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SLM_ENV_QUIET=1 source "$SCRIPT_DIR/workspace.env"

STATE_DIR="${SLM_QEMU_STATE_DIR:-$HOME/.local/state/slm-qemu}"
ISO_PATH="${SLM_QEMU_ISO:-$HOME/ubuntu.iso}"
DISK_PATH="${SLM_QEMU_DISK:-$STATE_DIR/ubuntu-dev.qcow2}"
DEFAULT_SHARED_DIR="$(cd "$SLM_DESKTOP_SOURCE_DIR" && pwd)"
SHARED_DIR="${SLM_QEMU_SHARED_DIR:-$DEFAULT_SHARED_DIR}"
MEMORY_MB="${SLM_QEMU_MEMORY_MB:-8192}"
CPU_COUNT="${SLM_QEMU_CPUS:-4}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
DISPLAY_BACKEND="${SLM_QEMU_DISPLAY:-gtk}"
BOOT_MODE="auto"
ATTACH_ISO=0
HEADLESS=0
RESET_UEFI_VARS=0
ENABLE_CLIPBOARD=1

usage() {
    cat <<EOF
Usage: bash dev/qemu-run.sh [options]

Options:
  --iso PATH            Attach ISO from PATH and use it as cdrom
  --disk PATH           Override qcow2 path. Default: $DISK_PATH
  --shared-dir PATH     Host directory shared to guest. Default: $SHARED_DIR
  --memory MB           Guest memory in MB. Default: $MEMORY_MB
  --cpus N              Guest CPU count. Default: $CPU_COUNT
  --ssh-port PORT       Host TCP port forwarded to guest:22. Default: $SSH_PORT
  --headless            Run without graphical window
  --with-iso            Attach default ISO path: $ISO_PATH
  --no-clipboard        Disable host<->guest clipboard sharing
  --bios                Force legacy BIOS boot
  --uefi                Force UEFI boot with OVMF
  --reset-uefi-vars     Recreate local OVMF vars file
  --help                Show this help

Env overrides:
  SLM_QEMU_ISO, SLM_QEMU_DISK, SLM_QEMU_SHARED_DIR, SLM_QEMU_MEMORY_MB,
  SLM_QEMU_CPUS, SLM_QEMU_SSH_PORT, SLM_QEMU_DISPLAY, SLM_QEMU_STATE_DIR
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --iso)
            ISO_PATH="$2"
            ATTACH_ISO=1
            shift 2
            ;;
        --disk)
            DISK_PATH="$2"
            shift 2
            ;;
        --shared-dir)
            SHARED_DIR="$2"
            shift 2
            ;;
        --memory)
            MEMORY_MB="$2"
            shift 2
            ;;
        --cpus)
            CPU_COUNT="$2"
            shift 2
            ;;
        --ssh-port)
            SSH_PORT="$2"
            shift 2
            ;;
        --headless)
            HEADLESS=1
            shift
            ;;
        --with-iso)
            ATTACH_ISO=1
            shift
            ;;
        --no-clipboard)
            ENABLE_CLIPBOARD=0
            shift
            ;;
        --bios)
            BOOT_MODE="bios"
            shift
            ;;
        --uefi)
            BOOT_MODE="uefi"
            shift
            ;;
        --reset-uefi-vars)
            RESET_UEFI_VARS=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-run] Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "[qemu-run] ERROR: qemu-system-x86_64 tidak ditemukan." >&2
    exit 1
fi

mkdir -p "$STATE_DIR"

if [[ ! -f "$DISK_PATH" ]]; then
    echo "[qemu-run] Disk belum ada: $DISK_PATH" >&2
    echo "[qemu-run] Jalankan: bash dev/qemu-create-disk.sh --disk \"$DISK_PATH\"" >&2
    exit 1
fi

if [[ "$ATTACH_ISO" == "1" && ! -f "$ISO_PATH" ]]; then
    echo "[qemu-run] ERROR: ISO tidak ditemukan: $ISO_PATH" >&2
    exit 1
fi

if [[ ! -d "$SHARED_DIR" ]]; then
    echo "[qemu-run] ERROR: shared dir tidak ditemukan: $SHARED_DIR" >&2
    exit 1
fi
SHARED_DIR="$(cd "$SHARED_DIR" && pwd)"

ACCEL_ARGS=("-machine" "type=q35")
CPU_MODEL="max"
if [[ -r /dev/kvm && -w /dev/kvm ]]; then
    ACCEL_ARGS=("-machine" "type=q35,accel=kvm")
    CPU_MODEL="host"
fi

OVMF_CODE=""
OVMF_VARS_TEMPLATE=""
for candidate in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/edk2/x64/OVMF_CODE.4m.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd; do
    if [[ -f "$candidate" ]]; then
        OVMF_CODE="$candidate"
        break
    fi
done
for candidate in \
    /usr/share/OVMF/OVMF_VARS_4M.fd \
    /usr/share/OVMF/OVMF_VARS.fd \
    /usr/share/edk2/x64/OVMF_VARS.4m.fd \
    /usr/share/edk2/ovmf/OVMF_VARS.fd; do
    if [[ -f "$candidate" ]]; then
        OVMF_VARS_TEMPLATE="$candidate"
        break
    fi
done

USE_UEFI=0
if [[ "$BOOT_MODE" == "uefi" ]]; then
    USE_UEFI=1
elif [[ "$BOOT_MODE" == "auto" && -n "$OVMF_CODE" && -n "$OVMF_VARS_TEMPLATE" ]]; then
    USE_UEFI=1
fi

QEMU_ARGS=(
    "${ACCEL_ARGS[@]}"
    -cpu "$CPU_MODEL"
    -smp "$CPU_COUNT"
    -m "$MEMORY_MB"
    -name "slm-dev-ubuntu"
    -rtc base=localtime
    -device virtio-balloon-pci
    -device virtio-rng-pci
    -device virtio-keyboard-pci
    -device virtio-mouse-pci
    -device intel-hda
    -device hda-duplex
    -device virtio-vga
    -drive "if=virtio,format=qcow2,file=$DISK_PATH"
    -nic "user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22"
    -virtfs "local,path=$SHARED_DIR,mount_tag=hostshare,security_model=none,id=hostshare"
)

if [[ "$HEADLESS" == "1" ]]; then
    QEMU_ARGS+=(-display none -serial mon:stdio)
else
    QEMU_ARGS+=(-display "$DISPLAY_BACKEND")
fi

if [[ "$ENABLE_CLIPBOARD" == "1" && "$HEADLESS" != "1" ]]; then
    if qemu-system-x86_64 -chardev help 2>/dev/null | grep -q 'qemu-vdagent'; then
        QEMU_ARGS+=(
            -device virtio-serial-pci
            -chardev qemu-vdagent,id=vdagent,name=vdagent,clipboard=on,mouse=off
            -device virtserialport,chardev=vdagent,name=com.redhat.spice.0
        )
    else
        echo "[qemu-run] WARN: qemu-vdagent backend tidak tersedia, clipboard sharing dinonaktifkan."
        ENABLE_CLIPBOARD=0
    fi
fi

if [[ "$ATTACH_ISO" == "1" ]]; then
    QEMU_ARGS+=(-drive "file=$ISO_PATH,media=cdrom,if=ide")
fi

if [[ "$USE_UEFI" == "1" ]]; then
    OVMF_VARS_LOCAL="$STATE_DIR/OVMF_VARS.fd"
    if [[ "$RESET_UEFI_VARS" == "1" || ! -f "$OVMF_VARS_LOCAL" ]]; then
        cp "$OVMF_VARS_TEMPLATE" "$OVMF_VARS_LOCAL"
    fi
    QEMU_ARGS+=(
        -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
        -drive "if=pflash,format=raw,file=$OVMF_VARS_LOCAL"
    )
else
    QEMU_ARGS+=(-boot menu=on)
fi

echo "[qemu-run] VM config"
echo "  disk       : $DISK_PATH"
if [[ "$ATTACH_ISO" == "1" ]]; then
    echo "  iso        : $ISO_PATH"
else
    echo "  iso        : <detached>"
fi
echo "  shared dir : $SHARED_DIR"
echo "  memory     : ${MEMORY_MB} MB"
echo "  cpus       : $CPU_COUNT"
echo "  ssh        : ssh -p $SSH_PORT user@127.0.0.1"
echo "  share tag  : hostshare"
if [[ "$ENABLE_CLIPBOARD" == "1" && "$HEADLESS" != "1" ]]; then
    echo "  clipboard  : enabled (qemu-vdagent)"
else
    echo "  clipboard  : disabled"
fi
if [[ "$USE_UEFI" == "1" ]]; then
    echo "  firmware   : UEFI ($OVMF_CODE)"
else
    echo "  firmware   : BIOS"
fi
echo ""
echo "[qemu-run] Mount shared repo di guest:"
echo "  sudo mkdir -p /mnt/hostshare"
echo "  sudo mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/hostshare"
echo ""

exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
