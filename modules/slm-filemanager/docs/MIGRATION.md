# Migration notes (staging -> external repo)

## Target
Split FileManager into external repository `slm-filemanager` with:
- `SlmFileManager::Core`
- `SlmFileManager::Qml` (later phase)

## Transitional status
- C++ code already canonical under `src/apps/filemanager`.
- QML already canonical under `Qml/apps/filemanager`.
- Standalone staging CMake package available in this folder.

## Next technical steps
1. Create history-filtered extraction branch for:
   - `src/apps/filemanager/*`
   - `Qml/apps/filemanager/*`
   - `tests/filemanager/*`
2. Move source ownership into standalone repo paths (`src/core`, `src/qml`, `tests`).
3. Publish CMake package config for `SlmFileManager::Core`.
4. Reintegrate into shell via submodule/subtree and package target.

## CI handoff readiness
- Standalone repo CI template is prepared in staging:
  - `.github/workflows/ci.yml`
  - `scripts/ci-smoke.sh`
- After extraction, copy these files to repository root and enable as default CI.
