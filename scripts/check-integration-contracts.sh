#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC="$ROOT_DIR/docs/architecture/DESKTOP_INTEGRATION_CONTRACTS.md"

fail() {
  echo "[check-integration-contracts][FAIL] $*" >&2
  exit 1
}

[[ -f "$DOC" ]] || fail "missing document: $DOC"

required_states=(
  activeWorkspaceId
  workspaceIds
  workspaceWindowMap
  activeWindowId
  activeAppId
  runningApps
  pinnedApps
  appWindowMap
  launchpadVisible
  searchVisible
  searchQuery
  dockVisibilityMode
  dockHoveredItem
  dockExpandedItem
  dragSession
  mountedVolumes
  recentFiles
  recentFolders
  selectedObjects
  fileOperationQueue
  notificationQueue
  themeMode
  accentColor
  iconTheme
  reduceMotion
  sessionRestoreState
)

for s in "${required_states[@]}"; do
  count="$(grep -cE "^\| \`$s\` \| " "$DOC" || true)"
  if [[ "$count" -ne 1 ]]; then
    fail "state '$s' must appear exactly once in owner matrix (found=$count)"
  fi
done

required_sections=(
  "## 1. State Owner Matrix"
  "## 2. Layer Separation"
  "## 3. Service Contracts"
  "## 4. Event Contracts"
  "## 5. Integration Guard Rules"
)
for h in "${required_sections[@]}"; do
  grep -qF "$h" "$DOC" || fail "missing section: $h"
done

echo "[check-integration-contracts] OK"
