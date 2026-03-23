# SLM Desktop TODO

## Done (2026-02-28)
- [x] Split `slm-devicesd` from `desktopd` runtime ownership.
- [x] Add `Ping()` DBus method on daemon services:
  - `org.slm.WorkspaceManager1`
  - `org.slm.SessionState1`
  - `org.slm.Desktop.FileOperations`
  - `org.slm.Desktop.Devices`
- [x] Add `desktopd` watchdog healthcheck for peer daemons with retry/backoff:
  - `org.slm.Desktop.FileOperations`
  - `org.slm.Desktop.Devices`
- [x] Add DBus timeout policy in `FileManagerApi` (`Mount/Eject/Copy/Move`) with canonical fallback error codes.
- [x] Add structured logging envelope (`request_id`, `job_id`, `caller`) across daemon services.
- [x] Extract FileManager context menu blocks from `FileManagerWindow.qml` into dedicated component file, while keeping it as the single active implementation path.
- [x] Add versioned capability contract (`GetCapabilities()` + `api_version`) for adaptive clients.
- [x] Harden destructive target validation (canonical path + symlink resolution + protected target list).
- [x] Add failure-injection tests (daemon restart during active operations, UI state recovery).
- [x] Add chaos test for repeated service flaps with randomized operation mix (`copy/move/delete/trash`) and bounded completion SLA.
- [x] Add daemon process-level chaos test (real `slm-fileopsd` restart) with auto-attach validation in `FileManagerApi`.

## Done (2026-03-02)
- [x] Integrate app scoring into Launchpad:
  - add `Recommended` section/tab from `AppManager.topApps()`,
  - keep `All` list fallback.
- [x] Make app scoring weights configurable via `UIPreferences`:
  - `launchWeight`,
  - `fileOpenWeight`,
  - `recencyWeight`.
- [x] Expose ranked app list over DBus:
  - add `ListRankedApps(limit)` contract for shared consumers.
- [x] Harden XBEL usage ingestion:
  - add fallback behavior for malformed/partial `recently-used.xbel`,
  - avoid score collapse when XBEL parse fails.
- [x] Add test coverage for app ranking stack:
  - `appmodel_xbel_fallback_test`,
  - `appmodel_scoring_weights_test`,
  - strengthened `workspacemanager_dbus_test` and `workspacectl_smoke_test` for `ListRankedApps(limit)` limit + score ordering.
- [x] Add long-run soak test profile (5-10 min) gated behind env flag for nightly CI.

## Next
- [x] Tothespot: add grouped sections for mixed results:
  - `Top Apps`,
  - `Recent Files`,
  - `Search Results`.
- [x] Tothespot: add hybrid ranking boost on non-empty query:
  - boost matches from `AppManager.topApps()`,
  - boost paths present in `FileManagerApi.recentFiles()`.
- [x] Tothespot: add keyboard quick-actions:
  - `Ctrl+Enter` => Open Containing Folder (file),
  - `Alt+Enter` => Properties (file/folder/app).
- [x] Tothespot: add optional right preview pane for selected result:
  - mime, size, modified, path summary.
- [x] Runtime hardening: suppress non-fatal `Connections` warnings on `CompositorStateModel`
  by setting `ignoreUnknownSignals: true` in `Qml/DesktopScene.qml`.
- [x] Add screenshot flow tests:
  - save dialog validation (`empty/invalid filename`),
  - overwrite prompt (`Cancel/Replace`) path,
  - folder chooser roundtrip back to save dialog.
- [x] Add portal file chooser keyboard tests:
  - `Enter` primary action,
  - `Esc` cancel,
  - `Backspace` and `Alt+Up` go-up behavior with/without textfield focus.
- [x] Add `PortalService` DBus contract tests for chooser passthrough payloads:
  - cancel payload (`ok=false`, `canceled=true`, `error=canceled`),
  - multi-select payload (`paths[]`, `uris[]`),
  - save payload (`mode=save`, single `path/uri`).
- [x] Standardize screenshot save error codes end-to-end:
  - `invalid-filename`,
  - `overwrite-canceled`,
  - `save-failed`,
  - `io-error`.
- [x] Add integration tests around screenshot save file operations:
  - overwrite exists + cancel path,
  - overwrite exists + replace path.
- [x] Add lightweight screenshot flow observability:
  - propagate `request_id` from capture -> save dialog -> chooser -> finalize logs.
- [x] Remove deprecated GLib bookmark timestamp usage in `filemanagerapi.cpp`:
  - migrate `g_bookmark_file_get_modified` calls to `g_bookmark_file_get_modified_date_time`.

## Next (2026-03-04)
- [x] Tothespot: add automated context menu logic test coverage:
  - menu entry mapping by result type (`app/file/folder`),
  - keyboard wrap/selection behavior (`Up/Down/Enter` path).
- [x] Tothespot: add structured telemetry for result activation:
  - fields: `query`, `result_type`, `action`, `provider`.
- [x] Tothespot: integrate `scripts/test-tothespot.sh` as CI gate:
  - PR: `quick`,
  - nightly: `--full`.
- [x] Tothespot: visual polish:
  - highlight query substring in result title,
  - sticky section headers on scroll.
- [x] Tothespot: harden activation telemetry ring buffer tests:
  - validate max capacity is enforced,
  - validate FIFO eviction order on burst activation events.
- [x] Tothespot: expose activation telemetry capacity via API metadata:
  - add `GetTelemetryMeta()` on search service,
  - make ring-buffer tests consume API capacity (no hardcoded constant).
- [x] Tothespot: show telemetry capacity in debug panel (`showDebugInfo`):
  - pull `telemetryMeta()` from service when overlay opens,
  - display `activationCapacity` for runtime observability.
- [x] Tothespot: add `Copy Path` context action for file/folder results.
- [x] Tothespot: add keyboard shortcuts:
  - `Ctrl+L` focus search input,
  - `Ctrl+C` copy selected file/folder path.
- [x] Tothespot: improve query responsiveness:
  - debounce to `150ms`,
  - generation guard to drop stale query results,
  - skip redundant non-forced reload for same query.
- [x] Tothespot: activation payload contract coverage:
  - validate `query/provider/result_type/action` for app/file/folder via DBus test.
- [x] Tothespot: compact debug line in overlay:
  - profile/update/capacity/last-activation shown in one concise row.
- [x] Tothespot: guard smoke coverage for latest UX contracts:
  - shortcut guards (`Ctrl+L`, `Ctrl+C`) in overlay,
  - query generation + debounce guard (`150ms`) in `Main.qml`,
  - context action `copyPath` + activation payload guard in `Main.qml`.
- [x] Tothespot: improve result grouping for mixed query output:
  - keep section order stable (`Top Apps`, `Recent Files`, `Folders`, `Files`),
  - preserve sticky section header behavior.
- [x] Tothespot: deduplicate cross-provider results:
  - normalize app key (`desktopId/desktopFile/executable/name`),
  - normalize path key (`absolutePath/path`) and keep best-ranked row.
- [x] Tothespot: add recency-decay ranking boost for path results:
  - use `recentFiles().lastOpened`,
  - apply 7-day exponential decay.
- [x] Tothespot: balance provider dominance in mixed results:
  - soft-cap one provider to ~60% of visible query results,
  - fallback fill when provider diversity is low.
- [x] Tothespot: add pinned seed block for empty query:
  - source from Dock pinned entries,
  - render as `Pinned` section above `Top Apps`.
  - reverted: pinned seed hidden by product decision (Dock already covers pinned access).

## Backlog (Tothespot)
- [ ] Add row-level quick actions (launch/open/reveal/copy) without opening context menu.
  - canceled by product decision: keep Tothespot rows minimal and use context menu only.
- [x] Add keyboard group navigation (`Tab`/`Shift+Tab`) and quick launch (`Ctrl+1..9`).
- [x] Add keyboard clear shortcut (`Ctrl+K`) to reset query and selection.
- [ ] Add optional preview pane for non-compact popup mode.
- [ ] Add provider health indicators with timeout/fallback status.
- [ ] Add cold-start cache for empty-query popular results.

## Next (Theme / macOS mimic)
- [x] Add global `Popup` + `Dialog` style tokens in `Style/` (single source for radius/padding/background).
- [x] Typography hardening final:
  - enforce `Theme.fontFamilyUi` / `Theme.fontFamilyDisplay` across remaining legacy labels.
  - phase-1 done: global `Style/Label.qml` + topbar/indicator `Text` family alignment.
  - phase-2 done: `Main.qml` portal/shell text + `FileManagerWindow.qml` sidebar/tab/dialog text alignment.
  - phase-3 done: full audit `Qml/` + `Style/` confirms no `Text` block missing `font.family`.
- [ ] Elevation/shadow tokens:
  - unify popup/window shadow strength (`elevationLow/Medium/High`).
- [ ] Motion tokens:
  - replace ad-hoc durations/easing with shared animation tokens.
- [ ] Semantic control states:
  - define `controlBgHover/Pressed/Disabled`, `textDisabled`, `focusRing`.
- [ ] FileManager density pass:
  - align toolbar/search/tab/status heights to compact token grid.

