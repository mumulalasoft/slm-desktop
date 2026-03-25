# SLM Desktop (Qt/QML)

A modular Qt Quick project targeting a macOS-like desktop environment on Linux.

## Quick structure guide

- Diagram index: `docs/architecture/DIAGRAM_INDEX.md`
- FileManager split compatibility matrix: `docs/architecture/FILEMANAGER_COMPATIBILITY_MATRIX.md`
- High-level structure: `docs/PROJECT_STRUCTURE.md`
- Architecture notes: `docs/ARCHITECTURE.md`
- AppExecutionGate flow diagram: `docs/ARCHITECTURE.md` (section: `Execution flow (one gate)`)
- AppExecutionGate detailed diagrams: `docs/architecture/APP_EXECUTION_GATE.md`
- SLM Action Framework module graph: `docs/architecture/SLM_ACTION_FRAMEWORK_MODULES.md`
- SLM Capability Service diagrams: `docs/architecture/SLM_CAPABILITY_SERVICE.md`
- Tothespot end-to-end flow: `docs/architecture/TOTHESPOT_FLOW.md`
- Desktop daemon contract: `docs/DESKTOP_DAEMON_ARCHITECTURE.md`
- DBus API changelog: `docs/DBUS_API_CHANGELOG.md`
- SDDM theme adaptation/install guide: `docs/SDDM_THEME.md`
- Modular target structure: `docs/MODULAR_STRUCTURE.md`
- Run mode matrix: `docs/RUN_MODES.md`
- Regression checklist: `docs/REGRESSION_CHECKLIST.md`
- Testing guide: `docs/testing.md`

## Current architecture

- `main.cpp`: app bootstrap and QML engine entry.
- `Main.qml`: top-level frameless window.
- `Qml/Theme.qml`: centralized singleton theme (macOS-like glass palette) with light/dark + system-follow mode.
- `Qml/DesktopScene.qml`: orchestration layer for desktop background, top bar, dock, launchpad, and workspace windows.
- `Qml/components/DesktopBackground.qml`: wallpaper and tint layer.
- `Qml/components/TopBar.qml`: macOS-like status/menu bar shell.
- `Qml/components/Dock.qml`: bottom dock container.
- `Qml/components/DockItem.qml`: reusable dock icon delegate.
- `Qml/components/Launchpad.qml`: app grid overlay.
- `Qml/components/AppWindow.qml`: draggable desktop window surface.
- `appexecutiongate.*`: one-gate app execution API for Dock/Shell/Launchpad/FileManager/Terminal.
- `appcommandrouter.*`: command action router on top of `AppExecutionGate` for external module integration.
  - includes in-memory recent route history (`recentEvents()`) for runtime debugging.
  - live properties for QML binding: `lastEvent`, `eventCount`, `failureCount`.
  - failure detail property: `lastError` (e.g. `unknown-action`, `missing-target`).
  - diagnostics helpers: `recentFailures(maxItems)` and `actionStats()`.
  - action introspection: `supportedActions()` and `isSupportedAction(action)`.
  - structured dispatch result: `routeWithResult(action, payload, source)` -> `{ ok, error, action, source }`.
  - compact debug payload: `diagnosticSnapshot()`.
  - JSON diagnostics export: `diagnosticsJson(pretty)`.
  - realtime result signal: `routedDetailed(resultMap)`.
- `kwinwaylandipcclient.*`: DBus bridge for KWin Wayland command/event integration.
- `kwinwaylandstatemodel.*`: window/space state model sourced from KWin Wayland.

### Compositor Bridge Context Properties

- `WindowingBackend`: backend facade (`backend`, `connected`, `capabilities`, `protocolVersion`, `eventSchemaVersion`, `eventReceived(...)`, `sendCommand(...)`, `hasCapability(...)`).
- `CompositorStateModel`: normalized model for QML (`viewId`, `title`, `appId`, `mapped`, `minimized`, `focused`) and switcher state (`switcherActive`, `switcherIndex`, `switcherCount`).

## Assets

- `images/wallpaper.jpeg`
- `icons/*.svg`

All listed assets are bundled through Qt resources in `CMakeLists.txt`.

## Portal OpenURI Configuration

