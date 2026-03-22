#!/usr/bin/env bash
set -euo pipefail

PATHS_FILE="${1:-modules/slm-filemanager/docs/EXTRACTION_PATHS.txt}"
FAILED=0

note() { printf '[filemanager-split-readiness] %s\n' "$*"; }
ok() { printf '[filemanager-split-readiness][OK] %s\n' "$*"; }
warn() { printf '[filemanager-split-readiness][WARN] %s\n' "$*"; }
fail() { printf '[filemanager-split-readiness][FAIL] %s\n' "$*" >&2; FAILED=1; }

if [[ ! -f "$PATHS_FILE" ]]; then
  fail "path manifest not found: $PATHS_FILE"
  exit 2
fi

note "checking extraction path manifest: $PATHS_FILE"

TOTAL=0
MISSING_WORKTREE=0
MISSING_HEAD=0

while IFS= read -r path; do
  [[ -z "$path" ]] && continue
  TOTAL=$((TOTAL + 1))

  if [[ ! -e "$path" ]]; then
    MISSING_WORKTREE=$((MISSING_WORKTREE + 1))
    fail "missing in working tree: $path"
    continue
  fi

  if git cat-file -e "HEAD:$path" >/dev/null 2>&1; then
    commit_count="$(git rev-list --count --all -- "$path" 2>/dev/null || echo 0)"
    ok "in HEAD history: $path (commit-count=${commit_count})"
  else
    MISSING_HEAD=$((MISSING_HEAD + 1))
    warn "not present in committed HEAD: $path"
  fi
done < "$PATHS_FILE"

note "summary total=${TOTAL} missing_worktree=${MISSING_WORKTREE} missing_head=${MISSING_HEAD}"

if [[ "$MISSING_WORKTREE" -gt 0 ]]; then
  fail "manifest contains paths missing from working tree"
fi

if [[ "$MISSING_HEAD" -gt 0 ]]; then
  fail "manifest paths are not fully committed; history-preserving split is blocked"
  note "next step: commit manifest paths first, then rerun split helper"
fi

if [[ "$FAILED" -ne 0 ]]; then
  exit 1
fi

ok "split readiness passed (history-preserving path available)"
