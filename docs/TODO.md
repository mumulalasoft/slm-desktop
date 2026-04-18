# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Fokus Aktif (Prioritas Tinggi)

### Storage & Automount
- [ ] QA cold reboot penuh (bukan hanya restart service) untuk persistence policy mount/automount.
- [ ] QA manual lintas kondisi media: locked, busy, unsupported FS, burst multi-partition.

### Startup Performance Desktop
- [ ] Turunkan waktu `qml.load.begin -> main.onCompleted` dengan profiling terarah per komponen.

---

## Program Network & Firewall

### Phase 1 (Wajib)
- [x] NftablesAdapter: jalankan `nft -f -` secara nyata.
- [x] PolicyEngine: reconcile fullstate setelah remove/clear IP policy.
- [x] UI Settings: Security -> Firewall smoke test contract.
  - verified via:
    - `firewallservice_dbus_contract_test`
    - `firewall_policyengine_contract_test`
    - `firewall_nftables_adapter_test`
    - `firewallserviceclient_contract_test`
    - `security_settings_routing_js_test`

### Phase 2
- [ ] Outgoing control (allow/block/prompt) berbasis app identity.
- [ ] Deteksi proses CLI/interpreter (python/node script target, parent/cwd/tty/cgroup).
- [ ] Popup policy yang kontekstual dan tidak spam.

### Phase 3
- [ ] Trust system (System/Trusted/Unknown/Suspicious) + policy adaptif.
- [ ] Behavior learning + auto suggestion.
- [ ] Quarantine mode untuk app mencurigakan.

---

## Notification
- [ ] Hardening multi-monitor behavior.

## Parking Lot (Tidak Aktif Sekarang)
- Struktur Dock: **dibekukan** pada state "last good" sampai siklus pengujian berikutnya.
- Rencana refactor arsitektur shell yang besar: lanjut setelah milestone stabilitas/firewall tercapai.

---

## Program (Hybrid Recovery System: Unbreakable Wayland Desktop)

### Execution Progress (Current)
- [~] `slm-recoveryd` core daemon scaffold implemented (DBus service, health reporting, crash-loop tracking, auto-recovery escalation).
- [~] Build integration added for `slm-recoveryd` (`cmake/Sources.cmake`, `CMakeLists.txt`, startup bootstrap in `main.cpp`).
- [~] User service assets added (`scripts/systemd/slm-recoveryd.service`, `scripts/install-recoveryd-user-service.sh`).
- [~] Escalation wiring integrated in login stack (`session-broker` reads recovery marker; `watchdog` clears stale recovery flags on healthy session).
- [~] Recovery ops tooling added (`scripts/recovery/recoveryctl.sh`, `scripts/recovery/request-bootloader-recovery.sh`).
- [~] `slm-recoveryd` bootloader hook added (optional helper via `SLM_RECOVERY_BOOTLOADER_HELPER`).
- [~] Contract test baseline added (`tests/recoveryservice_contract_test.cpp`).
- [~] Initial architecture output document added: `docs/architecture/HYBRID_RECOVERY_ARCHITECTURE.md`.
- [~] In-system Safe Mode page integrated (`Qml/recovery/SafeModePage.qml`) with simple-clean action UI and wired backend actions (restart/reset/disable-extension/reset-graphics/terminal/network/snapshot routing).
- [~] Recovery partition runtime pipeline ditingkatkan: build image + deploy-to-partition + boot entry install dengan `PARTUUID/UUID` root spec (`build-recovery-partition-image.sh`, `deploy-recovery-partition.sh`, `install-recovery-boot-entry.sh`, `detect-recovery-boot-entry.sh`).
- [~] Smoke non-destruktif pipeline recovery ditambahkan (`smoke-recovery-partition-pipeline.sh`) dan dipanggil dari `smoke-login-recovery.sh`.
- [~] Full safe-mode/recovery UI flow expanded per strict action list (Safe Mode now includes Repair System and Reinstall System actions; remaining gap: dedicated recovery-partition runtime integration and end-to-end validation).
- [~] Snapshot diff preview and guarded rollback execution from UI (`RecoveryApp.previewSnapshotDiff`, confirmation-gated restore flow in `Qml/recovery/SnapshotPage.qml`).

### TITLE
Design and Implement a Hybrid Recovery System for an "Unbreakable" Wayland Desktop OS

### GOAL
Design a fully integrated recovery architecture that guarantees:

* The system NEVER becomes unusable
* The user ALWAYS has access to a graphical recovery interface
* No scenario results in a black screen or terminal-only lockout

The system must combine:

1. In-system recovery (primary, seamless UX)
2. Dedicated recovery partition (low-level fallback)

---

### 1. ARCHITECTURE OVERVIEW

Design a hybrid system consisting of:

A. Main OS (Root System)
B. Recovery Partition (Independent minimal OS)
C. Shared Data Access Layer
D. Snapshot System (Btrfs or equivalent)
E. slm-recoveryd (core recovery daemon)

---

### 2. DISK LAYOUT (STRICT REQUIREMENT)

Define partition layout:

* EFI System Partition (FAT32)
* Root Partition (Btrfs recommended)
* User Data (optional separate subvolume)
* Recovery Partition (EXT4 or SquashFS-based)

Recovery Partition MUST:

* Be bootable independently
* Not depend on root filesystem
* Be protected from user modification

---

### 3. slm-recoveryd (MANDATORY DAEMON)

Implement a system daemon with responsibilities:

A. Health Monitoring:

* compositor startup success
* shell process stability
* login success/failure
* GPU/Wayland initialization
* critical services (network, portal, greeter)

