# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Release Execution Queue (v1.0)
- References:
  - `docs/RELEASE_PLAN.md`
  - `docs/REPO_SPLIT_PLAN.md`
  - `docs/contracts/README.md`
  - `docs/contracts/workspacemanager.md`
  - `docs/contracts/missingcomponents.md`
  - `docs/contracts/polkit-agent.md`
  - `docs/CI_TIER_MAP.md`
  - `docs/TRIAGE_SLA_OWNER_MAP.md`
  - `.github/workflows/ci.yml` (`build-and-test` now runs nightly lane)
  - `scripts/test.sh` adds `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=required|auto|skip`
  - `scripts/test.sh` adds `SLM_TEST_NIGHTLY_PACKAGE_POLICY_WRAPPER_MODE=required|auto|skip`
  - `.github/workflows/ci.yml` adds dedicated `packagepolicy-wrapper-smoke` nightly/manual job
  - `docs/RELEASE_COMPATIBILITY_MATRIX.md`
  - `docs/LOGIN_SPLIT_PREP.md`

## Backlog (Tothespot)
- [x] Add optional preview pane for non-compact popup mode.
  - `showPreviewPane` property (default `true`) + `hasPreviewContent` guard; visible when selected item has data.

## Backlog (Theme)
  - `elevationLow/Medium/High` semantic aliases added to `third_party/slm-style/qml/SlmStyle/Theme.qml`.
  - all duration tokens (`durationFast/Normal/Slow/Workspace/Micro/Sm/Md/Lg/Xl`) in `Theme.qml`, scaled by `_modeScale`.
  - physics tokens (`physicsSpring/Damping/Mass` × 3 presets + `physicsEpsilon`) in `Theme.qml`.
  - easing aliases (`easingDefault`, `easingLight`) added.
  - `controlBgHover`, `controlBgPressed`, `controlDisabledBg`, `controlDisabledBorder`, `textDisabled`, `focusRing` present in `Theme.qml` light/dark palettes.
- [x] FileManager density pass:
  - align toolbar/search/tab/status heights to compact token grid.
  - `FileManagerToolbar.qml`: `height: 36` → `Theme.controlHeightLarge` (4 controls).
  - `FileManagerTabBar.qml`: addTabPill `Layout.preferredHeight: 20` → `Theme.controlHeightCompact`.
  - Active components (HeaderBar/StatusBar) already token-aligned.
- [x] Add high-contrast mode toggle via `settingsd` SSOT + runtime Theme switch.
  - `settingsstore.cpp`: default `globalAppearance.highContrast = false` + bool validator.
  - `DesktopSettingsClient`: `highContrast` property + `setHighContrast()` + `highContrastChanged` signal.
  - `Theme.qml`: `readonly property bool highContrast` — reactive bind ke `DesktopSettings.highContrast`.
  - `AppearancePage.qml`: SettingCard "High Contrast" toggle antara Color Theme dan Accent Color.
- [ ] Add optional dynamic wallpaper sampling tint with contrast guardrail.
- [~] SlmStyle migration guardrail:
  - [ ] Migrate remaining flagged files (current baseline: 49 files from style-usage checker).

## Backlog (FileManager)
- [ ] Continue shrinking `FileManagerWindow.qml` by moving remaining dialog/menu glue into `Qml/apps/filemanager/`.

## Backlog (Global Menu)
- [ ] Runtime integration check global menu end-to-end on active GTK/Qt/KDE apps (verify active binding/top-level changes by focus switching).
  - helper available: `scripts/smoke-globalmenu-focus.sh` (`--strict --min-unique 2`).

## Program (Storage & Automount UX: Per-Partition, GIO/GVfs, Modern Linux Desktop)

### Guardrails Wajib
- [x] Jangan gunakan `mount/umount` command langsung di semua path runtime.
- [x] Semua operasi mount/unmount harus lewat GIO/GVfs (`GVolumeMonitor`, `GVolume`, `GMount`).
- [x] Semua policy berbasis partisi (`UUID`/fallback `PARTUUID`), bukan disk global.
- [x] System partitions (EFI/MSR/swap/recovery/LVM raw) tidak tampil di UI user biasa.
- [~] Notifikasi wajib satu per device attach event (tidak boleh spam per partition).
  - [x] Producer notifikasi dipindah ke service-layer global (`StorageAttachNotifier`) via `org.slm.Desktop.Storage` signal, grouped by `deviceGroupKey` + cooldown + action `[ Open ] [ Eject ]`.

### Phase 1 — Device Detection Layer (GIO/GVfs)
- [~] Buat `StorageManager` core service (desktopd/fileopsd integration point) untuk event storage.
  - [ ] Promote ke service-layer terpisah (`StorageManager`) agar reusable lintas FileManager/desktop notifications.
- [~] Wire `GVolumeMonitor` signals:
  - `volume-added`
  - `volume-removed`
  - `mount-added`
  - `mount-removed`
  - [~] Coalescer per device baseline aktif (debounce 4s, grouped by `deviceGroupKey`), lanjut refinement multi-monitor/session edge cases.
- [~] Tambahkan aggregator event per physical device untuk grouping partitions.
  - [x] Baseline grouping di sidebar berdasarkan `deviceGroupKey` (`Devices -> Drive -> child volumes`).
- [~] Tambahkan loading/settling state agar scan partisi tidak menghasilkan UI jitter.
  - [x] Baseline settle timer UI (`storageSidebarSettleMs` + `storageSettleTimer`) untuk menahan repaint sidebar saat burst event `StorageLocationsUpdated`.

### Phase 2 — Policy Engine (UUID-Based, Per Partition)
- [ ] Definisikan model input policy:
  - `uuid`, `partuuid`, `fstype`, `is_removable`, `is_system`, `is_encrypted`.
- [ ] Definisikan model output policy:
  - `action` (`mount|ignore|ask`), `auto_open`, `visible`, `read_only`, `exec`.
- [ ] Implement default policy:
  - EFI/MSR/recovery -> `hidden + ignore`
  - swap/LVM raw -> `hidden`
  - internal data -> `automount`, no auto-open
  - USB removable -> `automount`, no auto-open
  - SD card -> `automount`, optional auto-open
  - encrypted -> wait unlock
  - unknown fs -> no automount
- [ ] Tambahkan override resolution order:
  - `uuid` -> `partuuid` -> `device-serial + partition-index`.

### Phase 3 — Mount Executor (No Shell Mount)
  - `src/apps/filemanager/filemanagerapi_storage.cpp`
  - `src/daemon/devicesd/devicesmanager.cpp` (Mount/Eject path)
- [x] Tambahkan error mapping non-teknis untuk state:
  - locked, busy, unsupported, permission denied.
- [x] Tambahkan timeout + cancellation path untuk operasi mount/unmount panjang.

### Phase 4 — Notification & Volume Selector UX
- [~] Implement notification coalescer per device attach.
  - [~] UI polish + localization microcopy + selector keyboard navigation.
    - [x] Baseline keyboard navigation untuk volume selector (`Enter`/`Return` + hover-to-select).
- [~] Format notifikasi multi-partition:
  - `"External Drive connected"`
  - `"X volume tersedia"`
  - actions: `[ Open ] [ Eject ]`
- [~] Action `Open`:
  - jika 1 volume: buka langsung (single window)
  - jika >1 volume: tampilkan volume selector, jangan auto-open multiple windows.
- [~] Pastikan tidak pernah ada multiple notification per partition attach.

### Phase 5 — Sidebar Device UI
- [x] Render struktur:
  - `Devices`
  - `Drive Name`
  - child entries `Volume A/B/...`
- [x] Sembunyikan semua system partitions dari mode user biasa.
- [x] Gunakan microcopy non-teknis:
  - Mount -> `Open Drive`
  - Unmount -> `Eject`
  - Volume -> `Drive`
- [x] Tambahkan loading skeleton/state saat mount scan berjalan.

### Phase 6 — Properties Panel (Mount Behavior)
- [~] Tambahkan panel `Mount Behavior` per partition:
  - [x] Mount otomatis
  - [x] Mount otomatis + buka
  - [x] Tanya setiap kali
  - [x] Jangan pernah mount otomatis
  - [x] `storagePolicyForPath(path)` load on open (service API + FileManagerApi bridge).
  - [x] `setStoragePolicyForPath(path, patch)` save realtime (service API + FileManagerApi bridge).
  - control baseline: automount/ask, auto-open, visible, read-only, allow-exec
- [~] Tambahkan section `Visibility`:
  - tampilkan/simpan di sidebar
- [~] Tambahkan section `Access`:
  - read-write/read-only/allow exec
- [ ] Tambahkan section `Scope`:
  - partisi ini saja / semua partisi pada device ini.
  - [x] Baseline scope backend tersedia (`partition`/`device` via `setStoragePolicyForPath(..., scope)` + `policyScope`).
  - [~] Baseline UI wiring tersedia di Properties dialog tab `Mount` (scope + policy toggles).
    - [x] Tab tetap dapat dibuka saat policy belum tersedia + status/loading/error messaging.
    - [x] Polish copy/layout + state disabled granular (busy/updating/automount/read-only) untuk control Mount tab.
    - [x] Guard state baseline: `auto_open` otomatis off saat `automount=false`.
    - [x] Error text di Mount tab dinormalisasi non-teknis (locked/busy/unsupported/permission) tanpa expose kode mentah.

### Phase 7 — Policy Storage & Persistence
- [~] Simpan policy JSON per partition:
  - `uuid`, `automount`, `auto_open`, `visible`, `read_only`, `exec`.
    - `storagePolicyForPath(path)`
    - `setStoragePolicyForPath(path, policy)`
    - JSON store: `${AppConfigLocation}/storage-policies.json`
  - [~] Extracted persistence/migration into shared module `src/services/storage/StoragePolicyStore` (consumed by FileManagerApi).
  - [~] Dedicated runtime `StorageManager` kini aktif untuk monitor storage event + snapshot query/policy resolve shared.
  - [~] Baseline DBus façade tersedia via `org.slm.Desktop.Devices.GetStorageLocations` (powered by shared `StorageManager`).
  - [~] Endpoint façade dedicated `org.slm.Desktop.Storage` sudah ada:
    - `Ping`, `GetCapabilities`, `GetStorageLocations`, `StoragePolicyForPath`, `SetStoragePolicyForPath`, `Mount`, `Eject`, `ConnectServer`
    - signal `StorageLocationsChanged` tersedia dari `devicesd`
  - [ ] Lengkapi adopsi lintas shell/settings/filemanager + kontrak event real-time penuh dari façade dedicated.
- [~] Implement migration + fallback key chain (`uuid` -> `partuuid` -> serial+index).
  - [x] Runtime fallback chain aktif: `uuid` -> `partuuid` -> `serial-index` -> `device` -> `group`.
  - [x] Persisted-key migrator baseline aktif di `StoragePolicyStore` (schema v3, canonical key rewrite `uuid/partuuid/serial-index` saat load).
  - [x] Migrasi runtime-assisted `device/group` -> `serial-index` baseline aktif di `StorageManager` (promotion copy saat snapshot media tersedia).
  - [~] Cleanup migrasi lanjut (opsional): pruning key legacy `device/group` setelah periode kompatibilitas.
    - [x] Baseline prune opsional ditambahkan (gated env `SLM_STORAGE_POLICY_PRUNE_LEGACY_KEYS=1`), hanya hapus key legacy bila policy sama persis dengan `serial-index` target.
- [~] Tambahkan schema versioning + safe recovery untuk file policy corrupt.
  - [x] Migrator bertahap schema <= 3 ditambahkan (`migratePolicyEntriesForSchema`).
  - [~] Perluas pipeline migrator untuk schema selanjutnya + telemetry hasil migrasi.
    - [x] Baseline telemetry migrasi ditambahkan di `StoragePolicyStore::loadPolicyMap()` (debug log ringkas berbasis count: loaded/normalized/canonicalized/collisions/dropped/save status, tanpa expose key/path sensitif).

### Phase 8 — Special States UX
- [~] Locked state:
  - `"Drive terkunci"` + action `[ Unlock ]`.
  - [x] Baseline messaging non-teknis untuk error open/eject: `"Drive terkunci..."`.
- [~] Busy state:
  - `"Drive sedang digunakan"` + actions `[ Force Eject ] [ Cancel ]`.
  - [x] Baseline messaging non-teknis untuk error open/eject: `"Drive sedang digunakan..."`.
- [~] Unsupported state:
  - `"Drive tidak dikenali"` + actions `[ Mount Read-only ] [ Ignore ]`.
  - [x] Baseline messaging non-teknis untuk error open/eject: `"Drive tidak dikenali."`.
- [~] Pastikan messaging tanpa `/dev/sdX`, UUID, atau istilah teknis mentah.
  - [x] Sanitizer baseline di `FileManagerWindowActions.notifyResult()` untuk jalur storage (`Open Drive`/`Eject`) menghapus detail `/dev/*` dan UUID mentah.
  - [x] Mapper error storage dipusatkan di `FileManagerWindowActions` (`isStorageAction` + `nonTechnicalStorageError`) untuk normalisasi locked/busy/unsupported/permission/timeout/service-unavailable/not-found/cancelled secara konsisten lintas caller.

### Phase 9 — Integration & Hardening
- [ ] Integrasi penuh ke FileManager sidebar + notification system + device properties panel.
- [~] Tambahkan contract test:
  - [x] no shell mount usage
  - [x] no per-partition notification spam
  - [x] system partition hidden by default
  - [x] single-open behavior for multi-volume device.
  - baseline: `tests/storage_automount_contract_test.cpp`.
  - [x] persistence migration/canonicalization test: `tests/storagepolicystore_migration_test.cpp`.
- [~] Tambahkan smoke test runtime untuk USB multi-partition dan encrypted media.
  - [x] Baseline DBus smoke test ditambahkan: `tests/storage_runtime_smoke_test.cpp` (shape + optional hardware checks via env).
  - [x] Tambahkan lane hardware CI/dogfooding (manual `workflow_dispatch` + self-hosted label `storage-hw`) yang mengaktifkan env strict via `scripts/test-storage-runtime-suite.sh`:
    - `SLM_STORAGE_SMOKE_REQUIRE_SERVICE=1`
    - `SLM_STORAGE_SMOKE_REQUIRE_MULTI_PARTITION=1`
    - `SLM_STORAGE_SMOKE_REQUIRE_ENCRYPTED=1`
- [~] Tambahkan observability log ringkas (debug mode only) tanpa leak info sensitif.
  - [x] Baseline log migrasi policy `device/group -> serial-index` (jumlah entry promotion per snapshot-save).

### Acceptance Criteria
- [ ] Satu device attach menghasilkan satu notification.
- [ ] Multi-partition tidak auto-open multi-window.
- [ ] Semua mount/unmount lewat GIO/GVfs API.
- [ ] Policy per partition berjalan konsisten lintas reboot/session.
- [ ] UI tetap non-teknis, responsif, dan aman untuk user umum.

## Program (UX Animation System: State-Driven, macOS-like, Non-Patent)
- [ ] Tetapkan prinsip sistem animasi global:
  - state-driven animation (bukan trigger manual dari event UI).
  - setiap transisi harus jelas `origin -> transition -> destination`.
  - animasi menjadi bagian dari state global desktop, bukan efek per-widget.
- [ ] Bangun arsitektur berlapis:
  - `Compositor Layer`: `WindowLifecycleManager`, `WorkspaceManager`, `GlobalAnimationScheduler`, `FrameTimingController`.
  - `Animation Engine`: `StateMachine`, `TransitionResolver`, `PhysicsProfile`, `AnimationQueueInterruptManager`.
  - `UI Layer (QML)`: declarative state binding + micro interaction only.
