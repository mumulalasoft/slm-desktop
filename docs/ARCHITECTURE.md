# Architecture Notes

## Execution flow (one gate)

All app launches should pass through `AppExecutionGate`:

```mermaid
flowchart LR
    UI["UI Entry Points
Dock / Topbar / Launchpad / FileManager / Tothespot"] --> Router["AppCommandRouter (optional)"]
    Router --> Gate["AppExecutionGate"]
    Gate --> DockActivate["DockModel.activateOrLaunch(...)"]
    DockActivate --> NoteLaunch["DockModel.noteLaunchedEntry(...)"]
    NoteLaunch --> Refresh["DockModel.refreshRunningStates()"]
    Gate --> Runtime["AppRuntimeRegistry (launch hints / pending desktop path)"]
```

1. UI entry point (Dock / Shell / Launchpad / future FileManager / Terminal)
2. `AppCommandRouter` (optional action dispatch for external modules)
3. `AppExecutionGate` (`launchDesktopEntry(...)` / `launchEntryMap(...)`)
4. `DockModel.activateOrLaunch(...)`
5. `DockModel.noteLaunchedEntry(...)`
6. `DockModel.refreshRunningStates()` reconciles pinned + transient entries

This avoids fragmented launch logic and keeps monitoring consistent.

Recommended API by source:
- Dock item: `AppExecutionGate.launchDesktopEntry(...)`
- Launchpad / shortcut grid / file-manager style entries: `AppExecutionGate.launchEntryMap(entry, source)`
- Terminal-like command source: `AppExecutionGate.launchCommand(...)`

## Running app model

- Pinned entries come from `~/.Dock/*.desktop`.
- Runtime entries are transient and synthesized from:
  - observed windows/processes,
  - user launch hints (`AppRuntimeRegistry`),
  - pending desktop-path hints for startup lag.

## Shortcut model

- Source directory: `~/Desktop`.
- Supports shortcut types:
  - desktop file
  - file/folder
  - web URL
- Slot mapping persists in `~/Desktop/.desktop_shell_slot_map.json`.

## Preferences

`UIPreferences` owns persisted runtime options (motion, drag thresholds, autohide, verbose logging).

## Design constraints

- Keep shell interactions deterministic under polling lag.
- Prefer path-based identity for `.desktop` entries.
- Keep launch + monitoring idempotent so model rebuilds do not drop transient state.

## Portal

- Lihat `docs/PORTAL.md` untuk kontrak `org.slm.Desktop.Portal` (khususnya `OpenURI`), urutan prioritas policy scheme, dan konfigurasi runtime.
