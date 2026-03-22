# FileManager Modularization (In-Repo First)

## Decision
Continue **in-repo modularization first**, but with an explicit path toward a standalone `slm-filemanager` repository.

## Why phased split (not hard cut instantly)
- Integration is still tight with shell runtime (Theme, portal chooser, global menu, daemon DBus contracts).
- A hard cut without compatibility shims increases runtime breakage risk.
- Current phase focuses on extracting clean boundaries and stable contracts first.

## Target internal boundary
- C++ implementation namespace/path:
  - `src/apps/filemanager/`
- QML implementation path:
  - `Qml/apps/filemanager/`
- Contracts consumed by shell:
  - `FileManagerApi`
  - `FileManagerModel`
  - `FileManagerModelFactory`
  - file operations domain under `src/apps/filemanager/ops/`

## Current extraction status
1. C++ FileManager implementation moved under `src/apps/filemanager/` (done).
2. QML FileManager surface moved to `Qml/apps/filemanager/` (done).
3. File operations domain moved to `src/apps/filemanager/ops/` (done).
4. Compatibility shims kept under `src/filemanager/ops/*.h` for transition (done).
5. Standalone package target + isolated filemanager test suite structure in place:
   - `slm-filemanager-core`
   - `filemanager_test_suite`
   - gate script: `scripts/check-filemanager-extraction-gate.sh`
6. Standalone-repo staging skeleton added under `modules/slm-filemanager/` (done):
   - standalone staging build script:
     - `scripts/build-filemanager-standalone-staging.sh`
   - canonical alias target:
     - `SlmFileManager::Core`
   - split prep helper:
     - `scripts/prepare-filemanager-history-split.sh`
7. Shell bridge switched to link-based integration (done):
   - `appDesktop_Shell` links `SlmFileManager::Core`
   - direct FileManager source embedding removed from app target
   - optional external package switch:
     - `SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE`

## Practical migration phases
1. Move `.cpp` implementation files first (done), keep headers stable to avoid breakage.
2. Move public headers to `src/apps/filemanager/include/` with temporary compatibility shims (done).
3. Split large service classes (`FileManagerApi`) into smaller composition units (done: `recent`, `openwith`, `storage`, `ops`).
4. Group tests into `tests/filemanager/` and keep smoke + contract tests mandatory in CI (done).
5. Produce standalone build profile (`SlmFileManager` targets) and validate independent CI (build profile done, independent CI pending).
6. After API/build independence is proven, perform repo split cutover.

## Split-ready checklist
- FileManager can build and test standalone from a single CMake target group.
- No direct QML dependency on shell internals except typed public interfaces.
- DBus/API contracts are versioned and documented.
- Release cadence requirement differs from the main shell repository.
