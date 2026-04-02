#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="${1:-/tmp/slm-filemanager-repo}"
REMOTE_URL="${2:-}"
BRANCH="${3:-}"

if [[ -z "$REMOTE_URL" ]]; then
  echo "usage: $0 [repo-dir] <remote-url> [branch]" >&2
  echo "example: $0 /tmp/slm-filemanager-repo git@github.com:slm/slm-filemanager.git main" >&2
  exit 1
fi

if [[ ! -d "$REPO_DIR/.git" ]]; then
  echo "[filemanager-push] not a git repository: $REPO_DIR" >&2
  exit 1
fi

if [[ -z "$BRANCH" ]]; then
  BRANCH="$(git -C "$REPO_DIR" branch --show-current)"
  if [[ -z "$BRANCH" ]]; then
    BRANCH="main"
  fi
fi

if git -C "$REPO_DIR" remote get-url origin >/dev/null 2>&1; then
  git -C "$REPO_DIR" remote set-url origin "$REMOTE_URL"
else
  git -C "$REPO_DIR" remote add origin "$REMOTE_URL"
fi

echo "[filemanager-push] pushing ${BRANCH} from ${REPO_DIR} to origin"
git -C "$REPO_DIR" push -u origin "$BRANCH"
echo "[filemanager-push] done"
