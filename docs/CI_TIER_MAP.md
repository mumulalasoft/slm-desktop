# CI Tier Mapping (PR / Nightly / Weekly)

Last updated: 2026-03-28
Source of truth workflow: `.github/workflows/ci.yml`

## Purpose
- Define which lanes are required per cadence.
- Align release hardening gates with existing CI jobs and local scripts.

## Tier-1: PR Required (fast guardrails)

Trigger:
- `pull_request` (and `push` to `main` where applicable).

Primary jobs:
- `runtime-manifest-report`
- `repo-sanity`
- `quick-lint`
- `desktop-runtime-smoke`

Coverage intent:
- repository governance and script syntax sanity
- desktop runtime boot/log smoke (offscreen)
- backend smoke and tothespot contextmenu smoke

Local mirror commands:
- `scripts/smoke-runtime.sh --build-dir build --app-bin build/appSlm_Desktop`
- `scripts/smoke-backends.sh --build-dir build --app-bin build/appSlm_Desktop`
- `scripts/smoke-tothespot-contextmenu.sh --build-dir build --app-bin build/appSlm_Desktop`

## Tier-2: Nightly Required (release gate)

Trigger:
- `schedule` / `workflow_dispatch`

Primary jobs:
- `build-and-test`
- `filemanager-integration-modes`
- `filemanager-compatibility-pinned`
- `filemanager-recovery-strict-nightly`
- `portal-profile-matrix`
- `portal-interop-nightly`
- `tothespot-quick`
- `tothespot-full`

Coverage intent:
- full build + test suite and integration checks
- filemanager mode/pinned compatibility
- strict recovery gate
- portal interop/profile/artifacts matrix
- tothespot quick+full regression

Local mirror commands:
- `scripts/test.sh nightly build` (default `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=auto`)
- `scripts/test-filemanager-integration-modes.sh <build-dir>`
- `scripts/test-filemanager-dbus-gates.sh --build-dir <build-dir> --strict`

Notes:
- `scripts/test.sh nightly` supports:
  - `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=required` -> enforce real-session polkit smoke.
  - `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=auto` -> run only when user polkit service is present.
  - `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=skip` -> skip polkit runtime smoke.

## Tier-3: Weekly Required (soak / stress)

Trigger:
- Scheduled weekly workflow (recommended to add explicit cron lane) or manual run.

Target coverage:
- soak and long-running stability checks
- strict DBus recovery loops
- service restart resilience scenarios

Suggested baseline commands:
- `SLM_TEST_ENABLE_SOAK=1 SLM_TEST_SOAK_MINUTES=30 scripts/test.sh build`
- `scripts/test-filemanager-dbus-gates.sh --build-dir <build-dir> --strict`
- `ctest --test-dir <build-dir> -R "(portal_.*|workspacemanager_dbus_test)" --output-on-failure`

## Release Gate Matrix

- PR merge gate:
  - Tier-1 must be green.
- Nightly release health:
  - Tier-2 must be green.
- Weekly stability review:
  - Tier-3 trend must not show regression.

## Ownership

- Platform Team:
  - Tier-1/Tier-2 runtime integrity
- App Teams:
  - domain-specific nightly checks
- Release Owner:
  - weekly soak regression decision and escalation
