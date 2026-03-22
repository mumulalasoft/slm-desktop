# File Manager QML Architecture

This folder contains the File Manager UI split into focused QML components and small JS helpers.

## Component Map

- `FileManagerWindow.qml`
  - Main container/composer.
  - Wires all child components and routes events.
  - Keeps high-level state only (selection, rename visibility, DnD state, filters).

- `FileManagerToolbar.qml`
  - Top header controls (close, nav actions, view mode, search).

- `FileManagerSidebar.qml`
  - Left sidebar sections (favorites, recent, favorites library).

- `FileManagerContentView.qml`
  - Main content area (grid/list delegates, selection, activation, drag gesture source).

- `FileManagerStatusBar.qml`
  - Bottom status strip.

- `FileManagerContextMenus.qml`
  - Entry/background context menus.

- `FileManagerRenameSheet.qml`
  - Rename modal sheet.

- `FileManagerDragGhost.qml`
  - Drag-and-drop floating preview.

- `FileManagerRecentItem.qml`
  - Delegate for recent entries in sidebar.

- `FileManagerFavoriteItem.qml`
  - Delegate for favorite entries in sidebar.

## JS Helpers

- `FileManagerKeys.js`
  - Maps key events to semantic actions used by `FileManagerWindow`.

- `FileManagerDnd.js`
  - Handles DnD state transitions:
  - start (`begin`), move (`update`), release (`finish`).

- `FileManagerOps.js`
  - File operation logic:
  - copy/cut tracking, paste, trash move, rename apply.

- `FileManagerUtils.js`
  - Shared pure utility functions:
  - filename helpers, image extension checks, file URL conversion,
  - recent-time formatting, bucket/header helpers, target-path naming.

- `FileManagerLibrary.js`
  - Library/favorites state logic:
  - refresh recent/favorite models,
  - selected favorite sync/toggle,
  - open library path behavior.

## Data Flow (Short)

1. UI events come from component delegates (`Sidebar`, `ContentView`, `Toolbar`).
2. `FileManagerWindow` receives events and dispatches to helper modules.
3. Helpers call runtime services (`FileManagerApi`, `AppCommandRouter`) through window wrappers.
4. Models (`fileModel`, `recentModel`, `favoriteModel`) are updated.
5. UI re-renders from model/state changes.

## Data Flow (ASCII)

```text
FileManagerToolbar / FileManagerSidebar / FileManagerContentView
                           |
                           v
                   FileManagerWindow.qml
                           |
        +------------------+------------------+------------------+
        |                  |                  |                  |
        v                  v                  v                  v
 FileManagerKeys.js   FileManagerDnd.js  FileManagerOps.js  FileManagerLibrary.js
        \                  |                  |                  /
         \                 |                  |                 /
          +----------------+--------+---------+----------------+
                                   |
                                   v
                            FileManagerUtils.js
                                   |
                                   v
                    FileManagerApi / AppCommandRouter
                                   |
                                   v
                      fileModel + recentModel + favoriteModel
                                   |
                                   v
                             UI refresh/update
```

## Editing Guidance

- Add new visual behavior in a focused QML component first.
- Add new non-visual logic in JS helpers, keep `FileManagerWindow` as orchestration.
- Prefer passing explicit parameters to helpers instead of relying on global objects.
- Keep helpers side-effect scoped to their responsibility.
