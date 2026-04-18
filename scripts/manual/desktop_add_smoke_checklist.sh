#!/usr/bin/env bash
set -euo pipefail

DESKTOP_DIR="${HOME}/Desktop"

printf "Desktop Add/Drag Smoke Checklist\n"
printf "Date: %s\n" "$(date '+%Y-%m-%d %H:%M:%S %Z')"
printf "Desktop dir: %s\n\n" "$DESKTOP_DIR"

if [[ ! -d "$DESKTOP_DIR" ]]; then
  printf "[WARN] Desktop folder not found: %s\n" "$DESKTOP_DIR"
fi

printf "Current desktop item count: "
find "$DESKTOP_DIR" -mindepth 1 -maxdepth 1 | wc -l
printf "\n"

cat <<'TXT'
Run these checks in one session:

[1] Launchpad -> Context Menu -> Add to Desktop
- Right click app A in Launchpad, choose "Add to Desktop".
- Expected: icon appears on desktop, label/icon correct, no duplicate first icon.

[2] Launchpad Drag -> Desktop Drop
- Drag app B from Launchpad and drop at a distinct desktop position.
- Expected: shortcut created, item appears near drop cell.

[3] Dock Drag -> Desktop Drop
- Drag a pinned dock app upward into desktop area.
- Expected: desktop shortcut created (copy semantics), dock order unchanged.

[4] Desktop Context Menu Actions
- Desktop right click -> Arrange Icons submenu.
- Trigger "Snap to Grid" then "Clean Up Desktop".
- Expected: no overlap, deterministic placement, no item lost.

[5] Filesystem Sync
- Copy a file manually into ~/Desktop via terminal/file manager.
- Expected: new icon appears automatically.
- Delete/rename it externally.
- Expected: desktop view updates without stale ghost item.

[6] Row/Column Zero Guard
- Move one item to top-left first slot (row 0 / col 0).
- Expected: item remains visible after refresh/restart.

[7] Restart Persistence
- Restart shell/session.
- Expected: positions persist; item count equals ~/Desktop count.

Debug logs to observe:
- [desktop-add] ...
- [desktop-sync] ...
TXT

printf "\nDone.\n"