B. Crash Loop Detection:

* if compositor OR shell fails >3 times within 10 seconds → trigger recovery state

C. Automatic Recovery Actions:

* revert last config change
* disable last extension/module
* fallback to default theme/UI
* restart affected services

D. Escalation Logic:

* if recovery fails → trigger Safe Mode
* if Safe Mode fails → signal bootloader to enter Recovery Partition

---

### 4. IN-SYSTEM RECOVERY (PRIMARY UX)

Must operate inside main OS.

#### SAFE MODE REQUIREMENTS:

* Launch minimal session:

  * software rendering (llvmpipe fallback)
  * no blur / heavy effects
  * minimal Qt/QML UI

#### SAFE MODE UI MUST INCLUDE:

* Restart Desktop
* Reset Desktop Settings (non-destructive)
* Disable Extensions
* Reset Graphics Stack
* Rollback Snapshot
* Open Terminal (advanced users)
* Network Repair

#### RULE:

Safe Mode MUST NOT depend on:

* GPU acceleration
* user configuration
* external plugins

---

### 5. RECOVERY PARTITION (FALLBACK SYSTEM)

#### CONTENTS:

A minimal independent OS including:

* init system (systemd or minimal init)
* Wayland compositor (fallback, wlroots-based preferred)
* Qt/QML lightweight UI
* slm-recoveryd (recovery mode)
* disk tools:

  * mount
  * fsck
  * btrfs tools
* network tools (DHCP + WiFi)

#### BOOT REQUIREMENTS:

* Bootable via bootloader fallback entry
* Automatically triggered if:

  * main OS fails to boot
  * kernel panic
  * root filesystem corruption

---

### 6. RECOVERY PARTITION RESPONSIBILITIES

Once booted, it MUST:

1. Detect and mount main root filesystem

2. Detect snapshot system

3. Provide UI actions:

   * Repair System
   * Rollback Snapshot
   * Reset Desktop Only
   * Reinstall System
   * Open Terminal

4. Ensure safe operations:

   * no destructive action without confirmation
   * preview changes before rollback

---

### 7. SNAPSHOT SYSTEM (CRITICAL)

Use Btrfs (preferred) OR equivalent.

#### AUTO SNAPSHOT TRIGGERS:

* before system update
* before driver install
* before major config change
* before package removal of critical components

#### SNAPSHOT NAMING:

Examples:

* "Before NVIDIA Driver Install"
* "Before System Update"
* "Before Theme Change"

#### ROLLBACK FLOW:

* user selects snapshot
* system shows diff preview (basic)
* confirm action
* perform rollback
* reboot

---

### 8. GPU FAILURE HANDLING

STRICT REQUIREMENTS:

* If Wayland compositor fails → fallback to software rendering
* If GPU driver is broken → force llvmpipe
* GPU MUST NEVER block UI from appearing

---

### 9. PROTECTED COMPONENTS

Prevent removal or corruption of:

* compositor
* shell
* greeter
* portal backend
* network manager

If user attempts removal:

* show warning dialog
* require explicit override

---

### 10. BOOT FLOW (STRICT)

Implement:

NORMAL:
Boot → Main OS → Desktop

FAILURE LEVEL 1:
Crash → slm-recoveryd → auto fix

FAILURE LEVEL 2:
Repeated crash → Safe Mode

FAILURE LEVEL 3:
Boot failure → Recovery Partition

FAILURE LEVEL 4 (optional):
No local recovery → Internet Recovery

RULE:
User MUST ALWAYS reach a graphical interface.

---

### 11. INTERNET RECOVERY (OPTIONAL ADVANCED)

If enabled:

* detect no valid system
* connect to WiFi
* download recovery image
* reinstall OS

---

### 12. UI REQUIREMENTS

ALL recovery UIs MUST:

* be graphical (NO terminal-first design)
* be consistent with desktop branding
* be usable via mouse and keyboard
* avoid GPU-heavy effects

---

### 13. DEVELOPER MODE

Provide tools:

* simulate crash
* force safe mode
* view logs
* test rollback

---

### 14. OUTPUT REQUIRED

Agent MUST produce:

1. Architecture diagram
2. Partition layout
3. Bootloader integration (GRUB/systemd-boot)
4. systemd service definitions
5. slm-recoveryd design
6. Safe Mode QML UI
7. Recovery Partition UI
8. Snapshot integration plan

---

### HARD RULES (NON-NEGOTIABLE)

* NEVER allow black screen without fallback
* NEVER rely on GPU for recovery UI
* NEVER require terminal for basic recovery
* ALWAYS provide at least one working graphical path

END.

---

## NEW SECTION — Desktop View As Core System Component

Implement a Desktop View system as a core part of the shell architecture.

This is NOT a visual feature.
This is a SYSTEM COMPONENT.

The Desktop View MUST be implemented as a File Manager instance rendering the directory ~/Desktop.

---

========================================
CORE RULE (NON-NEGOTIABLE)
========================================

Desktop View = FileManager instance (path: ~/Desktop)

NOT:
- wallpaper layer
- fallback UI
- special empty state
- static icon renderer

FAIL if:
- Desktop does not use FileManager backend
- Menu is not coming from FileManager
- Desktop has no real filesystem binding

---

========================================
ARCHITECTURE OVERVIEW
========================================

You MUST implement these components:

1. DesktopSurface (QML layer)
2. FileManager Backend (core logic)
3. DesktopViewController (state manager)
4. DesktopMenuProvider (menu source)
5. Integration with global-menud

---

========================================
1. DESKTOP SURFACE (QML)
========================================

