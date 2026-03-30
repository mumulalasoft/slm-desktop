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

## Local Git Hooks (Recommended)
Install repository hooks once:
```bash
scripts/install-git-hooks.sh
```

Current pre-commit checks:
- Animation token lint pada file QML yang di-stage (`scripts/check-animation-token-usage.sh staged`)
- UI style lint pada file QML yang di-stage (`scripts/lint-ui-style.sh staged`)

Current pre-push checks:
- Full animation token lint (`scripts/check-animation-token-usage.sh all`)
- UI style lint (`scripts/lint-ui-style.sh all`)
  - Baseline allowlist file exists at `config/lint/ui-style-allowlist.txt` and is currently empty.

Temporary bypass (if absolutely needed):
```bash
SLM_SKIP_PRECOMMIT=1 git commit -m "..."
```

Disable staged UI lint only:
```bash
SLM_PRECOMMIT_UI_LINT=0 git commit -m "..."
```

```bash
SLM_SKIP_PREPUSH=1 git push
```

Manual full scan (recommended before PR):
```bash
scripts/check-animation-token-usage.sh all
scripts/lint-ui-style.sh all
```

Strict UI lint without allowlist baseline:
```bash
SLM_UI_LINT_USE_ALLOWLIST=0 scripts/lint-ui-style.sh all
```

## Useful Commands
```bash
scripts/smoke-runtime.sh --build-dir build --app-bin build/appSlm_Desktop
scripts/smoke-backends.sh --build-dir build --app-bin build/appSlm_Desktop
scripts/smoke-tothespot-contextmenu.sh --build-dir build --app-bin build/appSlm_Desktop
```
