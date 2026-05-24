# Settings Split Cutover (Execution Notes)

## Current status
- `slm-settings` source boundary in monorepo:
  - `src/apps/settings/*`
  - `Qml/apps/settings/*`
- Split helper scripts added:
  - `scripts/prepare-settings-history-split.sh`
  - `scripts/push-settings-split.sh`
- Extraction manifest:
  - `modules/slm-settings/docs/EXTRACTION_PATHS.txt`

## Recommended flow
1. Generate split repo:
   - `ALLOW_SNAPSHOT_FALLBACK=0 scripts/prepare-settings-history-split.sh /tmp/slm-settings-repo`
2. Push to remote:
   - `scripts/push-settings-split.sh /tmp/slm-settings-repo <remote-url> [branch]`
3. Set branch protection on `main`.
4. Integrate back via submodule/package pin after CI baseline is green.

## Notes
- If `git-filter-repo` is unavailable, script falls back to:
  - `fast-export/import` history-preserving path when available, or
  - snapshot mode (no history).
- Snapshot mode is only for bootstrap; rerun in history-preserving mode before production cutover.