Responsibilities:
- Render icons from ~/Desktop
- Handle mouse interaction:
  - click
  - selection
  - drag
  - context menu trigger
- Display grid layout

Constraints:
- MUST NOT be a normal window
- MUST be part of shell scene
- MUST sit above wallpaper and below application windows

FAIL if:
- DesktopSurface is implemented as a standalone window
- Uses WindowStaysOnTopHint
- Competes with app windows in stacking

---

========================================
2. FILE MANAGER BACKEND (REQUIRED)
========================================

Responsibilities:
- Read directory ~/Desktop
- Track file list
- Track selection
- Provide actions (open, rename, delete, etc.)
- Provide menu data

Must be reusable:
- same backend used for:
  - desktop view
  - file manager window

FAIL if:
- Desktop uses custom logic separate from FileManager
- Logic duplicated between desktop and file manager

---

========================================
3. DESKTOP VIEW CONTROLLER
========================================

Responsibilities:
- Bind FileManager to DesktopSurface
- Maintain state:

State:
- path = ~/Desktop
- mode = desktop_view
- selection = list of selected items

Must emit:
- selectionChanged
- directoryChanged

---

========================================
4. DESKTOP MENU PROVIDER (CRITICAL)
========================================

You MUST implement a MenuProvider for Desktop View.

Definition:

DesktopMenuProvider:
  appId: filemanager
  role: desktop_view
  path: ~/Desktop
  selection: currentSelection

This provider is used by global-menud.

FAIL if:
- Desktop menu is hardcoded
- Desktop menu is fallback menu
- Menu not based on selection

---

========================================
5. GLOBAL MENU INTEGRATION
========================================

Rules:

IF active window exists:
    use window.menuProvider

ELSE:
    use DesktopMenuProvider

This is REQUIRED.

FAIL if:
- Menu disappears when no app is active
- Menu switches to fallback instead of DesktopMenuProvider

---

========================================
6. MENU BEHAVIOR (DYNAMIC)
========================================

Menu MUST change based on selection.

Case 1: No selection
- New Folder
- Open Terminal Here

Case 2: Single file selected
- Open
- Rename
- Compress
- Move to Trash
- Properties

Case 3: Multiple selection
- Compress
- Move to Trash

FAIL if:
- Menu is static
- Selection has no effect

---

========================================
7. DATA MODEL (REQUIRED)
========================================

FileItem:
- path
- name
- type
- icon
- isExecutable
- isHidden

DesktopState:
- path = ~/Desktop
- items = list<FileItem>
- selection = list<FileItem>

MenuContext:
- selectionCount
- selectionTypes

---

========================================
8. Z-ORDER (STRICT)
========================================

Layer order:

TopBar > Dock > Launchpad > Windows > DesktopSurface > Wallpaper

FAIL if:
- Desktop overlaps windows
- Desktop hidden behind wallpaper
- Menu appears behind desktop

---

========================================
9. INTERACTION MODEL
========================================

- Clicking desktop:
  → Desktop becomes active context

- Clicking icon:
  → updates selection
  → updates menu

- Double click:
  → open file

- Right click:
  → context menu (optional, separate from global menu)

---

========================================
10. RESILIENCE (UNBREAKABLE)
========================================

Must handle:

- ~/Desktop missing → recreate
- FileManager crash → restart backend
- MenuProvider unavailable → retry

Fallback menu is ONLY allowed if:
- FileManager backend fails completely

---

========================================
11. PERFORMANCE
========================================

- No blocking IO on UI thread
- Use async file listing
- Incremental updates

---

========================================
12. FORBIDDEN IMPLEMENTATIONS
========================================

DO NOT:

- Create Desktop as fake UI without FS binding
- Use static list of icons
- Hardcode menu items
- Treat Desktop as empty state
- Skip FileManager backend

---

========================================
SUCCESS CRITERIA
========================================

Implementation is valid ONLY if:

- Desktop renders real files from ~/Desktop
- Menu comes from FileManager (not fallback)
- Menu changes based on selection
- Desktop behaves like an application context
- Global menu works even when no app is open

---

FINAL STATEMENT:

Desktop View MUST behave as:
FileManager(path=~/Desktop, mode=desktop_view)

Anything else is INVALID.

---

## Desktop View Spatial Placement Program (Qt/QML)

### Scope dan Tujuan Produk
- [ ] Desktop View menjadi icon layer modern di atas wallpaper.
- [ ] User dapat memindah icon ke area kosong mana saja secara bebas.
- [ ] Posisi icon stabil, tidak lompat-lompat.
- [ ] Perubahan isi `~/Desktop` tersinkron otomatis.
- [ ] Drop dari Launchpad dan Dock membuat shortcut valid lalu ditempatkan ke slot kosong yang legal.
- [ ] Re-arrange otomatis/manual tersedia.
- [ ] Siap untuk perubahan ukuran layar, work area, scale factor, dan jalur multi-monitor.

### Kontrak Arsitektur Wajib
- [ ] Buat `DesktopView.qml` sebagai root desktop icon layer.
- [ ] Buat `DesktopItem.qml` untuk render icon+label+state interaksi.
- [ ] Sediakan `DesktopModel` dengan field minimum:
  - [ ] `id`
  - [ ] `filePath`
  - [ ] `displayName`
  - [ ] `iconName` / `iconSource`
  - [ ] `itemType`
  - [ ] `screenId` / `monitorId`
  - [ ] `cellX` / `cellY`
  - [ ] `posX` / `posY`
  - [ ] `isPinnedPosition`
  - [ ] `lastKnownOrder`
  - [ ] `sourceType`
  - [ ] `mtime` / `version`
