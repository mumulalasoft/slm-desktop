#!/usr/bin/env bash
# scripts/dev/qemu-run.sh — Jalankan Ubuntu VM untuk development Desktop Shell.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"
SLM_ENV_QUIET=1 source "$SCRIPT_DIR/workspace.env"

STATE_DIR="$(qemu_dev_state_dir)"
ISO_PATH="${SLM_QEMU_ISO:-$HOME/ubuntu.iso}"
DISK_PATH="${SLM_QEMU_DISK:-$STATE_DIR/ubuntu-dev.qcow2}"
DEFAULT_SHARED_DIR="$(cd "$SLM_DESKTOP_SOURCE_DIR" && pwd)"
SHARED_DIR="${SLM_QEMU_SHARED_DIR:-$DEFAULT_SHARED_DIR}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PASSWORD="${SLM_QEMU_SSH_PASSWORD:-}"
DISPLAY_BACKEND="${SLM_QEMU_DISPLAY:-gtk}"
POINTER_MODE="${SLM_QEMU_POINTER:-tablet}"
HOSTSHARE_PATH="${SLM_QEMU_HOSTSHARE_PATH:-/mnt/hostshare}"
AUTO_MOUNT_HOSTSHARE="${SLM_QEMU_AUTO_MOUNT_HOSTSHARE:-0}"
PERFORMANCE_PROFILE="${SLM_QEMU_PROFILE:-performance}"
ENABLE_GL="${SLM_QEMU_GL:-1}"
DISK_CACHE="${SLM_QEMU_DISK_CACHE:-none}"
DISK_AIO="${SLM_QEMU_DISK_AIO:-native}"
DISK_IO="${SLM_QEMU_DISK_IO:-iothread}"
BOOT_MODE="auto"
ATTACH_ISO=0
HEADLESS=0
RESET_UEFI_VARS=0
ENABLE_CLIPBOARD=1
SNAPSHOT="${SLM_QEMU_SNAPSHOT:-0}"
WAIT_SSH="${SLM_QEMU_WAIT_SSH:-0}"

default_qemu_memory_mb() {
    local mem_total_kb=0
    if [[ -r /proc/meminfo ]]; then
        mem_total_kb="$(awk '/^MemTotal:/ {print $2}' /proc/meminfo)"
    fi
    if [[ "$mem_total_kb" =~ ^[0-9]+$ && "$mem_total_kb" -gt 0 ]]; then
        local mem_mb=$((mem_total_kb / 1024 / 2))
        if ((mem_mb > 16384)); then
            mem_mb=16384
        elif ((mem_mb < 8192)); then
            mem_mb=8192
        fi
        printf '%s' "$mem_mb"
    else
        printf '%s' 8192
    fi
}

default_qemu_cpu_count() {
    local host_cpus=4
    if command -v nproc >/dev/null 2>&1; then
        host_cpus="$(nproc --all)"
    fi
    if [[ ! "$host_cpus" =~ ^[0-9]+$ || "$host_cpus" -lt 1 ]]; then
        host_cpus=4
    fi
    if ((host_cpus > 8)); then
        printf '%s' 8
    else
        printf '%s' "$host_cpus"
    fi
}

MEMORY_MB="${SLM_QEMU_MEMORY_MB:-$(default_qemu_memory_mb)}"
CPU_COUNT="${SLM_QEMU_CPUS:-$(default_qemu_cpu_count)}"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-run.sh [options]

Options:
  --iso PATH            Attach ISO from PATH and use it as cdrom
  --disk PATH           Override qcow2 path. Default: $DISK_PATH
  --shared-dir PATH     Host directory shared to guest. Default: $SHARED_DIR
  --memory MB           Guest memory in MB. Default: $MEMORY_MB
  --cpus N              Guest CPU count. Default: $CPU_COUNT
  --ssh-port PORT       Host TCP port forwarded to guest:22. Default: $SSH_PORT
  --ssh-user USER       SSH user for guest automation. Default: $SSH_USER
  --hostshare-path PATH Mount point in guest. Default: $HOSTSHARE_PATH
  --profile NAME        Performance profile: performance or compat. Default: $PERFORMANCE_PROFILE
  --headless            Run without graphical window
  --with-iso            Attach default ISO path: $ISO_PATH
  --auto-mount          Try to auto-mount hostshare in the guest over SSH
  --no-auto-mount       Do not auto-mount hostshare in the guest over SSH
  --gl                  Enable virtio GL graphics when available
  --no-gl               Disable virtio GL graphics
  --no-clipboard        Disable host<->guest clipboard sharing
  --pointer MODE        Pointer device: tablet, usb-tablet, or mouse. Default: $POINTER_MODE
  --bios                Force legacy BIOS boot
  --uefi                Force UEFI boot with OVMF
  --reset-uefi-vars     Recreate local OVMF vars file
  --snapshot            Discard all disk writes on exit (safe for testing)
  --wait-ssh            Headless: start VM in background, wait for SSH, then return
  --help                Show this help

