# FileManager Module (Internal)

This directory contains FileManager implementation code that is still part of the main SLM Desktop repository.

Current scope:
- API core:
  - `filemanagerapi.cpp`
  - `include/filemanagerapi.h`
- API feature modules:
  - `filemanagerapi_open.cpp`
  - `filemanagerapi_archive.cpp`
  - `filemanagerapi_favorites.cpp`
  - `filemanagerapi_watch.cpp`
  - `filemanagerapi_thumbnail.cpp`
  - `filemanagerapi_batch.cpp`
  - `filemanagerapi_batch_control.cpp`
  - `filemanagerapi_recent.cpp`
  - `filemanagerapi_openwith.cpp`
  - `filemanagerapi_storage.cpp`
  - `filemanagerapi_fs.cpp`
  - `filemanagerapi_listing.cpp`
  - `filemanagerapi_io.cpp`
  - `filemanagerapi_metadata.cpp`
  - `filemanagerapi_portal.cpp`
- API daemon/file-ops modules:
  - `filemanagerapi_daemon_common.h`
  - `filemanagerapi_daemon_state.cpp`
  - `filemanagerapi_daemon_transfer.cpp`
  - `filemanagerapi_daemon_remove.cpp`
  - `filemanagerapi_daemon_emptytrash.cpp`
  - `filemanagerapi_daemon_control.cpp`
  - `filemanagerapi_daemon_recovery_state.cpp`
  - `filemanagerapi_daemon_recovery_signals.cpp`
- Models/factory:
  - `filemanagermodel.cpp`
  - `filemanagermodelfactory.cpp`
  - `include/filemanagermodel.h`
  - `include/filemanagermodelfactory.h`

Notes:
- Canonical public headers are now under `include/`.
- Root-level headers are temporary compatibility shims for migration speed.
- Deprecated shim `.cpp` files have been removed after module split consolidation.

Boundary and ownership:
- `filemanagerapi_*` is the UI-facing orchestration layer for FileManager.
- Sync/local operations:
  - `filemanagerapi_fs.cpp`
  - `filemanagerapi_io.cpp`
  - `filemanagerapi_metadata.cpp`
  - `filemanagerapi_listing.cpp`
  - `filemanagerapi_watch.cpp`
- Async daemon-backed file operations:
  - `filemanagerapi_daemon_transfer.cpp`
  - `filemanagerapi_daemon_remove.cpp`
  - `filemanagerapi_daemon_emptytrash.cpp`
  - `filemanagerapi_daemon_control.cpp`
  - `filemanagerapi_daemon_recovery_state.cpp`
  - `filemanagerapi_daemon_recovery_signals.cpp`
  - shared helpers: `filemanagerapi_daemon_common.h`, `filemanagerapi_daemon_state.cpp`
- UI contract (Qt signals) remains centralized in `include/filemanagerapi.h`:
  - batch state/progress (`batchOperationStateChanged`, `fileOperationProgress`)
  - completion/error signals (`fileBatchOperationFinished`, `batchOperationTaskError`)
  - async response signals (`openActionFinished`, `openWithApplicationsReady`, `portalFileChooserFinished`)

Migration checklist (to separate repository):
- Stage 1: Freeze interface
  - Keep `include/filemanagerapi.h`, `include/filemanagermodel.h`, and `include/filemanagermodelfactory.h` as compatibility boundary.
  - Avoid new cross-module includes from unrelated desktop components.
- Stage 2: Isolate build inputs
  - Move all `src/apps/filemanager/*` entries into a dedicated CMake list/group.
  - Ensure tests compile by linking only the isolated FileManager source group.
- Stage 3: Externalize dependencies
  - Replace direct references to desktop internals with explicit service interfaces (DBus or narrow adapter classes).
  - Keep daemon file-ops calls stable via `org.slm.Desktop.FileOperations`.
- Stage 4: Create standalone package
  - Build `libslmfilemanager` (or equivalent target) with the same public headers.
  - Export a minimal API surface and install rules.
- Stage 5: Integrate back as dependency
  - Main `Slm_Desktop` consumes the standalone package instead of local sources.
  - Keep root-level shim headers temporarily for source compatibility.
- Stage 6: Remove legacy shims
  - Drop temporary compatibility shims after all callers migrate.
  - Keep regression tests for DBus contracts and UI dialogs green before final cleanup.