## Done (2026-03-05)
- [x] Theme token hardening: elevation + focus + disabled state foundations.
  - Added semantic tokens in `Theme.qml`:
    - `focusRing`, `focusRingStrong`,
    - `textDisabled`,
    - `controlDisabledBg`, `controlDisabledBorder`,
    - `elevationShadowLow/Medium/High`.
- [x] Unified popup elevation in shared template.
  - `Style/PopupSurface.qml` now applies elevation variants (`low/medium/high`) with tokenized shadow colors.
- [x] Global focus ring pass (phase-1).
  - Applied on core controls:
    - `Style/TextField.qml`,
    - `Style/ComboBox.qml`,
    - `Style/Button.qml`,
    - `Style/ToolButton.qml`.
- [x] Global disabled-state pass (phase-1).
  - Applied tokenized disabled text/bg/border on:
    - `Style/Button.qml`,
    - `Style/ToolButton.qml`,
    - `Style/TextField.qml`,
    - `Style/ComboBox.qml`,
    - `Style/CheckBox.qml`,
    - `Style/RadioButton.qml`,
    - `Style/Switch.qml`,
    - `Style/MenuItem.qml`.
- [x] FileManager recent hardening to GIO/Freedesktop-only contract:
  - remove legacy local recent persistence (`QSettings/filemanager.ini`) from `FileManagerApi`.
  - remove manual recent writes from FileManager open actions; rely on GIO/app stack.
  - remove deprecated recent API surface (`setRecentThumbnail`, `addRecentFile`).

## Backlog (Theme)
- [ ] Add high-contrast mode toggle in `UIPreferences` + runtime Theme switch.
- [ ] Add reduced-motion mode toggle:
  - scale/disable non-essential transitions globally.
- [ ] Add optional dynamic wallpaper sampling tint with contrast guardrail.

## Done (2026-03-14)
- [x] Add permission operations playbook:
  - `docs/security/PERMISSION_PLAYBOOK.md` for capability onboarding, method mapping, deny/audit contract, and DBus/headless test troubleshooting.
- [x] Link permission playbook into architecture reading index (`docs/architecture/DIAGRAM_INDEX.md`).
- [x] Add capability decision matrix:
  - `docs/security/CAPABILITY_MATRIX.md` summarizing default decision by trust level.

## Next (FileManager maintenance)
- [x] Extract `sidebarContextMenu` from `FileManagerWindow.qml` into `Qml/apps/filemanager/FileManagerMenus.qml`.
- [x] Add smoke test for filemanager dialog/component load to catch QML `... is not a type` regressions early.
- [x] Remove unused wrapper APIs from `FileManagerWindow.qml` to shrink root surface and reduce coupling.
- [ ] Continue shrinking `FileManagerWindow.qml` by moving remaining dialog/menu glue into `Qml/apps/filemanager/`.
- [x] Remove now-unused `backgroundMenu` API if product keeps folder-context behavior for blank area.
- [x] Add focused smoke for rename/properties dialog open path (including required `hostRoot` contracts).

## Next (FileManager modularization in-repo)
- [x] Move FileManager C++ implementations to `src/apps/filemanager/` while keeping public headers stable in root for fast migration.
- [x] Move FileManager public headers to `src/apps/filemanager/include/` + introduce compatibility shim headers (temporary, deprecate later).
- [x] Split `FileManagerApi` into smaller units (`recent`, `openwith`, `storage`, `ops`) with one facade.
  - [x] Phase-1: extract `recent` domain implementation to `src/apps/filemanager/filemanagerapi_recent.cpp` while preserving public facade/API.
  - [x] Phase-2: extract `openwith` domain implementation to `src/apps/filemanager/filemanagerapi_openwith.cpp` while preserving public facade/API.
  - [x] Phase-3: extract `storage` discovery/cache + async wrappers to `src/apps/filemanager/filemanagerapi_storage.cpp` while preserving public facade/API.
  - [x] Phase-4: extract `ops` orchestration domain implementation.
    - moved `fileoperations*` + `globalprogresscenter` to `src/apps/filemanager/ops/`.
    - updated build graph (`CMakeLists.txt`, `cmake/Sources.cmake`) to new path.
    - added temporary compatibility shim headers under `src/filemanager/ops/` for transition.
- [x] Isolate FileManager-specific tests under `tests/filemanager/` and wire them into `test_headless`.
  - moved core suite files:
    - `fileoperationsmanager_test`,
    - `fileoperationsservice_dbus_test`,
    - `fileopctl_smoke_test`,
    - `filemanagerapi_daemon_recovery_test`,
    - `filemanagerapi_fileops_contract_test`,
    - `filemanager_dialogs_smoke_test`.
- [x] Add extraction checklist gate for future repo split (API freeze + standalone build + release boundary).
  - script: `scripts/check-filemanager-extraction-gate.sh`

## Next (FileManager split to standalone repository)
- [ ] Phase-0: define split boundary and ownership.
  - [x] freeze public contract candidates:
    - C++ facade: `FileManagerApi`,
    - DBus contracts: `org.slm.Desktop.FileOperations`,
    - QML public surface under `Qml/apps/filemanager/`.
  - [x] list shared deps that stay in desktop shell vs moved to filemanager repo:
    - keep shared in shell: `Theme`, `MotionController`, `AppExecutionGate`,
    - move to filemanager repo: file ops orchestration, recent/openwith/storage domain code, filemanager-specific QML.
- [ ] Phase-1: make in-repo package self-contained before extraction.
  - [x] complete `FileManagerApi` domain split (`ops` remaining phase).
  - [x] move FileManager QML surface from `Qml/components/filemanager` to `Qml/apps/filemanager` + update runtime imports/CMake path macros.
  - [x] move FileManager tests to `tests/filemanager/` and ensure standalone target group passes.
    - target group: `slm-filemanager-core` + `filemanager_test_suite`
  - [x] add dedicated CMake option/profile to build only FileManager stack + tests.
    - option: `SLM_BUILD_FILEMANAGER_STANDALONE_TARGET`
    - profile script: `scripts/build-filemanager-profile.sh`
- [x] Phase-2: create new repository skeleton (`slm-filemanager`).
  - [x] seed repo modules:
    - `src/core` (api/domain),
    - `src/qml` (UI),
    - `src/daemon` (`slm-fileopsd` integration client),
    - `tests/`.
    - staging path in monorepo: `modules/slm-filemanager/`
    - staging standalone build: `scripts/build-filemanager-standalone-staging.sh`
    - namespaced core target alias available: `SlmFileManager::Core`
  - [x] import history with path filter (preserve blame) for:
    - `src/apps/filemanager/*`,
    - `Qml/apps/filemanager/*`,
    - related tests/docs.
    - helper script prepared: `scripts/prepare-filemanager-history-split.sh`
    - extraction manifest: `modules/slm-filemanager/docs/EXTRACTION_PATHS.txt`
    - history-preserving fallback active when `git-filter-repo` is unavailable:
      - uses `git fast-export/import`.
    - readiness helper available:
      - `scripts/check-filemanager-split-readiness.sh`
    - manifest expanded for self-contained split boundary:
      - added `src/core/actions/*`, `src/core/permissions/*`,
        `src/core/execution/{appexecutiongate,appruntimeregistry}*`,
        `metadataindexserver.*`, `dbuslogutils.h`, `urlutils.h`.
    - split output now auto-materializes standalone root scaffolding:
      - `CMakeLists.txt`, `cmake/*`, `.github/workflows/ci.yml`, `scripts/ci-smoke.sh`.
    - split output now auto-commits scaffolding:
      - commit message: `Add standalone split scaffolding (build + CI)`.
- [ ] Phase-3: integration bridge back to desktop shell.
  - [x] consume standalone filemanager as:
    - git submodule or subtree (decide one),
    - exported CMake package target (`SlmFileManager::Core`, `SlmFileManager::Qml`).
    - cutover notes documented: `docs/architecture/FILEMANAGER_SPLIT_CUTOVER.md`
    - CMake bridge implemented:
      - `SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE` (OFF by default)
      - `appSlm_Desktop` now links `SlmFileManager::Core` (no direct embed FileManager sources).
    - staging package config exported:
      - `SlmFileManagerConfig.cmake`
      - `SlmFileManagerTargets.cmake`
    - integration mode validation script:
      - `scripts/test-filemanager-integration-modes.sh`
      - validates in-tree profile + external `find_package(SlmFileManager)` build path.
    - external-package MOC linkage fixed by including QObject headers in standalone target sources.
    - external-package duplicate-symbol linkage fixed:
      - app target removes overlapping FileManager-owned sources when
        `SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=ON`.
    - bootstrap helper for external package consumption:
      - `scripts/bootstrap-external-filemanager-package.sh`
      - supports package-only mode via `SLM_EXTERNAL_FM_SKIP_SHELL=1`.
  - [ ] keep backward-compat shim in shell for one release cycle:
    - [x] header include shims,
    - [x] QML import alias shims (`Qml/components/filemanager/*` -> canonical `Qml/apps/filemanager/*` forwarding surface).