- [ ] Buat `DesktopPlacementEngine` sebagai satu-satunya source of truth posisi.
- [ ] Buat `DesktopFileWatcher` untuk event add/remove/rename/move/metadata + debounce.
- [ ] Buat `DesktopShortcutImporter` untuk drop Launchpad/Dock.
- [ ] Buat `DesktopPositionStore` untuk persist posisi restart-safe.

### Larangan Implementasi (Hard Rule)
- [ ] Jangan gunakan `GridView`/`Grid`/`Flow` sebagai source of truth posisi.
- [ ] Jangan gunakan index delegate sebagai posisi item desktop.
- [ ] Repeater hanya boleh untuk render; `x/y` harus dari PlacementEngine.
- [ ] Posisi logis utama harus `cellX/cellY`, bukan truthy/falsy index.

### Desain Grid Logis (Virtual Grid)
- [ ] Definisikan `cellWidth` dan `cellHeight` tetap.
- [ ] Definisikan `workArea` valid (exclude topbar + reserved margins + policy dock overlap).
- [ ] Hitung `columns = max(1, floor(workAreaWidth / cellWidth))`.
- [ ] Hitung `rows = max(1, floor(workAreaHeight / cellHeight))`.
- [ ] Validasi cell:
  - [ ] `Number.isInteger(cellX)`
  - [ ] `Number.isInteger(cellY)`
  - [ ] `cellX >= 0 && cellX < columns`
  - [ ] `cellY >= 0 && cellY < rows`
- [ ] Render posisi:
  - [ ] `x = workArea.x + cellX * cellWidth`
  - [ ] `y = workArea.y + cellY * cellHeight`

### Bugfix Kritis Row 0 / Column 0
- [ ] Audit semua validasi koordinat, hilangkan pola truthy/falsy.
- [ ] Pastikan tidak ada kondisi seperti:
  - [ ] `if (cellX && cellY)`
  - [ ] `if (!row || !column)`
  - [ ] `if (item.row && item.column)`
- [ ] Gunakan sentinel `-1` atau `null` untuk posisi unset.
- [ ] Jangan pernah gunakan `0` sebagai tanda uninitialized.
- [ ] Tambah test eksplisit untuk cell `(0,0)`, `(0,1)`, `(1,0)`, `(1,1)`.

### Placement Engine API Minimum
- [ ] `computeGrid()`
- [ ] `firstFreeCell()`
- [ ] `nearestFreeCell()`
- [ ] `canPlace(cellX, cellY)`
- [ ] `placeItem(itemId, cellX, cellY)`
- [ ] `moveItem(itemId, targetCellX, targetCellY)`
- [ ] `rearrange(mode)`
- [ ] `reflowOnGeometryChange()`
- [ ] `resolveCollisions()`
- [ ] Deterministik untuk input state yang sama.

### Alur Sinkronisasi Filesystem
- [ ] Startup:
  - [ ] Scan `~/Desktop`
  - [ ] Load persisted positions
  - [ ] Tempatkan item valid pada posisi tersimpan
  - [ ] Item tanpa posisi masuk `firstFreeCell()`
- [ ] File baru: insert model + place slot kosong + persist.
- [ ] File hapus: remove model + kosongkan occupancy.
- [ ] Rename/move: pertahankan posisi bila identity masih match.
- [ ] Tangani atomic rename agar tidak menimbulkan ghost duplicate.

### Integrasi Launchpad dan Dock
- [ ] Definisikan payload drag internal:
  - [ ] `application/x-slm-app-entry`
  - [ ] `application/x-slm-desktop-item`
  - [ ] `text/uri-list` bila relevan
- [ ] Payload minimum:
  - [ ] `appId`
  - [ ] `desktopFilePath`
  - [ ] `iconName`
  - [ ] `displayName`
  - [ ] `exec`
  - [ ] `source`
- [ ] Validasi payload sebelum create shortcut.
- [ ] Buat shortcut aman (`.desktop` wrapper bila perlu).
- [ ] Tempatkan ke target cell / nearest free cell.
- [ ] Persist posisi dan refresh model inkremental.

### Re-arrange dan Snap
- [ ] Implement action:
  - [ ] Sort by Name
  - [ ] Sort by Type
  - [ ] Sort by Date Modified
  - [ ] Snap to Grid
  - [ ] Clean Up Desktop
- [ ] Semua operasi re-arrange wajib lewat PlacementEngine.
- [ ] Fill mode minimal:
  - [ ] `vertical-first`
  - [ ] `horizontal-first`
- [ ] Anchor minimal:
  - [ ] `top-left`
  - [ ] `top-right`
  - [ ] `bottom-left`
  - [ ] `bottom-right`

### Interaksi Drag & Drop
- [ ] Drag preview jelas (ghost/placeholder).
- [ ] Target cell di-highlight saat drag.
- [ ] Snap preview ke grid.
- [ ] Commit posisi hanya saat drop, bukan per pixel.
- [ ] Drop invalid area harus revert ke posisi awal.

### Persistensi Posisi
- [ ] Simpan `identityKey`, `canonicalPath`, `cellX`, `cellY`, `monitor/workArea signature`, `timestamp`.
- [ ] Restore posisi saat restart shell.
- [ ] Clamp/remap saat grid berubah (resolusi/work area/scale).
- [ ] Jika slot tidak valid, gunakan `nearestFreeCell`.

