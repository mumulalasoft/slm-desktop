# Desktop Integration Contracts

Dokumen ini adalah kontrak integrasi operasional lintas shell, app model, workspace, storage, file operations, search, notifications, dan settings.

## 1. State Owner Matrix (SSOT)

| State | Owner |
|---|---|
| `activeWorkspaceId` | `desktop-workspaced` |
| `workspaceIds` | `desktop-workspaced` |
| `workspaceWindowMap` | `desktop-workspaced` |
| `activeWindowId` | `desktop-shelld` |
| `activeAppId` | `desktop-shelld` |
| `runningApps` | `desktop-appd` |
| `pinnedApps` | `desktop-appd` |
| `appWindowMap` | `desktop-appd` |
| `launchpadVisible` | `desktop-shelld` |
| `searchVisible` | `desktop-shelld` |
| `searchQuery` | `desktop-shelld` (broker to `desktop-searchd`) |
| `dockVisibilityMode` | `desktop-settingsd` |
| `dockHoveredItem` | `desktop-shelld` |
| `dockExpandedItem` | `desktop-shelld` |
| `dragSession` | `desktop-shelld` |
| `mountedVolumes` | `desktop-storaged` |
| `recentFiles` | `desktop-storaged` |
| `recentFolders` | `desktop-storaged` |
| `selectedObjects` | `desktop-shelld` |
| `fileOperationQueue` | `desktop-fileopsd` |
| `notificationQueue` | `desktop-notifyd` |
| `themeMode` | `desktop-settingsd` |
| `accentColor` | `desktop-settingsd` |
| `iconTheme` | `desktop-settingsd` |
| `reduceMotion` | `desktop-settingsd` |
| `sessionRestoreState` | `desktop-shelld` |

Rule:
- Setiap state di atas hanya boleh memiliki satu owner.
- UI/QML hanya consumer, bukan owner lintas-komponen.

## 2. Layer Separation

- Service layer:
  - `desktop-appd`, `desktop-workspaced`, `desktop-storaged`, `desktop-fileopsd`, `desktop-searchd`, `desktop-notifyd`, `desktop-settingsd`, `desktop-shelld`
- State layer:
  - state global dibaca melalui service contracts (DBus/client bridge), bukan state lokal per-UI
- UI layer:
  - Dock, Launchpad, Shell, FileManager, TopBar bertindak sebagai renderer/interactor
- IPC/Event contracts:
  - kontrak event minimum ada pada section 4

## 3. Service Contracts (Current Runtime)

Status service saat ini di codebase:
- App registry model: `desktop-appd` (`src/daemon/appd/*`)
- Storage SSOT: `org.slm.Desktop.Storage` (`src/daemon/devicesd/storageservice.*`)
- File operations SSOT: `org.slm.Desktop.FileOperations` (`src/apps/filemanager/ops/fileoperationsservice.*`)
- Settings SSOT: `org.slm.Desktop.Settings` (`src/services/settingsd/settingsservice.*`)
- Search broker internal: `org.slm.Desktop.Search.v1` (`src/daemon/portald/portalservice.cpp`)

Planned/ongoing integration:
- `desktop-workspaced` sebagai SSOT workspace/focus mapping
- `desktop-actiond` sebagai provider aksi lintas-komponen
- `desktop-notifyd` sebagai queue notifikasi/badge global
- `desktop-shelld` sebagai orkestrator layer/focus/activation/drag session

## 4. Event Contracts (Minimum)

| Event | Owner | Primary Consumers |
|---|---|---|
| `app.launched` | `desktop-appd` | shell, dock, search |
| `app.activated` | `desktop-shelld` | dock, workspace UI |
| `app.closed` | `desktop-appd` | shell, dock |
| `workspace.switched` | `desktop-workspaced` | shell, dock, overview |
| `window.active.changed` | `desktop-shelld` | dock, topbar, workspace |
| `dock.item.invoked` | `desktop-shelld` | appd/workspaced router |
| `launchpad.opened` | `desktop-shelld` | shell overlays |
| `launchpad.closed` | `desktop-shelld` | shell overlays |
| `drag.started` | `desktop-shelld` | dock, filemanager, workspace |
| `drag.updated` | `desktop-shelld` | dock, filemanager, workspace |
| `drag.ended` | `desktop-shelld` | dock, filemanager, workspace |
| `fileop.started` | `desktop-fileopsd` | notify, shell, filemanager |
| `fileop.progress` | `desktop-fileopsd` | notify, shell, filemanager |
| `fileop.completed` | `desktop-fileopsd` | notify, shell, filemanager |
| `fileop.failed` | `desktop-fileopsd` | notify, shell, filemanager |
| `mount.added` | `desktop-storaged` | shell, filemanager |
| `mount.removed` | `desktop-storaged` | shell, filemanager |
| `recent.changed` | `desktop-storaged` | dock, search, filemanager |
| `notification.posted` | `desktop-notifyd` | shell, dock |
| `notification.cleared` | `desktop-notifyd` | shell, dock |
| `settings.changed` | `desktop-settingsd` | shell, dock, launchpad, filemanager |

## 5. Integration Guard Rules

- Dock tidak boleh menjadi owner `pinnedApps`/`runningApps`.
- Launchpad tidak boleh menyimpan app registry lokal.
- FileManager tidak boleh menjadi owner `mountedVolumes` global.
- Search UI mana pun tidak boleh memakai backend terpisah dari broker global.
- Semua perubahan owner state wajib update dokumen ini.