- [ ] Phase-4: CI/release hardening.
  - [x] add independent CI pipeline template for standalone repo:
    - template workflow: `modules/slm-filemanager/.github/workflows/ci.yml`
    - smoke entrypoint: `modules/slm-filemanager/scripts/ci-smoke.sh`
    - current scope in staging: standalone configure/build/install + package-configure smoke.
    - post-extraction follow-up:
      - wire migrated headless tests,
      - add QML load smoke,
      - add DBus daemon recovery tests in standalone repo CI.
    - external repo CI observed passing on `main` after push.
  - [x] add compatibility CI in desktop shell against pinned filemanager tag.
    - workflow job: `filemanager-compatibility-pinned`
    - script: `scripts/test-filemanager-compatibility-pinned.sh`
  - [x] define versioning and compatibility matrix:
    - `slm-desktop` tag ↔ `slm-filemanager` tag.
    - documented at `docs/architecture/FILEMANAGER_COMPATIBILITY_MATRIX.md`
- [ ] Exit gates before full cutover.
  - [x] zero direct include from shell to filemanager private headers.
  - [x] no unresolved runtime QML import path from old location.
  - [x] all file operations and context-menu flows pass smoke + regression suite.
    - gate script: `scripts/test-filemanager-smoke-regression.sh`
    - unified runner entrypoint: `scripts/test.sh filemanager-smoke <build_dir>`
  - [x] one rollback plan documented (pin previous integrated commit).
    - `docs/architecture/FILEMANAGER_ROLLBACK_PLAN.md`
  - [x] split daemon-recovery coverage into:
    - default stable gate: `filemanagerapi_daemon_recovery_test`
    - opt-in stress gate: `filemanagerapi_daemon_recovery_strict_test`
      (`SLM_RUN_STRICT_FILEOPS_RECOVERY=1`)
  - [x] wire CI policy:
    - stable DBus recovery gate on regular pipeline.
    - strict recovery gate on nightly/manual pipeline.
  - [x] unify DBus gate entrypoint script:
    - `scripts/test-filemanager-dbus-gates.sh --stable|--strict`

## Next (Global Menu integration)
- [x] Add DBusMenu live listeners (`LayoutUpdated` / `ItemsPropertiesUpdated`) for active app menu to reduce stale top-level menu state without tighter polling.
- [x] Add dynamic submenu rendering for native app DBusMenu (GTK/KDE/Qt), not only SLM fallback service.
- [x] Add per-app menu cache with explicit invalidation on focus/app switch and menu layout revisions.
- [x] Add `globalmenuctl` diagnostics CLI: show registrar services, active menu binding, and top-level dump for fast triage.
- [x] Add `globalmenuctl_smoke_test` and wire it into `test_headless` gate.
- [x] Add runtime diagnostics script `scripts/test-globalmenu.sh` and document standard debug flow in `README.md`.

## Next (Global Menu hardening)
- [ ] Runtime integration check global menu end-to-end on active GTK/Qt/KDE apps (verify active binding/top-level changes by focus switching).
  - helper available: `scripts/smoke-globalmenu-focus.sh` (`--strict --min-unique 2`).
- [x] Add automated test for global menu cache invalidation behavior (focus switch + layout update).

## Next (Clipboard service hardening)
- [ ] Integrate native `wl-data-control` low-level path for Wayland clipboard watching (event-driven primary path, keep Qt/X11 fallback).

## Next (Portal Adapter hardening)
- [ ] Tighten DBus request/session object model to align with `org.freedesktop.portal.Request` / `org.freedesktop.portal.Session` semantics.
  - [x] lifecycle semantics hardened in adapter objects:
    - request responds once (`Response`) and `Close()` cancels pending request only.
    - session supports `Close()` / revoke flow with explicit state transitions.
  - [x] object path/id generation made collision-safe (timestamp + monotonic sequence).
  - [x] session operation guard added:
    - reject cross-domain operations via `session-capability-mismatch`.
  - [x] response payload normalized for portal-like contract:
    - `response` numeric code,
    - `results` map,
    - `requestHandle` / `sessionHandle` aliases (legacy fields tetap ada).
  - [ ] remaining:
    - [ ] tighten request/session DBus payload shape closer to freedesktop portal response contract,
      - [x] base `response/results/handle` shape added.
      - [x] impl portal bridge normalizes per-method payload:
        - `response/results` always present,
        - file chooser exposes normalized `results.uris` + `results.choices`,
        - screenshot exposes stable `results.uri`,
        - screencast operations expose normalized `results.session_handle`.
      - [x] add deterministic method-level semantic keys:
        - OpenURI: `results.handled`
        - FileChooser: `results.current_filter`
        - ScreenCast: `results.streams` / `results.sources_selected` / `results.stopped`
      - [x] add normalized snake_case request handle:
        - `results.request_handle` present across impl bridge responses.
      - [x] add top-level compatibility aliases:
        - `request_handle` and `session_handle` mirrored at response top-level.
      - [x] add impl FileChooser contract test for success/cancel normalization:
        - success => `response=0` + normalized `results.uris`
        - canceled => `response=2` + deterministic `results` keys.
      - [x] make fallback/error paths deterministic for impl ScreenCast semantics:
        - `results.sources_selected` and `results.stopped` always present in corresponding operations.
      - [x] enrich Screencast operation payload in adapter path for client contract stability:
        - `SelectSources` returns `results.selected_sources`,
        - `Start` returns deterministic synthetic stream descriptor when PipeWire stream is not yet wired,
        - `Stop` preserves `results.stopped` + `results.session_closed`.
      - [x] add adapter lifecycle test for Screencast session operation semantics:
        - verifies `selected_sources` + synthetic `streams` payload,
        - verifies `closeSession=false` keeps session alive,
        - verifies `closeSession=true` closes session object.
      - [x] add forward-compatible stream descriptor ingestion in ScreencastStart:
        - if backend provides `streams[]` or `node_id/stream_id`, adapter uses it directly,
        - fallback to synthetic stream only when descriptor is missing.
      - [x] add impl bridge hook to optional capture provider service:
        - `BridgeScreenCastStart` now queries `org.slm.Desktop.Capture:GetScreencastStreams(...)`
          (3-arg and backward-compatible 2-arg signature),
        - if provider returns stream descriptors, they override empty fallback streams.
      - [x] add payload snapshot regression test for impl interfaces:
        - `tests/implportal_payload_snapshot_test.cpp` verifies required fields across
          FileChooser/OpenURI/Screenshot/ScreenCast (+ sub-operations).
      - [x] align detailed payload keys per interface with xdg-desktop-portal frontend expectations.
        - [x] canonicalize `session_handle` aliasing in shared impl response normalizer
          (`results.session_handle` + top-level `session_handle` mirrored from
          `sessionHandle/sessionPath` where available).
        - [x] DBus regression extended for `GlobalShortcuts` flow to assert stable
          `session_handle` presence and deterministic operation payload keys (`bound`).
    - [x] add explicit request/session versioned interface surface for impl portal split:
      - service `org.freedesktop.impl.portal.desktop.slm`
      - path `/org/freedesktop/portal/desktop`
      - bridge methods: `FileChooser`, `OpenURI`, `Screenshot`, `ScreenCast`.
      - [x] split interface adaptors on same path:
        - `org.freedesktop.impl.portal.FileChooser`
        - `org.freedesktop.impl.portal.OpenURI`
        - `org.freedesktop.impl.portal.Screenshot`
        - `org.freedesktop.impl.portal.ScreenCast`
- [x] Integrate real trusted shell consent UI flow into `PortalDialogBridge` (production consent path, no stub-only fallback).
  - `PortalDialogBridge` kini memprioritaskan jalur DBus trusted UI
    (`org.slm.Desktop.PortalUI` / `ConsentRequest`) secara async.
  - fallback in-process signal tetap didukung untuk embedding/test UI.
  - fallback terakhir fail-closed (`deny`) dipertahankan saat UI bridge unavailable.
- [x] Add automated tests for portal request/session lifecycle and policy decision persistence behavior.
  - added: `tests/portal_adapter_lifecycle_persistence_test.cpp`
  - covers:
    - request lifecycle (`AwaitingUser` -> final state, close/cancel semantics),
    - session lifecycle (activate/close/revoke + manager cleanup),
    - permission persistence roundtrip via `PortalPermissionStoreAdapter`,
    - `PolicyEngine` readback from stored decisions (`stored-policy` path).
- [x] Implement `CAPABILITY_PORTAL_MAPPING.md` phase-1 (bootstrap API wiring):
  - Share (`org.desktop.portal.Share`),
  - Screenshot (`org.desktop.portal.Screenshot`),
  - GlobalShortcuts session bootstrap.
  - Added dedicated permission capabilities for:
    - `Screenshot.CaptureArea`,
    - `GlobalShortcuts.CreateSession/Register/List/Activate/Deactivate`,
    - `Screencast.CreateSession/SelectSources/Stop`.