### Checklist Uji Wajib
- [ ] Item pertama di `(0,0)` tampil benar.
- [ ] Drag item ke row `0` berhasil.
- [ ] Drag item ke column `0` berhasil.
- [ ] Drop shortcut dari Launchpad ke desktop berhasil.
- [ ] Drop shortcut dari Dock ke desktop berhasil.
- [ ] File baru di `~/Desktop` muncul otomatis.
- [ ] File dihapus dari `~/Desktop` hilang otomatis.
- [ ] Restart shell mempertahankan posisi.
- [ ] Resize/resolution change tidak menumpuk item di `(0,0)`.
- [ ] Tidak ada item hilang karena koordinat `0` dianggap falsy.
- [ ] Re-arrange tidak merusak posisi item.
- [ ] Snap to Grid tidak menyebabkan overlap.

### Deliverable Implementasi
- [ ] Penjelasan arsitektur singkat.
- [ ] Struktur file komponen.
- [ ] Implementasi `DesktopView.qml`.
- [ ] Implementasi `DesktopItem.qml`.
- [ ] Implementasi `DesktopPlacementEngine` (C++ preferred, JS helper acceptable jika rapi).
- [ ] Integrasi watcher `~/Desktop`.
- [ ] Integrasi drop Launchpad dan Dock.
- [ ] Integrasi penyimpanan posisi persisten.
- [ ] Checklist bugfix row 0 / column 0.
- [ ] Skenario test manual.

### Milestone Eksekusi (M1/M2/M3)

#### M1 - Fondasi Spatial Placement (Target: Week 1)
- [ ] Buat `DesktopPlacementEngine` deterministik + occupancy map 2D.
- [ ] Buat `DesktopView.qml` tanpa `GridView/Grid/Flow` sebagai source posisi.
- [ ] Buat `DesktopItem.qml` (render, select, hover, drag state).
- [ ] Implement virtual grid metrics (`cellWidth`, `cellHeight`, `workArea`, `columns`, `rows`).
- [ ] Implement render absolut `x/y` dari `cellX/cellY`.
- [ ] Tambah guard koordinat eksplisit (`>= 0`, integer, within bounds).
- [ ] DoD M1:
  - [ ] Item `(0,0)` selalu tampil.
  - [ ] Tidak ada validasi truthy/falsy untuk koordinat.
  - [ ] Drag ke row 0/column 0 valid.

#### M2 - Sinkronisasi FS + Persistensi + DnD Integrasi (Target: Week 2)
- [ ] Implement `DesktopFileWatcher` debounced (100-300ms).
- [ ] Implement `DesktopPositionStore` (identity stabil + restore posisi).
- [ ] Implement startup flow: scan desktop -> load posisi -> resolve collision -> place missing.
- [ ] Integrasi drop payload Launchpad (`application/x-slm-app-entry`).
- [ ] Integrasi drop payload Dock (`application/x-slm-desktop-item`).
- [ ] Implement `DesktopShortcutImporter` + pembuatan shortcut aman.
- [ ] DoD M2:
  - [ ] File add/remove/rename/move di `~/Desktop` sinkron tanpa full reset.
  - [ ] Restart shell mempertahankan posisi.
  - [ ] Drop Launchpad/Dock membuat item valid dan langsung terpasang.

#### M3 - Re-arrange, Hardening, QA (Target: Week 3)
- [ ] Implement `Snap to Grid` dan `Clean Up Desktop`.
- [ ] Implement sort: Name, Type, Date Modified lewat PlacementEngine.
- [ ] Implement `reflowOnGeometryChange()` untuk resize/scale/workarea changes.
- [ ] Tambahkan test manual checklist penuh + regression guard row/column 0.
- [ ] Hardening edge case: atomic rename, burst FS events, collision recovery.
- [ ] DoD M3:
  - [ ] Tidak ada selisih jumlah render vs isi `~/Desktop`.
  - [ ] Tidak ada item hilang di row 0 / column 0.
  - [ ] Re-arrange dan snap stabil tanpa overlap.

### Owner dan Estimasi (Hari Kerja)

#### M1 - Owner Matrix
- [ ] `DesktopPlacementEngine` (C++).
  - Owner: Core Shell Engineer
  - Estimasi: 2.5 hari
- [ ] `DesktopView.qml` (absolute placement + interaction container).
  - Owner: QML UI Engineer
  - Estimasi: 2 hari
- [ ] `DesktopItem.qml` (icon, label, hover/select/drag visual).
  - Owner: QML UI Engineer
  - Estimasi: 1.5 hari
- [ ] Guard koordinat + bugfix row/column 0 audit.
  - Owner: Core Shell Engineer
  - Estimasi: 1 hari
- [ ] Integrasi dasar engine <-> view + smoke test.
  - Owner: Core Shell Engineer + QA Engineer
  - Estimasi: 1 hari
- [ ] Total estimasi M1.
  - Owner: Team Desktop View
  - Estimasi: 8 hari

#### M2 - Owner Matrix
- [ ] `DesktopFileWatcher` + debounce + event classifier.
  - Owner: Platform/Filesystem Engineer
  - Estimasi: 1.5 hari
- [ ] `DesktopPositionStore` + restore migration path.
  - Owner: Platform/Filesystem Engineer
  - Estimasi: 1.5 hari
- [ ] Startup reconciliation flow (scan/load/place/collision).
  - Owner: Core Shell Engineer
  - Estimasi: 1.5 hari
- [ ] `DesktopShortcutImporter` + shortcut creation policy.
  - Owner: Integration Engineer
  - Estimasi: 1.5 hari
- [ ] Integrasi drop payload Launchpad + Dock.
  - Owner: Integration Engineer + QML UI Engineer
  - Estimasi: 1 hari