- [ ] Definisikan state machine inti:
  - `WindowState`: `Hidden`, `Opening`, `Active`, `Inactive`, `Minimizing`, `Minimized`, `Closing`.
  - `WorkspaceState`: `Idle`, `Switching`, `Overview`.
- [ ] Definisikan tabel transition rules wajib:
  - `Hidden -> Opening -> Active`
  - `Active -> Minimizing -> Minimized`
  - `Minimized -> Opening -> Active`
  - `Active -> Closing -> Hidden`
  - `Active <-> Inactive`
  - Semua perubahan state harus lewat transition resolver (tidak boleh langsung animasi dari event handler).
- [ ] Implement event-to-state trigger pipeline:
  - `onWindowCreated -> Opening`
  - `onWindowFocused -> Active`
  - `onWindowUnfocused -> Inactive`
  - `onWindowMinimized -> Minimizing`
  - `onWorkspaceChanged -> Switching`
  - Larangan: `onClick/onEvent` langsung start animasi lifecycle.
- [~] Bangun `GlobalAnimationScheduler`:
  - priority: `HIGH` (window open/close), `MEDIUM` (workspace switch), `LOW` (micro interaction).
  - interrupt rule: `HIGH` interrupt `LOW`; `LOW` tidak boleh interrupt `HIGH`.
  - workspace transition menunda/menahan micro interaction berat.
  - coalescing/debounce untuk event beruntun.
    - lifecycle ownership + active priority tracking (`Low/Medium/High`)
    - micro-interaction suppression when lifecycle priority is active
    - scheduler coalescing helper (`shouldCoalesce*`)
    (covers `allow/canRunPriority`, suppression state, and coalescing behavior).
    - `workspaceVisible/swipeActive/swipeSettling` drive lifecycle begin/end through `MotionController`
  - [~] Extend lifecycle hooks to window lifecycle transitions (open/minimize/close/focus) from compositor events.
- [~] Standardisasi timing global (single source of truth):
  - `Fast=120ms`, `Normal=220ms`, `Slow=320ms`, `Workspace=400ms`.
  - implemented token baseline in `third_party/slm-style/qml/SlmStyle/Theme.qml`: `durationFast/Normal/Slow/Workspace` (+ safe-mode off gate).
  - notification stack migrated to global tokens/easing.
  - desktop/workspace baseline migrated: `Qml/DesktopScene.qml`, `Qml/components/workspace/WorkspaceOverlay.qml`.
  - overlay/topbar/indicators baseline migrated: `Qml/components/overlay/{DockWindow,ClipboardOverlay,ClipboardOverlayWindow}.qml`, `Qml/components/topbar/{TopBar,TopBarMainMenuControl,TopBarSearchButton}.qml`, `Qml/components/indicators/IndicatorManager.qml`.
  - applet/compositor/shell baseline migrated: `Qml/components/applet/{ScreencastIndicator,SoundApplet,BluetoothApplet,ClipboardApplet,BatteryApplet,InputCaptureIndicator,NetworkApplet,BatchOperationIndicator,DatetimeApplet}.qml`, `Qml/components/compositor/CompositorSwitcherOverlay.qml`, `Qml/components/dock/DockReorderMarker.qml`, `Qml/components/shell/ShellShortcutTile.qml`, `Qml/components/{AppWindow}.qml`, `Qml/components/desktop/DesktopBackground.qml`.
  - settings/filemanager/dock/polkit batch migrated: `Qml/apps/settings/{Main,Sidebar}.qml`, `Qml/apps/settings/components/{SettingToggle,SettingCard,FontPickerDialog}.qml`, `Qml/apps/filemanager/{FileManagerContentView,FileManagerHeaderBar,FileManagerToolbar,FileOperationsProgressOverlay,FileManagerShareSheet,FileManagerMenus}.qml`, `Qml/components/dock/{Dock,DockItem}.qml`, `Qml/polkit-agent/AuthDialog.qml`.
  - greeter/launchpad/print/portalchooser batch migrated: `Qml/greeter/{Main,LoginPage}.qml`, `Qml/components/launchpad/Launchpad.qml`, `Qml/components/print/{PrintAdvancedPanel,PrinterFeatureSection}.qml`, `Qml/components/portalchooser/PortalChooserPathBar.qml`.
  - final QML sweep complete: no remaining hardcoded `duration:<number>` / `easing.type: Easing.*` outside token definitions in `third_party/slm-style/qml/SlmStyle/Theme.qml` (verified via `rg`).
  - hapus hardcoded duration tersebar; migrasi ke `AnimationTokens`.
- [~] Standardisasi easing & physics:
  - default `EaseOutCubic`.
  - alternatif ringan `EaseOutQuad`.
  - spring untuk transisi natural tertentu.
  - larang `Linear` untuk lifecycle utama.
      - implemented in `third_party/slm-style/qml/SlmStyle/Theme.qml`: `physicsSpringGentle/Default/Snappy`, `physicsDamping*/physicsMass*`, `physicsEpsilon`.
      - migrated: `DockReorderMarker.qml`, `DockItem.qml`, `NotificationApplet.qml`, `NotificationCenter.qml`.
- [~] Definisikan profile animasi window (safe/non-patent):
  - open: `scale 0.96->1.0`, `opacity 0->1` — implemented in `WorkspaceOverviewScene` thumbnail entrance.
  - minimize: dock icon focus-bounce on minimize (spatial cue without genie effect).
  - close: scale down + fade out — future compositor extension (shell cannot animate window surfaces directly).
    - event `window-opened/window-shown/window-unminimized` -> profile `window.open` (`preset=smooth`, release `Theme.durationNormal`)
    - event `window-minimized` -> profile `window.minimize` (`preset=snappy`, release `Theme.durationFast`)
    - event `window-closing/window-closed` -> profile `window.close` (`preset=snappy`, release `Theme.durationFast`)
    - profile push/pop aman (restore channel+preset `MotionController` setelah lifecycle selesai/destruction)
    - `WorkspaceOverviewScene` thumbnail entrance: `scale 0.96→1.0`, `opacity 0→1` (`durationNormal`, `easingDecelerate`) on new window while overview is open.
    - `Dock.notifyWindowLifecycle()` → `DockAppDelegate.matchesWindowAppId()` → launch-bounce on window-opened, focus-bounce on window-minimized.
    - `DesktopScene.processCompositorLifecycleEvent()` routes `appId` to dock after setting MotionController profile.
  - switch: slide horizontal antar workspace (+ optional parallax ringan).
  - overview: scale-down window + grid sederhana.
- [~] Definisikan active/inactive visual response:
  - trigger dari focus compositor.
  - perubahan ringan shadow/opacity, tanpa animasi berat.
    - focus compositor (`windowData.focused`) memicu `focusBlend` state.
    - perubahan ringan border/opacity/glow dengan `Theme.durationFast` + `Theme.easingLight`.
    - tanpa animasi berat; hanya emphasis visual halus untuk active vs inactive.
- [~] Implement kondisi disable/degrade animasi:
  - CPU tinggi, GPU fallback, battery saver, dan accessibility `reduce motion`. (triggers — future work)
    - implemented runtime gate: `SLM_SESSION_MODE in {safe,recovery}` -> `SafeModeActive=true`,
      `Theme.duration* ~= 0`, `Theme.transitionDuration ~= 0`, `MotionController.reducedMotion=true`.
    - `DesktopSettings.settingValue("animation.mode")` sebagai sumber mode animasi runtime.
    - `Theme.animationMode` reads `DesktopSettings` reactively.
    - `Theme._modeScale`: `full=1.0`, `reduced=0.60`, `minimal=0.25` — all duration tokens scaled.
    - `MotionController.reducedMotion` set true for reduced/minimal at startup + runtime in `main.cpp`.
  - semua animasi lewat frame clock vsync.
  - pastikan sinkron lintas window/workspace untuk hindari tearing/jitter.
  - implemented: `AnimationScheduler::setExternalDriving(true)` suspends internal `QTimer`;
    `AnimationScheduler::windowFrame()` provides the same dt logic triggered externally.
  - `MotionController::enableVsyncDriving()` + `windowFrame()` exposed as Q_INVOKABLE.
  - `main.cpp`: after QML loads, connects `QQuickWindow::afterAnimating` →
    `MotionController::windowFrame()` so gesture-driven spring physics ticks exactly at vsync.
- [ ] Batasan micro interaction (UI-only/QML):
  - hover, click feedback, toggle.
  - durasi 100-150ms.
  - tidak masuk scheduler lifecycle compositor.
  - [~] baseline scheduler guard wired:
    - `MotionController.allowMotionPriority(LowPriority)` integrated for micro transitions in
      `Qml/components/dock/DockItem.qml`,
      `Qml/components/dock/Dock.qml`,
      `Qml/components/topbar/{TopBarSearchButton,TopBarMainMenuControl,TopBarScreenshotControl,TopBarBrandSection}.qml`,
      `Qml/apps/filemanager/FileManagerContentView.qml` (list/columns row hover/opacity transitions),
      `Qml/apps/settings/{Sidebar,components/SettingCard}.qml`,
      `Qml/apps/settings/components/FontPickerDialog.qml`,
      `Qml/components/portalchooser/PortalChooserListRow.qml`,
      `Qml/components/shell/ShellShortcutTile.qml`,
      `Qml/components/notification/NotificationCard.qml`,
      `Qml/components/notification/BannerContainer.qml`,
      `Qml/components/applet/{NetworkApplet,SoundApplet,BluetoothApplet,BatteryApplet,ClipboardApplet,NotificationApplet,PrintJobApplet,DatetimeApplet,ScreencastIndicator,InputCaptureIndicator}.qml`.
      `AppIndicatorItem.qml` (tray hover color/border),
      `WindowControlsCapsule.qml` (title-button scale/opacity),
      `SettingToggle.qml` (toggle bg color + thumb x),
      `FileManagerMenus.qml` (share action row color),
      `FileManagerHeaderBar.qml` (search field width),
      `Settings/Main.qml` (back button + search border color).
      Test extended to cover all 6 new files.
- [ ] Terapkan anti-patent guideline:
  - hindari genie minimize, dock bounce khas Apple, dan mission-control layout identik.
  - gunakan spatial movement sederhana + scale/fade + easing internal SLM.
- [ ] Deliverables arsitektur:
  - dokumen arsitektur `state machine + scheduler + transition rules`.
  - kontrak event compositor -> state system.
  - peta migrasi animasi existing ke global token/scheduler.
  - checklist audit: tidak ada lifecycle animation yang dipicu langsung dari handler UI.
  - CI guardrail lint animasi aktif: `scripts/check-animation-token-usage.sh` + target `lint_animation_tokens` + default run di `scripts/test.sh` + workflow CI nightly.
  - local guardrail aktif: pre-commit hook (`.githooks/pre-commit`) + installer `scripts/install-git-hooks.sh`.
  - local pre-push guardrail aktif: `.githooks/pre-push` (full animation lint + UI style lint).
  - UI lint baseline guardrail aktif: `scripts/lint-ui-style.sh` support allowlist `config/lint/ui-style-allowlist.txt` (new violations fail, debt lama tetap terpantau).

## Program (desktop-appd: App Lifecycle Manager + Running App Registry)

> Service tunggal sebagai **otoritas status aplikasi** untuk Dock, App Switcher, Launchpad, Global Search, Notification Routing, dan System Monitor.

### Prinsip Wajib
- `desktop-appd` adalah **UX-aware process registry**, bukan process manager OS.
- **App-centric**, bukan PID-centric — satu app bisa punya banyak process dan banyak window.
- Event-driven via DBus; tidak boleh polling berat.
- Tidak semua process ditampilkan ke UI — UI exposure policy yang menentukan.
- Tahan error: satu app bermasalah tidak boleh crash service.

### Guardrails Wajib
- [ ] Jangan tampilkan semua process ke dock — hanya yang lolos UI exposure policy.
- [ ] Jangan treat CLI sebagai GUI app.
- [ ] Jangan bergantung hanya pada PID — gunakan appId sebagai primary key.
- [ ] Jangan polling berat — semua state change harus event-driven.
- [ ] Jangan hardcode app mapping tanpa override system.
- [ ] Debounce event burst agar tidak banjiri UI thread.
- [ ] Fallback graceful jika metadata `.desktop` tidak ditemukan.
- [ ] Gunakan cache untuk mapping PID ↔ appId ↔ window.

---

### Arsitektur Modul

```text
desktop-appd/
 ├── lifecycle-engine/        # state machine CREATED→LAUNCHING→RUNNING→…→TERMINATED
 ├── app-registry/            # AppEntry store, PID↔appId↔window mapping
 ├── classification/          # ClassificationEngine: tentukan kategori UX
 ├── exposure-policy/         # UIExposurePolicy: dock/switcher/tray/hidden
 ├── event-bus/               # internal fan-out ke subscriber
 ├── dbus-api/                # org.desktop.Apps interface
 ├── launcher-integration/    # intercept launch, capture appId+PID+timestamp
 └── compositor-integration/  # Wayland/compositor window events
```

---

### Model Data — AppEntry

```json
{
  "appId": "org.kde.dolphin",
  "category": "kde",
  "state": "running",
  "isFocused": false,
  "isVisible": true,
  "isMinimized": false,
  "windowCount": 2,
  "pids": [1234],
  "windows": [
    { "windowId": "win-1", "workspace": 1, "title": "Home" }
  ],
  "lastActiveTs": 0,
  "uiExposure": {
    "dock": true,
    "appSwitcher": true,
    "launchpad": false,
    "systemMonitor": true,
    "tray": false
  }
}
```

---

### Lifecycle Engine

State machine:

```text
CREATED → LAUNCHING → RUNNING → IDLE → BACKGROUND
        → UNRESPONSIVE → CRASHED → TERMINATED
```

Trigger:
- `launch`: via launcher interception — masuk state `LAUNCHING`
- `window created`: via compositor — transisi ke `RUNNING`
- `no response > threshold`: state `UNRESPONSIVE`
- `abnormal exit`: state `CRASHED`
- `all pids exited`: `TERMINATED` → hapus AppEntry → emit `AppRemoved`

---

### Classification Engine

Kategori:

```text
shell | gui-app | gtk | kde | qt-generic | qt-desktop-fallback
cli-app | background-agent | system-ignore | unknown
```

Strategi resolusi (urutan prioritas):
1. Override/whitelist config eksplisit
2. `.desktop` metadata (`X-KDE-*`, `Categories`)
3. Toolkit detection (env var / linked libs)
4. Parent process (terminal → CLI app)
5. Window existence (ada window = GUI)
6. Fallback rule (`unknown`)

Harus extensible: override via config file per-app.

---

### UI Exposure Policy Matrix

| Category            | Dock | Switcher | Tray | System Monitor |
| ------------------- | ---- | -------- | ---- | -------------- |
| shell               | no   | no       | no   | optional       |
| gui-app             | yes  | yes      | no   | yes            |
| gtk                 | yes  | yes      | no   | yes            |
| kde                 | yes  | yes      | no   | yes            |
| qt-generic          | yes  | yes      | no   | yes            |
| qt-desktop-fallback | yes  | yes      | no   | yes            |
| cli-app             | no   | no       | no   | yes            |
| background-agent    | no   | no       | yes  | yes            |
| system-ignore       | no   | no       | no   | no             |
| unknown             | no   | optional | no   | yes            |

---

### DBus Interface — `org.desktop.Apps`

**Methods:**
- `ListRunningApps()` → `AppEntry[]`
- `GetApp(appId)` → `AppEntry`
- `GetAppWindows(appId)` → `WindowInfo[]`
- `GetAppState(appId)` → `string`
- `LaunchApp(appId)`
- `ActivateApp(appId)`
- `MinimizeApp(appId)`
- `CloseApp(appId)`
- `RestartApp(appId)`