- [x] Implement `CAPABILITY_PORTAL_MAPPING.md` phase-2:
  - Screencast full session flow (`CreateSession/SelectSources/Start/Stop`),
  - Search summary APIs (`Query*` direct safe summaries).
  - progress:
    - portal methods added: `ScreencastSelectSources`, `ScreencastStart`, `ScreencastStop`,
    - session operation mediator path added (operate on existing session with owner/active checks),
    - search summary portal surface completed on `PortalService` for:
      - `QueryClipboardPreview`, `QueryFiles`, `QueryContacts`, `QueryEmailMetadata`, `QueryCalendarEvents`,
      - provider filtering + summary/redaction path verified in `tests/portalservice_dbus_test.cpp`.
    - screencast backend wiring validated:
      - impl portal `Start` refreshes stream payload from `org.slm.Desktop.Screencast:GetSessionStreams`
        (fallback `org.slm.Desktop.Capture:GetScreencastStreams`),
      - backend mode `pipewire`/`portal-mirror` covered by DBus tests.
- [x] Implement `CAPABILITY_PORTAL_MAPPING.md` phase-3:
  - PIM metadata + picker APIs,
  - OpenWith query/invoke split,
  - Actions query/invoke split.
  - progress:
    - PIM/search surface (portal service) added:
      - `QueryMailMetadata` (alias contract to `QueryEmailMetadata`),
      - resolve/read methods: `ResolveContact`, `ResolveEmailBody`, `ResolveCalendarEvent`,
        `ReadContact`, `ReadCalendarEvent`, `ReadMailBody`,
      - picker request methods: `PickContact`, `PickCalendarEvent`.
    - high-risk resolve/read endpoints now enforce argument + user-gesture gate at portal boundary.
    - capability mapper extended for PIM resolve/read/pick aliases (`Accounts.*` capability mapping).
    - DBus regression extended in `tests/portalservice_dbus_test.cpp` for `ResolveEmailBody`
      invalid-argument and gesture-required contract.
    - Actions query/invoke split surface added:
      - `QueryActions`, `QueryContextActions`, `InvokeAction` on `PortalService`,
      - action query path integrated with `org.freedesktop.SLMCapabilities` (`SearchActions` / `ListActionsWithContext`),
      - mapper entries for `QueryActions` / `QueryContextActions` (direct) and `InvokeAction` (request + gesture).
    - DBus regression extended in `tests/portalservice_dbus_test.cpp` for actions query contract.
    - `InvokeAction` gesture gate contract covered in DBus regression.
- [ ] Implement `CAPABILITY_PORTAL_MAPPING.md` phase-4 (high-risk):
  - Clipboard mediated history/content APIs,
  - Notification history read policy (internal-only or per-app scoped),
  - Full-content resolve endpoints (`ReadMailBody`, `ResolveClipboardResult`).
  - progress:
    - `ResolveClipboardResult` surface added on `PortalService` with dedicated mapper capability
      (`Search.ResolveClipboardResult`).
    - high-risk guard enforced at boundary: result-id required + user-gesture required.
    - DBus regression extended in `tests/portalservice_dbus_test.cpp` for
      `ResolveClipboardResult` invalid-argument / gesture-required contract.
    - Notification history read path added conservatively:
      - `QueryNotificationHistoryPreview` + `ReadNotificationHistoryItem` on `PortalService`,
      - mapped to `Notifications.ReadHistory` capability,
      - default behavior remains deny/internal-only on portal surface.
    - DBus regression extended for notification history preview/read deny contract.

## Next (xdg-desktop-portal-slm roadmap execution)
- [ ] Phase-0 Discovery & contract freeze:
  - [x] Arsitektur target `xdg-desktop-portal-slm` + tier prioritas portal ditulis.
  - [x] Canonical capability-to-portal mapping ditulis (`docs/architecture/CAPABILITY_PORTAL_MAPPING.md`).
  - [x] Putuskan daftar portal yang diimplementasikan native vs fallback ke backend lain per milestone distro (`docs/architecture/PORTAL_NATIVE_FALLBACK_PLAN.md`).
- [ ] Phase-1 Shared infrastructure (wajib):
  - [x] Service identity + registration untuk backend impl `org.freedesktop.impl.portal.*` (tanpa memutus API internal lama).
  - [x] Request/Session lifecycle hardening + contract tests.
  - [x] PermissionStore adapter hardening + tests persistensi keputusan (allow/deny scope-aware).
  - [x] Parent-window/app identity resolver yang konsisten lintas toolkit.
- [ ] Phase-2 Core compatibility portals:
  - [x] OpenURI
  - [x] FileChooser
  - [x] Settings/Appearance
  - [x] Notification (send path aman + anti-spam guard)
  - [x] Inhibit
- [ ] Phase-3 File/document mediation:
  - [x] Documents integration (tokenized/document-store flow)
  - [x] Trash mediation path
  - [x] OpenWith portal surface (query vs invoke split)
- [ ] Phase-4 Capture stack:
  - [x] Screenshot flow hardening (request-based + picker)
  - [x] Screencast session flow end-to-end (PipeWire backend binding + revoke/close semantics)
    - progress:
      - [x] impl guard untuk `CreateSession/SelectSources/Start/Stop` (handle/parent/session path).
      - [x] `Stop` mendukung auto-close session (`closeSession=true`) + hasil `session_closed`.
      - [x] tambah revoke flow eksplisit:
        - `RevokeScreencastSession(sessionHandle, reason)` pada impl bridge,
        - backend `revokeSession(...)` + fallback kompatibel ke `closeSession(...)`,
        - emisi signal `ScreencastSessionRevoked(...)`.
      - [x] tambah `org.slm.Desktop.Capture` provider konkret di `desktopd`:
        - method `GetScreencastStreams(sessionPath, appId, options)` + fallback signature lama,
        - normalisasi stream descriptor + cache stream per-session untuk stabilitas hasil bridge.
      - [x] tambah lifecycle control method di capture provider:
        - `SetScreencastSessionStreams`, `ClearScreencastSession`, `RevokeScreencastSession`,
        - impl bridge sinkronkan close/revoke ke capture provider supaya cache stream tidak stale.
      - [x] impl bridge melakukan upsert stream final dari `ScreenCast.Start` ke capture provider
        (`SetScreencastSessionStreams`) untuk konsistensi cache lintas service.
      - [x] dukung format descriptor stream PipeWire tuple-style (`[node_id, a{sv}]`) pada capture provider
        + refresh otomatis saat payload start masih synthetic-only.
      - [x] tambah handoff stream descriptor lintas-daemon:
        - metadata `screencast.streams` disimpan pada session backend saat `ScreencastStart`,
        - method internal `org.slm.Desktop.Portal:GetScreencastSessionStreams(sessionPath)` diekspos,
        - `CaptureService` mengonsumsi method ini sebagai sumber internal saat opsi/cache session kosong.
      - [x] hardening cache lifecycle:
        - `CaptureService` clear cache berdasarkan signal impl portal (state changed/revoked).
      - [x] remove legacy probe path:
        - fallback `pw-dump` dihapus; lookup stream kini deterministik
          (`options` -> cache session -> metadata session portal).
      - [x] add event-driven ingest path:
        - komponen `CaptureStreamIngestor` (desktopd) subscribe signal `org.slm.Desktop.Screencast`,
        - stream update dipush ke `CaptureService` via `SetScreencastSessionStreams`.
      - [x] activate `org.slm.Desktop.Screencast` service in `desktopd` runtime
        (`desktopd_main`) so ingest path works without test-only emitters.
      - [x] route impl portal screencast lifecycle/stream publish to
        `org.slm.Desktop.Screencast` as primary path with compatibility fallback
        to `org.slm.Desktop.Capture`.
      - [x] wire `ScreencastService` to impl-portal screencast lifecycle signals and
        sync stream payload from internal portal metadata on active-session changes.
      - [x] extract `ScreencastStreamBackend` abstraction in `desktopd` (current impl: portal metadata mirror).
      - [x] add backend mode selector + runtime fallback diagnostics
        (`SLM_SCREENCAST_STREAM_BACKEND`, `Ping/GetState` backend status fields).
      - [x] wire optional PipeWire build integration in CMake (`libpipewire-0.3`, `libspa-0.2`)
        and native backend init/deinit path (`pw_init/pw_deinit`) for mode `pipewire`.
      - [x] bind dasar `ScreencastStreamBackend` ke registry PipeWire untuk candidate stream discovery
        (dengan fallback metadata portal).
      - [x] tambah korelasi dasar session portal -> node PipeWire menggunakan node hints dari metadata session.
      - [x] enforce session-scoped matching di mode PipeWire (tanpa fallback kandidat global saat hint session tersedia).
      - [x] tambah property-level correlation untuk session hints (`node_name`, `app_name`, `source_type`) saat `node_id` tidak memadai.
      - [x] hardening multi-stream density:
        - scoring matcher (exact/contains/source-type) + top-band selection untuk mengurangi false-positive.
      - [x] expose revoke control path in trusted UI model (`ScreencastPrivacyModel::revokeSession`)
        + explicit refresh on `ScreencastSessionRevoked` signal.
      - [x] finalize revoke semantics lintas service:
        - add trusted UI revoke path (`ScreencastPrivacyModel::revokeSession`),
        - add e2e test `screencast_revoke_e2e_test` to validate cross-service cleanup
          (`ImplPortalService` -> `ScreencastService` -> `CaptureService`) + active state reset.
      - [x] tambah parity e2e untuk close-all:
        - `CloseAllScreencastSessions` memverifikasi cleanup multi-session lintas service
          + cache provider tidak kembali ke `cache` untuk sesi yang sudah ditutup.
      - [x] tambah DBus integration test sesi backend nyata (tanpa seed stream manual)
        untuk flow `CreateSession -> SelectSources -> Start -> Stop(closeSession=true)`
        dan validasi transisi `GetScreencastState` aktif->idle.
      - [x] tambah smoke test runtime-guarded untuk mode backend `pipewire`
        (`pipewire_backend_session_smoke`, auto-skip bila backend fallback/non-aktif).
  - [x] Privacy indicator integration (recording state)
    - progress:
      - [x] expose capture state signal dari impl backend (`ScreencastSessionStateChanged`).
      - [x] wire signal ke UI/topbar indicator via `ScreencastPrivacyModel` + `ScreencastIndicator`.
      - [x] add quick control to stop one/all active screencast sessions from indicator popup.
      - [x] tambah DBus test untuk trusted UI model (`screencastprivacymodel_dbus_test`)
        mencakup refresh state + aksi stop/revoke/stop-all.
      - [x] tambah status aksi terakhir (stopped/revoked) di `ScreencastPrivacyModel`
        untuk memperjelas konteks perubahan sesi pada popup indikator.
      - [x] final polish UI/privacy copy + e2e test dengan sesi screencast nyata
        (termasuk smoke test runtime-guarded backend `pipewire`).
