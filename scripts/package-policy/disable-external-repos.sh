#!/usr/bin/env bash
set -euo pipefail

if [[ "$EUID" -ne 0 ]]; then
  echo "disable-external-repos: requires root" >&2
  exit 3
fi

APT_DIR="/etc/apt"
SOURCES_LIST="$APT_DIR/sources.list"
SOURCES_D="$APT_DIR/sources.list.d"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
BACKUP_DIR="${SLM_PACKAGE_POLICY_STATE_DIR:-/var/lib/slm-package-policy}/repo-backups/$STAMP"
mkdir -p "$BACKUP_DIR"

is_official_uri() {
  local uri="$1"
  uri="$(echo "$uri" | tr '[:upper:]' '[:lower:]')"
  [[ "$uri" == *"archive.ubuntu.com"* ]] && return 0
  [[ "$uri" == *"security.ubuntu.com"* ]] && return 0
  [[ "$uri" == *"ports.ubuntu.com"* ]] && return 0
  [[ "$uri" == *"deb.debian.org"* ]] && return 0
  [[ "$uri" == *"security.debian.org"* ]] && return 0
  [[ "$uri" == *"packages.linuxmint.com"* ]] && return 0
  [[ "$uri" == *"ppa.launchpadcontent.net"* ]] && return 1
  [[ "$uri" == *"download.opensuse.org"* ]] && return 1
  [[ "$uri" == *"dl.google.com"* ]] && return 1
  [[ "$uri" == *"repo.steampowered.com"* ]] && return 1
  return 1
}

disable_list_file() {
  local file="$1"
  local changed=0

  [[ -f "$file" ]] || return 0
  cp -a "$file" "$BACKUP_DIR/$(basename "$file")"

  local tmp
  tmp="$(mktemp)"
  while IFS= read -r line || [[ -n "$line" ]]; do
    local trimmed
    trimmed="$(echo "$line" | sed 's/^\s*//;s/\s*$//')"
    if [[ -z "$trimmed" || "$trimmed" == \#* ]]; then
      echo "$line" >> "$tmp"
      continue
    fi

    if [[ "$trimmed" == deb* ]]; then
      local uri
      uri="$(echo "$trimmed" | awk '{for(i=1;i<=NF;i++){if($i ~ /^https?:\/\//){print $i; exit}}}')"
      if [[ -n "$uri" ]] && ! is_official_uri "$uri"; then
        echo "# slm-disabled-external-repo $STAMP: $line" >> "$tmp"
        changed=1
        continue
      fi
    fi

    echo "$line" >> "$tmp"
  done < "$file"

  if [[ "$changed" -eq 1 ]]; then
    mv "$tmp" "$file"
    echo "disabled external repo entries in $file"
  else
    rm -f "$tmp"
  fi
}

disable_sources_file() {
  local file="$1"
  local changed=0
  local in_stanza=0
  local stanza_has_official=0
  local stanza_lines=()

  [[ -f "$file" ]] || return 0
  cp -a "$file" "$BACKUP_DIR/$(basename "$file")"

  flush_stanza() {
    if [[ "$in_stanza" -eq 1 ]]; then
      if [[ "$stanza_has_official" -eq 0 ]]; then
        local has_enabled=0
        for sline in "${stanza_lines[@]}"; do
          if [[ "$sline" == "Enabled:"* ]]; then
            echo "Enabled: no"
            changed=1
            has_enabled=1
          else
            echo "$sline"
          fi
        done
        if [[ "$has_enabled" -eq 0 ]]; then
          echo "Enabled: no"
          changed=1
        fi
      else
        for sline in "${stanza_lines[@]}"; do
          echo "$sline"
        done
      fi
    fi
    stanza_lines=()
    in_stanza=0
    stanza_has_official=0
  }

  local tmp
  tmp="$(mktemp)"
  while IFS= read -r line || [[ -n "$line" ]]; do
    local trimmed
    trimmed="$(echo "$line" | sed 's/^\s*//;s/\s*$//')"

    if [[ -z "$trimmed" ]]; then
      flush_stanza >> "$tmp"
      echo >> "$tmp"
      continue
    fi

    in_stanza=1
    stanza_lines+=("$line")

    if [[ "$trimmed" == URIs:* ]]; then
      local uris
      uris="${trimmed#URIs:}"
      for uri in $uris; do
        if is_official_uri "$uri"; then
          stanza_has_official=1
          break
        fi
      done
    fi
  done < "$file"

  flush_stanza >> "$tmp"

  if [[ "$changed" -eq 1 ]]; then
    mv "$tmp" "$file"
    echo "disabled external repo stanzas in $file"
  else
    rm -f "$tmp"
  fi
}

if [[ -f "$SOURCES_LIST" ]]; then
  disable_list_file "$SOURCES_LIST"
fi

if [[ -d "$SOURCES_D" ]]; then
  shopt -s nullglob
  for file in "$SOURCES_D"/*.list; do
    disable_list_file "$file"
  done
  for file in "$SOURCES_D"/*.sources; do
    disable_sources_file "$file"
  done
  shopt -u nullglob
fi

apt-get update || true

echo "external repository disable pass completed"