`org.slm.Desktop.Portal` untuk method `OpenURI` mendukung whitelist scheme yang bisa diatur saat runtime.

Prioritas sumber whitelist (tinggi -> rendah):

1. `options.allowedSchemes` (payload request)
2. env `SLM_PORTAL_OPENURI_ALLOWED_SCHEMES`
3. config file `~/.config/slm/portald.conf` (atau `$XDG_CONFIG_HOME/slm/portald.conf`)
4. default built-in (`file,http,https,mailto,ftp,smb,sftp`)

Contoh config:

```ini
openuri_allowed_schemes=file,https,mailto,smb
```

Lihat detail kontrak portal di `docs/PORTAL.md`.

## Theme usage

- Open runtime theme menu: top bar `Theme` button or Apple-style menu -> `Theme`.
- Modes: `Follow System`, `Light`, `Dark`.
- Programmatic dark/light toggle: `Theme.toggleMode()`.
- Programmatic explicit mode: `Theme.useLightMode()`, `Theme.useDarkMode()`.
- Follow system palette: `Theme.useSystemMode()`.
- Access tokens anywhere in QML: `Theme.color("tokenName")`.
- Theme switch transition: global desktop crossfade + color animations keyed by `Theme.transitionDuration`.

## Global menu style override

- Menu visuals are overridden globally via Qt Quick Controls custom style in `Style/`.
- Active style files:
  - `Style/Menu.qml`
  - `Style/MenuItem.qml`
  - `Style/MenuSeparator.qml`
  - `Style/qmldir`
- Runtime setup is in `main.cpp` (`QQuickStyle::setStyle(...)`) and applies to all `Menu`, `MenuItem`, and `MenuSeparator` instances in the app.
- Edit `Style/Menu.qml` (and related files) to change Tahoe menu appearance once for the whole app.

## Tothespot Scope Naming (Canonical)

- Canonical search scope name is `tothespot`.
- Use `scope=tothespot` for SearchAction context and global-search queries.
- Legacy non-canonical scope names are not accepted by parser/runtime.

## Build

Required development packages (Ubuntu/Debian):
- `qt6-base-dev`
- `qt6-declarative-dev`
- `libglib2.0-dev` (provides `gio-unix-2.0` for `.desktop` access)

```bash
cmake -S . -B build
cmake --build build -j
```

## Testing

Gunakan script helper agar test stabil di environment headless:

```bash
scripts/test.sh
```

Dengan build dir custom:

```bash
scripts/test.sh /path/to/build-dir
```

Catatan:
- Script akan set `QT_QPA_PLATFORM=offscreen` (jika belum diset), menjalankan dulu suite kontrak file-operations (`fileoperationsmanager_test`, `fileoperationsservice_dbus_test`, `fileopctl_smoke_test`), lalu menjalankan seluruh `ctest --output-on-failure`.
- Script akan menjalankan UI style lint (`scripts/lint-ui-style.sh`) sebelum test suite.
- Regex suite file-operations bisa dioverride lewat env `SLM_TEST_FILEOPS_REGEX`.
- Untuk skip lint di test lokal: `SLM_TEST_SKIP_UI_LINT=1`.
- Soak profile nightly (opsional, off by default):
  - `SLM_TEST_ENABLE_SOAK=1` aktifkan mode soak setelah full suite.
  - `SLM_TEST_SOAK_MINUTES=5` durasi soak (menit).
  - `SLM_TEST_SOAK_REGEX='^(filemanagerapi_daemon_recovery_test)$'` target test soak.

Contoh soak 5 menit:

```bash
SLM_TEST_ENABLE_SOAK=1 \
SLM_TEST_SOAK_MINUTES=5 \
scripts/test.sh
```

Smoke check backend (startup crash guard):

```bash
scripts/smoke-backends.sh
scripts/smoke-runtime.sh
scripts/test-globalmenu.sh
scripts/test-globalmenu.sh --strict
scripts/test-globalmenu.sh --build-dir /path/to/build
scripts/smoke-globalmenu-focus.sh --strict --samples 20 --interval 1 --min-unique 2
```

UI style lint standalone:

```bash
scripts/lint-ui-style.sh
cmake --build build --target lint_ui_style
```

## CLI UIPreference

Binary CLI: `uipreference`

