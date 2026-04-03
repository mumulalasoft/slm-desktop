# Release Compatibility Matrix (Draft v1.0)

Last updated: 2026-03-28
Status: Draft (Week-2 deliverable)
Related:
- `docs/RELEASE_PLAN.md`
- `docs/REPO_SPLIT_PLAN.md`
- `docs/CI_TIER_MAP.md`
- `docs/contracts/README.md`

## Purpose
- Define compatibility expectations across:
  - platform
  - login
  - settings
  - filemanager
  - style
- Provide release gate visibility before full repo split.

## Components and Source of Truth
- `platform`: current monorepo runtime core (`src/core`, `src/daemon`, `src/services`).
- `login`: current monorepo login stack (`src/login`, `Qml/greeter`, `Qml/polkit-agent`, `Qml/recovery`).
- `settings`: current monorepo settings app (`src/apps/settings`, `Qml/apps/settings`).
- `filemanager`: current integration mode + external compatibility checks.
- `style`: `third_party/slm-style` token/component source.

## Matrix (Current Draft)

| Platform baseline | Login baseline | Settings baseline | FileManager baseline | Style baseline | Status | Enforced by |
|---|---|---|---|---|---|---|
| `main` | in-tree login stack | in-tree settings | in-tree + external compatibility (`>=0.1.0` pinned lane) | `third_party/slm-style` pinned by submodule state | Active | `.github/workflows/ci.yml` nightly jobs + `scripts/test.sh nightly` |
| `release/x.y` (future) | `release/x.y` (future split repo) | `release/x.y` (future split repo) | `release/x.y` (future split repo) | tagged style release | Planned | cross-repo nightly matrix (to be added after split) |

## Compatibility Rules
1. DBus/API contract changes must be additive by default.
2. Removal/rename requires one release-cycle deprecation window.
3. Critical service contracts must have provider + consumer tests.
4. Style token changes that affect system dialogs require visual smoke validation.
5. Login/auth/recovery compatibility has priority over non-critical UI deltas.

## Current Enforcement Mapping
- Platform/login/settings core:
  - `build-and-test` job (`scripts/test.sh nightly build`)
- FileManager integration:
  - `filemanager-integration-modes`
  - `filemanager-compatibility-pinned`
  - `filemanager-recovery-strict-nightly`
- Style/runtime smoke:
  - `desktop-runtime-smoke`
  - `tothespot-quick`
  - `tothespot-full`
- Portal profile compatibility:
  - `portal-profile-matrix`
  - `portal-interop-nightly`

## Known Gaps (to close before GA)
- No separate repo tags yet for login/settings/filemanager in official matrix rows.
- No explicit cross-repo pinned-version matrix job yet (post-split task).
- Real-session polkit runtime smoke depends on host/service availability; keep mode tracked.

## Post-Split Target Matrix (Planned)

| Platform repo | Login repo | Settings repo | FileManager repo | Style repo | Required lane |
|---|---|---|---|---|---|
| `platform@main` | `login@main` | `settings@main` | `filemanager@main` | `style@main` | nightly integration |
| `platform@release/x.y` | `login@release/x.y` | `settings@release/x.y` | `filemanager@release/x.y` | `style@vX.Y` | RC gate |
| `platform@hotfix/x.y.z` | compatible tags only | compatible tags only | compatible tags only | compatible tag | hotfix validation |

## Update Workflow
1. Update this matrix doc first when compatibility policy changes.
2. Update CI lane mapping in `docs/CI_TIER_MAP.md`.
3. Update contract docs in `docs/contracts/*` when interfaces change.
4. Link any breaking-risk change in PR description and release notes.
