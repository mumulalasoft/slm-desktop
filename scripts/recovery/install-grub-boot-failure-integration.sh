#!/usr/bin/env bash
set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SNIPPET_SRC="${SLM_GRUB_SNIPPET_SRC:-$SELF_DIR/grub-recovery-snippet.cfg}"
TARGET_SCRIPT="${SLM_GRUB_RECOVERY_SCRIPT:-/etc/grub.d/42_slm_recovery}"
REGEN=1

usage() {
  cat <<USAGE
Usage: $0 [--no-regenerate]

Install SLM recovery snippet to GRUB and regenerate grub.cfg.
USAGE
}

die() { echo "[install-grub-recovery][FAIL] $*" >&2; exit 1; }
log() { echo "[install-grub-recovery] $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-regenerate) REGEN=0; shift ;;
    --help|-h) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ "${EUID}" -eq 0 ]] || die "must run as root"
[[ -f "$SNIPPET_SRC" ]] || die "snippet source missing: $SNIPPET_SRC"

mkdir -p "$(dirname "$TARGET_SCRIPT")"

cat > "$TARGET_SCRIPT" <<SCRIPT
#!/bin/sh
set -e
cat <<'EOF_GRUB'
$(cat "$SNIPPET_SRC")
EOF_GRUB
SCRIPT

chmod 0755 "$TARGET_SCRIPT"
log "installed: $TARGET_SCRIPT"

if [[ "$REGEN" -eq 0 ]]; then
  log "skip grub config regeneration (--no-regenerate)"
  exit 0
fi

if command -v update-grub >/dev/null 2>&1; then
  update-grub
  log "grub config regenerated via update-grub"
  exit 0
fi

if command -v grub-mkconfig >/dev/null 2>&1; then
  if [[ -d /boot/grub ]]; then
    grub-mkconfig -o /boot/grub/grub.cfg
    log "grub config regenerated: /boot/grub/grub.cfg"
    exit 0
  fi
  if [[ -d /boot/grub2 ]]; then
    grub-mkconfig -o /boot/grub2/grub.cfg
    log "grub config regenerated: /boot/grub2/grub.cfg"
    exit 0
  fi
fi

die "cannot regenerate grub config automatically (no update-grub/grub-mkconfig)"