```bash
uipreference ls
uipreference --help
uipreference man
uipreference <app-name> list
uipreference <app-name> get <key> [fallback]
uipreference <app-name> set <key> <value>
uipreference <app-name> reset <key>
```

Examples:

```bash
uipreference ls
uipreference SLM get dock.motionPreset
uipreference SLM set dock.autoHideEnabled true
uipreference SLM reset debug.verboseLogging
```

## CLI Windowing Audit (kwin-only)

Binary CLI: `windowingctl`

```bash
windowingctl --help
windowingctl matrix --pretty
windowingctl matrix --backend kwin-wayland --wait-ms 500 --pretty
windowingctl watch --backend kwin-wayland --duration-ms 5000
```

## Workspace Daemon CLI

Binary CLI: `workspacectl`

```bash
workspacectl --help
workspacectl healthcheck --pretty
workspacectl overview on
workspacectl overview off
workspacectl overview toggle
workspacectl appgrid
workspacectl desktop
workspacectl present org.kde.dolphin
workspacectl switch 2
workspacectl switch-delta -1
workspacectl move kwin:org.kde.konsole:Konsole:1 3
workspacectl move-focused-delta 1
```

`workspacectl` will prefer `org.slm.WorkspaceManager` and automatically fallback to legacy
`org.desktop_shell.WorkspaceManager` if needed.

## File Operations Daemon CLI

Binary CLI: `fileopctl`

```bash
fileopctl --help
fileopctl copy /tmp /path/to/file --wait
fileopctl move /tmp /path/to/file --wait
fileopctl trash /path/to/file --wait
fileopctl delete /path/to/file --wait
fileopctl empty-trash --wait
fileopctl pause <job-id>
fileopctl resume <job-id>
fileopctl cancel <job-id>
fileopctl get-job <job-id>
fileopctl list-jobs
```

## Devices Daemon CLI

Binary CLI: `devicectl`

```bash
devicectl --help
devicectl mount /dev/sdb1 --pretty
devicectl eject /dev/sdb1 --pretty
devicectl unlock /dev/sdb2 "your-passphrase" --pretty
devicectl format /dev/sdb1 ext4 --pretty
```

## Global Menu Diagnostics CLI

Binary CLI: `globalmenuctl`

```bash
globalmenuctl --help
globalmenuctl dump --pretty
globalmenuctl healthcheck --pretty
```

Global menu quick test script:

```bash
scripts/test-globalmenu.sh
scripts/test-globalmenu.sh --strict
scripts/test-globalmenu.sh --compact
```

Catatan:
- default script `non-strict`: jika `healthcheck` return `2` (registrar/service belum aktif), hasil tetap PASS dengan warning.
- gunakan `--strict` untuk fail-fast saat global menu belum tersedia.
- `smoke-globalmenu-focus.sh` dipakai untuk verifikasi perubahan binding saat focus switching antar app GTK/Qt/KDE.

## Install FileOps + Devices Daemon Service

```bash
scripts/install-fileops-devices-user-service.sh
# or explicit binary path
scripts/install-fileops-devices-user-service.sh /abs/path/to/desktopd
```

## Install Split FileOps + Devices Daemons (Optional)

```bash
scripts/install-split-fileops-devices-user-services.sh
# or explicit binaries:
scripts/install-split-fileops-devices-user-services.sh /abs/path/to/slm-fileopsd /abs/path/to/slm-devicesd
```

Script ini juga memasang drop-in `slm-desktopd.service.d/split-services.conf` agar
`desktopd` tidak mengklaim DBus name `FileOperations` dan `Devices` saat mode split aktif.

Output mencakup:
- `staticMatrix` (capability matrix statik per backend)
- `runtimeCapabilities` (capability effective backend aktif)
- `protocolVersion`, `eventSchemaVersion`, `connected`
- mode `watch` mengeluarkan JSON lines realtime (`kind`: snapshot/connected-changed/capabilities-changed/event-received).

## Next integration milestones

1. Replace static models with Linux system providers (DBus/NetworkManager/UPower/PipeWire).
2. Add a real app launcher process (`QProcess`) and window tracking layer.
3. Add workspace management and focus/z-order handling.
4. Add theme/token files and animation tuning.