Env overrides:
  SLM_QEMU_ISO, SLM_QEMU_DISK, SLM_QEMU_SHARED_DIR, SLM_QEMU_MEMORY_MB,
  SLM_QEMU_CPUS, SLM_QEMU_SSH_PORT, SLM_QEMU_SSH_USER, SLM_QEMU_DISPLAY,
  SLM_QEMU_SSH_PASSWORD, SLM_QEMU_POINTER, SLM_QEMU_HOSTSHARE_PATH,
  SLM_QEMU_AUTO_MOUNT_HOSTSHARE, SLM_QEMU_STATE_DIR, SLM_QEMU_PROFILE,
  SLM_QEMU_GL, SLM_QEMU_DISK_CACHE, SLM_QEMU_DISK_AIO, SLM_QEMU_DISK_IO,
  SLM_QEMU_SNAPSHOT, SLM_QEMU_WAIT_SSH
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
        --ssh-user)
            SSH_USER="$2"
            shift 2
            ;;
        --hostshare-path)
            HOSTSHARE_PATH="$2"
            shift 2
            ;;
        --profile)
            PERFORMANCE_PROFILE="$2"
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
        --auto-mount)
            AUTO_MOUNT_HOSTSHARE=1
            shift
            ;;
        --no-auto-mount)
            AUTO_MOUNT_HOSTSHARE=0
            shift
            ;;
        --gl)
            ENABLE_GL=1
            shift
            ;;
        --no-gl)
            ENABLE_GL=0
            shift
            ;;
        --no-clipboard)
            ENABLE_CLIPBOARD=0
            shift
            ;;
        --pointer)
            POINTER_MODE="$2"
            shift 2
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
        --snapshot)
            SNAPSHOT=1
            shift
            ;;
        --wait-ssh)
            WAIT_SSH=1
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

case "$PERFORMANCE_PROFILE" in
    performance|compat)
        ;;
    *)
        echo "[qemu-run] ERROR: profile tidak valid: $PERFORMANCE_PROFILE" >&2
        echo "[qemu-run] Valid: performance, compat" >&2
        exit 1
        ;;
esac

case "$DISK_IO" in
    iothread|simple)
        ;;
    *)
        echo "[qemu-run] ERROR: disk io mode tidak valid: $DISK_IO" >&2
        echo "[qemu-run] Valid: iothread, simple" >&2
        exit 1
        ;;
esac

if [[ "$PERFORMANCE_PROFILE" == "compat" ]]; then
    ENABLE_GL=0
    DISK_CACHE="${SLM_QEMU_DISK_CACHE:-writeback}"
    DISK_AIO="${SLM_QEMU_DISK_AIO:-threads}"
    DISK_IO="${SLM_QEMU_DISK_IO:-simple}"
fi

if [[ "$ATTACH_ISO" == "1" ]]; then
    if [[ "$BOOT_MODE" == "uefi" ]]; then
        echo "[qemu-run] WARN: installer ISO boot is forced to BIOS to avoid OVMF boot entry issues."
    fi
    BOOT_MODE="bios"
fi

if [[ ! -f "$DISK_PATH" ]]; then
    echo "[qemu-run] Disk belum ada: $DISK_PATH" >&2
    echo "[qemu-run] Jalankan: bash scripts/dev/qemu-create-disk.sh --disk \"$DISK_PATH\"" >&2
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

case "$POINTER_MODE" in
    tablet|usb-tablet|mouse)
        ;;
    *)
        echo "[qemu-run] ERROR: pointer mode tidak valid: $POINTER_MODE" >&2
        echo "[qemu-run] Valid: tablet, usb-tablet, mouse" >&2
        exit 1
        ;;
esac

