# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Release Execution Queue (v1.0)
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
- [ ] Add optional preview pane for non-compact popup mode.

## Backlog (Theme)
- [x] Elevation/shadow tokens:
  - `elevationLow/Medium/High` semantic aliases added to `third_party/slm-style/qml/SlmStyle/Theme.qml`.
- [x] Motion tokens:
  - all duration tokens (`durationFast/Normal/Slow/Workspace/Micro/Sm/Md/Lg/Xl`) in `Theme.qml`, scaled by `_modeScale`.
  - physics tokens (`physicsSpring/Damping/Mass` × 3 presets + `physicsEpsilon`) in `Theme.qml`.
  - easing aliases (`easingDefault`, `easingLight`) added.
- [x] Semantic control states:
  - `controlBgHover`, `controlBgPressed`, `controlDisabledBg`, `controlDisabledBorder`, `textDisabled`, `focusRing` present in `Theme.qml` light/dark palettes.
- [ ] FileManager density pass:
  - align toolbar/search/tab/status heights to compact token grid.
- [ ] Add high-contrast mode toggle in `UIPreferences` + runtime Theme switch.
- [x] Add reduced-motion mode toggle:
  - `UIPreferences.animationMode` (`full`/`reduced`/`minimal`) + `Theme._modeScale` + `MotionController.reducedMotion` wired in `main.cpp`.
- [ ] Add optional dynamic wallpaper sampling tint with contrast guardrail.
- [~] SlmStyle migration guardrail:
  - [x] Runtime default Qt Quick Controls style switched to `SlmStyle` (`main.cpp`, `src/apps/settings/main.cpp`, `src/login/greeter/main.cpp`).
  - [x] Add audit script `scripts/check-slm-style-usage.sh` + build target `lint_slm_style_usage`.
  - [ ] Migrate remaining flagged files (current baseline: 49 files from style-usage checker).

## Backlog (FileManager)
- [ ] Continue shrinking `FileManagerWindow.qml` by moving remaining dialog/menu glue into `Qml/apps/filemanager/`.

## Backlog (Global Menu)
- [ ] Runtime integration check global menu end-to-end on active GTK/Qt/KDE apps (verify active binding/top-level changes by focus switching).
  - helper available: `scripts/smoke-globalmenu-focus.sh` (`--strict --min-unique 2`).

## Backlog (Clipboard)

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
  - [x] Foundation implemented in `src/core/motion/{slmanimationscheduler,slmmotioncontroller}`:
    - lifecycle ownership + active priority tracking (`Low/Medium/High`)
    - micro-interaction suppression when lifecycle priority is active
    - scheduler coalescing helper (`shouldCoalesce*`)
  - [x] Added core regression test for scheduler priority/suppression contract in `tests/slmmotioncore_test.cpp`
    (covers `allow/canRunPriority`, suppression state, and coalescing behavior).
  - [x] Workspace lifecycle integration baseline in `Qml/DesktopScene.qml`:
    - `workspaceVisible/swipeActive/swipeSettling` drive lifecycle begin/end through `MotionController`
  - [~] Extend lifecycle hooks to window lifecycle transitions (open/minimize/close/focus) from compositor events.
    - [x] Baseline `DesktopScene` hook from `CompositorStateModel.lastEvent` for open/close/minimize transitions (HIGH priority lifecycle + timed release).
    - [x] Add explicit focus active/inactive lifecycle mapping from normalized compositor events (LOW priority lifecycle + coalescing).
    - [x] Ensure `CompositorStateModel.lastEvent.event` always populated via fallback normalization in `KWinWaylandStateModel`.
    - [x] Add canonical event contract in backend adapter (`WindowingBackendManager`) so event names stay stable lint/tested.
      - [x] Baseline canonical lifecycle normalization added (focus/open/close/minimize aliases -> canonical event names).
      - [x] Added contract test coverage (`windowingbackendmanager_test`) for workspace command mapping + lifecycle canonicalization.
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
  - [x] Physics token global (`spring/damping/mass/epsilon`) sebagai single source + migrasi komponen yang masih literal.
      - implemented in `third_party/slm-style/qml/SlmStyle/Theme.qml`: `physicsSpringGentle/Default/Snappy`, `physicsDamping*/physicsMass*`, `physicsEpsilon`.
      - migrated: `DockReorderMarker.qml`, `DockItem.qml`, `NotificationApplet.qml`, `NotificationCenter.qml`.