**Signals:**
- `AppAdded(appId)`
- `AppRemoved(appId)`
- `AppStateChanged(appId, state)`
- `AppWindowsChanged(appId)`
- `AppFocusChanged(appId, focused)`
- `AppUpdated(appId)`

Semua signal harus granular — tidak boleh satu event global "sesuatu berubah".

---

### Event Flow

```text
onLaunch(appId):
  pid = spawn(exec)
  create AppEntry { state=LAUNCHING }
  monitor(pid)

onWindowCreated(window):
  appId = resolve(window.pid | window.appId)
  attach window ke AppEntry
  state → RUNNING
  emit AppStateChanged, AppWindowsChanged

onNoResponse(appId, duration > threshold):
  state → UNRESPONSIVE
  emit AppStateChanged(appId, "unresponsive")

onProcessExit(pid, exitCode):
  if exitCode != 0 → state = CRASHED
  if all pids gone → remove AppEntry, emit AppRemoved
  else → emit AppStateChanged

onFocusChanged(windowId, focused):
  appId = resolveWindow(windowId)
  update AppEntry.isFocused
  emit AppFocusChanged(appId, focused)
```

---

### Dock Integration

Dock wajib:
1. On init: call `ListRunningApps()` → populate running indicators
2. Subscribe signals: `AppAdded`, `AppRemoved`, `AppStateChanged`, `AppFocusChanged`
3. Render state indicator:

| State        | UX                        |
| ------------ | ------------------------- |
| launching    | animasi pulse             |
| running      | dot                       |
| active       | dot terang / highlight    |
| background   | dot redup                 |
| unresponsive | warning badge             |

4. Klik behavior:
   - not running → `LaunchApp(appId)`
   - running, single window → `ActivateApp(appId)`
   - minimized → restore via `ActivateApp`
   - multi-window → tampilkan window list / cycle

---

### Phase 1 — Launch Tracking + Basic Registry + Dock Integration
- [ ] Buat service skeleton `desktop-appd` (D-Bus activatable, systemd user unit).
- [ ] Implement `AppEntry` model + in-memory registry.
- [ ] Implement launcher interception: capture `appId`, `exec`, `timestamp`, `PID`.
- [ ] Implement PID monitor: detect exit (normal vs abnormal).
- [ ] Expose `ListRunningApps`, `GetApp`, `AppAdded`, `AppRemoved`, `AppStateChanged` via `org.desktop.Apps`.
- [ ] Wire Dock ke `desktop-appd`: subscribe signals, render launching/running/active indicators.
- [ ] Implement klik behavior Dock: launch / activate / restore / window list.

### Phase 2 — Classification Engine + UI Exposure Policy
- [ ] Implement `ClassificationEngine`: resolusi kategori via `.desktop`, toolkit, parent PID, window existence, whitelist, fallback.
- [ ] Implement `UIExposurePolicy`: hitung `uiExposure` per AppEntry dari kategori.
- [ ] Wire policy ke Dock, App Switcher, Launchpad — filter berdasarkan `uiExposure`.
- [ ] Buat format config override per-app (YAML/JSON, path `~/.config/desktop-appd/overrides.json`).
- [ ] Implement `AppFocusChanged` signal dari compositor window focus events.

### Phase 3 — Lifecycle Lengkap + Context Integration
- [ ] Implement hang detection: timeout / no-redraw / no-input-response → `UNRESPONSIVE`.
- [ ] Implement multi-window support: `AppEntry.windows[]`, `AppWindowsChanged` signal.
- [ ] Implement `IDLE` / `BACKGROUND` state tracking.
- [ ] Integrasi dengan SSOT: `policy restart`, `timeout hang`, `visibility rules`, per-app override.
- [ ] Context awareness hook: `power.lowPower == true` → suspend background apps.
- [ ] App Action API lengkap: `MinimizeApp`, `CloseApp`, `RestartApp`.
- [ ] Observability: debug log ringkas (mode debug only, tidak leak info sensitif).

### Acceptance Criteria
- [ ] Dock hanya menampilkan app yang lolos `uiExposure.dock == true`.
- [ ] CLI app tidak pernah muncul di Dock atau App Switcher.
- [ ] Launch tracking mencatat semua app yang dijalankan via launcher.
- [ ] Semua state change dikirim sebagai DBus signal granular.
- [ ] Service tidak crash jika satu app bermasalah.
- [ ] Tidak ada polling berat — semua update berbasis event.

## Program (Fondasi Arsitektur Shell: Layer System & State Machine)

> Desain ulang fondasi shell desktop menjadi arsitektur layer-based yang stabil, state-driven,
> dan "unbreakable". Inspirasi: macOS layer model.
> Terminologi: **TopBar** (bukan MenuBar), **WorkspaceLayer** (bukan OverviewLayer),
> **ToTheSpotLayer** (bukan SpotlightLayer).

### Prinsip Utama

- Shell adalah **state machine tunggal** — satu source of truth untuk seluruh UI mode.
- UI dibagi ke dua kategori: **Persistent Layer** (tidak pernah di-destroy) dan
  **Transient / Overlay Layer** (muncul-hilang sesuai state).
- Tidak ada "page switching" atau navigation-based UI. Semua mode adalah state, bukan halaman.
- Animasi harus reversible, interruptible, dan berbasis state — bukan trigger event.

### Diagram Arsitektur Layer (ShellRoot)

```
ShellRoot                        z-order (low → high)
 ├─ WallpaperLayer                 0   — static background, never redrawn
 ├─ DesktopLayer                   10  — desktop icons, widgets
 ├─ WorkspaceLayer                 20  — window stack, compositor surface, focus management
 ├─ DockLayer            [P]       30  — persistent, opacity-only changes
 ├─ TopBarLayer          [P]       40  — persistent, context-aware controls
 ├─ LaunchpadLayer       [T]       50  — fullscreen overlay, lazy-loaded
 ├─ WorkspaceOverviewLayer [T]     60  — mission-control style overview
 ├─ ToTheSpotLayer       [T]       70  — global search, lightweight overlay
 ├─ NotificationLayer    [T]       80  — banners + notification center
 └─ SystemModalLayer               90  — polkit, crash dialogs, system alerts

[P] = Persistent — always alive, never unmounted
[T] = Transient  — created/shown on demand, state-preserved on hide
```

### Klasifikasi Layer

#### Persistent Layers — tidak boleh crash, tidak boleh di-unmount

- **DockLayer**: selalu aktif. Hanya berubah opacity, blur/material, dan input priority.
  Tidak di-hide atau di-destroy saat overlay muncul.
- **TopBarLayer**: selalu visible. Status system + entry point quick actions +
  context-aware controls (menampilkan menu app yang aktif, indikator, dsb.).
- **WorkspaceLayer**: mengelola window stacking, focus, workspace switching.
  Tidak pernah di-reset saat mode berubah — hanya state-nya yang berubah (blurred, dimmed, dsb.).

#### Transient / Overlay Layers — state-preserved, lazy-loaded

- **LaunchpadLayer**: fullscreen overlay, workspace di-blur/dim, dock tetap visible (opsional dim).
- **WorkspaceOverviewLayer**: tampilkan semua workspace + window, dock visible (opsional redup).
- **ToTheSpotLayer**: search global, overlay ringan di atas semua layer, tidak ganggu workspace state.
- **NotificationLayer**: banner transient + notification center. Independent dari mode lain.

### State Machine — `ShellState`

```
ShellState {
  // Mode flags — kombinasi diperbolehkan (kecuali yang mutex)
  showDesktop:        bool   // window fade-out, DesktopLayer ditonjolkan
  focusMode:          bool   // semua elemen non-aktif dim

  // Overlay visibility — diatur oleh state, bukan event langsung
  overlayState {
    launchpadVisible:    bool
    overviewVisible:     bool
    toTheSpotVisible:    bool
    notificationsVisible: bool
  }

  // Per-layer state cache
  dockState {
    opacity:          real   // 1.0 normal, 0.7 saat overlay, 0.0 saat showDesktop gestur
    inputEnabled:     bool
    blurred:          bool
  }

  topBarState {
    opacity:          real
    activeAppTitle:   string
    inputEnabled:     bool
  }

  workspaceState {
    blurred:          bool   // true saat launchpad/overview aktif
    dimAlpha:         real   // 0.0–0.6
    interactionBlocked: bool
    activeWorkspaceId: int
  }
}
```

**Yang harus dihindari:**
```
// ❌ navigation-based — jangan lakukan ini
currentPage = "desktop" | "launchpad" | "overview"

// ✅ state-based — gunakan ini
overlayState.launchpadVisible = true
workspaceState.blurred = true
dockState.opacity = 0.7
```

### Interface Antar Layer

Setiap layer mengekspos kontrak minimal:

```
interface PersistentLayer {
  // State setters — tidak ada side-effect antar layer
  setBlurred(enabled: bool, alpha: real)
  setDimmed(enabled: bool, alpha: real)
  setInputEnabled(enabled: bool)
  setOpacity(value: real, animated: bool)
}

interface TransientLayer : PersistentLayer {
  show(animated: bool)
  hide(animated: bool)
  readonly property bool visible
  readonly property bool animating
}

interface ShellStateController {
  // Single entry point untuk semua mode change
  requestMode(mode: ShellMode, source: InputSource)
  cancelMode(mode: ShellMode)
  toggleOverlay(overlay: OverlayType)
}
```

Layer tidak boleh mengubah state layer lain secara langsung —
semua perubahan melalui `ShellStateController`.

### Event Flow (Input → State → Render)

```
InputEvent (keyboard / mouse / gesture / D-Bus)
    │
    ▼
InputRouter
    │  (menentukan target layer berdasarkan mode aktif)
    ▼
ShellStateController.requestMode() / toggleOverlay()
    │
    ├─ update ShellState (single write)
    │
    ▼
StateBindings (reaktif — setiap layer bind ke ShellState)
    │
    ├─ DockLayer.setOpacity / setBlurred
    ├─ WorkspaceLayer.setBlurred / setDimmed
    ├─ TopBarLayer update context
    └─ TransientLayer.show / hide
    │
    ▼
AnimationController (runs entrance/exit transitions)
    │
    ▼
Render (compositor frame)
```

### Behavior Rules Per Mode

#### Show Desktop
- `workspaceState.dimAlpha → 0.4`, window content fade-out
- `DesktopLayer` opacity → 1.0
- `DockLayer` dan `TopBarLayer` tetap fully active
- Trigger: hot-corner, keyboard shortcut, gesture

#### LaunchpadLayer aktif
- `overlayState.launchpadVisible = true`
- `workspaceState.blurred = true`, `dimAlpha → 0.5`
- `dockState.opacity → 0.7` (masih visible)
- `topBarState.inputEnabled = false` (tapi visible)
- Dismiss: klik background, Escape, keyboard shortcut

#### WorkspaceOverviewLayer aktif
- `overlayState.overviewVisible = true`
- `workspaceState.interactionBlocked = false` (masih bisa drag window)
- `dockState.opacity → 0.85`
- Tidak ada blur di TopBar

#### ToTheSpotLayer aktif
- `overlayState.toTheSpotVisible = true`
- Workspace tidak di-blur — overlay ringan saja
- `dockState` tidak berubah
- Semua layer lain tetap interaktif (focus intercept only)

#### Multiple overlay aktif bersamaan
- ToTheSpot bisa coexist dengan overlay lain (z-order tertinggi)
- Launchpad + Overview: mutex — hanya satu aktif
- Notification: selalu independent dari overlay state

### Z-Order Policy

Normal state:
```
WallpaperLayer(0) < DesktopLayer(10) < WorkspaceLayer(20)
  < DockLayer(30) < TopBarLayer(40) < Overlays(50–80) < SystemModal(90)
```

Saat overlay aktif (contoh Launchpad):
```
WallpaperLayer(0) < DesktopLayer(10) < WorkspaceLayer(20, dimmed)
  < DockLayer(30, 0.7) < TopBarLayer(40) < LaunchpadLayer(50) < ToTheSpot(70) < SystemModal(90)
```

Aturan:
- SystemModalLayer selalu di atas semua layer (z=90)
- ToTheSpotLayer selalu di atas overlay lain tapi di bawah SystemModal (z=70)
- DockLayer tidak pernah di-raise/lower — z-order static, hanya opacity berubah

### Recovery Strategy (Unbreakable Rules)

```
Rule 1: Persistent layer tidak boleh crash karena overlay
  → TransientLayer berjalan dalam isolated QML context
  → Error di TransientLayer → log + hide gracefully → state reset

Rule 2: Overlay crash → fallback ke base shell
  → ShellStateController mendeteksi overlay timeout / null reference
  → Auto-dismiss overlay, reset ShellState ke default
  → DockLayer + TopBarLayer tetap accessible

Rule 3: WorkspaceLayer state preserved meskipun overlay gagal
  → workspaceState tidak di-clear saat mode change
  → window stack di-restore dari compositor state model

Rule 4: Input tidak boleh deadlock
  → InputRouter memiliki timeout: jika layer tidak respond dalam 500ms
    → fallback ke DockLayer + TopBarLayer input

Rule 5: SystemModalLayer bypass semua state
  → polkit / crash dialogs muncul di z=90 tanpa perlu ShellState update
  → selalu interactable, tidak dipengaruhi overlay state
```

### Testing Scenarios

- [ ] Toggle LaunchpadLayer berulang cepat (< 100ms interval) tanpa state corruption
- [ ] Masuk WorkspaceOverview lalu buka app dari DockLayer — overview dismiss, app launch normal
- [ ] Aktifkan Show Desktop saat drag window — drag cancel gracefully, window position restored
- [ ] Trigger ToTheSpotLayer saat LaunchpadLayer aktif — keduanya coexist tanpa z-order conflict
- [ ] Simulasi crash/null pada LaunchpadLayer — base shell (Dock + TopBar) tetap responsive
- [ ] WorkspaceLayer state preserved saat launchpad dismiss — focused window tidak berubah
- [ ] SystemModalLayer muncul saat overview aktif — modal menang, overview tetap di belakang
- [ ] Rapid mode switching: desktop → launchpad → overview → tothespot dalam < 1s
- [ ] WorkspaceLayer blur/unblur cycle 10x tanpa frame drop
- [ ] InputRouter deadlock sim: layer tidak respond → timeout → fallback confirmed

### Implementation Phases

#### Phase 1 — Audit & Baseline (refactor, no new feature)
  - Findings: boolean state machine (launchpadVisible, workspaceVisible, styleGalleryVisible),
    direct assignments in signal handlers — no formal state machine.
  - Anti-pattern: `TopBarWindow.qml:19` hidden `!desktopScene.launchpadVisible` → **fixed**.
  - Created `Qml/ShellZOrder.qml` singleton with named constants.
  - DockWindow and TopBarWindow updated to use `ShellZOrder.dock` / `ShellZOrder.topBar`.
  - Remaining: internal DesktopScene z-values (different coordinate space, future phase).
  - Created `Qml/ShellState.qml`: overlay flags + derived per-layer opacity/blur state.
  - DesktopScene syncs `launchpadVisible`, `workspaceOverviewVisible`, `styleGalleryVisible`.
  - Main.qml syncs `toTheSpotVisible`.
  - WorkspaceWindow, TopBarWindow, DockWindow now bind to `ShellState` values.
  - TopBar: removed `!desktopScene.launchpadVisible` guard — now always visible, opacity-dimmed.
  - LaunchpadWindow: offset `launchpadFrame` to `y: panelHeight` so TopBar panel shows through.
  - DockWindow: opacity driven by `ShellState.dockOpacity` (0.0 while launchpad shows own dock).
  - Audit confirmed: no `workspaceState = {}` or full reset on mode change — state preserved. ✓