start_hostshare_auto_mount() {
    if [[ "$AUTO_MOUNT_HOSTSHARE" != "1" ]]; then
        return 0
    fi
    if ! command -v ssh >/dev/null 2>&1; then
        echo "[qemu-run] WARN: ssh tidak ditemukan, auto-mount hostshare dinonaktifkan."
        return 0
    fi

    qemu_dev_ensure_state_dir

    local known_hosts
    known_hosts="$(qemu_dev_known_hosts)"
    local log_path="$STATE_DIR/hostshare-mount.log"
    local hostshare_path_q
    printf -v hostshare_path_q '%q' "$HOSTSHARE_PATH"
    local remote_cmd
    remote_cmd="sudo -n mkdir -p $hostshare_path_q && if findmnt -rn $hostshare_path_q >/dev/null 2>&1; then echo hostshare already mounted at $hostshare_path_q; else sudo -n mount -t 9p -o trans=virtio,version=9p2000.L,access=any hostshare $hostshare_path_q; fi"

    local -a ssh_base=(
        -o ConnectTimeout=2
        -o ConnectionAttempts=1
        -o PreferredAuthentications=publickey,password
        -o PubkeyAuthentication=yes
        -o NumberOfPasswordPrompts=0
        -o StrictHostKeyChecking=accept-new
        -o "UserKnownHostsFile=$known_hosts"
        -p "$SSH_PORT"
        "$SSH_USER@127.0.0.1"
    )
    local -a ssh_cmd=(ssh -F /dev/null)
    if [[ -n "$SSH_PASSWORD" ]]; then
        if command -v sshpass >/dev/null 2>&1; then
            ssh_cmd=(sshpass -p "$SSH_PASSWORD" ssh -F /dev/null)
            remote_cmd="sudo -S mkdir -p $hostshare_path_q && if findmnt -rn $hostshare_path_q >/dev/null 2>&1; then echo hostshare already mounted at $hostshare_path_q; else sudo -S mount -t 9p -o trans=virtio,version=9p2000.L,access=any hostshare $hostshare_path_q; fi"
            ssh_base=(
                -o ConnectTimeout=2
                -o ConnectionAttempts=1
                -o PreferredAuthentications=password,publickey
                -o PubkeyAuthentication=yes
                -o StrictHostKeyChecking=accept-new
                -o "UserKnownHostsFile=$known_hosts"
                -p "$SSH_PORT"
                "$SSH_USER@127.0.0.1"
            )
        else
            echo "[qemu-run] WARN: SLM_QEMU_SSH_PASSWORD diset, tapi sshpass tidak ditemukan; auto-mount butuh SSH key atau sshpass."
        fi
    fi

    (
        echo "[qemu-run] hostshare auto-mount helper started"
        echo "  user : $SSH_USER"
        echo "  port : $SSH_PORT"
        echo "  path : $HOSTSHARE_PATH"
        for _ in $(seq 1 90); do
            if "${ssh_cmd[@]}" "${ssh_base[@]}" true; then
                echo "[qemu-run] guest SSH ready, mounting hostshare"
                if [[ -n "$SSH_PASSWORD" && "${ssh_cmd[0]}" == "sshpass" ]]; then
                    printf '%s\n%s\n' "$SSH_PASSWORD" "$SSH_PASSWORD" \
                        | "${ssh_cmd[@]}" "${ssh_base[@]}" "$remote_cmd"
                else
                    exec "${ssh_cmd[@]}" "${ssh_base[@]}" "$remote_cmd"
                fi
                exit $?
            fi
            sleep 2
        done
        echo "[qemu-run] WARN: guest SSH tidak siap/login non-interaktif gagal; hostshare belum di-mount otomatis."
        echo "[qemu-run] Hint: pasang SSH key ke guest, atau jalankan dengan SLM_QEMU_SSH_PASSWORD='<password>' jika sshpass tersedia di host."
    ) >"$log_path" 2>&1 &

    echo "[qemu-run] Auto-mount hostshare enabled"
    echo "  guest path : $HOSTSHARE_PATH"
    echo "  ssh user   : $SSH_USER"
    echo "  log        : $log_path"
}

ACCEL_ARGS=("-machine" "type=q35,vmport=off")
CPU_MODEL="max"
ACCEL_LABEL="tcg"
if [[ -r /dev/kvm && -w /dev/kvm ]]; then
    ACCEL_ARGS=("-machine" "type=q35,accel=kvm,vmport=off")
    CPU_MODEL="host"
    ACCEL_LABEL="kvm"
fi

if [[ "$ATTACH_ISO" == "1" ]]; then
    if [[ -r /dev/kvm && -w /dev/kvm ]]; then
        ACCEL_ARGS=("-machine" "type=pc,accel=kvm,vmport=off")
    else
        ACCEL_ARGS=("-machine" "type=pc,vmport=off")
    fi
fi