- [~] Definisikan profile animasi window (safe/non-patent):
  - open: `scale 0.96->1.0`, `opacity 0->1` — implemented in `WorkspaceOverviewScene` thumbnail entrance.
  - minimize: dock icon focus-bounce on minimize (spatial cue without genie effect).
  - close: scale down + fade out — future compositor extension (shell cannot animate window surfaces directly).
  - [x] Baseline state-driven profile mapping diterapkan di `Qml/DesktopScene.qml`:
    - event `window-opened/window-shown/window-unminimized` -> profile `window.open` (`preset=smooth`, release `Theme.durationNormal`)
    - event `window-minimized` -> profile `window.minimize` (`preset=snappy`, release `Theme.durationFast`)
    - event `window-closing/window-closed` -> profile `window.close` (`preset=snappy`, release `Theme.durationFast`)
    - profile push/pop aman (restore channel+preset `MotionController` setelah lifecycle selesai/destruction)
  - [x] Shell-side visual feedback:
    - `WorkspaceOverviewScene` thumbnail entrance: `scale 0.96→1.0`, `opacity 0→1` (`durationNormal`, `easingDecelerate`) on new window while overview is open.
    - `Dock.notifyWindowLifecycle()` → `DockAppDelegate.matchesWindowAppId()` → launch-bounce on window-opened, focus-bounce on window-minimized.
    - `DesktopScene.processCompositorLifecycleEvent()` routes `appId` to dock after setting MotionController profile.
- [x] Definisikan profile animasi workspace:
  - switch: slide horizontal antar workspace (+ optional parallax ringan).
    - [x] `workspace.switch` lifecycle wired in `DesktopScene.qml` (`MediumPriority`, timed release `durationWorkspace`).
    - [x] `WorkspaceOverlay` switch Behavior easing fixed to `easingDecelerate` (OutCubic).
  - overview: scale-down window + grid sederhana.
    - [x] `workspace.overview` lifecycle already active via `syncWorkspaceLifecycleState`.
    - [x] `WindowThumbnail` grid layout animation upgraded to `durationNormal` + `easingDecelerate`.
- [~] Definisikan active/inactive visual response:
  - trigger dari focus compositor.
  - perubahan ringan shadow/opacity, tanpa animasi berat.
  - [x] Baseline diterapkan pada `Qml/components/workspace/WindowThumbnail.qml`:
    - focus compositor (`windowData.focused`) memicu `focusBlend` state.
    - perubahan ringan border/opacity/glow dengan `Theme.durationFast` + `Theme.easingLight`.
    - tanpa animasi berat; hanya emphasis visual halus untuk active vs inactive.
- [~] Implement kondisi disable/degrade animasi:
  - CPU tinggi, GPU fallback, battery saver, dan accessibility `reduce motion`. (triggers — future work)
  - [x] safe mode/recovery mode: paksa mode animasi `off` (tanpa transition dekoratif).
    - implemented runtime gate: `SLM_SESSION_MODE in {safe,recovery}` -> `SafeModeActive=true`,
      `Theme.duration* ~= 0`, `Theme.transitionDuration ~= 0`, `MotionController.reducedMotion=true`.
  - [x] sediakan mode `full`, `reduced`, `minimal`.
    - `UIPreferences.animationMode` property + `setAnimationMode` + stored in `windowing/animationMode`.
    - `Theme.animationMode` reads `UIPreferences.animationMode` reactively.
    - `Theme._modeScale`: `full=1.0`, `reduced=0.60`, `minimal=0.25` — all duration tokens scaled.
    - `MotionController.reducedMotion` set true for reduced/minimal at startup + runtime in `main.cpp`.
- [x] Integrasi frame sync:
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
    - [x] Add UI guard regression test: `tests/motion_scheduler_ui_guard_test.cpp` (critical micro-interaction files must keep `allowMotionPriority`).
    - [x] Rollout guard ke komponen hover-heavy lain:
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
  - [x] baseline debt reduction complete: notification/applet/workspace/dock/system/overlay/print/style migrated; `ui-style-allowlist` now empty.

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
- [ ] Audit `Qml/DesktopScene.qml`: identifikasi semua "page switch" pattern, ganti ke state binding
- [ ] Audit semua z-order assignment di overlay windows — buat konstanta di Theme atau ShellConst
- [ ] Definisikan `ShellState` sebagai QML singleton / C++ `QObject` dengan `Q_PROPERTY` per field
- [ ] Pastikan `DockLayer` dan `TopBarLayer` tidak pernah di-`visible: false` atau `destroy()`
- [ ] Verifikasi `WorkspaceLayer` tidak di-reset pada setiap mode change

#### Phase 2 — ShellStateController
- [ ] Implementasi `ShellStateController` (C++ `QObject`, exposed ke QML) dengan:
  - `requestMode(ShellMode mode, QString source)`
  - `cancelMode(ShellMode mode)`
  - `toggleOverlay(OverlayType overlay)`
  - `Q_PROPERTY ShellState currentState`
- [ ] Semua overlay (Launchpad, Overview, ToTheSpot) bind opacity/visibility ke `ShellState`
- [ ] Semua persistent layer bind opacity/blur/input ke `ShellState`
- [ ] Unit test: `shell_state_controller_test` — mode request, cancel, concurrent overlay