#### Phase 2 — ShellStateController ✓ DONE
  - Q_PROPERTY writable: launchpadVisible, workspaceOverviewVisible, toTheSpotVisible, styleGalleryVisible, showDesktop
  - Q_PROPERTY readonly (derived): topBarOpacity, dockOpacity, workspaceBlurred, workspaceBlurAlpha, workspaceInteractionBlocked, anyOverlayVisible
  - Q_INVOKABLE: setXxx(), toggleXxx(), dismissAllOverlays()
  - Registered as context property "ShellStateController" via AppStartupBridge
  DesktopScene.qml and Main.qml call ShellStateController.setXxx() instead of writing ShellState directly

#### Phase 3 — InputRouter ✓ DONE
  LockScreen(100) > ToTheSpot(30) > Launchpad(20) > WorkspaceOverview(10) > BaseLayer(0)
  - LockScreen: only `shell.lock`
  - Launchpad: blocks `workspace.*` and `window.move_*`
  - WorkspaceOverview: blocks `shell.tothespot` and `shell.clipboard`
  - ToTheSpot: blocks `shell.workspace_overview` and `workspace.*`
  `if (root.lockScreenVisible) return` inline guards with `ShellInputRouter.canDispatch(action)`
  when it fires → `ShellStateController.dismissAllOverlays()` + `overlayDismissTimedOut` signal
  timeout guard fire + cancel, layerChanged signal guards

#### Phase 4 — Recovery & Crash Isolation
  dipindah ke sub-window atau deferred Loader dengan error boundary
  (`ShellLayerWatchdog` — stuck-overlay timer, `requestRecovery()` → `dismissAllOverlays()`)
- [ ] `WorkspaceLayer` state recovery dari `CompositorStateModel` setelah overlay crash
  (`ShellLayerWatchdog` 1s timer + `persistentLayerRestored` Connections in Main.qml)
  (`shell_layer_watchdog_test` — 24 cases: stuck detection, recovery, load-error isolation, rapid-toggle stress)

#### Phase 5 — Hardening & Contract Tests
  (all 5 above in `shell_contract_test` — 27 cases total)

## Program (Notification System Refresh: macOS-like Notification Center)
  - `Notification Service` (backend daemon, source of truth)
  - `Notification UI` (QML only, model-driven rendering)
  - `Integration Layer` (D-Bus + portal + shell hooks)
- [~] Finalize notification types:
  - Banner (transient, top-right, max 3 visible, auto-dismiss 5–8s, hover pause, swipe dismiss)
  - Notification Center (persistent, slide-in right panel, grouped history, virtualized list)
  - card radius `14`
  - padding `12–16`
  - vertical rhythm `8`
  - light bg `rgba(255,255,255,0.7)`
  - dark bg `rgba(30,30,30,0.7)`
  - blur + subtle shadow + elevation layering
  - banner entry: slide-right-to-left + fade
  - banner exit: fade + slight shrink
  - center panel: slide-in right `300ms` + dim overlay
  - motion uses non-linear easing curves (no linear feel)
  - mouse: hover pause, click default action, drag/swipe dismiss
  - keyboard: `Super+N` toggle center, arrows navigate, `Enter` open, `Delete` dismiss
  - optional edge gesture to open center
- [~] Implement inline action system:
  - compact action buttons
  - max 2–3 actions per card (`NotificationCard` now capped at 3)
  - support reply/open/accept/decline patterns
- [~] Add advanced policy features:
  - Do Not Disturb (suppress banner, keep center)
  - priority routing (`low=center`, `normal=banner`, `high=sticky banner`)
  - smart grouping by app/thread
  - rich payload support (image preview, progress, media controls)
  - Service: `org.example.Desktop.Notification`
  - Methods: `Notify`, `Dismiss`, `GetAll`, `ClearAll`, `ToggleCenter`
  - Signals: `NotificationAdded`, `NotificationRemoved`
  - Add internal mapping plan to SLM naming convention when stabilized
  - consume `org.freedesktop.Notifications`
  - translate to internal notification model + priority/group semantics
  - `id`, `app_id`, `title`, `body`, `icon`, `timestamp`, `actions[]`, `priority`, `group_id`, `read/unread`
  - `NotificationManager.qml`
  - `BannerContainer.qml`
  - `NotificationCard.qml`
  - `NotificationCenter.qml`
  - `NotificationGroup.qml`
- [~] Performance hardening:
  - enforce max visible banners = 3
  - virtualized `ListView` for center/history
  - avoid expensive blur invalidation/recomputation
  - prefer GPU-friendly effects
- [ ] Deep system integration:
  - global search indexing/query for recent notifications
  - per-app notification permission controls (portal permission store integration)
  - top panel icon + unread badge + toggle behavior
- [ ] Multi-monitor logic:
  - banners on active monitor
  - center opens on focused screen
- [ ] Edge-case policy:
  - fullscreen app: minimal/delayed banners
  - gaming mode: queue banners
  - crash recovery: persist/restore queue + history state
- [ ] Notification sound subsystem:
  - subtle sound categories: `message`, `warning`, `system alert`
  - per-category mute/volume policy hooks
- [ ] Storage strategy:
  - in-memory active queue
  - persistent history (JSON/DB) with retention policy
- [~] Security & abuse control:
  - app identity validation
  - optional sandbox/portal permission gating
- [ ] Developer API wrapper:
  - provide simple helper contract:
    - `notify({app, title, body, priority, actions})`
- [ ] Optional extensions (post-v1):
  - mobile sync
  - smart prioritization
  - timeline view

### Deliverables (Notification Program)

## Program (Solid & Unbreakable)
  - `MissingComponents.blockingMissingComponentsForDomain(domain)`
  - `MissingComponents.hasBlockingMissingForDomain(domain)`
- [~] Add end-to-end regression tests:
  - [~] UI flow baseline (headless harness): missing dependency -> warning UI -> install -> refresh -> restored (`settings_missing_components_ui_test`).
  - [~] UI flow real-session polkit dialog integration lane added (`scripts/smoke-polkit-agent-runtime.sh`, gated by `SLM_POLKIT_RUNTIME_SMOKE=1`).
  - [ ] Promote runtime lane from smoke to strict CI gate after host/session reliability baseline stabilizes.
  - Implemented in `DaemonHealthMonitor`: `degraded`, `reasonCodes`, persistent `timeline` (+ file path + size) exposed via `DaemonHealthSnapshot()`.
  - `slm-watchdog` now snapshots healthy config and persists `last_good_snapshot` id.
  - `slm-session-broker` recovery mode now prioritizes rollback to `last_good_snapshot`, then previous, then safe baseline.
  - rollback priority covered by `sessionbroker_recovery_rollback_test`.

## Next (Portal Adapter hardening)
- [ ] Phase-5 InputCapture:
  - [ ] Bind provider to real compositor pointer-capture/barrier primitives (currently stateful contract + optional command hook only).
- [ ] Phase-6 Privacy/data portals:
  - [ ] Camera
  - [ ] Location
  - [ ] PIM metadata/resolution portals (contacts/calendar/mail metadata first, body last)
- [ ] Phase-7 Hardening & interop:
  - [ ] DBus contract/regression suite GTK/Qt/Electron/Flatpak

## Next (Permission Foundation)

---

## Inisiatif: Polkit Authentication Agent (SLM Desktop)

> **Konteks:** Kembangkan polkit authentication agent milik desktop sendiri — bukan bergantung pada agent desktop lain. Fokus: stabil, aman, ringan, mudah diuji, terintegrasi rapi dengan session desktop. Core agent dipisah dari shell utama agar crash agent tidak menjatuhkan desktop shell. Target runtime Linux modern dengan systemd user session.

### Tujuan

1. Membuat Polkit authentication agent resmi milik desktop.
2. Agent berjalan otomatis saat user login ke session grafis.
3. Agent mendaftarkan diri sebagai session authentication agent Polkit.
4. Saat ada request autentikasi, agent menampilkan dialog native desktop.
5. Dialog mendukung: pesan autentikasi, identity/admin target, input password, tombol cancel, state loading, state password salah.
6. Agent aman terhadap: multi request, cancel, timeout, request source mati di tengah proses, restart agent, session lock.
7. Gunakan systemd user service untuk lifecycle.
8. Rancang agar nantinya bisa diintegrasikan dengan lockscreen dan shell desktop.

### Kebutuhan teknis

- Gunakan Polkit API resmi: `libpolkit-agent-1` + `libpolkit-gobject-1`.
- Authority tetap dikelola `polkitd`; agent hanya menangani registration, authentication flow orchestration, dan UI.
- Jangan memindahkan responsibility authorization decision ke agent.
- C++ untuk core agent, QML untuk UI.
- Signal/slot atau pola event-driven yang rapi.
- Hindari ketergantungan yang tidak perlu.
- Single-instance per session.
- Keamanan password: jangan log password, segera bersihkan buffer, minimalkan jejak data sensitif.

### Output yang dibutuhkan

1. Ringkasan arsitektur tingkat tinggi
2. Tahapan implementasi per fase
3. Struktur direktori proyek
4. Daftar class/module utama
5. State machine autentikasi
6. Integrasi systemd user service
7. Checklist security hardening
8. Checklist testing
9. Risiko implementasi yang perlu diwaspadai
10. Saran urutan pengerjaan paling efisien

### Preferensi desain

- Dialog autentikasi harus terasa seperti komponen sistem, bukan popup aplikasi biasa.
- Mudah di-theme sesuai desktop.
- Siapkan jalur untuk multi-monitor, HiDPI, accessibility, dark mode, localization.
- Siapkan kemungkinan fallback minimal jika UI shell belum siap.
- Agent memberi tahu shell bahwa dialog autentikasi sedang aktif, tanpa coupling berlebihan.

### Roadmap implementasi

#### Fase 1 — MVP: Agent registration + dialog minimal


#### Fase 2 — Stabilisasi: multi-request, error handling, state machine

- [ ] State machine lengkap: `Idle → Pending → Authenticating → Success/Failed/Cancelled`
- [ ] Queue multi-request: satu dialog aktif, sisanya antri
- [ ] Handle: cancel dari user, cancel dari caller (request source mati), timeout
- [ ] State `PasswordWrong`: shake animation + pesan error, tanpa clear password
- [ ] State `Loading`: spinner, disable input
- [ ] Auto-cancel jika caller process tidak ada lagi (poll/DBus watch)
- [ ] Restart handling: agent mati → systemd restart → re-register ke polkit

#### Fase 3 — Integrasi shell + hardening

- [ ] Notifikasi ke shell: emit DBus signal `AuthDialogActive(bool)` → shell bisa dim/blur atau lock input
- [ ] Integrasi dengan lockscreen: jika session terkunci saat auth request masuk → queue sampai unlock
- [ ] Security hardening: clear password buffer setelah AuthSession selesai, no-logging guard, input sanitization
- [ ] Accessibility: label ARIA, keyboard navigation, focus management
- [ ] Multi-monitor: dialog muncul di monitor aktif/focused
- [ ] HiDPI: image provider icon scaled, layout fluid

#### Fase 4 — Polish + production hardening

- [ ] Unit test: AuthSessionManager queue, state machine transitions, cancel/timeout paths
- [ ] Integration test: mock polkit authority, verify agent registration + response
- [ ] Localization: string extraction, i18n-ready QML
- [ ] Dark mode: dialog ikut tema desktop
- [ ] Audit log: catat request masuk/keluar (tanpa password), untuk observability
- [ ] Fallback mode: jika QML engine gagal load, tampilkan dialog via QWidget minimal

### Struktur direktori yang diusulkan

```
src/
  polkit-agent/
    main.cpp                      # entry point, single-instance guard
    polkitagent.cpp/h             # PolkitAgentListener subclass (GLib bridge)
    authsessionmanager.cpp/h      # queue, lifecycle, timeout management
    authsession.cpp/h             # satu authentication session
    authdialogcontroller.cpp/h    # QObject jembatan C++ ↔ QML
    dbus/
      agentnotifier.cpp/h         # emit DBus signal AuthDialogActive ke shell
Qml/
  polkit-agent/
    AuthDialog.qml                # dialog utama
    PasswordField.qml             # field password + shake animation
    AuthIdentityView.qml          # tampilan identity (ikon, nama, realm)
systemd/
  slm-polkit-agent.service        # user service unit
```

### Class utama C++

| Class | Tanggung jawab |
|---|---|
| `SlmPolkitAgent` | Subclass `PolkitAgentListener`, register/unregister ke polkit, terima `InitiateAuthentication` callback |
| `AuthSessionManager` | Queue request, buat/destroy `AuthSession`, enforce single-dialog-at-a-time |
| `AuthSession` | Wrap `PolkitAgentSession` GLib object, expose signal Qt: `showError`, `showInfo`, `completed` |
| `AuthDialogController` | QObject yang di-expose ke QML: property `message`, `identity`, `actionId`; invokable `authenticate(password)`, `cancel()` |
| `AgentNotifier` | Publish `org.slm.desktop.AuthAgent` DBus interface: signal `AuthDialogActive(bool)` |

### State machine autentikasi

```
[Idle]
  │ InitiateAuthentication received
  ▼
[Pending]           ← request masuk queue jika dialog sudah aktif
  │ dialog ready, user siap
  ▼
[Authenticating]    ← password di-submit ke PolkitAgentSession
  │
  ├─ Success   → emit Completed(true)  → [Idle]
  ├─ Failed    → [PasswordWrong]       → kembali ke [Authenticating] atau [Cancelled]
  ├─ Cancelled → emit Completed(false) → [Idle]
  └─ Timeout   → emit Completed(false) → [Idle]

[PasswordWrong]
  │ retry atau cancel
  ├─ retry  → [Authenticating]
  └─ cancel → [Cancelled] → [Idle]
```

### Systemd user service

```ini
# ~/.config/systemd/user/slm-polkit-agent.service
[Unit]
Description=SLM Desktop Polkit Authentication Agent
PartOf=graphical-session.target
After=graphical-session.target
ConditionEnvironment=WAYLAND_DISPLAY

[Service]
ExecStart=/usr/libexec/slm-polkit-agent
Restart=on-failure
RestartSec=2s
TimeoutStopSec=5s

[Install]
WantedBy=graphical-session.target
```

### Security hardening checklist

- [ ] Password tidak pernah masuk log (journal, qDebug, stderr)
- [ ] Buffer password di-zero setelah `AuthSession` selesai (ZeroMemory / memset_s / `SecureZeroMemory`)
- [ ] Tidak ada core dump yang berisi password (set `RLIMIT_CORE=0` di main)
- [ ] Single-instance guard via `QLocalServer` atau lock file
- [ ] Verifikasi caller: pastikan `PolkitSubject` valid sebelum tampilkan dialog
- [ ] Tidak expose detail internal error ke user (tampilkan pesan generik)
- [ ] Dialog tidak bisa di-dismiss tanpa explicit cancel (tidak bisa klik luar dialog)
- [ ] Timeout auto-cancel jika tidak ada respons user dalam N detik

### Testing checklist

- [ ] Unit: `AuthSessionManager` queue — request masuk saat dialog aktif masuk antri dengan benar
- [ ] Unit: state machine — semua transisi valid, transisi invalid di-reject
- [ ] Unit: timeout — session auto-cancel setelah timeout
- [ ] Unit: cancel dari caller — process mati → session cleanup tanpa crash
- [ ] Integration: `pkexec ls /root` → dialog muncul → password benar → sukses
- [ ] Integration: password salah → state `PasswordWrong` → retry → sukses
- [ ] Integration: cancel → proses `pkexec` exit dengan kode error
- [ ] Integration: dua request berurutan → keduanya selesai dengan benar
- [ ] Integration: agent restart di tengah session → request berikutnya OK
- [ ] Manual: dialog muncul di monitor yang sedang digunakan (multi-monitor)
- [ ] Manual: HiDPI — icon dan font tidak blur
- [ ] Manual: dark mode — dialog mengikuti tema desktop