- [ ] Phase-5 Input/control advanced:
  - [x] GlobalShortcuts session flow production path (conflict resolution + persistence policy)
    - [x] add impl portal interface `org.freedesktop.impl.portal.GlobalShortcuts`
      (`CreateSession/BindShortcuts/ListBindings/Activate/Deactivate`).
    - [x] add session-owner checks + accelerator conflict detection (cross-session).
    - [x] add lightweight persistence for app-approved bindings (`persist=true`).
  - [x] Clipboard mediated contract (summary vs full content, conservative default deny)
    - [x] `PortalService::QueryClipboardPreview` sanitized to summary-only clipboard payload
      (`previewText/itemType/timestampBucket`, no raw content leak).
    - [x] `PortalService::RequestClipboardContent` hardened with strict input validation
      (requires `clipboardId`/`resultId`) and explicit user-gesture gate.
    - [x] portal path now forces `includeSensitive=false` and does not trust
      caller-provided `initiatedFromOfficialUI`.
    - [x] mediator context hardening: only trusted callers (official/privileged)
      can assert `initiatedFromOfficialUI`; third-party spoofing ignored.
    - [x] DBus regression: `portalservice_dbus_test::clipboardRequests_validationContract`.
  - [ ] InputCapture
    - [x] phase-0 skeleton added on impl portal surface:
      - interface `org.freedesktop.impl.portal.InputCapture`,
      - methods: `CreateSession`, `SetPointerBarriers`, `Enable`, `Disable`, `Release`.
    - [x] backend mediation wiring added:
      - `PortalCapabilityMapper` mappings for `InputCapture*` methods,
      - session capability compatibility in `PortalAccessMediator`.
    - [x] permission foundation extended for InputCapture capability set
      (`CreateSession/SetPointerBarriers/Enable/Disable/Release`) + sensitivity/gesture defaults.
    - [x] DBus contract smoke coverage added in `implportalservice_dbus_test`
      (callability + stable portal response envelope).
    - [x] phase-1 session operation semantics in mediator:
      - `SetPointerBarriers` persists barriers metadata in session context,
      - `Enable/Disable` toggles session runtime flag (`inputcapture.enabled`),
      - `Release` supports close-session semantics (`session_closed`).
    - [x] lifecycle regression added in `portal_adapter_lifecycle_persistence_test`
      for `InputCapture` operation flow (barriers -> enable -> disable -> release).
    - [x] host backend service wired in `desktopd`:
      - new `InputCaptureService` (`org.slm.Desktop.InputCapture`) with session-state API,
      - impl-portal bridge syncs InputCapture operations to host service (`host_synced` payload),
      - DBus smoke coverage added in `inputcaptureservice_dbus_test`.
    - [x] input backend abstraction introduced in `desktopd`:
      - `InputCaptureBackend` interface + default backend factory,
      - `InputCaptureService` now forwards session/barrier/enable/disable/release to backend path
        and returns backend report in response payload.
    - [x] barrier contract hardening:
      - validate barrier payload (`id`, axis-aligned geometry) on `SetPointerBarriers`,
      - add negative DBus regression for invalid barrier shape in `inputcaptureservice_dbus_test`.
    - [x] host conflict gate + mediation hint:
      - only one active capture session at a time in host service,
      - conflicting `EnableSession` now returns `permission-denied` with conflict reason,
      - host supports optional preemption path (`allow_preempt`) for mediated takeover flow,
      - portal bridge now propagates explicit host `ok=false` responses as portal errors
        (while still graceful when host service is unavailable).
    - [x] portal-side mediated retry hook for capture takeover:
      - `BridgeInputCaptureEnable` can perform one controlled host retry
        when conflict is returned and preempt option is explicitly enabled.
    - [x] security hardening for takeover trigger:
      - preempt retry now requires trusted mediation context (trusted caller + gesture + mediated flag),
      - direct untrusted preempt attempts are denied with
        `preempt-requires-trusted-mediation`,
      - regression covered in `implportalservice_dbus_test`.
      - host `ok=false` failures on `Enable` are asserted to propagate as portal error
        (regression covered in `implportalservice_dbus_test`).
      - host conflict details (`conflict_session`, `requires_mediation`) propagated
        into portal denial payload for UI mediation hooks.
    - [ ] next:
      - [~] implement real compositor/input backend path (barrier placement, pointer lock/capture),
        - [x] external helper-process adapter wired (`SLM_INPUTCAPTURE_HELPER`) as production hook
          for compositor-specific backend integration,
        - [x] native DBus-forward adapter wired (`SLM_INPUTCAPTURE_DBUS_SERVICE/PATH/IFACE`)
          to bind InputCapture backend directly to compositor service without helper process,
        - [x] regression test for helper backend path added in `inputcaptureservice_dbus_test`
          (temporary executable helper + capability/state verification),
        - [x] regression test for DBus-direct backend path added in
          `inputcaptureservice_dbus_test` (fake backend service registration),
        - [x] regression test for DBus-direct auto-detect default backend
          (`org.slm.Desktop.InputCaptureBackend`) added in `inputcaptureservice_dbus_test`,
        - [~] implement native compositor binding (no helper) for default runtime path,
          - [x] auto-detect native compositor DBus services
            (`org.slm.Compositor.InputCaptureBackend` / `org.slm.Compositor.InputCapture`)
            and route through `dbus-direct` with mode `native-compositor-dbus`,
          - [x] regression test added for compositor-native auto-detect path
            in `inputcaptureservice_dbus_test`,
          - [x] end-to-end contract test added with real runtime provider
            `CompositorInputCaptureBackendService` (non-fake) in
            `inputcaptureservice_dbus_test`,
          - [x] runtime provider service contract implemented in shell process:
            `CompositorInputCaptureBackendService`
            (`org.slm.Compositor.InputCaptureBackend`) with session/barrier/enable/disable/release API,
          - [x] provider observability added:
            `GetState` now exposes `active` + `active_session`,
            DBus properties/signals for `activeSession` and enabled session count,
          - [x] provider command-forward contract extended:
            `SetPointerBarriers` now forwards compositor command (`inputcapture set-barriers ...`)
            in addition to enable/disable/release bridge commands,
            covered by deterministic provider contract test
            `contract_realCompositorProviderCommandForwarding` in
            `inputcaptureservice_dbus_test`,
          - [x] provider response observability extended:
            session operation results now include
            `results.compositor_command_forwarded` (and reason when false)
            to distinguish stateful fallback vs actual compositor command acceptance.
          - [x] primitive observability extended:
            session operation results now include
            `results.compositor_primitive_source` (`structured`/`dbus`)
            for native-path diagnostics and policy gates.
          - [x] strict-native command gate added (opt-in):
            `SetPointerBarriers` / `EnableSession` / `DisableSession` support
            `options.require_compositor_command=true` to hard-fail + rollback
            when compositor command path is unavailable (covered in
            `contract_realCompositorProviderRequireCompositorCommandRollback`).
          - [x] primitive compositor bridge (DBus) wired as direct native path:
            provider can call compositor primitive interface via
            `SLM_INPUTCAPTURE_PRIMITIVE_SERVICE/PATH/IFACE`
            for `SetPointerBarriers`/`EnableSession`/`DisableSession`/`ReleaseSession`,
            with response observability:
            `compositor_primitive_configured` + `compositor_primitive_applied`,
            covered in `contract_realCompositorProviderPrimitiveBridgePath`.
          - [x] runtime primitive object published by shell:
            `CompositorInputCapturePrimitiveService`
            at `/org/slm/Compositor/InputCapture`
            (`org.slm.Compositor.InputCapture`) for native bridge routing.
          - [x] regression added for runtime primitive structured bridge preference:
            `CompositorInputCapturePrimitiveService` now verified to prefer structured
            `set/enable/disable/release` bridge methods before `sendCommand` fallback
            (`contract_primitiveServiceStructuredBridgePreferred` in
            `inputcaptureservice_dbus_test`).
          - [x] compositor command bridge hardening in `KWinWaylandIpcClient`:
            `inputcapture ...` command path now supports explicit DBus bridge
            (`SLM_INPUTCAPTURE_COMPOSITOR_*`) and avoids self-recursion by default.
          - [x] provider now prefers structured native primitive bridge:
            `CompositorInputCaptureBackendService` invokes
            `set/enable/disable/releaseInputCaptureSession` on command bridge first,
            then falls back to DBus primitive adapter path.
            Covered by regression
            `contract_realCompositorProviderStructuredPrimitivePreferred`.
          - [ ] bind provider to real compositor pointer-capture/barrier primitives (currently stateful contract + optional command hook).
      - [ ] add conflict/policy UX mediation flow for sensitive capture entry points.
        - [x] conflict denial payload now exposes explicit mediation hints:
          `mediation_action=inputcapture-preempt-consent`,
          `mediation_scope=session-conflict`
          on host + compositor provider + portal bridge paths.
        - [x] portal denial payload now also includes normalized
          `results.mediation` object:
          `{ required, action, scope, reason, conflict_session }`
          to simplify trusted UI consent flow wiring.
        - [x] `BridgeInputCaptureEnable` now supports trusted UI consent retry flow:
          on conflict + preempt request, portal calls
          `org.slm.Desktop.PortalUI.ConsentRequest` and retries host enable
          with `mediatedByTrustedUi=true` when approved.
        - [x] shell trusted UI now renders consent dialog for `PortalUI.ConsentRequest`
          (`PortalConsentDialogWindow`) with actions:
          `Allow Once` / `Always Allow` / `Deny`, wired to
          `PortalUiBridge.submitConsentResult`.
        - [x] consent decision persistence + audit wired on portal preempt mediation:
          `BridgeInputCaptureEnable` now stores `allow_always`/`deny_always`
          (or explicit `persist=true`) into `PermissionStore` for capability
          `InputCaptureEnable`, records audit event, and exposes
          `results.host_preempt_mediation_policy`.
