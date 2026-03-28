# Desktop Daemon Architecture (desktopd)

## Tujuan
- Menjadikan kontrol desktop/windowing sebagai service tunggal di user-session.
- Mengurangi logic orchestration di QML/UI.
- Menyediakan API stabil untuk workspace (canonical), overview (legacy alias), appgrid, dan present-window.

## Komponen
- `src/daemon/desktopd/desktopd_main.cpp`
  - Entry point daemon (`QCoreApplication`).
  - Inisialisasi backend dan state model.
  - Menjalankan watchdog healthcheck untuk daemon peer:
    - `org.slm.Desktop.FileOperations`
    - `org.slm.Desktop.Devices`
- `DesktopDaemonService` (`desktopdaemonservice.h/.cpp`)
  - DBus service endpoint.
  - Export method/signal yang dikonsumsi shell atau klien lain.
- `slm-fileopsd` dan `slm-devicesd`
  - Daemon terpisah untuk operasi file dan perangkat.
  - `desktopd` tidak lagi memiliki ownership service devices.
- `WorkspaceManager` (`workspacemanager.h/.cpp`)
  - Domain logic workspace/window presentation.
  - Bridge ke `WindowingBackendManager`, `SpacesManager`, `CompositorStateModel`.
- `WindowingBackendManager` + `KWinWaylandIpcClient` + `KWinWaylandStateModel`
  - Transport command/event dan snapshot window/space dari backend KWin.

## DBus Contract v1
- Service: `org.slm.WorkspaceManager`
- Object: `/org/slm/WorkspaceManager`
- Interface: `org.slm.WorkspaceManager1`

### Legacy Compatibility Alias
- Service: `org.desktop_shell.WorkspaceManager`
- Object: `/org/desktop_shell/WorkspaceManager`
- Interface: `org.desktop_shell.WorkspaceManager1`
- Implementasi: forward ke `WorkspaceManager` yang sama (tanpa duplikasi state).

### Methods
- `Ping() -> a{sv}`
- Canonical workspace methods:
  - `ShowWorkspace()`
  - `HideWorkspace()`
  - `ToggleWorkspace()`
- Legacy alias methods:
  - `ShowOverview()`
  - `HideOverview()`
  - `ToggleOverview()`
- `ShowAppGrid()`
- `ShowDesktop()`
- `PresentWindow(app_id) -> bool`
- `PresentView(view_id) -> bool`
- `CloseView(view_id) -> bool`
- `SwitchWorkspace(index)`
- `SwitchWorkspaceByDelta(delta)`
- `MoveWindowToWorkspace(window, index) -> bool`
- `MoveFocusedWindowByDelta(delta) -> bool`
- `ListRankedApps(limit) -> aa{sv}`
- `DiagnosticSnapshot() -> a{sv}`
- `DaemonHealthSnapshot() -> a{sv}`

### Signals
- Canonical workspace signals:
  - `WorkspaceShown()`
  - `WorkspaceVisibilityChanged(visible)`
- Legacy alias signals:
  - `OverviewShown()`
  - `OverviewVisibilityChanged(visible)`
- `WorkspaceChanged()`
- `WindowAttention(window)`

### Canonical Command/Event Routing
- Command canonical:
  - `workspace on|off|toggle`
- Command legacy fallback:
  - `overview on|off|toggle`
- Event canonical:
  - `workspace-open`
  - `workspace-close`
  - `workspace-toggle`
- Event legacy alias:
  - `overview-open`
  - `overview-close`
  - `overview-toggle`
- `WindowingBackendManager` memetakan command sukses ke event canonical `workspace-*`
  dan menyertakan `legacy_event_alias` untuk kompatibilitas observability lama.

## Session State Contract v1
- Service: `org.slm.SessionState`
- Object: `/org/slm/SessionState`
- Interface: `org.slm.SessionState1`

### Legacy Compatibility Alias
- Service: `org.desktop_shell.SessionState`
- Object: `/org/desktop_shell/SessionState`
- Interface: `org.desktop_shell.SessionState1`
- Implementasi: forward ke `SessionStateManager` yang sama.

### Methods
- `Ping() -> a{sv}`
- `Inhibit(reason, flags) -> uint`
- `Logout()`
- `Shutdown()`
- `Reboot()`
- `Lock()`

### Signals
- `SessionLocked()`
- `SessionUnlocked()`
- `IdleChanged(idle)`
- `ActiveAppChanged(app_id)`

## File Operations Contract v1
- Service: `org.slm.Desktop.FileOperations`
- Object: `/org/slm/Desktop/FileOperations`
- Interface: `org.slm.Desktop.FileOperations`

### Methods
- `Ping() -> a{sv}`
- `Copy(uris, destination) -> id`
- `Move(uris, destination) -> id`
- `Delete(uris) -> id`
- `Trash(uris) -> id`
- `EmptyTrash() -> id`
- `Pause(id) -> bool`
- `Resume(id) -> bool`
- `Cancel(id) -> bool`
- `GetJob(id) -> a{sv}`
- `ListJobs() -> aa{sv}`

