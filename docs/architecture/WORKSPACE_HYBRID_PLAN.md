# Workspace + Mission Control Hybrid Plan (Phase-0 Audit)

Dokumen ini adalah hasil audit awal untuk implementasi Workspace + Mission Control hybrid
dengan prinsip **reuse-first** (minim perubahan invasif).

## Scope target
- Rebrand domain UX dari `overview` menjadi `workspace` (bertahap, non-breaking).
- Workspace overview menampilkan:
  - strip workspace horizontal di atas,
  - thumbnail detail hanya untuk workspace aktif di tengah,
  - dock tetap terlihat dan aware lintas workspace.
- Dynamic workspace rules:
  - setiap window tepat 1 workspace,
  - selalu ada trailing empty workspace.

## Reusable components (existing)

### 1) Workspace backend orchestration
- `src/core/workspace/workspacemanager.h`
- `src/core/workspace/workspacemanager.cpp`
- `src/daemon/desktopd/desktopdaemonservice.h`
- `src/daemon/desktopd/desktopdaemonservice.cpp`
- `src/core/workspace/workspacecompatservice.h`
- `src/core/workspace/workspacecompatservice.cpp`

Sudah tersedia:
- switch workspace: `SwitchWorkspace(int)`
- move window ke workspace: `MoveWindowToWorkspace(window, index)`
- present/focus by app/view: `PresentWindow`, `PresentView`
- event bridge `Workspace*` (canonical) + `Overview*` (legacy alias), `WorkspaceChanged`, `WindowAttention`

### 2) Window-to-space mapping
- `src/core/workspace/spacesmanager.h`
- `src/core/workspace/spacesmanager.cpp`

Sudah tersedia:
- mapping `viewId -> space`
- helper: `windowBelongsToActive`, `assignmentSnapshot`
- cleanup assignment stale window: `clearMissingAssignments`

### 3) Compositor state + window model
- `src/core/workspace/windowingbackendmanager.*`
- `src/core/workspace/kwinwaylandipcclient.*`
- `src/core/workspace/kwinwaylandstatemodel.*`

Sudah tersedia:
- command transport (`workspace` canonical + `overview` fallback, `space set`, `space count`, `focus-view`, `close-view`)
- state model windows + activeSpace + spaceCount
- event bridge ke QML

### 4) Existing workspace overview UI and scene wiring
- `Qml/components/WorkspaceOverlay.qml` (canonical)
- `Qml/components/OverviewOverlay.qml` (compat alias)
- `Qml/DesktopScene.qml`

Sudah tersedia:
- overlay workspace overview aktif
- strip space basic (`S1..Sn`) + add workspace
- filtering windows by active workspace
- sinkronisasi `SpacesManager` <-> `CompositorStateModel`

### 5) Dock runtime integration
- `Qml/components/Dock.qml`
- `Qml/components/dock/DockAppDelegate.qml`
- `dockmodel.h`
- `dockmodel.cpp`
- `src/core/execution/appcommandrouter.cpp`

Sudah tersedia:
- app activation route via `AppCommandRouter` / `AppExecutionGate`
- quick actions pada context menu dock
- indikator running/focused

### 6) Animation and gesture infra
- `src/core/motion/*`
- `Qml/DesktopScene.qml` (workspace swipe channel: `workspace.swipe.offset`)

Sudah tersedia:
- motion controller channel-based
- swipe-to-switch workspace pada normal mode
- settle animation interruptible

## Gaps vs target behavior

1) **Dynamic workspace invariant belum terpenuhi**
- `SpacesManager` masih fixed range (`spaceCount` clamp 1..12) + no trailing-empty enforcement.

2) **Workspace strip belum “workspace strip model”**
- Saat ini strip dibangun langsung dari `SpacesManager.spaces()`, belum ada occupancy/state model tersendiri.

3) **Window drag thumbnail ke workspace strip belum ada**
- `WorkspaceOverlay.qml` belum punya DnD thumbnail -> tab workspace.

4) **Dock click lintas workspace belum eksplisit terkontrak**
- `PresentWindow` sudah melakukan switch ke space window target, tapi belum ada layer kontrak khusus dock-centric behavior.

5) **Shortcut contract target belum sesuai requirement baru**
- Requirement minta `Super+S`, `Super+Left/Right`, `Ctrl+Super+Left/Right`, `Esc`.
- Existing defaults masih dominan `Alt+...` di preferences/compositor key binding.

6) **Terminologi legacy `overview` masih ada**
- API DBus/signal/command sudah workspace-first di jalur utama, tetapi alias legacy masih dipertahankan untuk kompatibilitas.

