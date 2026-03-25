# FileManager Split Cutover (Execution Notes)

## Current status
- In-repo modularization is complete enough for split staging.
- Standalone staging package exists at `modules/slm-filemanager/`.
- Snapshot split repository can be generated with:
  - `scripts/prepare-filemanager-history-split.sh /tmp/slm-filemanager-repo`
  - output now includes root standalone scaffolding:
    - `CMakeLists.txt`
    - `cmake/Sources.cmake`
    - `.github/workflows/ci.yml`
    - `scripts/ci-smoke.sh`
  - scaffolding commit is created automatically in split repo:
    - `Add standalone split scaffolding (build + CI)`
- Shell integration already uses link-based FileManager core target:
  - `appSlm_Desktop` -> `SlmFileManager::Core`
  - toggle via CMake option:
    - `SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE`

## Preferred cutover path
1. Install `git-filter-repo` and regenerate split with history preserved:
   - `ALLOW_SNAPSHOT_FALLBACK=0 scripts/prepare-filemanager-history-split.sh /tmp/slm-filemanager-repo`
2. Push `/tmp/slm-filemanager-repo` to new remote `slm-filemanager`.
   - repo should be clean immediately after script run (`git status` empty).
   - helper:
     - `scripts/push-filemanager-split.sh /tmp/slm-filemanager-repo <remote-url> [branch]`
   - with GitHub CLI auth:
     - `gh auth setup-git`
     - `git -C /tmp/slm-filemanager-repo push -u origin main`
3. Enable baseline branch protection on external repo (`main`):
   - require status check: `standalone-core`
   - require 1 review
   - enforce admins + linear history
   - keep this as minimum gate before wider team writes.
4. Choose integration strategy in shell:
   - submodule (recommended for explicit version pin), or
   - subtree (recommended if single-repo operational flow is preferred).
5. Replace direct in-tree source lists with package consumption:
   - `find_package(SlmFileManager CONFIG REQUIRED)`
   - link `SlmFileManager::Core`
6. Keep compatibility shims for one release cycle, then remove.

## External package bootstrap helper
- Script:
  - `scripts/bootstrap-external-filemanager-package.sh`
- Default flow:
  - clone/update `slm-filemanager` external repo,
  - build/install package,
  - configure/build `Slm_Desktop` with `SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=ON`.
- Package-only mode:
  - `SLM_EXTERNAL_FM_SKIP_SHELL=1 scripts/bootstrap-external-filemanager-package.sh ...`
- Note:
  - Slm_Desktop external configure currently requires `Qt6ShaderTools` on host build env.

## Transitional guarantees already available
- Canonical include path:
  - `src/apps/filemanager/include/*`
- Canonical QML path:
  - `Qml/apps/filemanager/*`
- Legacy compatibility QML path (temporary, one release cycle):
  - `Qml/components/filemanager/*` (forwarding shim to canonical surface)
- Aggregate test target:
  - `filemanager_test_suite`
- Standalone staging build:
  - `scripts/build-filemanager-standalone-staging.sh`
- Standalone package config export/install (staging):
  - `SlmFileManagerConfig.cmake`
  - `SlmFileManagerTargets.cmake`
  - compatibility alias exported:
    - `SlmFileManager::Core`
- Extraction gate:
  - `scripts/check-filemanager-extraction-gate.sh`
  - includes guard that standalone module keeps QObject headers in target source list (AUTOMOC safety).
- Integration mode end-to-end check:
  - `scripts/test-filemanager-integration-modes.sh`
  - runs in-tree profile + standalone install + external package shell build.
- Split readiness preflight:
  - `scripts/check-filemanager-split-readiness.sh`
  - verifies extraction manifest paths exist in working tree and committed `HEAD` history.
- DBus recovery test matrix:
  - default stable gate:
    - `scripts/test-filemanager-dbus-gates.sh --build-dir <build> --stable`
  - strict stress gate (opt-in):
    - `scripts/test-filemanager-dbus-gates.sh --build-dir <build> --strict`
  - CI policy:
    - stable gate in normal CI jobs.
    - strict gate in nightly/manual CI job.
- Rollback playbook:
  - `docs/architecture/FILEMANAGER_ROLLBACK_PLAN.md`
- Compatibility/version matrix:
  - `docs/architecture/FILEMANAGER_COMPATIBILITY_MATRIX.md`