- [ ] Regression pass sinkronisasi `~/Desktop`.
  - Owner: QA Engineer
  - Estimasi: 1 hari
- [ ] Total estimasi M2.
  - Owner: Team Desktop View
  - Estimasi: 8 hari

#### M3 - Owner Matrix
- [ ] Re-arrange actions (sort/snap/cleanup) via PlacementEngine.
  - Owner: Core Shell Engineer
  - Estimasi: 2 hari
- [ ] Geometry reflow (resize, scale factor, work area changes).
  - Owner: Core Shell Engineer
  - Estimasi: 1.5 hari
- [ ] Hardening edge case (atomic rename, burst event, collision recovery).
  - Owner: Platform/Filesystem Engineer
  - Estimasi: 1.5 hari
- [ ] Full manual checklist execution + bug triage.
  - Owner: QA Engineer
  - Estimasi: 2 hari
- [ ] Stabilization fixes dari hasil QA.
  - Owner: Team Desktop View
  - Estimasi: 1 hari
- [ ] Total estimasi M3.
  - Owner: Team Desktop View
  - Estimasi: 8 hari

#### Ringkasan Durasi Program
- [ ] Total estimasi implementasi + hardening.
  - Owner: Tech Lead Desktop
  - Estimasi: 24 hari kerja (3 milestone x 8 hari)
- [ ] Buffer risiko integrasi lintas komponen.
  - Owner: Tech Lead Desktop
  - Estimasi: 3-5 hari kerja tambahan

### Kanban Ringkas (Harian)

#### Backlog
- [ ] P0 - M1: `DesktopPlacementEngine` deterministik + occupancy map. (Owner: Core Shell Engineer)
- [ ] P0 - M1: `DesktopView.qml` absolute placement tanpa `GridView/Grid/Flow`. (Owner: QML UI Engineer)
- [ ] P0 - M1: Audit row 0 / column 0 (hapus truthy/falsy coordinate checks). (Owner: Core Shell Engineer)
- [ ] P0 - M1: `DesktopItem.qml` state lengkap (select/hover/drag/rename-ready). (Owner: QML UI Engineer)
- [ ] P1 - M2: `DesktopFileWatcher` debounced + event classifier. (Owner: Platform/Filesystem Engineer)
- [ ] P1 - M2: `DesktopPositionStore` + restore/migration posisi. (Owner: Platform/Filesystem Engineer)
- [ ] P1 - M2: `DesktopShortcutImporter` + drop Launchpad/Dock. (Owner: Integration Engineer)
- [ ] P2 - M3: Re-arrange (sort/snap/cleanup) via PlacementEngine. (Owner: Core Shell Engineer)
- [ ] P2 - M3: Geometry reflow hardening (resize/scale/workarea). (Owner: Core Shell Engineer)
- [ ] P2 - M3: QA full checklist + stabilization. (Owner: QA Engineer)

#### In Progress
- [ ] Investigasi mismatch render init (`~/Desktop` jumlah 4, render 3) dengan fokus bug slot `row 0 / column 0`.
  - Owner: Core Shell Engineer
  - Target output: patch penempatan deterministik startup + verifikasi manual ulang.
- [ ] Migrasi desktop surface ke source of truth placement engine (hilangkan ketergantungan posisi pada flow delegate grid bawaan).
  - Owner: QML UI Engineer
  - Target output: `DesktopView.qml` + `DesktopItem.qml` awal terintegrasi.
- [ ] Definisikan kontrak payload drop Launchpad/Dock dan importer shortcut tunggal.
  - Owner: Integration Engineer
  - Target output: dokumen kontrak + adaptor import awal.

#### Done
- [x] Menambahkan program implementasi Desktop View spatial placement lengkap beserta checklist teknis.
  - Artefak: `docs/TODO.md` section `Desktop View Spatial Placement Program (Qt/QML)`.
- [x] Menambahkan milestone eksekusi `M1/M2/M3` + definition of done per fase.
  - Artefak: `docs/TODO.md` section `Milestone Eksekusi (M1/M2/M3)`.
- [x] Menambahkan owner matrix dan estimasi hari kerja.
  - Artefak: `docs/TODO.md` section `Owner dan Estimasi (Hari Kerja)`.
- [x] Menambahkan Kanban ringkas harian untuk tracking implementasi.
  - Artefak: `docs/TODO.md` section `Kanban Ringkas (Harian)`.

#### Blocked
- [ ] Bug klasik render `row 0 / column 0` masih menyebabkan selisih jumlah item pada fase init.
  - Sebab blokir: arsitektur render saat ini masih memadukan state slot-map lama dengan pipeline delegate berbasis grid visual.
  - Mitigasi: percepat migrasi ke placement engine sebagai source of truth tunggal; startup placement wajib sekuensial-deterministik.
  - ETA unblock: setelah delivery M1 + verifikasi checklist `(0,0)`.

### Ritme Eksekusi Harian

#### WIP Limit
- [ ] Maksimal 3 item di `In Progress` dalam waktu bersamaan.
- [ ] Minimal 1 item `P0` aktif sampai blocker utama (`row 0 / column 0`) closed.
- [ ] Item baru tidak boleh masuk `In Progress` sebelum ada item selesai atau dipindah ke `Blocked`.

#### Template Standup (Isi Harian)
- [ ] Yesterday:
  - [ ] Item yang selesai (link commit/PR/test).
- [ ] Today:
  - [ ] 1-3 item aktif (harus referensi ke Backlog P0/P1/P2).
