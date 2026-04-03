# FileManager Split Rollback Plan

## Scope
Rollback strategy if external `SlmFileManager` package integration causes runtime/build regressions.

## Trigger conditions
- `slm-desktop` fails to link/run in external package mode.
- FileManager QML/runtime regressions in smoke testing.
- Critical file operation regressions in release candidate.

## Rollback path
1. Disable external package consumption:
   - Configure with `-DSLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=OFF`
2. Rebuild shell and daemon targets:
   - `cmake --build <build-dir> --target slm-desktop desktopd -j8`
3. Re-run extraction gate + core smoke checks:
   - `scripts/check-filemanager-extraction-gate.sh <build-dir>`
4. Pin release to last known-good integrated commit in monorepo.

## Compatibility guarantees kept during rollback
- Public shim headers remain available:
  - `filemanagerapi.h`
  - `filemanagermodel.h`
  - `filemanagermodelfactory.h`
- Canonical QML runtime path stays:
  - `Qml/apps/filemanager/*`

## Operational notes
- Keep one release cycle with compatibility shims enabled before hard-removal.
- Keep `scripts/test-filemanager-integration-modes.sh` in CI to detect re-breakage early.
