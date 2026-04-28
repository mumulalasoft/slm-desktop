#!/usr/bin/env bash
# scripts/dev/qemu-ssh.sh — Helper SSH ke Ubuntu guest via port forwarding QEMU.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"

qemu_dev_ssh_main "$@"
