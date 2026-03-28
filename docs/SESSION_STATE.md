# SLM Desktop Session State

Last updated: 2026-03-28 13:09 WIB

## Scope (Current)
- Focus: **desktop development** (settings/workspace/dock/filemanager integration).
- Login stack (`greetd + slm-greeter + broker`) moved to maintenance branch/testing track.
- VM test for Ubuntu 26.04 postponed until stable release (beta library gaps).

## Decision Log
1. Long chat context is now treated as archived context.
2. Use this file as the canonical summary for next sessions.
3. Keep host session on `lightdm + pantheon` for daily development stability.

## Current Status Snapshot
- FileManager split groundwork exists; dedicated module/repo path is active.
- Settings app exists but still needs module completion and UX polishing.
- Workspace hybrid (overview -> workspace rebranding) implemented partially; continue stabilization.
- Dock has migrated toward liquid style; behavior polishing ongoing.
- Portal/capability foundation exists; hardening continues in parallel.

## Active Priorities (Desktop)
1. **Settings**
   - complete missing categories and backend bindings
   - ensure search/deeplink/open shortcut flow stable
2. **Workspace**
   - stabilize transitions/shortcuts/focus sync
   - reduce regressions in compositor visibility filtering
3. **Dock + Launchpad**
   - smoothness/perf pass
   - interaction consistency with workspace state
4. **FileManager Integration**
   - keep non-technical UX (sharing/actions)
   - continue modular extraction cleanup

## Guardrails
- No destructive platform switch on host without explicit rollback script.
- For login stack changes: always keep a tested fallback (`lightdm` path).
- Keep logs separated by component (`greeter`, `broker`, `desktop`) for debugging.

## Next Session Bootstrap
Read only:
1. `docs/SESSION_STATE.md`
2. `docs/TODO.md`
3. Relevant module doc for the task at hand

Then execute targeted task without replaying full historical context.
