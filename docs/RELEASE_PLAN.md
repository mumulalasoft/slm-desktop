# SLM Desktop Release Plan (v1.0)

Last updated: 2026-03-28
Owner: Core Desktop Team

## Goals
- Deliver SLM Desktop v1.0 with stable runtime and recovery guarantees.
- Enforce "solid & unbreakable" policy across desktop, login, and recovery flows.
- Keep release scope disciplined: hardening first, feature expansion second.

## Scope
- In scope:
  - Platform stability (core/daemon/services).
  - Login reliability (greeter + polkit agent + session broker + watchdog + recovery app).
  - Missing dependency detection/install UX in critical paths.
  - Theme consistency for system dialogs and core surfaces.
- Out of scope for v1.0:
  - Large new feature families (except blocker fixes).
  - Major protocol/API redesigns that break existing clients.

## 12-Week Release Train

## Phase R0: Stabilization Baseline (Week 1-2)
- Freeze non-critical feature work.
- Label all open defects with severity: `blocker`, `high`, `normal`.
- Define and lock v1.0 quality gates.
- Mandatory green:
  - `scripts/test.sh`
  - `scripts/test.sh nightly`
  - login + polkit runtime smoke

Exit criteria:
- No unknown crash-class issue in active priorities.
- All critical test lanes run at least once per day.

## Phase R1: Hardening (Week 3-6)
- Complete daemon recovery and degraded-state handling paths.
- Extend contract tests for DBus services used by UI and recovery.
- Promote runtime smoke checks from optional to enforced nightly.
- Add service restart/regression checks for:
  - `desktopd`
  - `fileopsd`
  - `envd`
  - `slm-polkit-agent`

Exit criteria:
- 0 open blocker bugs.
- Crash-loop auto-rollback path validated in repeated runs.
- Missing dependency install flow validated for critical domains.

## Phase R2: UX + Theme Consistency (Week 7-8)
- Unify tokens:
  - elevation/shadow
  - motion
  - control states
  - focus ring
- Ensure system dialogs follow style tokens and accessibility basics.
- Finish compact density pass on FileManager and key Settings pages.

Exit criteria:
- No visual regressions on core system dialogs.
- Theme switch and reduced-motion/high-contrast controls behave consistently.

## Phase R3: RC + Packaging (Week 9-10)
- Enter release-candidate mode (bugfix only).
- Freeze DBus contracts used by released components.
- Produce installable artifacts for target distro baseline.
- Run full regression matrix on at least two Debian-family environments.

Exit criteria:
- RC passes all gates for 5 consecutive days.
- Packaging and upgrade path verified.

## Phase R4: GA (Week 11-12)
- Final fix window: blocker/high only.
- Tag and release:
  - `v1.0.0-rcN` -> `v1.0.0`
- Publish release notes and rollback playbook.

Exit criteria:
- 14-day window with no blocker and no unresolved high crash.
- All mandatory lanes green.

## Quality Gates (Must Pass)
- Unit and contract:
  - DBus contract tests for workspace/service manager/health snapshots.
  - Missing dependency controller and install pipeline tests.
- Runtime:
  - polkit runtime smoke in real session lane.
  - login stack smoke (greeter/broker/watchdog minimal happy path).
- UI:
  - settings/recovery headless smoke.
  - no fatal QML load errors in critical apps.

## Severity & SLA
- `blocker`: fix before merge to release branch.
- `high`: fix within current phase window.
- `normal`: allowed only if non-regression and documented as known issue.

## Branching Strategy
- `main`: active integration branch.
- `release/x.y`: stabilization and RC branch.
- `hotfix/*`: post-release urgent fixes.

Rules:
- No direct commit to `release/x.y` without CI green.
- Cherry-pick fix-only changes from `main` to release branch.

## CI Tiers
- Tier-1 (PR required):
  - fast unit/contract subset.
- Tier-2 (nightly required):
  - full test suite + runtime smoke (`scripts/test.sh nightly`).
- Tier-3 (weekly required):
  - soak loops and service restart stress checks.

## Release Deliverables
- Version tags and release notes.
- Test report summary by tier.
- Known issues list (if any).
- Rollback instructions.
- Component compatibility matrix.

## Risk Register (v1.0)
- Login stack regressions under real host session combinations.
- Dependency installer variability across distro/package manager states.
- Theme token drift between apps if shared style contract is bypassed.
- Daemon/API coupling causing delayed refactor risk.

Mitigations:
- Keep runtime smoke lane mandatory.
- Keep missing dependency flows tested with mocked and real-session variants.
- Keep contract tests as compatibility lock for split repos.

## Definition of Done (v1.0)
- 0 `blocker`, 0 unresolved `high` crash defects.
- Mandatory CI tiers green with stable trend.
- Recovery and rollback flows verified end-to-end.
- Missing component guidance+install works in critical domains.
- Release artifacts and rollback docs complete.
