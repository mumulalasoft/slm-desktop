#!/usr/bin/env bash
# Prepare the live ISO environment for slm-installer.
#
# Invoked by slm-installer-prepare.service before calamares.service starts.
# See docs/SLM_INSTALLER_BACKEND.md §11.
#
# Responsibilities:
#   - Ensure efivars is writable (re-mount rw if needed).
#   - Load the btrfs kernel module.
#   - Mount the optional driver squashfs at /opt/slm-drivers/ if present.
#   - Refuse to run if /sys/firmware/efi is missing — the installer cannot
#     proceed on legacy BIOS and we surface that early.

set -euo pipefail

readonly DRIVER_SQUASHFS="/cdrom/slm-drivers-optional.squashfs"
readonly DRIVER_MOUNT="/opt/slm-drivers"

log() { printf '[prepare-live-env] %s\n' "$*"; }

require_uefi() {
    if [[ ! -d /sys/firmware/efi ]]; then
        log "FATAL: /sys/firmware/efi missing — SLM Desktop requires UEFI."
        exit 1
    fi
}

ensure_efivars_writable() {
    if [[ -d /sys/firmware/efi/efivars ]] && ! mountpoint -q /sys/firmware/efi/efivars; then
        log "Mounting efivars."
        mount -t efivarfs efivarfs /sys/firmware/efi/efivars
    fi
}

load_btrfs() {
    if ! lsmod | grep -q '^btrfs'; then
        log "Loading btrfs kernel module."
        modprobe btrfs
    fi
}

mount_drivers() {
    if [[ -f "$DRIVER_SQUASHFS" ]]; then
        mkdir -p "$DRIVER_MOUNT"
        if ! mountpoint -q "$DRIVER_MOUNT"; then
            log "Mounting driver squashfs at $DRIVER_MOUNT."
            mount -t squashfs -o loop "$DRIVER_SQUASHFS" "$DRIVER_MOUNT"
        fi
    else
        log "No driver squashfs found; continuing without it."
    fi
}

main() {
    require_uefi
    ensure_efivars_writable
    load_btrfs
    mount_drivers
    log "Live environment ready."
}

main "$@"
