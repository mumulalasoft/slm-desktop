# Defect Triage SLA Owner Map (v1.0)

Last updated: 2026-03-28
Related:
- `docs/RELEASE_PLAN.md`
- `docs/CI_TIER_MAP.md`

## Purpose
- Standardize bug severity, response SLA, and ownership.
- Keep release decisions objective during stabilization.

## Severity Levels

## `blocker`
Definition:
- Breaks core flow or causes data/session safety risk.
- Examples:
  - login blocked for valid users
  - auth/recovery path dead
  - crash loop without recovery
  - mandatory CI lane consistently red

SLA:
- Acknowledge: <= 2 hours (working hours).
- Mitigation owner assigned: <= 4 hours.
- Fix or rollback plan: same day.

Release policy:
- Cannot ship with open blocker.
- Must be fixed or reverted before merge to release branch.

## `high`
Definition:
- Significant user-visible regression in critical path, but workaround may exist.
- Examples:
  - key system dialog unusable on common path
  - unstable daemon with partial degraded behavior
  - dependency install pipeline fails in critical domain

SLA:
- Acknowledge: <= 1 business day.
- Owner assigned: same day.
- Fix target: within active phase window.

Release policy:
- No unresolved high crash defect allowed at GA.

## `normal`
Definition:
- Non-critical defect with workaround or limited impact.
- Examples:
  - visual mismatch not affecting core action
  - edge-case non-fatal warning in non-critical module

SLA:
- Acknowledge: <= 2 business days.
- Owner assigned: <= 3 business days.
- Fix target: backlog or current sprint depending risk.

Release policy:
- Allowed only if documented in known issues and marked non-regression.

## Ownership Map

## Incident Commander (rotation)
- Scope:
  - triage facilitation
  - severity arbitration
  - escalation and release/no-release recommendation
- Backup:
  - Release Owner

## Platform Team
- Scope:
  - `src/core/*`, `src/daemon/*`, `src/services/*`
  - workspace manager and platform DBus contracts
  - runtime CI integrity

## Login Team
- Scope:
  - `src/login/*`
  - `Qml/greeter/*`, `Qml/polkit-agent/*`, `Qml/recovery/*`
  - session entry/auth/recovery reliability

## Settings Team
- Scope:
  - `src/apps/settings/*`, `Qml/apps/settings/*`
  - settings backends, deep links, module UX

## FileManager Team
- Scope:
  - `src/apps/filemanager/*`, `Qml/apps/filemanager/*`
  - sharing/install guidance and file operations UX

## Style/UX Team
- Scope:
  - `third_party/slm-style/*`
  - token consistency and system dialog visual QA

## Triage Workflow
1. New issue enters board with provisional severity.
2. Incident Commander validates severity.
3. Owner team assigned by scope.
4. Add one of:
   - `fix-now`
   - `mitigate-and-followup`
   - `defer-with-known-issue`
5. Track status in daily release standup.

## Required Labels
- Severity:
  - `sev:blocker`
  - `sev:high`
  - `sev:normal`
- Area:
  - `area:platform`
  - `area:login`
  - `area:settings`
  - `area:filemanager`
  - `area:style`
- State:
  - `state:triage`
  - `state:in-progress`
  - `state:mitigated`
  - `state:blocked`
  - `state:done`

## Release Escalation Rules
- Escalate immediately to release owner when:
  - blocker is older than SLA
  - same high issue reopens > 2 times
  - nightly gate red for 2 consecutive runs
- Trigger release hold when:
  - any open blocker exists
  - critical login/auth/recovery path unstable in nightly

## Meeting Cadence
- Daily:
  - blocker/high triage sync (15 minutes).
- Weekly:
  - trend review:
    - count by severity
    - reopen rate
    - mean time to mitigation

## Minimal Metrics
- `MTTA` (mean time to acknowledge) by severity.
- `MTTM` (mean time to mitigate) by severity.
- Reopen rate for blocker/high.
- Nightly stability trend for Tier-2 lanes.
