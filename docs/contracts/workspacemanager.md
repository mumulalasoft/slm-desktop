# Contract: WorkspaceManager

Service identity:
- Bus: `org.slm.WorkspaceManager`
- Path: `/org/slm/WorkspaceManager`
- Interface: `org.slm.WorkspaceManager1`

## Stable methods (baseline)
- `Ping() -> a{sv}`
  - Required fields:
    - `ok` (bool)
    - `service` (string)
    - `api_version` (string)
- `GetCapabilities() -> a{sv}`
  - Required fields:
    - `ok` (bool)
    - `api_version` (string)
    - `capabilities` (string list)
- `DiagnosticSnapshot() -> a{sv}`
  - Required fields:
    - `serviceRegistered` (bool)
    - `activeSpace` (int)
    - `spaceCount` (int)
    - `daemonHealth` (map)
- `DaemonHealthSnapshot() -> a{sv}`
  - Required fields:
    - `fileOperations` (map)
    - `devices` (map)
    - `baseDelayMs` (int)
    - `maxDelayMs` (int)
    - `degraded` (bool)
    - `reasonCodes` (list)
    - `timeline` (list of row maps)
    - `timelineSize` (int)
    - `timelineFile` (string, non-empty when monitor active)

## Stable signals (baseline)
- `OverviewShown()`
- `WorkspaceChanged()`
- `WindowAttention(a{sv})`

## Timeline row shape
- Required keys per row map:
  - `tsMs` (int64 epoch ms)
  - `peer` (string)
  - `code` (string)
  - `severity` (string)
  - `message` (string)

## Degraded/offline behavior
- If daemon health monitor is absent:
  - `DaemonHealthSnapshot()` returns empty map.
  - `DiagnosticSnapshot().daemonHealth` is empty map.

## Compatibility policy
- Additive keys are allowed.
- Existing required keys must not change semantic meaning.
- Any removal/rename requires deprecation notice for one release cycle.

## Test coverage
- Provider contract test:
  - `tests/workspacemanager_dbus_test.cpp`
