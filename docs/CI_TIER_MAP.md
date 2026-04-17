# CI Tier Mapping (PR / Nightly / Weekly)

Last updated: 2026-04-12
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
- `policy-core-only`
- `desktop-runtime-smoke`

Coverage intent:
- repository governance and script syntax sanity
- desktop runtime boot/log smoke (offscreen)
- backend smoke and tothespot contextmenu smoke

Local mirror commands:
- `scripts/test-policy-core-suite.sh build/toppanel-Debug`
- `scripts/smoke-runtime.sh --build-dir build --app-bin build/slm-desktop`
- `scripts/smoke-backends.sh --build-dir build --app-bin build/slm-desktop`
- `scripts/smoke-tothespot-contextmenu.sh --build-dir build --app-bin build/slm-desktop`

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
- `storage-runtime-hardware-smoke` (weekly scheduled + manual dogfooding lane on self-hosted `storage-hw`)

Coverage intent:
- full build + test suite and integration checks
- filemanager mode/pinned compatibility
- strict recovery gate
- portal interop/profile/artifacts matrix
- tothespot quick+full regression

Local mirror commands:
- `scripts/test.sh nightly build` (default `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=auto`)
- `scripts/test.sh` default/full suite now excludes label `baseline-flaky` unless `SLM_TEST_FULL_EXCLUDE_LABELS` di-override.
- Baseline lane manual:
  - `scripts/test.sh baseline-flaky <build-dir>`
- CI nightly (`build-and-test`) juga menjalankan lane `baseline-flaky` sebagai non-blocking report (`continue-on-error: true`).
- Stable settings policy checks berada di label `policy-core` (`settingsapp_policy_core_test`) dan ikut full/default suite.
- `scripts/test-filemanager-integration-modes.sh <build-dir>`
- `scripts/test-filemanager-dbus-gates.sh --build-dir <build-dir> --strict`
- `scripts/test.sh secret-consent <build-dir>` (or `scripts/test-secret-consent-suite.sh <build-dir>`)
- `scripts/test-storage-runtime-suite.sh <build-dir>` (set `SLM_STORAGE_RUNTIME_HARDWARE_MODE=required` for strict hardware dogfooding)
  - strict mode juga mengaktifkan `SLM_STORAGE_SMOKE_REQUIRE_POLICY_PERSISTENCE=1` untuk check write/read/restore policy via DBus.
  - pada lane hardware mingguan, strict mode juga mengaktifkan `SLM_STORAGE_SMOKE_REQUIRE_SERVICE_RESTART=1` dan menjalankan phased persistence check:
    - phase `prepare` (write policy marker) -> restart `slm-devicesd.service` -> phase `verify` (read marker + restore).

Notes:
- `scripts/test.sh nightly` supports:
  - `SLM_TEST_NIGHTLY_FULL_EXCLUDE_LABELS` -> override exclude label set khusus full-suite nightly (untuk menghindari double-run lane dedicated).
  - `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=required` -> enforce real-session polkit smoke.
  - `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=auto` -> run only when user polkit service is present.
  - `SLM_TEST_NIGHTLY_POLKIT_RUNTIME_MODE=skip` -> skip polkit runtime smoke.
  - `SLM_TEST_NIGHTLY_SECRET_CONSENT_MODE=required|auto|skip` -> control secret-consent lane.
  - `SLM_TEST_NIGHTLY_POLICY_CORE_MODE=required|auto|skip` -> control stable settings policy lane (`policy-core`).
  - `SLM_TEST_NIGHTLY_POLICY_CORE_SKIP_BUILD=1|0` -> default `1` for test-only fast path in nightly policy-core lane.
  - `SLM_TEST_NIGHTLY_SECRET_CONSENT_SKIP_BUILD=1|0` -> default `1` for test-only fast path in nightly.
  - `SLM_TEST_NIGHTLY_STORAGE_RUNTIME_MODE=required|auto|skip` -> control storage runtime smoke lane (`storage-smoke` label).
  - `SLM_TEST_NIGHTLY_STORAGE_RUNTIME_SKIP_BUILD=1|0` -> default `1` for test-only fast path.
- Current CI nightly (`build-and-test`) sets:
  - `SLM_TEST_SKIP_UI_LINT=1`
  - `SLM_TEST_SKIP_CAPABILITY_MATRIX_LINT=1`
  - `SLM_TEST_NIGHTLY_FULL_EXCLUDE_LABELS=baseline-flaky;secret-consent;policy-core`
  - `SLM_TEST_NIGHTLY_SECRET_CONSENT_MODE=required`
  - `SLM_TEST_NIGHTLY_POLICY_CORE_MODE=required`
  - `SLM_TEST_NIGHTLY_POLICY_CORE_SKIP_BUILD=1`
  - `SLM_POLICY_CORE_SKIP_BUILD=1`
  - `SLM_POLICY_CORE_MIN_TESTS=1`
  - `SLM_POLICY_CORE_STRICT_NAMES=1`
  - `SLM_POLICY_CORE_EXPECTED_TESTS=settingsapp_policy_core_test`
  - `SLM_TEST_NIGHTLY_SECRET_CONSENT_SKIP_BUILD=1`

## Tier-3: Weekly Required (soak / stress)

Trigger:
- Scheduled weekly workflow tersedia (cron `0 3 * * 0`) + manual run.

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
