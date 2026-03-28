# SLM Desktop Repository Split Plan

Last updated: 2026-03-28
Owner: Core Platform Team

## Objective
- Split codebase into product boundaries without breaking runtime stability.
- Keep current development velocity while reducing long-term coupling.
- Make release ownership clearer per subsystem.

## Current Monorepo Baseline
- Core runtime and services:
  - `src/core/*`
  - `src/daemon/*`
  - `src/services/*`
  - `Qml/components/*`
- Apps:
  - `src/apps/settings/*`, `Qml/apps/settings/*`
  - `src/apps/filemanager/*`, `Qml/apps/filemanager/*`
- Login stack:
  - `src/login/*`
  - `Qml/greeter/*`
  - `Qml/polkit-agent/*`
  - `Qml/recovery/*`
- Shared style:
  - `third_party/slm-style`

## Target Repository Topology

## 1) `slm-platform` (keep as base repo)
- Contains:
  - `src/core/*`
  - `src/daemon/*`
  - `src/services/*`
  - `src/cli/*`
  - shared QML system components (`Qml/components/*`)
- Responsibility:
  - runtime contracts
  - foundational daemons
  - compatibility guarantees for downstream apps

## 2) `slm-login` (new repo, highest priority split)
- Move from monorepo:
  - `src/login/*`
  - `Qml/greeter/*`
  - `Qml/polkit-agent/*`
  - `Qml/recovery/*`
  - related systemd user services and scripts
- Responsibility:
  - session entry
  - authentication UI/flows
  - crash recovery UX and safety rails

## 3) `slm-settings` (new repo, medium priority split)
- Move from monorepo:
  - `src/apps/settings/*`
  - `Qml/apps/settings/*`
- Responsibility:
  - system settings UX
  - module-level settings backends
  - deep links and settings search behavior

## 4) `slm-filemanager` (optional/finalize after stabilization)
- Move from monorepo:
  - `src/apps/filemanager/*`
  - `Qml/apps/filemanager/*`
  - filemanager-specific helpers currently under app scope
- Responsibility:
  - file browsing UX
  - sharing workflow
  - file operation integration

## 5) `slm-style` (already externalized)
- Keep as standalone shared style token/component repo.
- Consume as submodule or pinned dependency in all UI repos.

## Split Order (Safe Sequence)
1. Split `slm-login`.
2. Stabilize `slm-login` <-> `slm-platform` contracts and CI.
3. Split `slm-settings`.
4. Finalize split for `slm-filemanager` after two stable sprints.

Rationale:
- Login stack has highest failure impact and should be isolated first.
- Settings has manageable coupling and clear API boundaries.
- FileManager split should follow after dependency guard and sharing flow are stable.

## Contract Strategy (Non-Negotiable)
- Introduce and maintain explicit interface docs in each repo:
  - `docs/contracts/*.md`
- Every consumed DBus endpoint must have:
  - provider contract test
  - consumer integration test
- Version each contract with backward compatibility notes.

## Build & Dependency Model
- `slm-platform` publishes:
  - DBus contract schema/docs
  - shared C++ utility package(s) if needed
  - optional generated headers for stable interfaces
- App/login repos consume platform contracts by:
  - tagged release reference
  - CI-pinned version matrix

## CI Federation Plan
- Per-repo CI:
  - unit + contract + lint
- Cross-repo nightly:
  - integration matrix with pinned versions:
    - `platform@main` + `login@main`
    - `platform@release/x.y` + `login@release/x.y`
    - same for settings/filemanager
- Runtime lane required:
  - real-session polkit smoke
  - recovery smoke
  - missing dependency install flow smoke

## Migration Checklist Per Repo
- Preparation:
  - inventory files and transitive includes
  - identify owned tests and runtime scripts
  - define minimum contract set
- Extraction:
  - move files with history
  - patch includes/build scripts
  - add temporary compatibility shims where needed
- Validation:
  - repo-local CI green
  - cross-repo integration green
  - runtime smoke green
- Cutover:
  - update monorepo references
  - freeze deprecated paths
  - document ownership handoff

## Ownership Matrix (Initial)
- Platform Team:
  - `slm-platform`, contract governance, cross-repo integration CI
- Login Team:
  - `slm-login`, greeter/polkit/recovery runtime reliability
- UX/Settings Team:
  - `slm-settings`, settings module completeness and UX consistency
- App Team:
  - `slm-filemanager`, sharing and operational robustness

## Risk & Mitigation
- Risk: hidden include/runtime coupling breaks split builds.
  - Mitigation: pre-split dependency inventory + staged shims.
- Risk: contract drift between provider and consumers.
  - Mitigation: contract tests required on both sides.
- Risk: release cadence desync across repos.
  - Mitigation: release train calendar + compatibility matrix.
- Risk: style divergence across apps.
  - Mitigation: single source of truth in `slm-style` + visual smoke.

## Done Criteria for Split Program
- Split repos created and actively used in CI.
- No critical flow regression vs monorepo baseline.
- Contract docs/tests in place and versioned.
- Release workflow supports independent hotfix per repo.