### Signals
- `JobsChanged()`
- `Progress(id, percent)`
- `ProgressDetail(id, current, total)`
- `Finished(id)`
- `Error(id)`
- `ErrorDetail(id, code, message)`

### Error Codes (canonical)
- `operation-failed`
- `invalid-destination`
- `cancelled`
- `job-cancelled`
- `copy-move-failed`
- `copy-or-move-failed`
- `delete-trash-failed`
- `delete-or-trash-failed`
- `empty-trash-failed`
- `protected-target`

## Devices Contract v1
- Service: `org.slm.Desktop.Devices`
- Object: `/org/slm/Desktop/Devices`
- Interface: `org.slm.Desktop.Devices`

### Legacy Compatibility Alias
- Service: `org.desktop_shell.Desktop.Devices`
- Object: `/org/desktop_shell/Desktop/Devices`
- Interface: `org.desktop_shell.Desktop.Devices`
- Implementasi: forward ke `DevicesManager` yang sama.

### Methods
- `Ping() -> a{sv}`
- `Mount(target) -> a{sv}`
- `Eject(target) -> a{sv}`
- `Unlock(target, passphrase) -> a{sv}`
- `Format(target, filesystem) -> a{sv}`

### Security Notes
- `Unlock` dan `Format` memerlukan authorisasi PolicyKit via `pkcheck`:
  - action `org.slm.desktop.devices.unlock`
  - action `org.slm.desktop.devices.format`
- Audit log akses sensitif dicatat di:
  - `~/.local/state/SLM/devices-audit.log`

## API Changelog
- See `docs/DBUS_API_CHANGELOG.md`.

## Alur Runtime
1. `desktopd` start.
2. `WindowingBackendManager` connect ke backend (`kwin-wayland`).
3. `WorkspaceManager` membaca state/snapshot dari `CompositorStateModel`.
4. `DesktopDaemonService` register DBus dan expose API.
5. UI/shell memanggil method DBus, daemon meneruskan ke domain manager.

## Boundary
- Daemon fokus orchestration window/workspace.
- UI fokus rendering/interaction.
- Command detail backend tetap disimpan di `WindowingBackendManager`.

## `daemonHealth` Payload Contract
`DiagnosticSnapshot()` menyertakan key `daemonHealth` yang identik dengan output `DaemonHealthSnapshot()`.

### Top-level keys
- `fileOperations: a{sv}`
- `devices: a{sv}`
- `baseDelayMs: int`
- `maxDelayMs: int`
- `degraded: bool`
- `reasonCodes: as` (unique active reason-code list from unhealthy peers)
- `timeline: aa{sv}` (bounded persistent health event timeline)
- `timelineSize: int`
- `timelineFile: string`

### Peer shape (`fileOperations` / `devices`)
- `service: string`
- `path: string`
- `iface: string`
- `registered: bool`
- `failures: int`
- `lastCheckMs: int64` (epoch ms)
- `lastOkMs: int64` (epoch ms, `0` jika belum ada ping sukses)
- `lastError: string` (kosong jika tidak ada fault terakhir)
- `nextCheckInMs: int` (`-1` jika timer tidak aktif)

### Semantic notes
- `registered=false` bisa muncul sesaat saat startup sebelum check pertama selesai.
- Fault yang sudah melewati check biasanya memiliki:
  - `registered=false`
  - `failures>0`
  - `lastError` non-kosong
- `lastCheckMs` harus monotonic per peer antar snapshot.
- `baseDelayMs` dan `maxDelayMs` adalah parameter retry/backoff watchdog.
- `degraded=true` saat minimal satu peer memiliki `failures>0`.
- `timeline` berisi event terstruktur:
  - `tsMs` (epoch ms)
  - `peer` (`monitor`, `fileOperations`, `devices`)
  - `code` (contoh: `monitor-started`, `service-not-registered`, `ping-failed`, `recovered`)
  - `severity` (`info`, `error`, `critical`)
  - `message` (ringkas + actionable)
  - optional: `service`, `failures`, `registered`, `details`

## Next Step
1. Tambah capability negotiation (`supportedCommands`, `supportedEvents`) di `DiagnosticSnapshot`.
2. Tambah retry/backoff policy untuk command gagal.
3. Tambah integration test DBus untuk contract v1.
4. Migrasi pemanggilan workspace dari QML ke DBus client bertahap.

## Operations
- Healthcheck CLI:
  - `workspacectl healthcheck --pretty`
- Workspace CLI:
  - `workspacectl switch <index>`
  - `workspacectl switch-delta <delta>`
  - `workspacectl move <window_or_viewid_or_appid> <index>`
  - `workspacectl move-focused-delta <delta>`
- User service install:
  - `scripts/install-desktopd-user-service.sh`
  - optional explicit binary: `scripts/install-desktopd-user-service.sh /abs/path/to/desktopd`
