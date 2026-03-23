# Contributing to SLM Desktop

## Branch Policy
- Default branch: `main`
- Direct push to `main` is blocked by branch protection.
- All changes must go through Pull Request.

## Required Checks
- `quick-lint`
- `desktop-runtime-smoke`

## Review Rules
- Minimum 1 approval.
- CODEOWNERS review is required.
- All review conversations must be resolved.

## Recommended Local Flow
1. Create a feature branch:
   - `git checkout -b feat/<short-topic>`
2. Commit scoped changes only.
3. Run relevant smoke/tests locally.
4. Push branch and open PR.
5. Merge only after required checks are green.

## Useful Commands
```bash
scripts/smoke-runtime.sh --build-dir build --app-bin build/appSlm_Desktop
scripts/smoke-backends.sh --build-dir build --app-bin build/appSlm_Desktop
scripts/smoke-tothespot-contextmenu.sh --build-dir build --app-bin build/appSlm_Desktop
```
