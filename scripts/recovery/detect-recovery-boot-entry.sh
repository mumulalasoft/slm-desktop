#!/usr/bin/env bash
set -euo pipefail

# Print best-effort recovery boot entry name/id for current bootloader.
#
# Priority:
# 1) explicit hint match
# 2) pattern match: recovery|rescue|safe
# 3) first available entry
#
# systemd-boot: returns entry id (filename without .conf)
# GRUB: returns menuentry title

ENTRY_HINT_RAW="${1:-recovery}"
ENTRY_HINT="$(echo "$ENTRY_HINT_RAW" | tr '[:upper:]' '[:lower:]' | xargs)"

log() { echo "[detect-recovery-entry] $*" >&2; }
die() { echo "[detect-recovery-entry][FAIL] $*" >&2; exit 1; }

choose_with_hint() {
  local hint="$1"
  shift || true
  local entries=("$@")
  local e lower

  if [[ "${#entries[@]}" -eq 0 ]]; then
    return 1
  fi

  if [[ -n "$hint" ]]; then
    for e in "${entries[@]}"; do
      lower="$(echo "$e" | tr '[:upper:]' '[:lower:]')"
      if [[ "$lower" == *"$hint"* ]]; then
        echo "$e"
        return 0
      fi
    done
  fi

  for e in "${entries[@]}"; do
    lower="$(echo "$e" | tr '[:upper:]' '[:lower:]')"
    if [[ "$lower" == *"recovery"* || "$lower" == *"rescue"* || "$lower" == *"safe"* ]]; then
      echo "$e"
      return 0
    fi
  done

  echo "${entries[0]}"
  return 0
}

detect_systemd_boot_entry() {
  local dirs=(
    "/boot/loader/entries"
    "/boot/efi/loader/entries"
    "/efi/loader/entries"
  )
  local d ids=() f

  for d in "${dirs[@]}"; do
    [[ -d "$d" ]] || continue
    while IFS= read -r -d '' f; do
      ids+=("$(basename "${f%.conf}")")
    done < <(find "$d" -maxdepth 1 -type f -name '*.conf' -print0 2>/dev/null || true)
  done

  if [[ "${#ids[@]}" -eq 0 ]]; then
    return 1
  fi

  choose_with_hint "$ENTRY_HINT" "${ids[@]}"
}

detect_grub_entry() {
  local cfg=""
  if [[ -f "/boot/grub/grub.cfg" ]]; then
    cfg="/boot/grub/grub.cfg"
  elif [[ -f "/boot/grub2/grub.cfg" ]]; then
    cfg="/boot/grub2/grub.cfg"
  else
    return 1
  fi

  local titles=()
  while IFS= read -r line; do
    titles+=("$line")
  done < <(sed -n "s/^menuentry '\([^']*\)'.*/\1/p" "$cfg" 2>/dev/null)

  if [[ "${#titles[@]}" -eq 0 ]]; then
    return 1
  fi

  choose_with_hint "$ENTRY_HINT" "${titles[@]}"
}

if command -v bootctl >/dev/null 2>&1; then
  if entry="$(detect_systemd_boot_entry)"; then
    echo "$entry"
    exit 0
  fi
fi

if command -v grub-reboot >/dev/null 2>&1 || [[ -f "/boot/grub/grub.cfg" || -f "/boot/grub2/grub.cfg" ]]; then
  if entry="$(detect_grub_entry)"; then
    echo "$entry"
    exit 0
  fi
fi

die "unable to detect recovery entry (hint='$ENTRY_HINT_RAW')"