- [ ] Phase-6 Privacy/data portals:
  - [ ] Camera
  - [ ] Location
  - [ ] PIM metadata/resolution portals (contacts/calendar/mail metadata first, body last)
- [ ] Phase-7 Hardening & interop:
  - [ ] DBus contract/regression suite GTK/Qt/Electron/Flatpak
    - [x] hardening untuk local dev bus-collision:
      DBus integration tests portal/capture kini auto-skip saat service name sudah di-claim
      proses lain (menghindari false-negative di sesi desktop aktif).
  - [ ] Race/cancellation/multi-session stress tests
    - [x] baseline multi-session stability regression added for InputCapture portal
      (`implportalservice_dbus_test::inputcapture_multisession_stability_contract`)
      covering create/barriers/enable-preempt/disable/release sequence across
      3 sessions with host sync assertions.
    - [x] post-release safety regression added:
      `implportalservice_dbus_test::inputcapture_postRelease_disableDenied_contract`
      asserts post-release disable is denied with stable host reason
      (`missing-session-path`), preventing stale-session reuse.
  - [ ] Packaging + `portals.conf` coexistence tests (gtk/kde/wlr fallback)
    - [x] add profile-based portals config templates:
      `portals.mvp.conf`, `portals.beta.conf`, `portals.production.conf`
      under `scripts/xdg-desktop-portal/`.
    - [x] add contract test `portal_portals_conf_strategy_test`
      to validate default routing + targeted fallback mapping.
      - [x] add guard that every `<iface>=slm` mapping in profile files is
        actually advertised by `slm.portal` (prevents broken resolver routes).
    - [x] installer supports profile rollout (`SLM_PORTAL_PROFILE`)
      and no longer force-overwrites `portals.conf` unless
      `SLM_PORTAL_SET_DEFAULT=1`.
      - [x] installer now supports `SLM_PORTAL_SKIP_SYSTEMCTL=1`
        to allow deterministic CI/integration tests without user systemd session.
    - [x] installer integration test added:
      `portal_install_script_integration_test`
      validates profile copy, non-overwrite behavior, and forced overwrite path.
      - [x] dry-run output contract guard added:
        validates deterministic installer status lines
        (`portal profile`, `set default`, `skip systemctl`, and skip-systemctl notice)
        under `SLM_PORTAL_SKIP_SYSTEMCTL=1`.
      - [x] invalid profile guard added:
        installer exits non-zero with actionable profile help text.
      - [x] legacy profile guard added:
        verifies `legacy` profile remains supported and maps to fallback-first template.
      - [x] profile normalization + precedence guard added:
        verifies profile argument is case-insensitive and CLI profile argument
        overrides `SLM_PORTAL_PROFILE` environment value.
      - [x] env-only profile selection guard added:
        verifies `SLM_PORTAL_PROFILE` is used when CLI profile argument is omitted.
      - [x] env-only profile case-insensitive guard added:
        verifies mixed-case `SLM_PORTAL_PROFILE` resolves correctly.
      - [x] production alias guard added:
        verifies `prod` alias is accepted and maps to production profile behavior.
      - [x] env-only production alias guard added:
        verifies `SLM_PORTAL_PROFILE=prod` works when CLI profile arg is omitted.
      - [x] precedence guard for invalid CLI whitespace profile added:
        verifies CLI whitespace profile still overrides env profile and fails fast.
      - [x] installer output path contract guard added:
        verifies dry-run output reports canonical installed paths under custom
        `XDG_CONFIG_HOME` / `XDG_DATA_HOME`.
      - [x] production no-overwrite + log consistency guard added:
        verifies with `SLM_PORTAL_SET_DEFAULT=0` that existing `portals.conf`
        is preserved while `slm-portals.conf` follows production profile.
      - [x] idempotency guard added:
        verifies repeated install with same profile does not drift generated
        artifacts (`portals.conf`, `slm-portals.conf`, `slm.portal`, dbus service,
        user unit).
      - [x] empty/whitespace profile guard added:
        empty profile argument falls back to `mvp`, while whitespace-only input is rejected
        with actionable error.
      - [x] custom binary path substitution guard added:
        verifies custom `BIN_PATH` is substituted into both
        `slm-portald.service` (`ExecStart=`) and DBus service (`Exec=`).
    - [x] add shell smoke gate `portal_profile_smoke_test`
      (profile matrix mvp/beta/production + mapping vs `slm.portal` coverage)
      for quick packaging/interop regression checks.
    - [x] add installed-artifacts smoke gate
      `portal_installed_artifacts_smoke_test`
      to validate user-level install outputs (`slm.portal`, dbus service,
      systemd user unit, `portals.conf`, `slm-portals.conf`) and overwrite policy.
    - [x] add strict profile snapshot contract test:
      `portal_profile_snapshot_test`
      validates exact `[preferred]` mappings for
      `portals.mvp.conf`, `portals.beta.conf`, `portals.production.conf`.
    - [x] add strict `slm.portal` snapshot contract test:
      `portal_slm_portal_snapshot_test`
      validates exact `DBusName`, `UseIn`, and advertised `Interfaces=` set.
    - [x] add strict DBus service template snapshot contract test:
      `portal_dbus_service_template_snapshot_test`
      validates `[D-BUS Service]` `Name` and `Exec` placeholder contract.
    - [x] add strict systemd unit template snapshot contract test:
      `portal_systemd_unit_template_snapshot_test`
      validates `scripts/systemd/slm-portald.service` contract
      (`Description`, `ExecStart` placeholder, restart policy, `WantedBy`).
    - [x] CI workflow now runs portal profile matrix
      (`mvp`, `beta`, `production`) using
      `portal-profile-smoke-test.sh <profile>`.
    - [x] artifact smoke now supports per-profile validation and is executed
      in CI profile matrix via
      `portal-installed-artifacts-smoke-test.sh <profile>`.
    - [x] add nightly portal interop CI job:
      `portal-interop-nightly`
      runs portal DBus/adapter subset + config/smoke subset
      on `schedule`/`workflow_dispatch`.

## Next (System Settings UI roadmap)
- [x] Phase-A UX polish (high impact, low risk):
  - [x] Tambah transisi UI konsisten (fade/slide) untuk:
    - sidebar selection -> panel switch,
    - search result list enter/exit,
    - deep-link jump highlight.
    - implemented:
      - animasi highlight/color di `Sidebar.qml`,
      - cross-fade + slight slide antar mode panel di `ContentPanel.qml`,
      - pulse highlight target setting di `SettingCard.qml`.
  - [x] Standarisasi design tokens komponen settings:
    - card/header/section spacing,
    - toggle/slider/dropdown visual consistency,
    - typography scale + HiDPI parity.
    - baseline token harmonization diterapkan pada komponen inti:
      `SettingCard`, `SettingGroup`, `SettingToggle` (animasi + visual consistency).
  - [x] Tambah smooth focus target saat deep link/search open setting
    (scroll-to + temporary highlight pulse).
    - implemented via highlight pulse transition pada `SettingCard`.