- [ ] Blockers:
  - [ ] Hambatan teknis/depedensi dan owner unblock.
- [ ] ETA:
  - [ ] Perkiraan selesai item aktif.

#### Aturan Update Board
- [ ] Update `In Progress` saat mulai kerja (bukan akhir hari).
- [ ] Update `Done` hanya jika ada bukti artefak (commit/PR/test pass).
- [ ] Update `Blocked` maksimal 15 menit setelah hambatan teridentifikasi.
- [ ] Review prioritas `P0/P1/P2` setiap akhir hari kerja.

### Risk Register dan Mitigasi

#### Risiko Teknis Utama
- [ ] R1: Slot-map legacy menyebabkan posisi awal tidak deterministik.
  - Dampak: Selisih jumlah render, item hilang di row/column 0.
  - Mitigasi: startup sequential placement + migrasi map terkontrol + checksum placement state.
  - Owner: Core Shell Engineer
- [ ] R2: Event burst filesystem memicu race (add/remove/rename beruntun).
  - Dampak: ghost item, duplicate, missing icon sementara.
  - Mitigasi: debounce + coalescing event + reconciliation pass terjadwal.
  - Owner: Platform/Filesystem Engineer
- [ ] R3: Drag-drop Launchpad/Dock tidak konsisten payload.
  - Dampak: shortcut gagal dibuat atau metadata tidak lengkap.
  - Mitigasi: kontrak payload tunggal + validator + fallback wrapper policy.
  - Owner: Integration Engineer
- [ ] R4: Reflow saat resize/scale merusak posisi persist.
  - Dampak: item tumpang tindih / melompat setelah perubahan geometri.
  - Mitigasi: reflow deterministic + nearest free remap + test matrix resolusi.
  - Owner: Core Shell Engineer

#### Trigger Eskalasi
- [ ] Jika mismatch render vs `~/Desktop` muncul 2 hari berturut-turut -> eskalasi ke Tech Lead.
- [ ] Jika P0 tidak selesai sesuai target week 1 -> freeze pekerjaan P2 dan fokus P0/P1.
- [ ] Jika test `(0,0)` gagal setelah merge -> rollback patch terakhir dan aktifkan hotfix branch.

### Acceptance Gate per Milestone

#### Gate M1 (Harus Lulus Semua)
- [ ] Tidak ada penggunaan `GridView/Grid/Flow` sebagai source posisi.
- [ ] Placement engine menjadi source of truth tunggal.
- [ ] Test manual `(0,0)`, `(0,1)`, `(1,0)`, `(1,1)` lulus.
- [ ] Drag ke row 0/column 0 lulus.
- [ ] Bukti artefak:
  - [ ] commit/PR
  - [ ] hasil test/rekaman validasi

#### Gate M2 (Harus Lulus Semua)
- [ ] File watcher sinkron add/remove/rename/move tanpa full reset.
- [ ] Posisi restart-safe dari position store.
- [ ] Drop Launchpad dan Dock membuat shortcut valid + terpasang benar.
- [ ] Tidak ada duplicate/ghost dari atomic rename.
- [ ] Bukti artefak:
  - [ ] commit/PR
  - [ ] test sinkronisasi filesystem

#### Gate M3 (Harus Lulus Semua)
- [ ] Re-arrange (sort/snap/cleanup) stabil via placement engine.
- [ ] Reflow geometri tidak merusak posisi.
- [ ] Checklist QA wajib lulus penuh.
- [ ] Tidak ada selisih jumlah render vs isi `~/Desktop`.
- [ ] Bukti artefak:
  - [ ] commit/PR final
  - [ ] laporan QA penutupan

### Runbook Eksekusi Mingguan

#### Week 1 (M1 - Fondasi)
- [ ] Senin:
  - [ ] finalisasi kontrak `DesktopPlacementEngine` + struktur model posisi.
  - [ ] setup branch kerja dan baseline test `(0,0)`.
- [ ] Selasa-Rabu:
  - [ ] implement engine + render absolut `DesktopView/DesktopItem`.
  - [ ] hapus jalur truthy/falsy pada koordinat grid.
- [ ] Kamis:
  - [ ] integrasi interaksi dasar (select/drag/drop commit).
  - [ ] validasi row 0 / column 0.
- [ ] Jumat:
  - [ ] smoke test, bugfix cepat, tutup Gate M1.

#### Week 2 (M2 - Sinkronisasi dan Integrasi)
- [ ] Senin:
  - [ ] implement `DesktopFileWatcher` debounced.
  - [ ] wiring startup reconcile scan/load/place.
- [ ] Selasa:
  - [ ] implement `DesktopPositionStore` + restore path.
- [ ] Rabu-Kamis:
  - [ ] integrasi `DesktopShortcutImporter` untuk Launchpad/Dock.
  - [ ] validasi payload + kebijakan wrapper shortcut.
- [ ] Jumat:
  - [ ] test sinkronisasi filesystem + restart-safe.
  - [ ] tutup Gate M2.

#### Week 3 (M3 - Hardening dan QA)
- [ ] Senin-Selasa:
  - [ ] implement re-arrange (sort/snap/cleanup).
  - [ ] implement geometry reflow dan collision recovery.
- [ ] Rabu:
  - [ ] jalankan checklist QA wajib penuh.
- [ ] Kamis:
  - [ ] triage bug dan stabilization pass.
- [ ] Jumat:
  - [ ] final acceptance review + tutup Gate M3.

### Template Laporan Progres Mingguan

