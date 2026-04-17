# slm-filemanager (staging standalone package)

This directory is a **pre-split staging package** for extracting FileManager into
its own repository (`slm-filemanager`).

Current goal:
- keep monorepo build stable,
- prove a standalone CMake boundary,
- prepare clean handoff to external repo (subtree/submodule + package targets).

## Current structure
- `src/core/` : FileManager core API/domain placeholder boundary.
- `src/qml/` : QML surface placeholder boundary.
- `src/daemon/` : daemon/client integration placeholder boundary.
- `tests/` : standalone test placeholder boundary.
- `cmake/Sources.cmake` : source manifest used by standalone staging build.

## Build (staging)
```bash
cmake -S modules/slm-filemanager -B build/slm-filemanager
cmake --build build/slm-filemanager --target slm-filemanager-core -j"$(nproc)"
```

## CI template (for extracted repo)
- GitHub Actions workflow template:
  - `.github/workflows/ci.yml`
- Local/CI smoke entrypoint:
  - `scripts/ci-smoke.sh`
- Validates:
  - standalone configure/build/install,
  - package-configure smoke via `find_package(SlmFileManager)`.

## Notes
- Sources are still referenced from monorepo paths (`src/apps/filemanager`, `Qml/apps/filemanager`).
- This is intentional for transition until history-filtered extraction is executed.
- Canonical target alias: `SlmFileManager::Core`.