- [x] Phase-B Binding architecture (core modularity):
  - [x] Implement `BindingResolver` untuk parse `backend_binding` declarative:
    - `gsettings:<schema>/<key>`
    - `dbus:<service>/<path>/<iface>/<prop|method>`
    - `settings:<namespace>/<key>`
  - [x] Implement `BindingFactory` + instance cache agar module QML tidak wiring manual.
    - `SettingsApp.createBinding(...)` + `createBindingFor(moduleId, settingId, defaultValue)` ditambahkan.
    - module contoh dimigrasikan: `appearance` (`theme`, `accent`) dan `network` (`wifi`, `ethernet`).
  - [x] Tambah parser contract test awal (`settings_bindingresolver_test`).
  - [x] Tambah async error/result contract standar untuk read/write/subscribe.
    - `SettingBinding` kini expose `lastError` + signal `operationFinished(operation, ok, message)`.
    - binding concrete (`dbus/gsettings/local/mock/network`) distandarkan ke
      lifecycle operasi (`beginOperation/endOperation`) dan error reporting seragam.
    - `DBusBinding` write path dipindahkan ke async call (`QDBusPendingCallWatcher`).
    - test kontrak ditambahkan: `settings_binding_contract_test`.
- [x] Phase-C Platform integration:
  - [x] Integrasi policy/polkit untuk aksi privileged per-setting (network/power/default-apps tertentu).
    - `ModuleLoader` parse `privileged_action` -> `privilegedAction`.
    - `SettingsApp.settingPolicy(moduleId, settingId)` expose policy map:
      `requiresAuthorization`, `privilegedAction`, `policySource`.
    - fallback default action map untuk setting sensitif awal:
      `network/wifi|ethernet`, `power/*`, `applications/*`.
    - `SettingsApp.requestSettingAuthorization(...)` wired ke `SettingsPolkitBridge`
      (`pkcheck --allow-user-interaction`) + dev override
      `SLM_SETTINGS_AUTH_ALLOW_ALL=1`.
    - tambah decision flow awal untuk auth request:
      `allow-once`, `allow-always`, `deny`, `deny-always`
      via `SettingsApp.requestSettingAuthorizationWithDecision(...)`.
    - grant cache per-setting (allow/deny always) disimpan di
      `${AppConfigLocation}/settings-permissions.ini`.
    - API grant management ditambahkan:
      - `settingGrantState(moduleId, settingId)`,
      - `clearSettingGrant(moduleId, settingId)`,
      - `clearAllSettingGrants()`.
    - UI `AuthorizationHint` mendukung menu keputusan + reset grant tersimpan.
    - tambah module Settings baru: `permissions`
      untuk inspeksi grant tersimpan + reset per-item / reset semua.
    - module `permissions` ditingkatkan:
      - filter/search grant lokal,
      - aksi `Open Setting` (deep-link ke module/setting terkait),
      - auto refresh via signal backend `grantsChanged`.
    - metadata grant kini menyimpan `updatedAt` (UTC epoch ms) dan ditampilkan di UI;
      daftar grant disortir berdasarkan waktu perubahan terbaru.
    - test ditambahkan: `settingsapp_policy_test`.
    - UI gate awal di module pages:
      - `NetworkPage.qml` (`wifi`) lock-until-authorized,
      - `PowerPage.qml` (`power-mode`, `suspend`) lock-until-authorized,
      - `ApplicationsPage.qml` (`default-browser`, `default-mail`) lock-until-authorized.
      - menampilkan reason saat deny/error dari bridge authorization.
    - Komponen reusable `AuthorizationHint.qml` ditambahkan agar permission surface
      konsisten lintas module settings.
  - [x] Pertahankan provider global search internal (`org.slm.Settings.Search.v1`) sebagai primary;
    tambah adapter opsional Tracker/KRunner sebagai extension package.
    - provider DBus internal aktif di `src/apps/settings/globalsearchprovider.*`
      dan dikonsumsi `GlobalSearchService::querySettings(...)`.
    - kontrak test tersedia di `globalsearchservice_dbus_test::querySettingsProvider_contract`
      (dapat skip bila bus name sudah dimiliki proses lain di sesi aktif).
  - [x] Tambah telemetry performa:
    - [x] module open latency (`SettingsApp.lastModuleOpenLatencyMs`)
    - [x] search latency + result count (`SearchEngine.lastSearchLatencyMs`, `lastSearchResultCount`)
    - [x] deep-link navigation latency (`SettingsApp.lastDeepLinkLatencyMs`)
    - [x] debug surface di UI Settings (`Qml/apps/settings/Main.qml`) untuk observability runtime.
  - [x] Network settings backend: kurangi hardcode UI dan gunakan jalur backend NetworkManager.
    - hapus daftar SSID simulasi pada `NetworkPage.qml`.
    - status/toggle Wi-Fi memakai binding backend (`network/wifi`, `network/ethernet`).
    - state mapping `NetworkManagerBinding` dirapikan ke konstanta state resmi NM.
  - [x] FileManager archive backend: migrasi primary path ke `file-roller`.
    - extract/compress kini memprioritaskan `file-roller` CLI.
    - fallback kompatibilitas ke `bsdtar` tetap dipertahankan jika `file-roller` tidak tersedia.
  - [x] UI per kategori settings: rapikan modul kategori yang masih statis/hardcoded.
    - update modul `bluetooth`, `display`, `sound`, `keyboard`, `mouse`
      agar memakai binding backend + highlight target setting.
- [x] Phase-D Hardening & compatibility:
  - [x] Contract tests metadata module + binding resolver malformed input cases.
    - resolver test diperluas: canonical DBus method parse, unsupported scheme, invalid gsettings spec.
    - test baru `settings_moduleloader_contract_test` untuk validasi metadata loader
      (skip invalid module/setting, parse `backend_binding`, `privileged_action`, `deepLink`, `anchor`).
  - [x] UI smoke tests:
    - sidebar navigation,
    - in-app search,
    - deep-link + breadcrumb + back stack.
    - test baru `settings_ui_smoke_test` memuat `Qml/apps/settings/Main.qml`
      dan semua halaman modul kategori utama.
  - [x] Missing-backend graceful fallback tests (DBus/GSettings unavailable).
    - test baru `settings_backend_fallback_test` untuk:
      - schema GSettings tidak tersedia -> gagal aman tanpa crash,
      - DBus binding invalid -> error kontrak terstandar,
      - unsupported binding scheme -> fallback `MockBinding`.
    - hardening runtime:
      - `GSettingsBinding` sekarang validasi schema sebelum `g_settings_new()`
        untuk mencegah fatal crash saat schema belum terpasang.

## Next (Workspace rebrand + Mission Control hybrid)
- [x] Phase-0 Audit & Reuse Mapping (wajib sebelum coding):
  - [x] Audit modul window model yang sudah ada (`WorkspaceManager`, `SpacesManager`, `WindowingBackendManager`, `KWinWaylandStateModel`).
  - [x] Audit focus/focused-window flow yang sudah ada dan tentukan source-of-truth tunggal untuk window aktif.
  - [x] Audit model Dock app grouping + aktivasi window lintas workspace.
  - [x] Audit scene filtering/render path yang sudah ada untuk memastikan normal mode hanya render active workspace.
  - [x] Audit util animasi yang sudah ada (`src/core/motion/*`) untuk enter/exit workspace overview + switch anim.
  - [x] Audit registrasi shortcut keyboard existing (`Super+S`, `Super+Arrow`, `Esc`) dan gap integrasi.
  - [x] Audit input/gesture routing existing untuk drag thumbnail -> workspace strip target.
  - [x] Tulis integration note: daftar komponen reused vs baru (file-level, owner, dependency).

- [x] Phase-1 Rebranding API/Domain (`overview` -> `workspace`):
  - [x] Tambah alias API non-breaking di backend DBus (`ShowWorkspace`, `HideWorkspace`, `ToggleWorkspace`) sambil pertahankan method lama untuk kompatibilitas.
  - [x] Rebranding signal/property internal dan label QML bertahap ke terminologi `workspace`.
  - [x] Update capability names dan telemetry labels terkait overview -> workspace (backward compatible mapping).
  - [x] Tambah changelog dan deprecation note untuk method overview lama.
  - [x] Rename `OverviewPreviewManager` -> `WorkspacePreviewManager` dengan alias context/property legacy non-breaking.
  - [x] Canonicalize key shortcut ke `windowing.bindWorkspace` dengan alias `windowing.bindOverview` + auto migration.
  - [x] Rename cache path preview ke `slm-desktop-workspace-previews` dengan fallback read dari path legacy.