### Risiko implementasi

| Risiko | Mitigasi |
|---|---|
| GLib event loop bentrok dengan Qt event loop | Gunakan `QEventLoop` + GLib integration atau run GLib di thread terpisah |
| `PolkitAgentListener` adalah GObject — interop C++ ribet | Buat thin C wrapper atau gunakan `gio-qt` bridge |
| Agent crash → semua `pkexec` hang | Systemd restart cepat (2s), polkit timeout sendiri setelah ~30s |
| Race condition multi-request | Queue dengan mutex, satu dialog aktif |
| Memory leak buffer password | RAII wrapper + explicit zero di destructor |
| Dialog muncul di belakang fullscreen app | Pastikan dialog memiliki `Qt::WindowStaysOnTopHint` atau protocol Wayland `xdg_activation` |

### Saran urutan pengerjaan

1. Buat `slm-polkit-agent` binary minimal: register ke polkit, print ke stdout saat ada request (belum ada dialog)
2. Tambah `AuthSession` + state machine sederhana
3. Buat `AuthDialogController` + dialog QML minimal (tanpa animasi)
4. Wire end-to-end: `pkexec` → dialog → password → sukses/gagal
5. Tambah queue multi-request + timeout
6. Tambah `AgentNotifier` DBus signal ke shell
7. Systemd user service + autostart
8. Polish: animasi, dark mode, accessibility, localization
9. Testing suite lengkap
10. Security hardening pass final

---

## Inisiatif: Login/Session Stack (SLM Desktop Foundation)

> **Konteks:** Anda adalah software architect dan engineer senior Linux desktop stack. Tugas Anda adalah merancang dan mengimplementasikan fondasi login/session stack untuk desktop Linux modern bernama **SLM Desktop**.

### Tujuan utama

Bangun arsitektur desktop yang **solid, opinionated, recoverable, dan self-healing**. Desktop tidak boleh mudah rusak hanya karena user mengganti konfigurasi. Jika konfigurasi baru menyebabkan crash loop, desktop harus bisa **mendeteksi, rollback, lalu masuk ke safe mode atau recovery mode**.

Target desain:

* desktop menentukan stack inti secara jelas
* login flow dikendalikan platform
* recovery dimulai sejak sebelum shell utama dijalankan
* konfigurasi desktop bersifat transactional
* crash loop harus bisa dideteksi dan dipulihkan otomatis
* user tetap punya jalur masuk aman meskipun sesi normal rusak

---

### Keputusan arsitektur yang wajib diikuti

#### 1. Jangan membuat display manager penuh sendiri

Gunakan **greetd** sebagai backend login daemon.

#### 2. SLM memiliki komponen sendiri di atas greetd

Implementasikan komponen berikut:

* `slm-greeter` → greeter grafis Qt/QML
* `slm-session-broker` → broker sesi resmi
* `slm-watchdog` → pemantau health sesi user
* `slm-recovery-app` → aplikasi pemulihan minimal
* `slm-configd` atau `ConfigManager` → pengelola konfigurasi transactional
* session desktop resmi SLM

#### 3. greetd tidak boleh langsung menjalankan shell desktop

Alur wajib:

* `greetd` → `slm-greeter` → autentikasi user → pilih mode startup
* → `slm-session-broker` → validasi state → tentukan mode final
* → compositor + shell + daemon inti

#### 4. Desktop harus opinionated

Komponen inti dianggap **required components**:

* greetd backend login
* slm-greeter
* slm-session-broker
* compositor resmi atau daftar compositor resmi
* slm shell
* settings daemon
* policy agent
* portal backend
* notification daemon
* recovery/watchdog service

Komponen inti ini tidak dianggap bebas tukar. Sistem harus bisa mendeteksi bila state platform tidak sesuai.

---

### Mode sesi yang wajib ada

#### A. Normal Session

Mode biasa: tema aktif, plugin resmi aktif, layout user aktif, animasi aktif.

#### B. Safe Session

Dipakai jika crash loop atau dipilih user:

* tema custom dimatikan
* plugin/extension non-esensial dimatikan
* panel/layout default, wallpaper default, animasi berat dimatikan
* konfigurasi berisiko diabaikan

#### C. Recovery Session

Dipakai untuk diagnosis:

* shell minimal atau recovery shell
* hanya UI pemulihan dasar: reset settings, disable plugin, restore snapshot, log viewer dasar

#### D. Factory Reset Action

Bukan menghapus data user — hanya reset konfigurasi desktop ke default aman.

---

### Prinsip recovery yang wajib

#### 1. Transactional config

Model wajib:

* current config, previous config, safe/default config, optional versioned snapshots

#### 2. Delayed commit

* config baru diterapkan → sistem menunggu satu periode health check
* jika sesi tetap sehat → config ditandai `good`
* jika crash sebelum stabil → rollback ke snapshot sebelumnya

#### 3. Atomic write

* tulis ke file sementara → fsync → rename

#### 4. Crash loop detection

* increment `crash_count` setiap crash pada startup awal
* jika melebihi threshold → rollback ke last known good → safe/recovery mode

#### 5. Recovery sejak login

Greeter harus menyediakan opsi: Start Desktop / Start in Safe Mode / Start Recovery / Power actions.

---

### State dan penyimpanan

Direktori: `~/.config/slm-desktop/`

File:

* `config.json` — konfigurasi aktif
* `config.prev.json` — snapshot sebelumnya
* `config.safe.json` — baseline aman / default
* `state.json` — runtime state: `crash_count`, `last_mode`, `last_good_snapshot`, `safe_mode_forced`, `recovery_reason`, `last_boot_status`
* `snapshots/` — versi tambahan

Broker membaca `state.json` pada setiap startup.

---

### Tanggung jawab tiap komponen

#### slm-greeter (Qt/QML)

* daftar user / input password
* pilih mode: normal / safe / recovery
* tombol power + branding SLM
* tampilkan pesan: "desktop dipulihkan dari crash", "konfigurasi terakhir dibatalkan", "safe mode aktif"

#### slm-session-broker (C++ Qt Core atau Rust)

* terima mode startup dari greeter
* baca `state.json`
* periksa integritas platform
* validasi config
* putuskan normal/safe/recovery
* lakukan rollback jika perlu
* siapkan environment sesi → launch compositor + shell
* catat status ke journal/log

#### slm-watchdog (C++ atau Rust)

* pantau apakah shell/compositor berhasil stabil
* jika crash sebelum healthy → catat startup failure + tulis state crash
* tandai startup sukses jika sesi bertahan cukup lama

#### slm-recovery-app (Qt/QML)

* reset theme, panel, layout monitor
* disable plugin
* restore default / restore snapshot
* tampilkan ringkasan error/log

#### ConfigManager (shared library)

* get/set/validate config (schema-aware)
* snapshot current → promote → rollback ke previous / safe / default
* atomic save

---

### Platform integrity check

Yang harus dicek:

* session yang dijalankan memang SLM
* broker dijalankan melalui session resmi
* compositor termasuk daftar resmi
* file config valid, schema version cocok
* komponen inti tersedia

Jika gagal: jangan jalankan sesi normal → masuk safe/recovery mode dengan alasan yang jelas.

---

### Struktur proyek yang diusulkan

```
src/
  login/
    greeter/          # slm-greeter (Qt/QML)
    session-broker/   # slm-session-broker
    watchdog/         # slm-watchdog
    recovery-app/     # slm-recovery-app
    libslmconfig/     # shared ConfigManager + state lib
sessions/
  slm.desktop         # session resmi → Exec=slm-session-broker
scripts/
  install-session.sh
```

---

### Integrasi greetd

* `greetd.toml` → `command = "slm-greeter"`
* Session resmi: `slm.desktop` → `Exec=/usr/libexec/slm-session-broker`
* Mode dipilih greeter → dikirim ke broker via argumen atau environment flag
* **Satu session resmi**, mode ditentukan greeter + broker — bukan banyak session terpisah

---

### Preferensi teknologi

* `slm-greeter`, `slm-recovery-app` → Qt/QML
* `slm-session-broker` → C++ Qt Core (atau Rust)
* `slm-watchdog` → C++ atau Rust
* Linux Wayland-first
* systemd/journald untuk logging dan watchdog integration

---

## Roadmap: SLM Unbreakable Package System (`slm-package-policy-service`)

Tujuan:
- [~] Desktop tidak rusak akibat install/remove paket
- [~] Semua transaksi paket melewati policy engine
- [ ] User tetap bisa install aplikasi eksternal dengan kontrol risiko
- [~] Recovery + rollback tersedia
- [~] Backend awal APT, arsitektur extensible

Arsitektur target:
- [~] `User/GUI/Terminal -> Wrapper(apt/apt-get/dpkg) -> slm-package-policy-service -> Simulator -> Rule Engine -> Backend Executor -> Audit + Recovery`

### Phase 2 - Trust & Source Control

- [ ] Rule extension

### Phase 3 - Advanced Safety

- [ ] Dependency chain detection (termasuk indirect removal dan autoremove)
- [ ] Risk scoring
  - [ ] touching core = high
  - [ ] external repo = medium
  - [ ] local deb = high
- [ ] Sandbox redirect
  - [ ] Jika tersedia Flatpak, arahkan rekomendasi install via Flatpak

### Phase 4 - Recovery System

- [ ] Recovery mode package incident

### Phase 6 - Hardening

- [ ] Edge case handling

Deliverables minimum:
- [ ] policy system berbasis JSON/YAML

Eksekusi:
- [~] Lanjut bertahap per fase
- [ ] Fokus reliability (simulation + policy), bukan banyak fitur

---

## Roadmap: FileManager Archive Service (`slm-archived`)

Tujuan:
- [~] UX arsip sederhana ala Finder (tanpa dialog teknis berlebihan)
- [~] File manager hanya UI layer, semua logika arsip di service
- [~] Aman default (anti path traversal, policy symlink/hardlink, resource limits)

Prinsip UX:
- [~] Arsip diperlakukan sebagai file biasa
- [~] Opsi advanced disembunyikan dari alur utama

### Phase B - Finder-like UX defaults
- [ ] Heuristik post-extract:
- [ ] Context menu minimal:

### Phase D - Security hardening
- [ ] Overwrite strategy aman + conflict resolution sederhana

### Phase E - Integration polish
- [ ] Drag-and-drop archive behavior konsisten
- [ ] Smart progress UX:
- [ ] Telemetry internal (success/fail/latency) untuk tuning
- [ ] Docs + compatibility matrix format archive

---

## Roadmap Prompt: XDG Portal Secret (`org.freedesktop.portal.Secret`)

Status:
  - `docs/architecture/XDG_PORTAL_SECRET_ARCHITECTURE.md`
  - DBus spec draft: `docs/contracts/org.freedesktop.portal.Secret.xml`
  - Secret daemon skeleton: `slm-secretd` + `src/services/secret/secretservice.*`
  - Portal backend stub: `src/daemon/portald/portal-adapter/PortalSecretBridge.*`
  - Contract test baseline: `secretservice_contract_test` (Store/Get/Delete/Describe)
  - Dialog copy contract test: `portal_consent_dialog_copy_contract_test`
  - Runtime guard behavior test: `portal_consent_dialog_behavior_test` (`Always Allow` for `Secret.Delete` requires explicit confirm)
  - Runtime guard behavior test: `portal_consent_dialog_behavior_test::secretDelete_nonPersistent_hidesAlwaysAllowControls`
  - Dialog receives `persistentEligible` and hides `Always Allow` when policy disallows persistence.
  - `PermissionStore` persistence now enforced by policy/spec eligibility (not only by UI response).
  - `ClearAppSecrets` forced to one-time consent (no persistent grant).
  - Integration contract: `portal_secret_integration_test::clearAppSecrets_consentAlwaysAllowDoesNotPersist_contract`
  - Integration contract: `portal_secret_integration_test::clearAppSecrets_denyAlwaysDoesNotPersistAndReprompts_contract`
  - Integration contract: `portal_secret_integration_test::deleteSecret_consentAlwaysAllowPersists_contract`
  - Integration contract: `portal_secret_integration_test::getSecret_consentAlwaysAllowPersists_contract`
  - Integration contract: `portal_secret_integration_test::storeSecret_consentAlwaysAllowPersists_contract`
  - Integration contract: `portal_secret_integration_test::secretConsentPayload_persistentEligible_matrix_contract`
  - Integration contract: `portal_dialog_bridge_payload_test` (payload field contract incl. `persistentEligible`)
  - Integration contract: `portal_secret_consent_contract_snapshot_test` (docs snapshot guard for consent invariants)
  - Settings MVP (`Permissions > App Secrets`) now includes:
    - read-only secret consent summary per app (`grantCount` + `lastUpdated`)
    - revoke stored secret access grants per app (`RevokeSecretConsentGrants`)
    - existing clear-secret-data action remains available (`ClearAppSecrets`)
    - headless-safe test hook in `SettingsApp` (`setPortalInvokerForTests`) for non-DBus contract coverage
  - Test suite runner:
    - build: `cmake --build build/toppanel-Debug --target secret_consent_test_suite -j8`
    - run: `ctest --test-dir build/toppanel-Debug -L secret-consent --output-on-failure`
    - one-shot: `scripts/test-secret-consent-suite.sh build/toppanel-Debug`
    - test-only fast path: `SLM_SECRET_CONSENT_SKIP_BUILD=1 scripts/test-secret-consent-suite.sh build/toppanel-Debug`
    - unified entrypoint: `scripts/test.sh secret-consent build/toppanel-Debug`
    - nightly toggle: `SLM_TEST_NIGHTLY_SECRET_CONSENT_MODE=required|auto|skip` (default `auto`)
    - nightly build toggle: `SLM_TEST_NIGHTLY_SECRET_CONSENT_SKIP_BUILD=1|0` (default `1`)
    - guard env: `SLM_SECRET_CONSENT_MIN_TESTS` / `SLM_SECRET_CONSENT_STRICT_NAMES` / `SLM_SECRET_CONSENT_EXPECTED_TESTS`
    - split settings-side secret consent contracts into dedicated target: `settingsapp_secret_consent_test` (non-DBus, headless-safe)
    - isolate legacy policy baseline test into label `baseline-flaky` (`settingsapp_policy_test`) and exclude by default from full/nightly suite
    - nightly CI report lane for `baseline-flaky` (non-blocking) to keep visibility without blocking release gates
    - add stable settings policy lane `policy-core` (`settingsapp_policy_core_test`) for deterministic coverage in main suite
    - nightly policy-core enforcement (`SLM_TEST_NIGHTLY_POLICY_CORE_MODE=required`) in `scripts/test.sh` + CI workflow
    - add dedicated runner `scripts/test-policy-core-suite.sh` with guard env (`MIN_TESTS` + `STRICT_NAMES` + `EXPECTED_TESTS`)
    - nightly fast-path toggle for policy-core (`SLM_TEST_NIGHTLY_POLICY_CORE_SKIP_BUILD=1|0`, default `1`)
    - nightly full-suite exclude override (`SLM_TEST_NIGHTLY_FULL_EXCLUDE_LABELS`) to avoid duplicate execution of dedicated lanes
    - CI nightly skips duplicate lint in `scripts/test.sh nightly` via `SLM_TEST_SKIP_UI_LINT=1` + `SLM_TEST_SKIP_CAPABILITY_MATRIX_LINT=1`
    - add PR/push quick gate job `policy-core-only` in CI workflow

Tujuan utama:
- [ ] Secret Portal sebagai lapisan standar resmi desktop untuk store/get/delete/manage secret.
- [ ] API dan trust model konsisten untuk app sandbox + non-sandbox.
- [ ] UX sederhana non-teknis, dengan keamanan terpusat dan recovery-friendly.

