# SLM DBus API Changelog

This document tracks user-visible DBus contract changes for SLM services.
Format per version:
- `Added`
- `Changed`
- `Deprecated`
- `Removed`

## Workspace Manager (`org.slm.WorkspaceManager1`)

### v1.4.0 (Unreleased)
- Added
  - `ShowWorkspace()`
  - `HideWorkspace()`
  - `ToggleWorkspace()`
  - `SwitchWorkspaceByDelta(delta)`
  - `MoveFocusedWindowByDelta(delta) -> bool`
  - `WorkspaceShown()` signal.
  - `WorkspaceVisibilityChanged(visible)` signal.
  - Canonical workspace command/event routing:
    - command: `workspace on|off|toggle` (preferred), `overview on|off|toggle` (legacy fallback)
    - event: `workspace-open|workspace-close|workspace-toggle` (preferred), `overview-*` (legacy alias)
- Changed
  - `GetCapabilities()` now includes:
    - `show_workspace`
    - `hide_workspace`
    - `toggle_workspace`
    - `switch_workspace_by_delta`
    - `move_focused_window_by_delta`
  - `GetCapabilities()` now also exposes:
    - `canonical_capabilities`
    - `legacy_capability_aliases` (`show/hide/toggle_overview -> *_workspace`)
  - Telemetry labels are normalized to workspace domain:
    - `show_workspace`
    - `hide_workspace`
    - `toggle_workspace`
  - `workspacectl` supports `workspace [on|off|toggle]` alias command.
  - `workspacectl workspace` is now the canonical CLI path; `workspacectl overview` remains a legacy alias.
  - `workspacectl` now supports:
    - `switch-delta <delta>`
    - `move-focused-delta <delta>`
  - UI preference key for workspace toggle shortcut is canonicalized to `windowing.bindWorkspace`.
  - Legacy preference key `windowing.bindOverview` remains accepted as compatibility alias and is auto-migrated.
- Deprecated
  - Legacy methods remain available for compatibility:
    - `ShowOverview()`
    - `HideOverview()`
    - `ToggleOverview()`
  - Legacy capability labels remain available as aliases:
    - `show_overview`
    - `hide_overview`
    - `toggle_overview`
  - Legacy event aliases:
    - `overview-open`
    - `overview-close`
    - `overview-toggle`
- Removed
  - None.

### v1.3.0 (Unreleased)
- Added
  - `DaemonHealthSnapshot() -> a{sv}`
- Changed
  - `DiagnosticSnapshot()` now includes `daemonHealth` payload.
  - `GetCapabilities()` now includes `daemon_health_snapshot`.
- Deprecated
  - None.
- Removed
  - None.

### v1.2.0 (Unreleased)
- Added
  - `ListRankedApps(limit) -> aa{sv}`
- Changed
  - `GetCapabilities()` now includes `list_ranked_apps`.
- Deprecated
  - None.
- Removed
  - None.

### v1.1.0
- Added
  - `HideOverview()`
  - `ToggleOverview()`
  - `PresentView(view_id) -> bool`
  - `CloseView(view_id) -> bool`
  - `OverviewVisibilityChanged(visible)` signal.
- Changed
  - None.
- Deprecated
  - None.
- Removed
  - None.

### v1.0.0
- Added
  - `ShowOverview()`
  - `ShowAppGrid()`
  - `ShowDesktop()`
  - `PresentWindow(app_id) -> bool`
  - `SwitchWorkspace(index)`
  - `MoveWindowToWorkspace(window, index) -> bool`
  - `DiagnosticSnapshot() -> a{sv}`
  - `OverviewShown()` signal.
  - `WorkspaceChanged()` signal.
  - `WindowAttention(window)` signal.
- Changed
  - None.
- Deprecated
  - None.
- Removed
  - None.

## Global Search (`org.slm.Desktop.Search.v1`)

### v1.1.0 (Unreleased)
- Added
  - None.
