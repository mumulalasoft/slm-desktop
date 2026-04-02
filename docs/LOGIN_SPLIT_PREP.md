# SLM Login Split Preparation (Week-3)

Last updated: 2026-03-28
Goal: Prepare extraction baseline for future `slm-login` repository split.
Related:
- `docs/REPO_SPLIT_PLAN.md`
- `docs/RELEASE_COMPATIBILITY_MATRIX.md`

## 1) File Ownership Inventory

## Owned source directories (candidate `slm-login`)
- `src/login/greeter/*`
- `src/login/libslmlogin/*`
- `src/login/polkit-agent/*`
- `src/login/recovery-app/*`
- `src/login/session-broker/*`
- `src/login/watchdog/*`

## Owned QML/UI assets
- `Qml/greeter/*`
- `Qml/polkit-agent/*`
- `Qml/recovery/*`

## Owned binaries (current CMake targets)
- `slm-greeter`
- `slm-polkit-agent`
- `slm-recovery-app`
- `slm-session-broker`
- `slm-watchdog`
- Shared static lib: `slm-login-lib` (`SlmLogin::Lib`)

## Shared/non-owned dependencies (must stay platform or be consumed)
- `src/core/prefs/uipreferences.*` (currently consumed by `slm-polkit-agent`)
- `Qml/components/system/MissingComponentsCard.qml` (used by greeter/recovery)
- `third_party/slm-style` QML module (used by polkit dialog style)

## 2) Include Dependency Inventory

## Internal login coupling (inside `src/login`)
- `session-broker` and `watchdog` depend on:
  - `src/login/libslmlogin/slmconfigmanager.*`
  - `src/login/libslmlogin/slmsessionstate.*`
  - `src/login/libslmlogin/slmplatformcheck.*`
- `recovery-app` depends on:
  - `slmconfigmanager.*`
  - `slmsessionstate.*`
- `greeter` depends on:
  - `slmsessionstate.*`

## Cross-module/platform coupling (outside `src/login`)
- `src/core/system/componentregistry.cpp` references login runtime components:
  - `slm-watchdog`
  - `slm-recovery-app`
- Polkit agent target currently compiles with:
  - `src/core/prefs/uipreferences.*`
- Recovery app now uses DBus client call for daemon health snapshot:
  - `org.slm.WorkspaceManager1::DaemonHealthSnapshot()`

## Test coupling to login modules
- `tests/slmconfigmanager_validation_test.cpp`
- `tests/slmsessionstate_io_test.cpp`
- `tests/sessionbroker_recovery_rollback_test.cpp`

Migration note:
- Keep login tests migrated with login repo, except tests that intentionally validate
  platform-facing contracts should remain in platform integration lane.

## 3) Runtime Script/Service Ownership Inventory

## Candidate ownership: login repo
- `scripts/login/*`
  - install/verify/fix/smoke scripts for greetd + runtime
- `scripts/install-polkit-agent-user-service.sh`
- `scripts/smoke-polkit-agent-runtime.sh`
- `scripts/systemd/slm-polkit-agent.service`
- `sessions/slm.desktop` (session entry currently points to `slm-session-broker`)

## Shared ownership (platform + login coordination)
- `scripts/test.sh`
  - includes nightly polkit runtime mode controls
- `.github/workflows/ci.yml`
  - nightly lane and runtime smoke orchestration

## Systemd/user runtime touchpoints (current install flow)
- User units installed/enabled by runtime installer:
  - `slm-polkit-agent.service`
  - plus platform services (`slm-desktopd`, `slm-portald`, `slm-fileopsd`, `slm-devicesd`, `slm-clipboardd`, `slm-envd`)
- During split, installer responsibilities should be split into:
  - login installer (greeter/broker/watchdog/recovery/polkit unit)
  - platform installer (desktop daemons/services)

## Extraction Checklist (Immediate Next Actions)
1. Freeze login module public contract surface (`libslmlogin` headers).
2. Add temporary compatibility shims for shared QML component imports.
3. Separate installer scripts into `login` vs `platform` ownership paths.
4. Add CI job draft for `scripts/login/smoke-login-recovery.sh` on schedule.
5. Track `componentregistry` references as explicit platform->login dependency.
