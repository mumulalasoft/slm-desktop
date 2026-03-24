# SLM Desktop TODO

## Backlog (Tothespot)
- [ ] Add optional preview pane for non-compact popup mode.
- [ ] Add provider health indicators with timeout/fallback status.
- [ ] Add cold-start cache for empty-query popular results.

## Backlog (Theme)
- [ ] Elevation/shadow tokens:
  - unify popup/window shadow strength (`elevationLow/Medium/High`).
- [ ] Motion tokens:
  - replace ad-hoc durations/easing with shared animation tokens.
- [ ] Semantic control states:
  - define `controlBgHover/Pressed/Disabled`, `textDisabled`, `focusRing`.
- [ ] FileManager density pass:
  - align toolbar/search/tab/status heights to compact token grid.
- [ ] Add high-contrast mode toggle in `UIPreferences` + runtime Theme switch.
- [ ] Add reduced-motion mode toggle:
  - scale/disable non-essential transitions globally.
- [ ] Add optional dynamic wallpaper sampling tint with contrast guardrail.

## Backlog (FileManager)
- [ ] Continue shrinking `FileManagerWindow.qml` by moving remaining dialog/menu glue into `Qml/apps/filemanager/`.

## Backlog (Global Menu)
- [ ] Runtime integration check global menu end-to-end on active GTK/Qt/KDE apps (verify active binding/top-level changes by focus switching).
  - helper available: `scripts/smoke-globalmenu-focus.sh` (`--strict --min-unique 2`).

## Backlog (Clipboard)
- [ ] Integrate native `wl-data-control` low-level path for Wayland clipboard watching (event-driven primary path, keep Qt/X11 fallback).

## Next (Portal Adapter hardening)
- [ ] Phase-5 InputCapture:
  - [ ] Bind provider to real compositor pointer-capture/barrier primitives (currently stateful contract + optional command hook only).
- [ ] Phase-6 Privacy/data portals:
  - [ ] Camera
  - [ ] Location
  - [ ] PIM metadata/resolution portals (contacts/calendar/mail metadata first, body last)
- [ ] Phase-7 Hardening & interop:
  - [ ] DBus contract/regression suite GTK/Qt/Electron/Flatpak
  - [ ] Race/cancellation/multi-session stress tests (baseline added; broader suite pending)

## Next (Permission Foundation)
- [ ] Integrate `DBusSecurityGuard` into live service endpoints (`desktopd` first):
  - clipboard/search/share/file-actions/accounts paths.
- [ ] Add consent mediation contract for `AskUser` decisions (portal-ready hook in broker).
- [ ] Add tests:
  - trust resolver classification,
  - policy defaults per trust level,
  - gesture-gated capability checks,
  - permission persistence/audit writes.