- Changed
  - Search scope canonical renamed to `tothespot`.
  - Clients must send `scope=tothespot` in search/action contexts.
- Deprecated
  - Legacy non-canonical scope naming.
- Removed
  - Parser compatibility alias for legacy non-canonical scope (breaking change).

## Portal Backend (`org.slm.Desktop.Portal` + `org.freedesktop.impl.portal.desktop.slm`)

### v1.1.0 (Unreleased)
- Added
  - New impl-facing bridge service:
    - service: `org.freedesktop.impl.portal.desktop.slm`
    - path: `/org/freedesktop/portal/desktop`
    - interface: `org.freedesktop.impl.portal.desktop.slm`
  - Split impl interfaces on same path:
    - `org.freedesktop.impl.portal.FileChooser`
    - `org.freedesktop.impl.portal.OpenURI`
    - `org.freedesktop.impl.portal.Screenshot`
    - `org.freedesktop.impl.portal.ScreenCast`
  - Bridge methods:
    - `FileChooser(handle, appId, parentWindow, options)`
    - `OpenURI(handle, appId, parentWindow, uri, options)`
    - `Screenshot(handle, appId, parentWindow, options)`
    - `ScreenCast(handle, appId, parentWindow, options)`
  - Response payload normalization (portal adapter):
    - `response` numeric code,
    - `results` map,
    - handle aliases: `requestHandle`, `sessionHandle`, `handle`.
- Changed
  - Request/session id generation now collision-safe (`timestamp + sequence`).
  - Session operation rejects cross-domain usage with `session-capability-mismatch`.
- Deprecated
  - None.
- Removed
  - None.

## Session State (`org.slm.SessionState1`)

### v1.0.0
- Added
  - `Inhibit(reason, flags) -> uint`
  - `Logout()`
  - `Shutdown()`
  - `Reboot()`
  - `Lock()`
  - `SessionLocked()` signal.
  - `SessionUnlocked()` signal.
  - `IdleChanged(idle)` signal.
  - `ActiveAppChanged(app_id)` signal.
- Changed
  - None.
- Deprecated
  - None.
- Removed
  - None.

## File Operations (`org.slm.Desktop.FileOperations`)

### v1.0.0
- Added
  - `Copy(uris, destination) -> id`
  - `Move(uris, destination) -> id`
  - `Delete(uris) -> id`
  - `Trash(uris) -> id`
  - `EmptyTrash() -> id`
  - `Progress(id, percent)` signal.
  - `Finished(id)` signal.
  - `Error(id)` signal.
- Changed
  - None.
- Deprecated
  - None.
- Removed
  - None.

### v1.1.0
- Added
  - `Pause(id) -> bool`
  - `Resume(id) -> bool`
  - `Cancel(id) -> bool`
  - `ProgressDetail(id, current, total)` signal.
- Changed
  - File operation jobs now support cooperative pause/resume/cancel.
- Deprecated
  - None.
- Removed
  - None.

### v1.2.0
- Added
  - `ErrorDetail(id, code, message)` signal.
- Changed
  - `Error(id)` tetap dipancarkan untuk kompatibilitas; klien disarankan memakai `ErrorDetail` saat tersedia.
  - Error code distandarkan (canonical) untuk file operations.
- Deprecated
  - None.
- Removed
  - None.

### v1.3.0
- Added
  - `GetJob(id) -> a{sv}`
  - `ListJobs() -> aa{sv}`
  - `JobsChanged()` signal.
- Changed
  - Destructive operations now reject critical targets (`protected-target`) including symlink-to-critical edge case.
- Deprecated
  - None.
- Removed
  - None.

## Devices (`org.slm.Desktop.Devices`)

### v1.0.0
- Added
  - `Mount(target) -> a{sv}`
  - `Eject(target) -> a{sv}`
  - `Unlock(target, passphrase) -> a{sv}`
  - `Format(target, filesystem) -> a{sv}`
- Changed
  - None.
- Deprecated
  - None.
- Removed
  - None.