QEMU_DEVICE_HELP="$(qemu-system-x86_64 -device help 2>/dev/null || true)"
GPU_DEVICE="virtio-vga"
DISPLAY_VALUE="$DISPLAY_BACKEND"
GRAPHICS_LABEL="virtio-vga"
if [[ "$ENABLE_GL" == "1" && "$HEADLESS" != "1" && "$DISPLAY_BACKEND" == "gtk" ]]; then
    if grep -q 'virtio-vga-gl' <<<"$QEMU_DEVICE_HELP"; then
        GPU_DEVICE="virtio-vga-gl"
        DISPLAY_VALUE="gtk,gl=on"
        GRAPHICS_LABEL="virtio-vga-gl"
    else
        echo "[qemu-run] WARN: virtio-vga-gl tidak tersedia, fallback ke virtio-vga."
        ENABLE_GL=0
    fi
fi

OVMF_CODE=""
OVMF_VARS_TEMPLATE=""
SEABIOS_BIN=""
if [[ "$ATTACH_ISO" != "1" ]]; then
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
fi

if [[ "$ATTACH_ISO" == "1" ]]; then
    for candidate in \
        /usr/share/seabios/bios.bin \
        /usr/share/qemu/bios.bin \
        /usr/share/pc-bios/bios.bin; do
        if [[ -f "$candidate" ]]; then
            SEABIOS_BIN="$candidate"
            break
        fi
    done
fi

USE_UEFI=0
if [[ "$BOOT_MODE" == "uefi" ]]; then
    USE_UEFI=1
fi
# Mode "auto" dan "bios" sama-sama default BIOS. UEFI harus eksplisit via --uefi
# karena installer Ubuntu via ISO selalu BIOS, dan disk hasil instalasi tidak
# punya UEFI boot entry.

QEMU_ARGS=(
    "${ACCEL_ARGS[@]}"
    -cpu "$CPU_MODEL"
    -smp "cpus=$CPU_COUNT,sockets=1,cores=$CPU_COUNT,threads=1"
    -m "$MEMORY_MB"
    -name "slm-dev-ubuntu"
    -rtc base=localtime
    -device virtio-balloon-pci
    -device virtio-rng-pci
    -device virtio-keyboard-pci
    -device intel-hda
    -device hda-duplex
    -device "$GPU_DEVICE"
    -object iothread,id=io0
    -drive "if=none,id=osdisk,format=qcow2,file=$DISK_PATH,cache=$DISK_CACHE,aio=$DISK_AIO,discard=unmap,detect-zeroes=unmap"
    -device "virtio-blk-pci,drive=osdisk,iothread=io0,bootindex=2"
    -nic "user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22"
    -virtfs "local,path=$SHARED_DIR,mount_tag=hostshare,security_model=none,id=hostshare"
)

if [[ "$DISK_IO" == "simple" ]]; then
    QEMU_ARGS=(
        "${ACCEL_ARGS[@]}"
        -cpu "$CPU_MODEL"
        -smp "cpus=$CPU_COUNT,sockets=1,cores=$CPU_COUNT,threads=1"
        -m "$MEMORY_MB"
        -name "slm-dev-ubuntu"
        -rtc base=localtime
        -device virtio-balloon-pci
        -device virtio-rng-pci
        -device virtio-keyboard-pci
        -device intel-hda
        -device hda-duplex
        -device "$GPU_DEVICE"
        -drive "if=virtio,format=qcow2,file=$DISK_PATH,cache=$DISK_CACHE,aio=$DISK_AIO,discard=unmap,detect-zeroes=unmap"
        -nic "user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22"
        -virtfs "local,path=$SHARED_DIR,mount_tag=hostshare,security_model=none,id=hostshare"
    )
fi

case "$POINTER_MODE" in
    tablet)
        if grep -q 'virtio-tablet-pci' <<<"$QEMU_DEVICE_HELP"; then
            QEMU_ARGS+=(-device virtio-tablet-pci)
        else
            echo "[qemu-run] WARN: virtio-tablet-pci tidak tersedia, fallback ke usb-tablet."
            QEMU_ARGS+=(-device qemu-xhci,id=usb -device usb-tablet,bus=usb.0)
            POINTER_MODE="usb-tablet"
        fi
        ;;
    usb-tablet)
        QEMU_ARGS+=(-device qemu-xhci,id=usb -device usb-tablet,bus=usb.0)
        ;;
    mouse)
        QEMU_ARGS+=(-device virtio-mouse-pci)
        ;;
esac

if [[ "$HEADLESS" == "1" ]]; then
    QEMU_ARGS+=(-display none -serial mon:stdio)
else
    QEMU_ARGS+=(-display "$DISPLAY_VALUE")
