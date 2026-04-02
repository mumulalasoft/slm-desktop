# slm-filemanager Roadmap

This roadmap is focused on post-split execution for `slm-filemanager` as an independent repository.

## Principles
- Stability first: API/ABI and DBus behavior must be predictable.
- CI-enforced quality: every merge must keep build/test/package contracts green.
- Explicit compatibility: desktop shell integration is always pinned by tag.
- One-cycle deprecation: compatibility shims removed only after one stable cycle.

## Timeline Overview
- M1 (2 weeks): Stabilize `v0.1.x` foundation.
- M2 (1-2 weeks): Desktop shell integration pinning.
- M3 (1 week): Ownership and workflow split.
- M4 (2-3 weeks): Hardening and packaging.
- M5 (after one stable cycle): Deprecation and cleanup.

## M1: Stabilize v0.1.x (2 weeks)
### Goals
- Establish reliable standalone CI and package contract.
- Freeze public API surface used by desktop shell.

### Deliverables
- CI pipeline: build, install, `find_package(SlmFileManager)` smoke.
- DBus stable gate in CI:
  - `fileoperationsservice_dbus_test`
  - `fileopctl_smoke_test`
  - `filemanagerapi_daemon_recovery_test`
- DBus strict recovery gate (nightly/manual).
- `CHANGELOG.md` and release `v0.1.0`.
- Build/test/install documentation.

### Exit Criteria
- All required CI jobs are green for 5 consecutive days.
- No unresolved P1/P2 regressions in file operations and recovery tests.

## M2: Desktop Integration Pinning (1-2 weeks)
### Goals
- Ensure desktop shell consumes external package safely via pinned version.

### Deliverables
- Shell pinned to `slm-filemanager` tag `v0.1.0`.
- Compatibility CI between shell and pinned tag.
- External package integration mode validated.
- Compatibility matrix updated.

### Exit Criteria
- Shell CI green in both in-tree and external package mode.
- No contract drift between shell and `slm-filemanager`.

## M3: Ownership Split (1 week)
### Goals
- Move day-to-day FileManager development ownership to standalone repo.

### Deliverables
- CODEOWNERS and review policy.
- Issue/PR workflow migration from monorepo.
- Monorepo reduced to integration bridge + contract tests.

### Exit Criteria
- New issues/features for FileManager land only in standalone repo.

## M4: Hardening & Packaging (2-3 weeks)
### Goals
- Improve runtime resilience and delivery confidence.

### Deliverables
- Extended regression suite (context menu/open-with/share/ops).
- Performance baseline for listing/thumb/cache and file ops.
- Release artifacts and packaging notes.
- Patch releases (`v0.1.1+`) as needed.

### Exit Criteria
- Regression suite stable.
- Packaging path reproducible.

## M5: Deprecation & Cleanup (post one stable cycle)
### Goals
- Remove temporary compatibility layers cleanly.

### Deliverables
- Remove legacy QML alias shims.
- Remove deprecated header shims in shell.
- Final migration notes.
- Release `v0.2.0` if cleanup includes breaking changes.

### Exit Criteria
- No consumer still requires legacy shim surface.
- Compatibility matrix updated and closed.

## Risk Watchlist
- API drift between shell and filemanager package.
- DBus behavior regressions under restart/recovery conditions.
- Hidden coupling to monorepo-only utilities.
- Prompt-fatigue equivalent in UX: too many breaking changes per release.

## Tracking
- Keep progress in `MILESTONES.md` and issue templates under `.github/ISSUE_TEMPLATE/`.
