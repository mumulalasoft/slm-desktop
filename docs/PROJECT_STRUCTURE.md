# Project Structure

## Root

- `main.cpp`: bootstrap Qt app, register context properties and QML module.
- `CMakeLists.txt`: build definition, grouped by C++ domain + QML/resources.
- `README.md`: quick-start, architecture summary, build instructions.
- `docs/architecture/DIAGRAM_INDEX.md`: indeks diagram arsitektur utama.
- `docs/ARCHITECTURE.md`: execution flow diagram including `AppExecutionGate` one-gate pipeline.
- `docs/architecture/SLM_ACTION_FRAMEWORK_MODULES.md`: modular scanner/parser/registry/resolver diagram.

## C++ runtime

- `appmodel.*`: launchpad app listing and filtering.
- `shortcutmodel.*`: desktop shortcut data source (`~/Desktop`) and operations.
- `dockmodel.*`: dock entries, running-app detection, transient launch handling.
- `appruntimeregistry.*`: short-lifecycle runtime hints for launch/run transitions.
- `appexecutiongate.*`: one-gate app execution API used by QML entry points.
- `appcommandrouter.*`: action router stub for future FileManager/Terminal/module integrations.
- `networkmanager.*`: network info provider.
- `uipreferences.*`: persisted UI/debug preferences.
- `cursorcontroller.*`: cursor busy state helper.
- `themeiconprovider.*`: themed icon resolver (`image://themeicon/...`).

## Daemons and DBus services

- `src/daemon/desktopd/desktopd_main.cpp` + `desktopdaemonservice.*`: main session daemon (`org.slm.Desktop.*` facade).
- `src/daemon/fileopsd/fileopsd_main.cpp` + `fileoperationsservice.*` + `fileoperationsmanager.*`: file operation daemon (`Copy/Move/Delete/Trash` + progress).
- `src/daemon/devicesd/devicesd_main.cpp` + `devicesservice.*` + `devicesmanager.*`: device daemon (`Mount/Eject/Unlock/Format`).
- `src/daemon/portald/portald_main.cpp` + `portalservice.*` + `portalmanager.*`: portal service (`Screenshot/ScreenCast/FileChooser/OpenURI/...`).
- `*compatservice.*`: backward-compat DBus names while migrating APIs.

## QML

- `Main.qml`: main shell window.
- `Qml/Theme.qml`: singleton theme tokens and dark/light behavior.
- `Qml/DesktopScene.qml`: orchestration layer for shell, dock, launchpad, overlays.
- `Qml/components/`
  - `FileManagerWindow.qml`: currently owns FileManager context-menu behavior (single source for active runtime path).
  - `Shell.qml`: desktop surface, shortcuts, drag/drop, selection model.
  - `shell/`
    - `ShellShortcutTile.qml`: shortcut tile delegate + pointer interactions.
    - `ShellSelectionRect.qml`: marquee rectangle for drag selection.
    - `ShellDragGhost.qml`: drag visual while reordering shortcuts.
  - `dock/`
    - `DockAppDelegate.qml`: dock app tile delegate + context interactions.
    - `DockMath.js`: shared dock geometry/math helpers.
    - `DockReorderController.qml`: reorder drag-state controller.
    - `DockReorderMarker.qml`: reorder indicator layer.
    - `DockDropPulse.qml`: drop feedback pulse layer.
  - `Dock.qml` + `DockItem.qml`: dock UI and interactions.
  - `Launchpad.qml`: launcher grid and search.
  - `TopBar.qml`, `NetworkIndicator.qml`, `DesktopBackground.qml`, `StyleGallery.qml`.

## Tests and tools

- `tests/`: unit/integration DBus tests and contracts.
- `src/cli/`: command-line utilities (`indicatorctl`, `windowingctl`, `workspacectl`, `fileopctl`, `devicectl`).
- `apps/tools/`: modularized tool targets (incremental migration target).
- `scripts/systemd/`: user service install/update scripts.
- `scripts/`: run, dev, and integration helper scripts.

## Styling and assets

- `Style/`: custom Qt Quick Controls style set.
- `icons/`, `images/`: bundled static resources.

## Maintenance rules (practical)

1. Single source of truth:
- Keep UI behavior in one place. Avoid duplicate context-menu implementations across multiple QML files.
- Shared visuals must live in `Style/` and be consumed by components, not restyled ad-hoc.

2. DBus boundaries:
- UI never performs privileged or long-running work directly; route via daemon/service class.
- Keep service contracts in one header pair per service (`*service.h/.cpp` + constants).

3. File size and ownership:
- `Main.qml` should stay orchestration-only; move heavy feature logic to `Qml/components/*` + C++ managers.
- Large QML files must be split by feature folder (`filemanager/`, `dock/`, `shell/`, `topbar/`).

4. Naming conventions:
- `*manager.*` = runtime/domain logic.
- `*service.*` = DBus endpoint.
- `*ctl.*` = CLI admin/testing entrypoint.
- `*compatservice.*` = temporary compatibility shim.

5. Cleanup policy:
- Remove dead properties/functions after feature migration.
- If duplicate implementation still exists for compatibility, mark with `TODO(slmmigrate):` and target file/phase.

## Suggested future split (next)

- Modular target folders have been prepared:
  - `modules/slm-desktop-core/`
  - `modules/slm-desktop-style/`
  - `apps/slm-desktop/`
  - `apps/tools/uipreference/`
  - `apps/tools/indicatorctl/`
- Detailed migration contract: `docs/MODULAR_STRUCTURE.md`.