Deliverable dokumen desain (wajib):
  - secure-by-default, least privilege, consent jelas, interoperabilitas, recovery-friendly.
  - pemetaan peran: app biasa/sandbox, xdg-desktop-portal, `xdg-desktop-portal-slm`, PermissionStore, bus, secret daemon, encrypted backend, Settings, shell/dialog, lock/login, recovery.
  - diagram arsitektur teks.
  - object path, interface, methods, argumen, return, error, async flow, request object usage, identity/binding app (sandbox vs non-sandbox).
  - bahas method: `StoreSecret`, `GetSecret`, `DeleteSecret`, `ListSecrets` (atau alternatif aman), `GrantAccess`, `RevokeAccess`, `DescribeSecret`, `ClearAppSecrets`, `LockSecrets`, `UnlockSecrets`.
  - jelaskan method yang sebaiknya tidak ada (jika ada) + alternatif.
  - trigger popup, allow always/one-time, re-prompt policy, read/write/delete split, cross-app access, system service policy, helper/background policy.
  - tabel policy decision.
  - alur dialog save/read/cross-app secret, trust indicator, wording sederhana, tombol utama/sekunder, re-auth vs normal consent, notif sukses/gagal, error non-teknis.
  - microcopy bahasa Inggris.
  - Settings (`Privacy & Security -> Secrets`)
  - login/unlock session
  - network & Wi-Fi
  - browser/web apps/cloud accounts
  - file manager (SMB/SFTP/WebDAV/cloud creds)
  - developer settings/debug
  - global search
  - clipboard/share/automation.
  - libsecret/Secret Service, GNOME Keyring, KWallet, backend internal, encrypted file store, systemd credentials (ephemeral), TPM/hardware-backed.
  - rekomendasi untuk MVP, v1 stabil, jangka panjang.
  - failure scenarios: daemon gagal start, storage korup, permission DB rusak, fallback behavior, safe mode/recovery, health check, watchdog, migration, backup/restore.
  - threat model + mitigasi: cross-app read, identity spoofing, helper abuse, memory/log/crash/clipboard/search/backup leak, brute force, lock-session exposure, root boundary.
  - FileChooser, Background, Notification, GlobalShortcuts, Screenshot/ScreenCast, PermissionStore, Account/Settings, OpenURI/Documents.
  - Phase 0..7 (foundation -> hardening) dengan tujuan, dependensi, risiko, DoD, test plan.
  - DBus/API, backend, daemon, portal backend, permission system, shell/dialog UI, settings, login/session, file manager, network, QA/security, recovery, packaging.
  - sandbox save token pertama kali
  - non-sandbox read own token
  - file manager save SMB password (`remember`)
  - online accounts refresh token
  - user revoke access dari Settings
  - daemon gagal start saat login
  - storage korup -> recovery flow
  - app coba baca secret app lain.
  - komponen build-vs-reuse, urutan implementasi paling realistis, fitur yang ditunda, tradeoff utama.

Preferensi keputusan arsitektur:
- [ ] MVP boleh reuse Secret Service/libsecret untuk percepatan.
- [ ] Jangka panjang boleh menuju secret backend milik desktop sendiri.
- [ ] Secret subsystem tidak boleh jadi single point of failure untuk boot sesi grafis.
- [ ] Selaras penuh dengan permission architecture desktop (policy sentral, auditable, recoverable).

## Program: SSOT Settings System (Shell Theme Identity + App Theme Routing)

Tujuan utama:
- [ ] Bangun `desktop-settingsd` sebagai SSOT tunggal untuk appearance, shell theme, app theme routing, icon routing, fallback policy.
- [ ] Pisahkan tegas domain tema: shell desktop vs aplikasi (GTK/KDE/Qt generic).
- [ ] Event-driven live update lintas shell + adapter tanpa restart sesi (sebisa mungkin).
- [ ] Recovery-safe: invalid setting tidak boleh membuat shell gagal start.

### Scope arsitektur (wajib)
- [ ] Satu SSOT mencakup: storage, schema, policy, event notification, adapter output, live update, app classification, fallback, recovery.
- [ ] Satu policy engine terpusat: `desktop-theme-policy`.
- [ ] Output multi-domain:
  - shell adapter
  - GTK adapter
  - KDE adapter
  - Qt desktop fallback adapter
- [ ] Larang model "satu literal theme untuk semua app".

### Phase 1 — Settings Core Service (`desktop-settingsd`)
- [~] Implement daemon service dengan API:
  - `GetSettings()`
  - `SetSetting(path, value)`
  - `SetSettingsPatch(patch)`
  - `SubscribeChanges()`
  - `ClassifyApp(appDescriptor)`
  - `ResolveThemeForApp(appDescriptor, options)`
- [~] Emit signal:
  - `SettingChanged`
  - `AppearanceModeChanged`
  - `ThemePolicyChanged`
  - `IconPolicyChanged`
- [~] SSOT keyspace minimum:
  - global appearance
  - shell theme
  - GTK routing
  - KDE routing
  - app theme policy
  - fallback policy
  - optional per-app override
  - implemented:
    - `src/daemon/settingsd/settingsd_main.cpp`
    - `src/services/settingsd/settingsservice.{h,cpp}`
    - `src/services/settingsd/settingsstore.{h,cpp}`
    - `tests/settingsservice_dbus_test.cpp`
    - legacy compatibility bridge (`UIPreferences`) sudah dihapus; jalur runtime via `DesktopSettings` (settingsd SSOT)
    - policy API bridge:
      - `ClassifyApp` + `ResolveThemeForApp` exposed from `desktop-settingsd`
      - integrated with `AppThemeClassifier` + `ThemePolicyEngine`

### Phase 2 — Schema settings (versioned + migratable)
- [~] Definisikan schema versioned (JSON) dengan default value + migrator:
  - `schemaVersion`
  - `globalAppearance`
  - `shellTheme`
  - `gtkThemeRouting`
  - `kdeThemeRouting`
  - `appThemePolicy`
  - `fallbackPolicy`
  - `perAppOverride`
- [~] Tambahkan `lastKnownGood` + recovery path bila schema/value invalid.
- [ ] Validasi ketat pada write (enum, range, nullable, compatibility checks).
  - implemented:
    - schema normalization + migration hook (`schemaVersion`) in `src/services/settingsd/settingsstore.cpp`
    - startup recovery from corrupt/invalid settings via `*.last-good.json`
    - rollback-on-failed-save path + atomic save (`QSaveFile`)
    - env override path for testability: `SLM_SETTINGSD_STORE_PATH`

### Phase 3 — App Theme Category System
- [~] Kategori app minimum:
  - `shell`, `gtk`, `kde`, `qt-generic`, `qt-desktop-fallback`, `unknown`
- [~] Modular classifier pipeline:
  - desktop file metadata
  - `Categories` / `X-KDE-*`
  - executable fingerprint
  - manual override map
- [ ] Rules:
  - shell selalu shell theme
  - gtk -> GTK routing
  - kde -> KDE routing
  - qt-generic -> evaluasi kompatibilitas
  - incompatible qt -> desktop fallback
  - unknown -> safe fallback
  - implemented baseline:
    - `src/services/theme-policy/appthemeclassifier.{h,cpp}`
    - manual override map (`appId`/`desktopFileId`/`executable`)
    - heuristik awal: shell/kde/gtk/qt-generic/unknown
    - tests: `tests/appthemeclassifier_test.cpp`

### Phase 4 — Theme Policy Engine (`desktop-theme-policy`)
- [~] Resolve final keputusan berdasarkan mode aktif (`dark`/`light`/`auto`):
  - `themeSource`
  - `iconSource`
  - `fallbackApplied`
- [~] Hasil kebijakan minimum:
  - shell -> shell theme mode aktif
  - gtk -> gtk theme + gtk icon
  - kde -> kde theme + kde icon
  - qt incompatible -> desktop fallback theme/icon
  - implemented baseline:
    - `src/services/theme-policy/themepolicyengine.{h,cpp}`
    - routing shell/gtk/kde/qt-generic/qt-desktop-fallback/unknown
    - compat switch `qtKdeCompatible`
    - tests: `tests/themepolicyengine_test.cpp`
- [ ] Policy matrix terdokumentasi dan ditest.

### Phase 5 — Adapter / Output Layer
- [ ] Shell adapter:
  - update token/theme runtime Qt/QML shell
  - shell tidak pernah di-drive langsung oleh tema KDE/GTK
- [ ] GTK adapter:
  - sinkron theme + icon theme aktif sesuai mode
- [ ] KDE adapter:
  - sinkron theme + icon theme aktif sesuai mode
- [ ] Qt desktop fallback adapter:
  - apply fallback style untuk Qt non-KDE incompatible
- [~] Runtime launch env routing baseline:
  - `AppExecutionGate` now sends app descriptor (`appId`, `desktopFileId`, `executable`, `categories`) to resolver.
  - `AppExecutionGate` now routes desktop launch through `AppLauncher` when resolver exists, even with `DockModel` attached (dock keeps focus/note responsibilities, launch env no longer bypassed by dock launch path).
  - descriptor enriched with `desktopKeys`; options include heuristic `qtKdeCompatible`.
  - `LaunchEnvResolver` now calls `ResolveThemeForApp` and overlays launch env hints:
    - common: `SLM_THEME_MODE`, `SLM_THEME_SOURCE`, `SLM_ICON_THEME_SOURCE`, `SLM_THEME_CATEGORY`
    - gtk: `GTK_THEME`, `ICON_THEME`
    - kde: `KDE_COLOR_SCHEME`, `KDE_ICON_THEME`
    - qt-fallback: `SLM_QT_FALLBACK_THEME`
  - Added conflict cleanup in resolver overlay:
    - gtk route clears KDE + qt-fallback env leftovers
    - kde route clears GTK + qt-fallback env leftovers
    - qt-fallback route clears GTK + KDE env leftovers
  - Added resolver overlay contract tests in `tests/launchenvresolver_test.cpp`:
    - category env mapping (`gtk`, `kde`, `qt-desktop-fallback`)
    - note-driven routing for `qt-generic` (`kde-compatible` and `desktop-fallback`)
    - unknown policy routing (`unknown-safe-fallback`, `unknown-kde-default`)
    - conflict env cleanup assertions
    - DBus-availability-safe behavior (auto-skip when session bus unavailable in headless environments)
  - Added end-to-end launch trace (gated by `SLM_THEME_POLICY_TRACE`) in:
    - `LaunchEnvResolver`: resolve stage + error stage logs
    - `AppLauncher`: emitted policy/env snapshot just before `GAppLaunchContext` execution
  - next: move from hint-only to dedicated adapter-backed apply paths (GTK/KDE/Qt fallback) with stricter compatibility checks.

### Phase 6 — Event flow & live update
- [ ] Alur standar perubahan mode:
  - user ubah mode -> `desktop-settingsd` commit -> policy recompute -> adapters apply -> UI refresh
- [ ] Pastikan propagation atomik dan konsisten lintas output.
- [ ] Tambah debounce/coalescing agar tidak spam apply saat burst update.

### Phase 7 — Settings UI contract
- [ ] Contract Settings pages:
  - `Appearance` (mode/accent/font/scale/cursor/icons)
  - `Desktop` (shell theme/panel/blur/radius/animation)
  - `Applications` (GTK/KDE themes, GTK/KDE icons, Qt fallback, per-app override)
- [ ] UI write harus lewat API SSOT; larang write langsung ke backend adapter.
- [ ] Jelaskan microcopy agar shell theme vs app theme tidak ambigu.
  - [~] Partial cutover implemented:
    - added `src/apps/settings/desktopsettingsclient.{h,cpp}` (DBus client to `org.slm.Desktop.Settings`)
    - exposed to QML as `DesktopSettings` from `src/apps/settings/main.cpp`
    - migrated `AppearancePage.qml` controls to SSOT-first read/write via `DesktopSettings`:
      - Color Theme
      - Accent Color
      - Font Scale
      - GTK Theme (light/dark)
      - KDE Color Scheme (light/dark)
      - GTK Icon Theme (light/dark)
      - KDE Icon Theme (light/dark)
      - Dock (animation style, auto-hide, drop pulse, drag sensitivity mouse/touchpad) via `dock.*` keyspace
      - Dock module (`modules/dock/DockPage.qml`) untuk icon size + magnification via `DesktopSettings` (`dock.iconSize`, `dock.magnificationEnabled`)
      - Print module (`modules/print/PrintPage.qml`) fallback printer via `DesktopSettings` (`print.pdfFallbackPrinterId`)
      - Keyboard module (`modules/keyboard/KeyboardPage.qml`) shortcut write/read via `DesktopSettings` (`windowing.bind*`, `shortcuts.*`)
      - Developer Overview (`modules/developer/DeveloperOverviewPage.qml`) animation policy via `DesktopSettings` (`windowing.animationEnabled`)
      - `ThemeManager` + settings bootstrap (`main.cpp`) sekarang source tema/icon mode dari `DesktopSettings` (tanpa fallback legacy)
      - `FontManager` migrated ke `DesktopSettings` (`fonts.defaultFont/documentFont/monospaceFont/titlebarFont`)
      - `WallpaperManager` migrated ke `DesktopSettings` (`wallpaper.uri`)
      - `Qml/apps/settings/{Main,StylePreviewGallery}.qml` theme sync sudah `DesktopSettings`-only (context `UIPreferences` tidak lagi diexpose dari settings app)
      - `src/apps/settings/*` sekarang tidak lagi mereferensikan `UIPreferences` (ThemeManager/FontManager/WallpaperManager/DesktopSettingsClient/Main cutover ke `DesktopSettings` only)
      - shell runtime sekarang expose `DesktopSettings` dari `main.cpp` (desktop process), sehingga QML shell tidak perlu baca `UIPreferences` untuk keyspace SSOT
      - migrated shell QML reads ke `DesktopSettings`:
        - `Qml/components/shell/Shell.qml` (theme/accent/font scale + wallpaper)
        - `Qml/components/shell/GlobalShortcutManager.qml` (shortcut bind via `DesktopSettings.keyboardShortcut`)
        - `Qml/DesktopScene.qml` (dock hide prefs + notification bubble duration via `DesktopSettings.settingValue`)
        - `Qml/components/shell/ShellUtils.js` (persist tothespot geometry via `DesktopSettings.setSettingValue`)
        - `Qml/DockSystem.qml` + `Qml/components/dock/Dock.qml` (dock icon size, magnification, motion preset, pulse, drag thresholds)
        - `Qml/components/launchpad/Launchpad.qml` + `Qml/components/overlay/LaunchpadWindow.qml` (wallpaper + compositor blur prefs)
        - `Qml/components/applet/BatteryApplet.qml` + `Qml/components/applet/NotificationApplet.qml` (battery/notification preference key via `DesktopSettings.settingValue`)
        - `Qml/components/topbar/TopBarScreenshotControl.qml` + `Qml/components/screenshot/ScreenshotSaveController.js` (screenshot prefs/saveFolder via `DesktopSettings`)
        - `Qml/components/workspace/WorkspaceOverlay.qml` (drag-edge timing prefs via `DesktopSettings.settingValue`)
        - `Qml/components/topbar/TopBar.qml` (search-profile + font-scale preference bridge ke `DesktopSettings`)
        - `Qml/components/portalchooser/PortalChooserController.js` (persisted chooser width/height/sort/filter prefs via `DesktopSettings`)
        - `Qml/components/print/PrintDialog.qml` (PDF fallback printer preference via `DesktopSettings`)
        - `Qml/apps/filemanager/FileManagerWindow.qml` (tab-state writer adapter dialihkan ke `DesktopSettings.setSettingValue`)
    - fallback legacy `UIPreferences` removed; runtime now `DesktopSettings`-only
    - `ApplicationsPage.qml` added "Theme" section (SSOT-backed):
      - `appThemePolicy.qtGenericAllowKdeCompat`
      - `appThemePolicy.qtIncompatibleUseDesktopFallback`
      - `fallbackPolicy.unknownUsesSafeFallback`
      - basic per-app override editor (`perAppOverride.<appId|desktopId>`)
      - policy preview panel (`ClassifyApp` + `ResolveThemeForApp`) for live routing verification
      - policy preview can auto-fill descriptor from selected app in sidebar ("Use Selected App")
      - "Use Selected App" now auto-runs classify + resolve preview (one-click verification)
      - Apply/Clear per-app override now auto-refreshes policy preview for immediate verification
    - `DesktopSettingsClient` extended with:
      - policy properties + setters
      - DBus helpers `ClassifyApp` and `ResolveThemeForApp` for preview/integration

