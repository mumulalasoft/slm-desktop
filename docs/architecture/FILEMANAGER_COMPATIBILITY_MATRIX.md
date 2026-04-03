# FileManager Compatibility Matrix

This matrix defines supported pairing between `slm-desktop` and `slm-filemanager`
package versions during split transition.

## Contract baseline
- CMake package: `SlmFileManagerConfig.cmake`
- Core target: `SlmFileManager::Core`
- Public headers:
  - `filemanagerapi.h`
  - `filemanagermodel.h`
  - `filemanagermodelfactory.h`
- DBus ops contract: `org.slm.Desktop.FileOperations`
- QML canonical surface: `Qml/apps/filemanager/*`
- Legacy shim surface (temporary one cycle): `Qml/components/filemanager/*`

## Supported matrix (current)

| slm-desktop series | slm-filemanager package | Status | Notes |
|---|---|---|---|
| `main` (current) | `0.1.x` | Supported | Baseline split/cutover track. |
| `main` (current) | `<0.1.0` | Unsupported | Missing package contract and split guarantees. |
| `main` (current) | `>=0.2.0` | Not validated yet | Requires explicit CI matrix expansion. |

## CI gates enforcing this matrix
- Integration modes:
  - `scripts/test-filemanager-integration-modes.sh`
- Pinned compatibility:
  - `scripts/test-filemanager-compatibility-pinned.sh`
  - expected version via `SLM_FILEMANAGER_PINNED_VERSION` (default `0.1.0`)
- DBus stable gate:
  - `scripts/test-filemanager-dbus-gates.sh --stable`

## Bump policy
When `slm-filemanager` minor/major changes:
1. Update this matrix first.
2. Update CI expected pinned version.
3. Re-run:
   - integration modes,
   - smoke/regression,
   - DBus stable gate,
   - strict recovery gate (nightly/manual).
4. Only then bump shell default compatibility target.