#### Ringkasan
- [ ] Milestone aktif: `M1/M2/M3`
- [ ] Status minggu ini: `On Track / At Risk / Blocked`
- [ ] Persentase progres aktual: `__%`

#### Capaian
- [ ] Item selesai (dengan referensi commit/PR/test):
  - [ ] ...

#### Risiko dan Blocker
- [ ] Risiko baru minggu ini:
  - [ ] ...
- [ ] Blocker aktif:
  - [ ] ...
- [ ] Rencana mitigasi:
  - [ ] ...

#### Rencana Minggu Berikutnya
- [ ] Fokus utama:
  - [ ] ...
- [ ] Target gate:
  - [ ] `Gate M1 / Gate M2 / Gate M3`

### Contoh Laporan Minggu 1 (Sample)

#### Ringkasan
- [x] Milestone aktif: `M1`
- [x] Status minggu ini: `At Risk`
- [x] Persentase progres aktual: `55%`

#### Capaian
- [x] Program Desktop View Spatial Placement + checklist lengkap ditambahkan ke dokumen kerja.
  - Referensi: [docs/TODO.md](/home/garis/Development/Qt/Desktop_Shell/docs/TODO.md)
- [x] Milestone `M1/M2/M3`, owner matrix, estimasi hari, dan Kanban harian sudah tersedia.
  - Referensi: [docs/TODO.md](/home/garis/Development/Qt/Desktop_Shell/docs/TODO.md)
- [x] Acceptance gate dan risk register sudah didefinisikan.
  - Referensi: [docs/TODO.md](/home/garis/Development/Qt/Desktop_Shell/docs/TODO.md)

#### Risiko dan Blocker
- [x] Risiko utama:
  - [x] Bug render klasik `row 0 / column 0` masih muncul.
  - [x] Mismatch jumlah item render vs isi `~/Desktop` pada fase init.
- [x] Blocker aktif:
  - [x] Arsitektur render saat ini masih membawa state lama berbasis slot-map/grid visual.
- [x] Rencana mitigasi:
  - [x] Prioritaskan `P0`: migrasi ke placement engine sebagai source of truth tunggal.
  - [x] Startup placement dipaksa sekuensial-deterministik sebelum optimisasi lanjutan.

#### Rencana Minggu Berikutnya
- [x] Fokus utama:
  - [x] Implementasi `DesktopPlacementEngine` minimal viable + occupancy map.
  - [x] Implementasi `DesktopView.qml` absolute placement tanpa flow layout bawaan.
  - [x] Validasi eksplisit cell `(0,0)`, `(0,1)`, `(1,0)`, `(1,1)`.
- [x] Target gate:
  - [x] `Gate M1` (minimal pass untuk bug row/column 0).

### Contoh Laporan Minggu 2 (Sample)

#### Ringkasan
- [x] Milestone aktif: `M2`
- [x] Status minggu ini: `On Track`
- [x] Persentase progres aktual: `78%`

#### Capaian
- [x] `DesktopFileWatcher` debounced aktif untuk event add/remove/rename/move.
  - Referensi: implementasi watcher + debounce timer (artefak commit/PR internal tim).
- [x] `DesktopPositionStore` baseline tersambung untuk restore posisi startup.
  - Referensi: store metadata posisi per identity key.
- [x] Integrasi awal importer shortcut Launchpad/Dock ke pipeline desktop item.
  - Referensi: adaptor payload drag internal + validasi minimum.

#### Risiko dan Blocker
- [x] Risiko utama:
  - [x] Potensi race saat burst event filesystem + atomic rename.
- [x] Blocker aktif:
  - [x] Belum ada coalescing event lanjutan untuk kasus rename kompleks.
- [x] Rencana mitigasi:
  - [x] Tambah reconciliation pass setelah burst.
  - [x] Tambah guard duplicate identity di tahap import/sync.

#### Rencana Minggu Berikutnya
- [x] Fokus utama:
  - [x] Finalisasi re-arrange (`Sort`, `Snap to Grid`, `Clean Up Desktop`).
  - [x] Hardening geometry reflow untuk resize/workarea/scale changes.
  - [x] Menutup seluruh checklist QA wajib.
- [x] Target gate:
  - [x] `Gate M2`

### Contoh Laporan Minggu 3 (Sample)

#### Ringkasan
- [x] Milestone aktif: `M3`
- [x] Status minggu ini: `On Track`
- [x] Persentase progres aktual: `100%`

#### Capaian
- [x] Re-arrange actions stabil via placement engine (`Sort/Snap/Clean Up`).
  - Referensi: modul action desktop + placement engine hooks.
- [x] Reflow geometri stabil pada perubahan resolusi/workarea/scale.
  - Referensi: logic `reflowOnGeometryChange()` + remap nearest free cell.
- [x] Checklist QA wajib lulus.
  - Referensi: hasil eksekusi checklist manual desktop view.
- [x] Tidak ada selisih jumlah render vs isi `~/Desktop`.
  - Referensi: verifikasi startup + runtime sync test.

#### Risiko dan Blocker
- [x] Risiko residual:
  - [x] Variasi perilaku compositor multi-monitor (future scope).
- [x] Blocker aktif:
  - [x] Tidak ada blocker kritis untuk release scope saat ini.
- [x] Rencana mitigasi:
  - [x] Masukkan multi-monitor hardening ke backlog fase berikutnya.

#### Rencana Minggu Berikutnya
- [x] Fokus utama:
  - [x] Monitoring pasca-rilis + perbaikan minor non-blocking.
  - [x] Persiapan fase lanjutan (multi-monitor optimization).
- [x] Target gate:
  - [x] `Gate M3` (closed)