### Phase 8 — Persistence, safety, recovery
- [ ] Storage:
  - JSON versioned + migration path
  - atomic write (`tmp -> fsync -> rename`)
- [ ] Safety:
  - input validation
  - rollback ke `lastKnownGood`
  - fallback ke defaults jika corrupt
- [ ] Recovery:
  - startup self-check
  - invalid schema/value tidak memblok shell startup

### Deliverables implementasi (dokumen + kontrak)
- [ ] Dokumen arsitektur end-to-end: SSOT -> policy -> adapters -> app domains.
- [ ] Schema final lengkap + contoh payload.
- [ ] Theme policy matrix untuk kategori:
  - shell
  - gtk
  - kde
  - qt-generic
  - qt-desktop-fallback
  - unknown
- [ ] Event flow mode change terdokumentasi.
- [ ] Pseudocode baseline:
  - read/write setting
  - classify app
  - resolve theme/icon
  - emit signal
  - apply adapters
- [ ] Struktur modul/file implementasi disepakati.

### Usulan struktur modul/file
- [ ] `src/daemon/settingsd/`:
  - `settingsservice.*`
  - `settingstore.*`
  - `settingsschema.*`
  - `settingsmigrator.*`
- [ ] `src/services/theme-policy/`:
  - `appthemeclassifier.*`
  - `themepolicyengine.*`
  - `themeresolution.*`
- [ ] `src/services/theme-adapters/`:
  - `shellthemeadapter.*`
  - `gtkthemeadapter.*`
  - `kdethemeadapter.*`
  - `qtfallbackadapter.*`
- [ ] `src/apps/settings/modules/appearance/`:
  - UI bridge ke `desktop-settingsd`
  - per-app override editor
- [ ] `docs/contracts/`:
  - settings API contract
  - event contract
  - policy matrix contract

### Guardrails (tidak boleh dilanggar)
- [ ] Shell tidak boleh ikut tema KDE/GTK secara langsung.
- [ ] App KDE tidak boleh dipaksa pakai shell theme.
- [ ] App Qt incompatible wajib fallback ke desktop policy.
- [ ] Satu perubahan mode harus sinkron ke semua output domain.
- [ ] SSOT tetap pusat tunggal; tidak ada sumber kebenaran kedua.

Dock status:
- [x] Dock state saat ini ditandai sebagai **last good**.
- [x] Instruksi tugas/roadmap Dock lama dibersihkan dari TODO aktif.
- [ ] Lanjut validasi Dock hanya di environment uji final (QEMU/target compositor), bukan dari host test yang tidak stabil.

## Program: Context Awareness (Adaptive UI) — `desktop-contextd`

Tujuan:
- [ ] Bangun daemon `desktop-contextd` untuk context-aware adaptive UI terintegrasi SSOT.
- [ ] Kumpulkan context sistem realtime, normalisasi ke model standar, emit event granular.
- [ ] Integrasikan ke Theme Policy Engine dan UI Behavior System tanpa melanggar preferensi user.

Prinsip wajib:
- [ ] Context ≠ setting (context dinamis, setting preferensi user).
- [ ] SSOT tetap source of truth; context hanya sinyal kondisi + input policy.
- [ ] Konflik policy wajib: `user_setting > context_automation`.
- [ ] Adaptasi harus smooth, predictable, non-intrusive.

### Arsitektur (Production-ready)
- [ ] Layer 1 — Sensors:
  - [ ] time/sunrise-sunset
  - [ ] power (AC/battery/level/low power via UPower/system source)
  - [ ] device profile (laptop/desktop/tablet/touch/input capability)
  - [ ] display/compositor (monitor count, fullscreen, DPI/scale, refresh rate)
  - [ ] activity (active app, workspace, focus/presentation mode)
  - [ ] system resources (CPU/RAM; GPU optional)
  - [ ] network (online/offline, metered, quality class)
  - [ ] attention (idle/active, DND, fullscreen suppression signal)
- [ ] Layer 2 — Context Normalization:
  - [ ] Raw sensor data diubah ke `ContextModel` terstandarisasi (tanpa expose raw schema).
- [ ] Layer 3 — Context Engine:
  - [ ] Merge context + diff vs previous state + debounce/coalesce + state publish.
- [ ] Layer 4 — Policy Integration:
  - [ ] Context consumed by theme/animation/notification/layout/control-center adapters.

### Context Model Final (canonical contract)
- [ ] Definisikan schema final (versioned) contoh:
  - `time.period`: `day|night`
  - `power.mode`: `ac|battery`, `power.level`, `power.lowPower`
  - `device.type`: `laptop|desktop|tablet`, `device.touchCapable`
  - `display.multiMonitor`, `display.fullscreen`, `display.scaleClass`, `display.refreshClass`
  - `activity.type`: `normal|focus|presentation|gaming`, `activity.appId`, `activity.workspaceId`
  - `system.performance`: `high|normal|low`
  - `network.state`: `online|offline`, `network.metered`, `network.quality`
  - `attention.idle`, `attention.dnd`, `attention.fullscreenSuppression`

### Event System (granular, event-driven)
- [ ] DBus/event bus events:
  - `ContextChanged(context)`
  - `PowerStateChanged(payload)`
  - `TimePeriodChanged(period)`
  - `ActivityChanged(payload)`
  - `PerformanceChanged(payload)`
- [ ] Anti-spam:
  - [ ] debounce burst updates
  - [ ] only emit changed fields
  - [ ] publish throttling for high-frequency sensors

### Rule Engine Design (WAJIB)
- [ ] Rule format declarative:
  - condition + action + priority + cooldown + override guard
- [ ] Baseline rules:
  - [ ] Low power: disable blur, reduce animation, limit background refresh
  - [ ] Night period + auto theme: set appearance mode dark
  - [ ] Fullscreen: suppress non-critical notifications
  - [ ] High load/low performance: disable heavy effects
  - [ ] Presentation: disable notifications + prevent dimming
- [ ] Rule constraints:
  - [ ] no direct hardcode behavior outside rule engine
  - [ ] user manual mode blocks context override

### SSOT Integration Strategy
- [ ] SSOT stores:
  - [ ] user preferences (manual/auto mode)
  - [ ] theme/policy configs
  - [ ] context automation policy toggles
- [ ] `desktop-contextd` only publishes context + recommendations.
- [ ] Apply pipeline:
  - [ ] ContextChanged -> RuleEngine -> PolicyAdapter -> SSOT-aware update request.
- [ ] Conflict resolver:
  - [ ] enforce `manual mode` as hard guardrail.

### DBus API Contract (`org.desktop.Context`)
- [ ] Service: `org.desktop.Context`
- [ ] Object: `/org/desktop/Context`
- [ ] Interface: `org.desktop.Context`
- [ ] Methods:
  - [ ] `GetContext() -> a{sv}`
  - [ ] `Subscribe() -> a{sv}` (or ack payload)
- [ ] Signals:
  - [ ] `ContextChanged(a{sv})`
  - [ ] `PowerChanged(a{sv})`
  - [ ] `ActivityChanged(a{sv})`
  - [ ] `TimePeriodChanged(s)`
  - [ ] `PerformanceChanged(a{sv})`

### Pseudocode baseline
- [ ] Update context loop:
  - [ ] read sensors -> normalize -> diff -> emit changed events.
- [ ] Apply rule flow:
  - [ ] on context changed -> evaluate rules -> apply adapters with SSOT guard.

### Struktur project
- [ ] `src/daemon/contextd/`
  - [ ] `main.cpp`
  - [ ] `dbus/contextservice.*`
  - [ ] `engine/contextengine.*`
  - [ ] `model/contextmodel.*`
  - [ ] `rules/ruleengine.*`
  - [ ] `eventbus/contextbus.*`
  - [ ] `sensors/{time,power,device,display,activity,system,network,attention}sensor.*`
  - [ ] `integration/{theme,animation,notification,layout}adapter.*`

### Safety & Reliability
- [ ] fallback saat sensor gagal (degrade gracefully, no crash)
- [ ] never block UI thread
- [ ] timeout + retry policy per sensor source
- [ ] health metric + watchdog hook
- [ ] startup-safe default context when sensors unavailable

### Dampak UI (controlled adaptation)
- [ ] Theme: auto dark/light with smooth transition
- [ ] Animation: full/reduced/minimal by context
- [ ] Notification: focus/fullscreen-aware suppression
- [ ] Layout: compact vs touch-friendly mode profile
- [ ] Control Center: context-relevant cards only

### Prioritas Implementasi
- [ ] Phase 1:
  - [ ] time + power + fullscreen
  - [ ] basic rule engine + DBus + SSOT integration guard
- [ ] Phase 2:
  - [ ] activity + system resource monitoring
  - [ ] performance-aware UI degradation policies
- [ ] Phase 3:
  - [ ] network context + predictive refinements
  - [ ] expanded adaptive behavior matrix

### Larangan
- [ ] jangan override setting user tanpa izin/mode auto
- [ ] jangan lakukan perubahan UI drastis mendadak
- [ ] jangan coupling langsung behavior ke GTK/KDE implementation details
- [ ] jangan hardcode behavior di luar rule engine

Progress implementasi (awal):
- [~] Skeleton daemon + DBus contract Phase 1 sudah dibuat:
  - `src/daemon/contextd/contextd_main.cpp`
  - `src/services/context/contextservice.{h,cpp}`
  - DBus service: `org.desktop.Context`
  - methods: `GetContext`, `Subscribe`, `Ping`
  - signals: `ContextChanged`, `PowerStateChanged`, `TimePeriodChanged`, `ActivityChanged`, `PerformanceChanged`
  - compatibility alias signal ditambahkan: `PowerChanged` (canonical baru) tetap berdampingan dengan `PowerStateChanged` (legacy)
  - normalized context baseline termasuk `time/power/device/display/activity/system/network/attention`
  - `time` sensor kini dukung policy SSOT `contextTime`:
    - `mode=local|sun`
    - `sunriseHour` / `sunsetHour`
    - fallback aman ke local-hour jika konfigurasi tidak valid
  - periodic refresh + diff-based emit (anti-spam dasar)
- [~] Basic rule engine Phase 1 sudah ditambahkan:
  - `src/services/context/ruleengine.{h,cpp}`
  - evaluasi rule baseline:
    - low power -> disable blur + reduce animation + limit background refresh
    - night/day + SSOT guard (`globalAppearance.colorMode == auto`) -> appearance mode suggestion
    - fullscreen -> notification suppression
    - low performance -> disable heavy effects
  - hasil rule dipublish pada `context.rules` (non-destructive, tidak force override setting user)
- [~] SSOT integration guard baseline:
  - `ContextService` membaca snapshot settings dari `org.slm.Desktop.Settings.GetSettings`
  - automation dikunci oleh mode user (`auto` vs manual) sesuai prinsip `user_setting > context_automation`
- [~] CMake integration:
  - `cmake/Sources.cmake` -> `SLM_CONTEXTD_SOURCES`
  - `CMakeLists.txt` -> executable target `desktop-contextd` + link `Qt6::Core Qt6::DBus`
- [~] DBus contract test baseline:
  - `tests/contextservice_dbus_test.cpp` ditambahkan untuk kontrak `Ping/GetContext/Subscribe` + guard `auto/manual`.
  - Target `contextservice_dbus_test` sudah terintegrasi ke `CMakeLists.txt`.
  - Verifikasi: `ctest -R contextservice_dbus_test` pass.
  - Guard `contextAutomation.*` tervalidasi untuk state disabled dan enabled (kontrak `rules.guard`).
  - Signal granularity power tervalidasi via `QSignalSpy`:
    - power diff emit `PowerChanged` + `PowerStateChanged`
    - context semantik identik tidak emit ulang.
  - `tests/contextclient_test.cpp` ditambahkan untuk kompatibilitas consumer:
    - `ContextClient` menerima `PowerChanged` (baru) dan `PowerStateChanged` (legacy).
    - stale-guard tervalidasi (payload power identik tidak memicu refresh ganda).
  - Catatan: di environment headless tanpa session bus test dapat `SKIP` (expected).
- [~] Fullscreen context sensor baseline:
  - `display.fullscreen` kini membaca state aktif via DBus KWin (`org.kde.KWin` -> `activeWindow` -> `org.kde.KWin.Window.fullScreen`).
  - Fallback aman ke `false` jika backend tidak tersedia; exposed `display.fullscreenSource = kwin|fallback`.
- [~] Notification integration baseline:
  - `NotificationManager` subscribe `org.desktop.Context.ContextChanged`.
  - Rule `notifications.suppress=true` kini menahan banner non-kritis (priority `normal/low`) dan tetap mengizinkan `high`.
  - Regression test ditambah di `notificationmanager_priority_routing_test` (context suppression contract).
- [~] System resource context baseline:
  - `system.performance` tidak lagi hardcoded; kini dihitung dari sensor runtime:
    - CPU load class via `/proc/loadavg` (`cpuLoadPercent`, `cpuSource`)
    - Memory pressure via `/proc/meminfo` (`memoryUsedPercent`, `memorySource`)
  - Klasifikasi awal:
    - `low` jika CPU/memory tinggi
    - `high` jika CPU ringan dan memory longgar
    - selain itu `normal`
  - Contract test `contextservice_dbus_test` diperluas untuk memvalidasi shape `system` context.
- [~] Network + attention + activity sensor baseline:
  - `network` kini membaca `org.freedesktop.NetworkManager` (`State`, `Connectivity`, `Metered`) dengan output ter-normalisasi:
    - `state: online|offline`
    - `quality: normal|limited|offline`
    - `metered: bool`
    - `source: networkmanager|fallback`
  - `attention.idle` kini membaca `org.freedesktop.ScreenSaver` (`GetActive` + `GetSessionIdleTime`) dengan fallback aman.
  - `activity.appId` baseline kini membaca active window KWin (desktopFileName/resourceClass/resourceName) jika tersedia.
  - Contract test `contextservice_dbus_test` diperluas untuk validasi shape `network/attention/activity`.
- [~] Context event throttling baseline:
  - `ContextService` kini memakai semantic compare (`normalizedForSemanticCompare`) untuk menahan spam emit akibat fluktuasi minor:
    - bucketize `cpuLoadPercent` / `memoryUsedPercent`
    - coarse bucket `attention.idleTimeSec`
    - abaikan field noisy `network.connectivityRaw`
  - `GetContext` tetap menyajikan snapshot terbaru, tetapi signal emit hanya saat perubahan semantik.