#### Phase 3 — InputRouter
- [ ] Implementasi `InputRouter` dengan layer priority table per `ShellMode`
- [ ] Keyboard shortcut dispatch lewat `InputRouter` — tidak langsung ke layer
- [ ] Gesture recognizer output → `InputRouter` → `ShellStateController`
- [ ] Timeout guard: layer non-response > 500ms → `InputRouter` fallback ke base layer
- [ ] Test: deadlock prevention, rapid switching, gesture cancel mid-transition

#### Phase 4 — Recovery & Crash Isolation
- [ ] Overlay context isolation: `LaunchpadLayer`, `WorkspaceOverviewLayer`, `ToTheSpotLayer`
  dipindah ke sub-window atau deferred Loader dengan error boundary
- [ ] `ShellStateController` mendeteksi overlay timeout → auto-dismiss + state reset
- [ ] `WorkspaceLayer` state recovery dari `CompositorStateModel` setelah overlay crash
- [ ] Persistent layer health check timer (1s interval): jika Dock/TopBar tidak visible → re-show
- [ ] Test: simulasi `Loader` error, null ref, timing crash → base shell always accessible

#### Phase 5 — Hardening & Contract Tests
- [ ] Kontrak test: `persistent_layer_stability_test` — toggle overlay 1000x, assert Dock+TopBar intact
- [ ] Kontrak test: `overlay_isolation_test` — overlay crash tidak propagate ke persistent layer
- [ ] Kontrak test: `state_machine_transition_test` — semua mode transition valid, tidak ada invalid state
- [ ] Kontrak test: `z_order_policy_test` — SystemModal selalu di atas, Dock z-order tidak berubah
- [ ] Performance test: mode switch latency < 16ms (single vsync frame untuk state update)

## Program (Notification System Refresh: macOS-like Notification Center)
- [x] Define architecture boundary (3 layers, no tight coupling):
  - `Notification Service` (backend daemon, source of truth)
  - `Notification UI` (QML only, model-driven rendering)
  - `Integration Layer` (D-Bus + portal + shell hooks)
- [~] Finalize notification types:
  - Banner (transient, top-right, max 3 visible, auto-dismiss 5–8s, hover pause, swipe dismiss)
  - Notification Center (persistent, slide-in right panel, grouped history, virtualized list)
- [ ] Lock visual spec tokens:
  - card radius `14`
  - padding `12–16`
  - vertical rhythm `8`
  - light bg `rgba(255,255,255,0.7)`
  - dark bg `rgba(30,30,30,0.7)`
  - blur + subtle shadow + elevation layering
- [ ] Lock animation spec:
  - banner entry: slide-right-to-left + fade
  - banner exit: fade + slight shrink
  - center panel: slide-in right `300ms` + dim overlay
  - motion uses spring curves (no linear feel)
- [x] Implement interaction model:
  - mouse: hover pause, click default action, drag/swipe dismiss
  - keyboard: `Super+N` toggle center, arrows navigate, `Enter` open, `Delete` dismiss
  - optional edge gesture to open center
- [ ] Implement inline action system:
  - compact action buttons
  - max 2–3 actions per card
  - support reply/open/accept/decline patterns
- [~] Add advanced policy features:
  - Do Not Disturb (suppress banner, keep center)
  - priority routing (`low=center`, `normal=banner`, `high=sticky banner`)
  - smart grouping by app/thread
  - rich payload support (image preview, progress, media controls)
- [x] Implement backend D-Bus service skeleton:
  - Service: `org.example.Desktop.Notification`
  - Methods: `Notify`, `Dismiss`, `GetAll`, `ClearAll`, `ToggleCenter`
  - Signals: `NotificationAdded`, `NotificationRemoved`
  - Add internal mapping plan to SLM naming convention when stabilized
- [x] Add freedesktop compatibility adapter:
  - consume `org.freedesktop.Notifications`
  - translate to internal notification model + priority/group semantics
- [x] Finalize data model contract:
  - `id`, `app_id`, `title`, `body`, `icon`, `timestamp`, `actions[]`, `priority`, `group_id`, `read/unread`
- [x] Implement QML component structure:
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
- [ ] Security & abuse control:
  - per-app rate limit
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
- [x] QML component structure document + ownership map
- [x] D-Bus interface definition (`org.example.Desktop.Notification`)
- [x] backend service skeleton (daemon + model + dispatcher)
- [x] UI mock structure (banner + center grouping states)
- [x] animation spec sheet (durations, springs, transitions)
- [x] end-to-end data flow diagram (source -> service -> ui -> action callback)

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

### Roadmap implementasi

#### Fase 1 — MVP


#### Fase 2 — Stabilisasi


#### Fase 3 — Recovery hardening


#### Fase 4 — UX polish


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

### Phase 1 - Core Protection (Wajib)


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

### Phase 5 - Software Center Integration


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

Arsitektur:

Phase A - Foundation

Phase B - Finder-like UX defaults
- [ ] Heuristik post-extract:
- [ ] Context menu minimal:

Phase C - Preview & Performance

Phase D - Security hardening
- [ ] Overwrite strategy aman + conflict resolution sederhana

Phase E - Integration polish
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