- [x] Phase-2 Backend Workspace Foundations:
  - [ ] Finalisasi model `Workspace` dinamis dengan invariant:
    - [x] setiap window tepat 1 workspace,
    - [x] selalu ada trailing empty workspace,
    - [x] trailing terisi => append empty baru,
    - [x] non-last kosong => auto-remove,
    - [x] active workspace selalu valid.
  - [x] Implement/extend `WindowWorkspaceBinding` sebagai source-of-truth mapping `windowId -> workspaceId`.
  - [ ] Extend `WorkspaceManager` API:
    - [x] switch workspace,
    - [x] move focused window ke workspace kiri/kanan,
    - [x] move arbitrary window ke target workspace (untuk drag-drop thumbnail).
  - [x] Extend `MultitaskingController` (atau padanan existing) untuk state machine:
    - [x] normal mode,
    - [x] workspace overview mode (tetap terbuka saat pindah workspace via strip).
  - [x] Tambah `WorkspaceStripModel` backend ringan (state/occupancy/count/active/placeholder) tanpa live preview berat.

- [x] Phase-3 Scene Filtering & Thumbnail Layout:
  - [x] Terapkan filtering render normal mode: hanya window active workspace.
  - [x] Overview mode:
    - [x] pusat menampilkan thumbnail window dari active workspace saja,
    - [x] strip workspace di top sebagai navigator.
  - [x] Implement `WindowThumbnailLayoutEngine`:
    - [x] layout grid adaptif,
    - [x] cached preview path jika live thumbnail mahal,
    - [x] geometry target untuk anim enter/exit.
  - [x] Tambah drag target highlight pada tab workspace strip.

- [x] Phase-4 QML Multitasking UI:
  - [x] Implement top workspace strip component (lightweight occupancy/state visuals).
  - [x] Implement panel thumbnail aktif workspace (click-to-focus/present).
  - [x] Drag & drop thumbnail:
    - [x] drop ke workspace existing => move window,
    - [x] drop ke trailing placeholder => create workspace + move window.
  - [x] Pertahankan dock visible di bawah saat overview aktif.
  - [x] Klik workspace strip switch workspace tapi overview tetap open.

- [x] Phase-5 Dock Workspace Integration:
  - [x] `DockWorkspaceIntegration`: klik app di dock
    - [x] jika window ada di workspace aktif -> present normal,
    - [x] jika window ada di workspace lain -> switch workspace lalu present.
  - [x] Pastikan indikator dock workspace-aware (active/running/focused) konsisten lintas workspace.
  - [x] Pastikan perilaku dock konsisten saat overview terbuka.

- [x] Phase-6 Keyboard/Input Integration:
  - [x] `Super+S` toggle workspace overview.
  - [x] `Super+Left/Right` switch workspace.
  - [x] `Ctrl+Super+Left/Right` move focused window antar workspace.
  - [x] `Esc` keluar overview.
  - [x] Tambah conflict-check shortcut existing + fallback policy.

- [x] Phase-7 Animation Integration (reuse motion infra):
  - [x] Enter/exit workspace overview animate window geometry ke/dari thumbnail layout.
  - [x] Workspace strip fade/slide in-out.
  - [x] Workspace switching saat overview tetap open dengan transisi halus.
  - [x] Drag hover/drop highlight animation untuk strip tabs.
  - [x] Hard limit performa: tanpa shader berat/live preview mahal sebagai default.

- [x] Phase-8 Tests, hardening, and rollout:
  - [x] Backend unit tests untuk invariant workspace dinamis + mapping window/workspace.
  - [x] DBus contract tests for alias rebranding + behavior compatibility.
  - [x] QML smoke tests untuk workspace strip + thumbnail drag/drop.
  - [x] End-to-end smoke:
    - [x] switch workspace while overview open,
    - [x] move window by keyboard,
    - [x] move window by drag to strip,
    - [x] dock click jump lintas workspace.
  - [x] Regression checklist update (`docs/REGRESSION_CHECKLIST.md`).

## Next (SLM Action Registry)
- [x] Add core parser for SLM desktop action extensions (`X-SLM-*`) from `.desktop` entries.
- [x] Add ACL parser + AST evaluator for action conditions (`AND/OR/NOT`, compare, `LIKE/MATCH/IN`, `any/all`).
- [x] Add discovery scanner + cache for:
  - `/usr/share/applications`
  - `~/.local/share/applications`
- [x] Add action filtering pipeline:
  - capability
  - target
  - ACL evaluation
  - stable sorting by `X-SLM-Priority`.
- [x] Add optional DBus registry service:
  - service: `org.freedesktop.SLMCapabilities`
  - path: `/org/freedesktop/SLMCapabilities`
  - methods: `ListActions`, `InvokeAction`, `Reload`.
- [x] Integrate invocation path to `AppExecutionGate` (replace direct detached process execution in registry).
- [x] Add unit tests for ACL parser/evaluator edge-cases and malformed expressions.
- [x] Add live file watcher (incremental refresh) on application dirs to avoid full rescans.
- [x] Wire action registry output into filemanager context menu generation path.
- [x] Extend internal model for unified capability metadata:
  - `ShareCapability`
  - `QuickActionCapability`
  - `DragDropCapability`
  - `SearchActionCapability`
  - `ActionContext` + context-based registry methods.
- [x] Parse capability-specific `X-SLM-*` keys from `[Desktop Action ...]`:
  - share targets/modes
  - quickaction scopes/args/visibility
  - dragdrop accepts/operation/destination/behavior
  - searchaction scopes/intent.
- [x] Extend DBus capability service API:
  - `ListActionsWithContext(capability, context)`
  - `InvokeActionWithContext(action_id, context)`

## Next (Permission Foundation)
- [x] Add permission foundation module set under `src/core/permissions/`:
  - `CallerIdentity`
  - `Capability`
  - `AccessContext`
  - `PolicyDecision`
  - `PolicyEngine`
  - `PermissionStore`
  - `PermissionBroker`
  - `DBusSecurityGuard`
  - `AuditLogger`
  - `TrustResolver`
  - `SensitiveContentPolicy` (optional policy layer).
- [x] Add SQLite schema and persistence primitives for:
  - `permissions`
  - `app_registry`
  - `audit_log`.
- [x] Add architecture documentation:
  - `docs/architecture/PERMISSION_FOUNDATION.md`.
- [ ] Integrate `DBusSecurityGuard` into live service endpoints (`desktopd` first):
  - clipboard/search/share/file-actions/accounts paths.
- [ ] Add consent mediation contract for `AskUser` decisions (portal-ready hook in broker).
- [ ] Add tests:
  - trust resolver classification,
  - policy defaults per trust level,
  - gesture-gated capability checks,
  - permission persistence/audit writes.
- [ ] Clipboard hardening: integrate low-level native Wayland `wl-data-control` capture path in `slm-clipboardd` (keep Qt clipboard path as fallback).

## Next (Animation Framework)
- [x] Add motion architecture document for SLM Desktop (scheduler/engine/solver/gesture/interruptibility/compositor split).
- [x] Add core C++ motion skeleton module under `src/core/motion/`:
  - `AnimationScheduler`
  - `AnimationEngine`
  - `MotionValue`
  - `SpringSolver`
  - `PhysicsIntegrator`
  - `GestureBinding`
  - `MotionController`
- [x] Expose `MotionController` to QML root context for declarative shell usage.
- [x] Add baseline unit test target `slmmotioncore_test` for spring convergence + gesture settle decision.
- [x] Integrate MotionController into launchpad reveal transition (`Main.qml`) with interruptible spring settle.
- [x] Integrate MotionController into workspace swipe settle path in `DesktopScene.qml`.
- [x] Integrate MotionController into tothespot reveal transition (`Main.qml`) with interruptible spring settle.
- [x] Add motion debug overlay (toggle via `motion.debugOverlay`) with channel/value/velocity/target + frame `dt`.
- [x] Extend motion debug overlay:
  - slow-motion multiplier.
  - dropped-frame counter.
- [x] Add reduced-motion policy hook via `UIPreferences`.
  - `ResolveDefaultAction(capability, context)`
  - `SearchActions(query, context)`.
- [x] Add default resolver logic:
  - DragDrop: prefer non-`ask` operation, fallback first candidate.
- [x] Add SearchAction ranking model (name/keywords/intent/priority/scope).
- [x] Add capability regression tests:
  - OpenWith unsupported behavior
  - SearchAction ranking ordering.
- [x] Integrate `Share` capability into unified share sheet UI builder.
- [x] Integrate `QuickAction` capability into launcher/dock/appmenu surfaces.
- [x] Keep FileManager Open With on native pipeline (SLM OpenWith disabled).
- [x] Integrate `DragDrop` capability into DnD chooser/default-op flow.
- [x] Integrate `SearchAction` capability into Tothespot global result provider.

## Next (SLM Action Framework modularization)
- [x] Add module skeleton package `src/core/actions/framework/`:
  - GIO scanner module
  - desktop entry parser module
  - metadata parser module
  - cache layer
  - registry index layer
  - capability resolver layer
  - capability builders/resolvers
  - action invoker
  - top-level framework orchestrator.
- [x] Add architecture doc for modular flow:
  - `docs/architecture/SLM_ACTION_FRAMEWORK_MODULES.md`.
- [x] Switch `SlmCapabilityService` runtime dependency from direct `ActionRegistry` to `SlmActionFramework`.
- [x] Route FileManager/Tothespot capability queries through framework facade methods.
