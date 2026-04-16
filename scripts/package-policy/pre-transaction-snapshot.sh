#!/usr/bin/env bash
set -euo pipefail

TOOL="${1:-unknown}"
shift || true

STATE_DIR_DEFAULT="/var/lib/slm-package-policy"
STATE_DIR="${SLM_PACKAGE_POLICY_STATE_DIR:-$STATE_DIR_DEFAULT}"

ensure_state_dir() {
  local target="$1"
  if mkdir -p "$target" 2>/dev/null; then
    echo "$target"
    return 0
  fi

  local fallback="${HOME:-/tmp}/.local/state/slm-package-policy"
  mkdir -p "$fallback"
  echo "$fallback"
}

STATE_DIR="$(ensure_state_dir "$STATE_DIR")"
SNAP_ROOT="$STATE_DIR/snapshots"
mkdir -p "$SNAP_ROOT"

SNAP_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
SNAP_DIR="$SNAP_ROOT/$SNAP_ID"
mkdir -p "$SNAP_DIR"

printf '%s\n' "$TOOL" > "$SNAP_DIR/tool.txt"
printf '%s\n' "$*" > "$SNAP_DIR/args.txt"
date -u +%Y-%m-%dT%H:%M:%SZ > "$SNAP_DIR/created_at.txt"

if command -v dpkg-query >/dev/null 2>&1; then
  dpkg-query -W -f='${Package}\t${Version}\n' > "$SNAP_DIR/packages.tsv" 2>/dev/null || true
fi

if command -v apt-mark >/dev/null 2>&1; then
  apt-mark showhold > "$SNAP_DIR/apt-hold.txt" 2>/dev/null || true
fi

if [[ -d /etc/apt ]]; then
  tar -C / -cf "$SNAP_DIR/apt-config.tar" \
    etc/apt/sources.list \
    etc/apt/sources.list.d \
    etc/apt/preferences \
    etc/apt/preferences.d \
    etc/apt/trusted.gpg \
    etc/apt/trusted.gpg.d 2>/dev/null || true
fi

# Capture desktop-critical config subset for quick restore path.
tar -C / -cf "$SNAP_DIR/desktop-critical.tar" \
  etc/pam.d \
  etc/systemd/system \
  etc/greetd \
  usr/share/wayland-sessions \
  usr/share/xsessions 2>/dev/null || true

printf '%s\n' "$SNAP_ID" > "$STATE_DIR/latest-snapshot"
printf '%s\n' "$SNAP_DIR" > "$STATE_DIR/latest-snapshot-path"

# Optional Btrfs read-only snapshot for fast rollback path.
create_btrfs_snapshot() {
  if ! command -v btrfs >/dev/null 2>&1; then
    return 0
  fi
  local root_fs
  root_fs="$(findmnt -no FSTYPE / 2>/dev/null || true)"
  if [[ "$root_fs" != "btrfs" ]]; then
    return 0
  fi
  if [[ ! -d /.snapshots ]]; then
    return 0
  fi

  local reason
  reason="$(printf '%s' "$TOOL" | tr -cd '[:alnum:]._:-')"
  [[ -n "$reason" ]] || reason="pkg"

  local btrfs_id
  btrfs_id="$(date -u +%Y%m%d-%H%M%S)-auto-${reason}"
  if btrfs subvolume snapshot -r / "/.snapshots/$btrfs_id" >/dev/null 2>&1; then
    printf '%s\n' "$btrfs_id" > "$SNAP_DIR/btrfs-snapshot-id.txt"
    printf '%s\n' "/.snapshots/$btrfs_id" > "$SNAP_DIR/btrfs-snapshot-path.txt"
  fi
}

create_btrfs_snapshot

echo "$SNAP_ID"