fi

if [[ "$ENABLE_CLIPBOARD" == "1" && "$HEADLESS" != "1" ]]; then
    if qemu-system-x86_64 -chardev help 2>/dev/null | grep -q 'qemu-vdagent'; then
        QEMU_ARGS+=(
            -device virtio-serial-pci
            -chardev qemu-vdagent,id=vdagent,name=vdagent,clipboard=on,mouse=on
            -device virtserialport,chardev=vdagent,name=com.redhat.spice.0
        )
    else
        echo "[qemu-run] WARN: qemu-vdagent backend tidak tersedia, clipboard sharing dinonaktifkan."
        ENABLE_CLIPBOARD=0
    fi
fi

if [[ "$ATTACH_ISO" == "1" ]]; then
    QEMU_ARGS+=(-cdrom "$ISO_PATH")
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
elif [[ "$ATTACH_ISO" == "1" && -n "$SEABIOS_BIN" ]]; then
    QEMU_ARGS+=(-bios "$SEABIOS_BIN")
fi

if [[ "$ATTACH_ISO" == "1" ]]; then
    QEMU_ARGS+=(-boot once=d,menu=on)
elif [[ "$USE_UEFI" != "1" ]]; then
    QEMU_ARGS+=(-boot menu=on)
fi

if [[ "$SNAPSHOT" == "1" ]]; then
    QEMU_ARGS+=(-snapshot)
fi

echo "[qemu-run] VM config"
echo "  disk       : $DISK_PATH"
if [[ "$ATTACH_ISO" == "1" ]]; then
    echo "  iso        : $ISO_PATH"
else
    echo "  iso        : <detached>"
fi
echo "  shared dir : $SHARED_DIR"
echo "  profile    : $PERFORMANCE_PROFILE"
echo "  accel      : $ACCEL_LABEL"
echo "  memory     : ${MEMORY_MB} MB"
echo "  cpus       : $CPU_COUNT"
echo "  graphics   : $GRAPHICS_LABEL"
echo "  disk cache : $DISK_CACHE/$DISK_AIO/$DISK_IO"
echo "  ssh        : ssh -p $SSH_PORT $SSH_USER@127.0.0.1"
echo "  share tag  : hostshare"
echo "  share path : $HOSTSHARE_PATH"
echo "  auto mount : $AUTO_MOUNT_HOSTSHARE"
echo "  pointer    : $POINTER_MODE"
if [[ "$ENABLE_CLIPBOARD" == "1" && "$HEADLESS" != "1" ]]; then
    echo "  clipboard  : enabled (qemu-vdagent, mouse integration on)"
else
    echo "  clipboard  : disabled"
fi
if [[ "$USE_UEFI" == "1" ]]; then
    echo "  firmware   : UEFI ($OVMF_CODE)"
elif [[ "$ATTACH_ISO" == "1" ]]; then
    echo "  firmware   : BIOS ($SEABIOS_BIN)"
else
    echo "  firmware   : BIOS"
fi
if [[ "$SNAPSHOT" == "1" ]]; then
    echo "  snapshot   : on (disk writes discarded on exit)"
fi
if [[ "$HEADLESS" == "1" && "$WAIT_SSH" == "1" ]]; then
    echo "  mode       : headless+wait-ssh (background)"
fi
echo ""
if [[ "$AUTO_MOUNT_HOSTSHARE" == "1" ]]; then
    start_hostshare_auto_mount
else
    echo "[qemu-run] Mount shared repo di guest:"
    echo "  bash scripts/dev/qemu-mount-hostshare.sh --user $SSH_USER --port $SSH_PORT"
    echo ""
fi

if [[ "$HEADLESS" == "1" && "$WAIT_SSH" == "1" ]]; then
    qemu-system-x86_64 "${QEMU_ARGS[@]}" &
    QEMU_BG_PID=$!
    echo "[qemu-run] VM started in background (PID $QEMU_BG_PID)"
    echo "[qemu-run] Waiting for SSH on port $SSH_PORT..."
    if qemu_dev_wait_ssh "$SSH_PORT" 90; then
        echo "[qemu-run] Guest SSH ready"
        echo "  ssh     : ssh -p $SSH_PORT $SSH_USER@127.0.0.1"
        echo "  stop vm : kill $QEMU_BG_PID"
    else
        echo "[qemu-run] ERROR: timeout waiting for SSH (90 attempts × 2s)" >&2
        kill "$QEMU_BG_PID" 2>/dev/null || true
        exit 1
    fi
else
    exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
fi