- [~] Shell runtime adapter baseline (context -> motion):
  - Refactor ke event-driven:
    - tambahkan `src/services/context/contextclient.{h,cpp}` (subscribe `org.desktop.Context.ContextChanged` + owner watch).
    - compatibility update: `ContextClient` juga subscribe `PowerChanged` + `PowerStateChanged` (legacy) dengan stale-check refresh guard.
    - `main.cpp` kini konsumsi `ContextClient.reduceAnimation` langsung (tanpa polling timer periodik).
  - Rule `ui.reduceAnimation` terintegrasi ke `MotionController.setReducedMotion(...)`.
  - Guardrail dipertahankan: preferensi user tetap prioritas (`animationMode=full` tidak dipaksa reduce oleh context).
- [~] SSOT automation gates + settings integration baseline:
  - `settingsd` menambah root `contextAutomation`:
    - `autoReduceAnimation`
    - `autoDisableBlur`
    - `autoDisableHeavyEffects`
  - Validasi tipe bool ditambahkan untuk semua key gate di atas.
  - `RuleEngine` kini hormati gate tersebut saat menghitung:
    - `ui.reduceAnimation`
    - `ui.disableBlur`
    - `ui.disableHeavyEffects`
  - `DesktopSettingsClient` + `AppearancePage` sudah expose toggle runtime untuk ketiga gate.
  - `DesktopSettingsClient` kini expose `contextTime` binding:
    - `contextTimeMode`
    - `contextTimeSunriseHour`
    - `contextTimeSunsetHour`
  - `AppearancePage` sudah menampilkan kontrol `contextTime`:
    - source mode `Local Time` / `Sunrise-Sunset`
    - editor jam `sunriseHour` / `sunsetHour` (aktif saat mode `sun`)
  - `tests/desktopsettingsclient_test.cpp` ditambahkan:
    - verifikasi load initial `contextTime`
    - verifikasi setter `contextTime` lewat DBus
    - verifikasi live update `SettingChanged` ke binding client
  - `tests/appearance_contexttime_ui_contract_test.cpp` ditambahkan:
    - kontrak UI `AppearancePage` untuk kontrol source `Local/Sunrise-Sunset`
    - kontrak enable/disable editor sunrise/sunset berdasarkan `contextTimeMode`
- [~] Shell adaptive effects integration baseline:
  - `ContextClient` expose `disableBlur`, `disableHeavyEffects`, dan `context` snapshot map.
  - Runtime integration aktif pada:
    - Launchpad (blur/anim gate)
    - Notification stack/cards/bubble (heavy effects gate)
    - Compositor switcher overlay (effects/animation gate)
    - `SlmStyle.PopupSurface` (shadow gate)
- [~] Developer diagnostics integration baseline:
  - `ContextClient` di-expose ke app `slm-settings`.
  - `DeveloperOverviewPage` menampilkan ringkas:
    - service status
    - resolved action dari `context.rules.actions`
    - gate status dari SSOT (`contextAutomation.*`)
  - `ContextDebugPage.qml` tersedia untuk raw context JSON + sensor source summary.

## Roadmap — Modern Network & Firewall System (Production-Ready)

### Objective
- Bangun sistem Network & Firewall terpadu untuk desktop Linux dengan prinsip:
  - user-centric (app-oriented, bukan network-centric)
  - secure by default
  - application-aware (berbasis app identity)
  - terintegrasi dengan desktop core (`desktop-appd`)
  - simple untuk user umum, advanced untuk power user

### Core Architecture (Wajib)
- Packet Engine (low level):
  - backend rule final: `nftables`
- Identity Layer (SSOT):
  - source of truth: `desktop-appd`
  - mapping wajib: PID -> app identity, CLI context (cwd/parent/tty), Flatpak App ID, `.desktop` app name/icon
  - output identity minimal:
    - `app_name`
    - `app_id`
    - `pid`
    - `executable`
    - `source` (`apt`/`flatpak`/`manual`)
    - `trust_level`
    - `context` (`gui`/`cli`/`interpreter`)
- Policy Engine:
  - service: `desktop-firewalld`
  - fungsi:
    - evaluasi koneksi incoming/outgoing
    - decision `allow`/`deny`/`prompt`
    - inject rule ke `nftables`
    - persist keputusan user

### Firewall Design
- Default policy (wajib):
  - Incoming: deny all
  - Outgoing: allow (v1)
  - Loopback: allow
  - Established: allow
- Firewall mode (wajib):
  - `Home`:
    - allow LAN
    - allow trusted apps
    - block unknown incoming
  - `Public`:
    - block all incoming
    - stealth mode on
    - optional restrict unknown outbound
  - `Custom`: full user control

### Network Management
- Wajib:
  - Network profiles: `Home`, `Office`, `Public WiFi`, `Custom`
  - Auto-detect profile berbasis `SSID`, `gateway`, `IP range`
  - Connection awareness:
    - network type (`wifi`/`ethernet`)
    - trusted/untrusted
    - metered/unmetered
  - Real-time monitoring:
    - `App -> IP -> port`
    - bandwidth usage
    - active connections

### Application Firewall (Core Feature)
- Incoming prompt flow (wajib):
  - Popup: app meminta menerima koneksi
  - Action: `Allow` / `Deny`
  - Option: `Remember`, `Only local network`
- Outgoing control (phase 2+):
  - Prompt app connect ke remote endpoint
  - Action: `Allow` / `Block` / `Block IP`

### CLI & Process Handling
- Klasifikasi process:
  - `system` (apt/systemd/resolver)
  - `developer` (git/npm/cargo/pip)
  - `network_tools` (curl/wget/ssh)
  - `interpreter` (python/node)
  - `unknown`
- Deteksi wajib:
  - PID, UID, parent process, CWD, TTY, cgroup
- Default policy CLI:
  - system: allow
  - developer: allow + log
  - network_tools: first-time prompt
  - unknown: prompt/restrict
- Interpreter handling:
  - parse argumen target script
  - tampilkan konteks script (`python script.py`, `node app.js`)

### IP Block Feature (Wajib)
- Rule type:
  - block single IP
  - block subnet (CIDR)
  - block list (multiple IP)
- Tambahan:
  - temporary block (`1h`, `24h`)
  - permanent block
  - hit counter
  - reason/note

### Trust System
- Trust levels:
  - `system`
  - `trusted` (repo resmi/signed)
  - `unknown`
  - `suspicious`
- Behavior:
  - trusted: optional auto-allow
  - unknown: prompt
  - suspicious: default block

### Smart Features (Wajib)
- Auto suggestion:
  - jika app sering diblok -> sarankan allow
- Behavior learning:
  - simpan kebiasaan user dan adapt policy
- Quarantine mode:
  - block semua network untuk app mencurigakan

### Settings UI (SSOT)
- Lokasi: `Settings -> Security -> Firewall`
- Section wajib:
  - Firewall toggle
  - Mode (`Home`/`Public`/`Custom`)
  - Allowed Apps
  - Blocked Apps
  - Blocked IP/Network
  - Active Connections
  - Reset Configuration

### UX Rules (Wajib)
- Jangan tampilkan port number di UI utama
- Jangan expose `nftables`/`iptables` ke user umum
- Tampilkan nama aplikasi, icon, dan konteks
- Semua keputusan harus bisa di-undo
- Popup tidak boleh spam

### Integration
- `desktop-appd`: app identity source of truth
- `systemd`: process grouping via cgroup
- `NetworkManager`: network state/provider

### Implementation Priority
- Phase 1 (wajib):
  - incoming firewall
  - IP block
  - basic UI
  - identity mapping
- Phase 2:
  - outgoing control
  - CLI detection
  - popup system
- Phase 3:
  - smart learning
  - trust system
  - quarantine

### Anti-Pattern (Jangan)
- Expose iptables langsung ke user
- Hanya port-based
- Tanpa app identity
- Ignore CLI process
- Popup tanpa konteks

### Success Criteria
- User tidak perlu memahami port/network detail
- Firewall tetap powerful
- Semua koneksi bisa ditelusuri ke app
- Sistem robust (unbreakable principle)
- UX setara/lebih baik dari macOS

### Execution Checklist (Milestone-Based)

#### Milestone 0 — Foundation & Contract Freeze
- [ ] Finalisasi schema SSOT `desktop-firewalld`:
  - [ ] `firewall.mode`
  - [ ] `firewall.enabled`
  - [ ] `firewall.defaultIncomingPolicy`
  - [ ] `firewall.defaultOutgoingPolicy`
  - [ ] `firewall.networkProfiles.*`
  - [ ] `firewall.rules.apps.*`
  - [ ] `firewall.rules.ipBlocks.*`
- [ ] Tetapkan DBus contract v1:
  - [ ] `org.slm.Desktop.Firewall` (`Ping`, `GetCapabilities`, `GetStatus`, `SetMode`, `SetEnabled`)
  - [ ] `EvaluateConnection`, `SetAppPolicy`, `SetIpPolicy`, `ListConnections`
  - [ ] signal `FirewallStateChanged`, `PolicyChanged`, `ConnectionObserved`
- [ ] Freeze identity payload contract dari `desktop-appd`.
- [ ] Definisikan mapping trust level + source normalization.
- [ ] Tambah contract test baseline untuk API shape + backward compatibility.

#### Milestone 0 — Technical Skeleton (Task Breakdown)
- [ ] Scaffold daemon target `desktop-firewalld`:
  - [ ] `src/daemon/firewalld/firewalld_main.cpp`
  - [ ] `src/daemon/firewalld/firewallservice.h`
  - [ ] `src/daemon/firewalld/firewallservice.cpp`
  - [ ] `src/daemon/firewalld/firewalltypes.h` (enum/DTO policy decision + mode)
- [ ] Define DBus adaptor/interface surface:
  - [ ] `src/daemon/firewalld/firewalldbusadaptor.h`
  - [ ] `src/daemon/firewalld/firewalldbusadaptor.cpp`
  - [ ] `src/daemon/firewalld/firewalldbus.xml` (authoritative contract source)
- [ ] Add policy engine skeleton:
  - [ ] `src/services/firewall/policyengine.h`
  - [ ] `src/services/firewall/policyengine.cpp`
  - [ ] `src/services/firewall/policystore.h`
  - [ ] `src/services/firewall/policystore.cpp` (versioned JSON + migration stub)
- [ ] Add packet engine adapter (nftables abstraction):
  - [ ] `src/services/firewall/nftablesadapter.h`
  - [ ] `src/services/firewall/nftablesadapter.cpp`
  - [ ] support methods: `ensureBaseRules()`, `applyAtomicBatch()`, `reconcileState()`
- [ ] Add identity bridge contract to `desktop-appd`:
  - [ ] `src/services/firewall/appidentityclient.h`
  - [ ] `src/services/firewall/appidentityclient.cpp`
  - [ ] payload validator for:
    - [ ] `app_name`, `app_id`, `pid`, `executable`
    - [ ] `source`, `trust_level`, `context`
- [ ] Add SSOT settings schema wiring:
  - [ ] extend settings schema docs/keyspace in `docs/TODO.md` + `docs/ARCHITECTURE.md`
  - [ ] settingsd baseline key registration (`firewall.*`) in settings store defaults
- [ ] Add test skeleton:
  - [ ] `tests/firewallservice_dbus_contract_test.cpp`
  - [ ] `tests/firewall_policyengine_contract_test.cpp`
  - [ ] `tests/firewall_nftables_adapter_test.cpp`
  - [ ] `tests/firewall_identity_mapping_contract_test.cpp`
- [ ] Add build wiring:
  - [ ] register new sources in `cmake/Sources.cmake`
  - [ ] add `desktop-firewalld` target + install rule in `CMakeLists.txt`
  - [ ] add CI lane stub `scripts/test-firewall-contract-suite.sh`

#### Milestone 1 — Phase 1 (Wajib MVP)
- [ ] Implement `desktop-firewalld` service skeleton + lifecycle.
- [ ] Implement default policy enforcement:
  - [ ] incoming deny all
  - [ ] outgoing allow
  - [ ] loopback allow
  - [ ] established allow
- [ ] Integrasi packet engine `nftables`:
  - [ ] table/chain bootstrap idempotent
  - [ ] atomic rule update
  - [ ] rollback on apply failure
- [ ] Integrasi identity layer (`desktop-appd`) untuk inbound app attribution.
- [ ] Implement IP block management:
  - [ ] single IP
  - [ ] subnet CIDR
  - [ ] list block
  - [ ] temporary/permanent
  - [ ] reason/note + hit counter
- [ ] UI basic (Settings -> Security -> Firewall):
  - [ ] toggle firewall
  - [ ] mode Home/Public/Custom
  - [ ] blocked IP/network list
  - [ ] reset configuration
- [ ] Acceptance gate Milestone 1:
  - [ ] incoming unsolicited blocked by default
  - [ ] mode switch reflected ke rule aktif
  - [ ] ip block persist lintas restart

#### Milestone 2 — Phase 2 (Interactive Control)
- [ ] Implement incoming prompt system:
  - [ ] popup allow/deny
  - [ ] remember decision
  - [ ] only local network option
  - [ ] anti-spam coalescing/throttling
- [ ] Implement outgoing control (opt-in mode):
  - [ ] allow/block/block-IP
  - [ ] endpoint + app context summary
- [ ] Implement CLI/process classifier:
  - [ ] system/developer/network_tools/interpreter/unknown
  - [ ] collect PID/UID/PPID/CWD/TTY/cgroup
  - [ ] interpreter target script extraction
- [ ] Implement network profile auto-detection:
  - [ ] SSID
  - [ ] gateway
  - [ ] IP range
  - [ ] trusted/untrusted + metered state
- [ ] Real-time active connection monitor:
  - [ ] app -> ip -> port internal model
  - [ ] bandwidth counters
  - [ ] UI list active connections
- [ ] Acceptance gate Milestone 2:
  - [ ] popup decision tersimpan dan replay tanpa spam
  - [ ] outgoing restriction aktif sesuai mode
  - [ ] CLI context muncul jelas di prompt/log

#### Milestone 3 — Phase 3 (Smart & Trust)
- [ ] Implement trust system engine:
  - [ ] system/trusted/unknown/suspicious
  - [ ] source/signature/reputation mapping
- [ ] Implement smart suggestion:
  - [ ] frequent-block detection
  - [ ] allow suggestion with undo
- [ ] Implement behavior learning:
  - [ ] local decision history
  - [ ] adaptive policy hints
- [ ] Implement quarantine mode:
  - [ ] full network block per app
  - [ ] one-click recover
- [ ] Acceptance gate Milestone 3:
  - [ ] suspicious default-block works
  - [ ] quarantine deterministic + reversible
  - [ ] learning tidak override explicit user decision

#### Milestone 4 — Hardening & Production Readiness
- [ ] Security hardening:
  - [ ] strict input validation
  - [ ] policy file versioning + migration
  - [ ] atomic write + crash-safe recovery
  - [ ] least-privilege boundaries antar service
- [ ] Reliability:
  - [ ] rule reconciliation on boot/resume
  - [ ] self-heal when nftables state drift detected
  - [ ] watchdog + healthcheck endpoint
- [ ] Observability:
  - [ ] structured audit log (privacy-safe)
  - [ ] counters: allow/deny/prompt hit rates
  - [ ] debug snapshot export
- [ ] Performance:
  - [ ] no UI jank saat burst connection events
  - [ ] bounded memory untuk connection history
- [ ] UX quality:
  - [ ] undo path untuk setiap decision
  - [ ] no raw nftables/iptables term di UI utama
  - [ ] no noisy technical jargon for non-advanced mode

#### Milestone 5 — Release & Rollout
- [ ] End-to-end regression suite:
  - [ ] firewall mode contract tests
  - [ ] identity mapping tests
  - [ ] prompt-flow UI contract tests
  - [ ] policy persistence + migration tests
- [ ] Canary rollout:
  - [ ] feature flags per module
  - [ ] staged enablement
  - [ ] rollback switch
- [ ] Post-release validation:
  - [ ] real-world false positive review
  - [ ] tuning default prompt policies
  - [ ] update docs/user guide