## Minimal new components (additive, non-invasive)

### 1) `WindowWorkspaceBinding` (backend helper)
Tujuan:
- centralize invariant logic untuk mapping window->workspace dan dynamic workspace auto-maintain.

Implementasi minimal:
- file baru:
  - `src/core/workspace/windowworkspacebinding.h`
  - `src/core/workspace/windowworkspacebinding.cpp`
- dipakai `WorkspaceManager` + `SpacesManager` (composition, bukan rewrite besar).

### 2) `WorkspaceStripModel` (backend view-model ringan)
Tujuan:
- expose state strip (active, occupied, trailing placeholder) tanpa live preview mahal.

Implementasi minimal:
- file baru:
  - `src/core/workspace/workspacestripmodel.h`
  - `src/core/workspace/workspacestripmodel.cpp`
- QML consume via context object seperti model lain.

### 3) `WindowThumbnailLayoutEngine` (QML-first, optional C++ later)
Tujuan:
- layout thumbnail adaptif + target geometry untuk anim.

Implementasi minimal:
- phase awal cukup JS/QML helper di `WorkspaceOverlay.qml` (non-invasive).
- promote ke C++ jika performa butuh.

### 4) `DockWorkspaceIntegration` (thin adapter)
Tujuan:
- explicit behavior dock click lintas workspace.

Implementasi minimal:
- file baru:
  - `src/core/workspace/dockworkspaceintegration.h`
  - `src/core/workspace/dockworkspaceintegration.cpp`
- wrapper tipis ke `WorkspaceManager::PresentWindow/PresentView`.

### 5) `MultitaskingController` (state coordinator)
Tujuan:
- satukan state `normal <-> workspace overview` dan menjaga overview tetap terbuka saat switch via strip.

Implementasi minimal:
- bisa mulai sebagai ekstraksi bertahap dari logic state di `Qml/DesktopScene.qml`.

## Rebranding strategy (`overview` -> `workspace`)

Prinsip:
- non-breaking dulu, rename bertahap.

Tahap:
1. Tambah alias method/signal/property baru (`ShowWorkspace`, dst) di backend.
2. Pertahankan method lama `ShowOverview` sebagai compat shim.
3. QML migrasi consumer bertahap ke nama baru.
4. Update `GetCapabilities()` agar key workspace jadi canonical dan key overview jadi legacy alias.
5. Catat deprecation di changelog.

## Keyboard/input migration notes

Target requirement:
- `Super+S` toggle workspace overview
- `Super+Left/Right` switch workspace
- `Ctrl+Super+Left/Right` move focused window
- `Esc` close workspace overview

Current state:
- `Esc` sudah handled di `WorkspaceOverlay.qml`
- workspace swipe gesture sudah ada di `DesktopScene.qml`
- binding low-level shortcut perlu update di backend/compositor binding layer (saat ini default banyak `Alt+...`).

## Integration note: reused vs new

### Reused (tanpa redesign mayor)
- `WorkspaceManager`
- `SpacesManager`
- `WindowingBackendManager`
- `KWinWaylandIpcClient`
- `KWinWaylandStateModel`
- `WorkspaceOverlay.qml` (+ `OverviewOverlay.qml` sebagai compat wrapper)
- `DesktopScene.qml`
- Dock stack (`Dock.qml`, `DockAppDelegate.qml`, `DockModel`)
- Motion framework (`src/core/motion/*`)

### New (additive)
- `WindowWorkspaceBinding`
- `WorkspaceStripModel`
- `DockWorkspaceIntegration`
- (optional later) `MultitaskingController` ekstraksi dari QML state
- (optional later) `WindowThumbnailLayoutEngine` C++ jika dibutuhkan performa

## Recommended patch order (production-oriented)
1. Backend invariant foundation (`WindowWorkspaceBinding` + `SpacesManager` dynamic rules).
2. Workspace strip model + QML wiring (pakai `WorkspaceOverlay.qml`).
3. DnD thumbnail -> workspace tab (termasuk trailing placeholder create).
4. Dock workspace integration adapter.
5. Rebranding alias API `overview` -> `workspace`.
6. Shortcut migration ke target combo.
7. Hardening tests (unit + DBus + QML smoke + E2E path).

## Risks & guardrails
- Risiko regressi behavior existing overview/dock:
  - mitigasi: alias compatibility + test contract existing.
- Risiko performa overview:
  - mitigasi: strip ringan (occupancy only), cached preview, no live previews default.
- Risiko drift source-of-truth space mapping:
  - mitigasi: satu binding helper (`WindowWorkspaceBinding`) + explicit invariant tests.
