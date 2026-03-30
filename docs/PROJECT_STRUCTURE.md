# Project Structure (Current)

Last updated: 2026-03-29

## Root

- `main.cpp`: bootstrap `slm-desktop`, register runtime managers + QML context.
- `CMakeLists.txt`: target graph (desktop, daemons, services, tests, tools).
- `CMakePresets.json`: configure/build presets (`dev-intree`, `dev-local-fm`, `release-installed-fm`, `ci`).
- `docs/`: architecture, policy, roadmap, test, release docs.
- `config/`: default runtime config and policy defaults (`config/package-policy/*`).

## Runtime Apps (GUI)

- `slm-desktop` (target utama desktop shell).
- `slm-greeter`
- `slm-settings`
- `slm-recovery-app`

Primary source:
- Desktop shell C++: root files (`appmodel.*`, `dockmodel.*`, `main.cpp`, dll).
- Desktop shell QML: `Main.qml`, `Qml/`, `Qml/apps/filemanager/`.

## Session/User Daemons

- `desktopd` -> `src/daemon/desktopd/`
- `slm-fileopsd` -> `src/daemon/fileopsd/`
- `slm-devicesd` -> `src/daemon/devicesd/`
- `slm-portald` -> `src/daemon/portald/`
- `slm-clipboardd` -> `src/daemon/clipboardd/`
- `slm-archived` -> `src/daemon/archived/`
- `slm-package-policy-service` -> `src/daemon/packagepolicyd/`
- `slm-envd`, `slm-envd-helper`, `slm-loggerd`, `slm-svcmgrd`

## Service Modules (C++)

Under `src/services/`:
- `archive/`: backend + job/state for archive operations (libarchive-first).
- `packagepolicy/`: apt simulation, rules engine, config/mapping loader, logger.
- `portal/`, `network/`, `bluetooth/`, `power/`, `sound/`, `session/`, dll.

## FileManager Domain

- C++: `src/apps/filemanager/`
  - `filemanagerapi_*.cpp` split by domain (archive, sharing, daemon control, batch control, etc).
  - Public include: `src/apps/filemanager/include/`.
- QML: `Qml/apps/filemanager/`
  - UI decomposition (`FileManagerWindow*.qml/js`, menus, handlers, runtime connections).
- Standalone staging module:
  - `modules/slm-filemanager/`

## Policy & Recovery Assets

- Package policy defaults:
  - `config/package-policy/protected-capabilities.yaml`
  - `config/package-policy/package-mappings/debian.yaml`
- Wrappers + recovery hooks:
  - `scripts/package-policy/wrappers/{apt,apt-get,dpkg}`
  - `scripts/package-policy/pre-transaction-snapshot.sh`
  - `scripts/package-policy/post-transaction-health-check.sh`
  - `scripts/package-policy/recover-last-snapshot.sh`
  - `scripts/package-policy/trigger-safe-mode-recovery.sh`

## Styling

- Shared style library: `third_party/slm-style/`
- Project theme shim/token bridge: `third_party/slm-style/qml/SlmStyle/Theme.qml`
- Legacy `Style/` directory is removed from this repo.

## Scripts and Ops

- `scripts/`: smoke tests, install helpers, integration tests, runtime checks.
- `scripts/systemd/`: user service unit templates.
- `scripts/login/`: runtime + login stack install/verify flow.
- `scripts/dbus/`: DBus service files/contracts for install/runtime.

## Tests

- `tests/`: unit, contract, DBus, and integration tests.
- Includes archive backend coverage (`tests/archivebackend_test.cpp`) and missing-component/policy gates.

## Architecture Pointers

- High-level architecture: `docs/ARCHITECTURE.md`
- Archive service blueprint + rollout: `docs/architecture/ARCHIVE_SERVICE_ARCHITECTURE.md`
- Package policy phase-1 details: `docs/architecture/SLM_PACKAGE_POLICY_PHASE1.md`
- Modular/split plan: `docs/MODULAR_STRUCTURE.md`, `docs/REPO_SPLIT_PLAN.md`
