#!/usr/bin/env bash
set -euo pipefail

STATE_DIR_DEFAULT="/var/lib/slm-package-policy"
STATE_DIR="${SLM_PACKAGE_POLICY_STATE_DIR:-$STATE_DIR_DEFAULT}"
APPLY=0

if [[ "${1:-}" == "--apply" ]]; then
  APPLY=1
fi

if [[ ! -d "$STATE_DIR" ]]; then
  STATE_DIR="${HOME:-/tmp}/.local/state/slm-package-policy"
fi

LATEST_PATH_FILE="$STATE_DIR/latest-snapshot-path"
if [[ ! -f "$LATEST_PATH_FILE" ]]; then
  echo "No snapshot metadata found." >&2
  exit 2
fi

SNAP_DIR="$(cat "$LATEST_PATH_FILE")"
if [[ ! -d "$SNAP_DIR" ]]; then
  echo "Snapshot path missing: $SNAP_DIR" >&2
  exit 2
fi

echo "Snapshot: $SNAP_DIR"
if [[ "$APPLY" -ne 1 ]]; then
  echo "Dry-run mode. Use --apply to restore apt config + desktop critical files."
  exit 0
fi

if [[ "$EUID" -ne 0 ]]; then
  echo "--apply requires root" >&2
  exit 3
fi

if [[ -f "$SNAP_DIR/apt-config.tar" ]]; then
  tar -C / -xf "$SNAP_DIR/apt-config.tar"
fi

if [[ -f "$SNAP_DIR/desktop-critical.tar" ]]; then
  tar -C / -xf "$SNAP_DIR/desktop-critical.tar"
fi

if command -v apt-get >/dev/null 2>&1; then
  apt-get update || true
fi

echo "Recovery apply completed. Reboot recommended."
