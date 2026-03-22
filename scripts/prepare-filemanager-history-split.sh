#!/usr/bin/env bash
set -euo pipefail

# Non-destructive helper to prepare a history-preserving split repository
# for slm-filemanager using git-filter-repo.
#
# Usage:
#   scripts/prepare-filemanager-history-split.sh [output-dir]
#
# Example:
#   scripts/prepare-filemanager-history-split.sh /tmp/slm-filemanager-repo
#
# Optional:
#   ALLOW_SNAPSHOT_FALLBACK=0 scripts/prepare-filemanager-history-split.sh ...
#   (force history-preserving mode only; fail if git-filter-repo is unavailable)

OUTPUT_DIR="${1:-/tmp/slm-filemanager-repo}"
ALLOW_SNAPSHOT_FALLBACK="${ALLOW_SNAPSHOT_FALLBACK:-1}"
WORK_DIR="$(mktemp -d /tmp/slm-filemanager-split.XXXXXX)"
PATHS_FILE="modules/slm-filemanager/docs/EXTRACTION_PATHS.txt"
HAS_HISTORY_PATHS=1
FORCE_SNAPSHOT_FALLBACK=0

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT

if [[ ! -f "$PATHS_FILE" ]]; then
  echo "[filemanager-split] missing path manifest: $PATHS_FILE" >&2
  exit 1
fi

rm -rf "$OUTPUT_DIR"

# Preflight: history-preserving split only works if path manifest exists in git history.
while IFS= read -r path; do
  [[ -z "$path" ]] && continue
  if ! git cat-file -e "HEAD:$path" >/dev/null 2>&1; then
    HAS_HISTORY_PATHS=0
    break
  fi
done < "$PATHS_FILE"

if [[ "$HAS_HISTORY_PATHS" != "1" ]] && [[ -x "scripts/check-filemanager-split-readiness.sh" ]]; then
  scripts/check-filemanager-split-readiness.sh "$PATHS_FILE" || true
fi

if ! command -v git-filter-repo >/dev/null 2>&1; then
  if [[ "$HAS_HISTORY_PATHS" != "1" ]]; then
    if [[ "$ALLOW_SNAPSHOT_FALLBACK" != "1" ]]; then
      echo "[filemanager-split] manifest paths are not present in HEAD history; cannot perform history-preserving split." >&2
      exit 1
    fi
    FORCE_SNAPSHOT_FALLBACK=1
  fi
  if [[ "$FORCE_SNAPSHOT_FALLBACK" != "1" ]] && git fast-export --help >/dev/null 2>&1; then
    echo "[filemanager-split] git-filter-repo not found; using git fast-export/import fallback (history preserved)."
    echo "[filemanager-split] cloning current repo to temporary workspace..."
    git clone --no-local . "$WORK_DIR/repo"
    pushd "$WORK_DIR/repo" >/dev/null
    CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    PATH_SPECS=()
    while IFS= read -r path; do
      [[ -z "$path" ]] && continue
      PATH_SPECS+=("$path")
    done < "$OLDPWD/$PATHS_FILE"
    mkdir -p "$OUTPUT_DIR"
    pushd "$OUTPUT_DIR" >/dev/null
    git init -q
    popd >/dev/null
    git fast-export --tag-of-filtered-object=drop --all -- "${PATH_SPECS[@]}" | (cd "$OUTPUT_DIR" && git fast-import --quiet)
    if git -C "$OUTPUT_DIR" show-ref --verify --quiet "refs/heads/${CURRENT_BRANCH}"; then
      git -C "$OUTPUT_DIR" checkout -q "${CURRENT_BRANCH}"
    elif git -C "$OUTPUT_DIR" show-ref --verify --quiet "refs/heads/main"; then
      git -C "$OUTPUT_DIR" checkout -q main
    elif git -C "$OUTPUT_DIR" show-ref --verify --quiet "refs/heads/master"; then
      git -C "$OUTPUT_DIR" checkout -q master
    fi
    git -C "$OUTPUT_DIR" gc --prune=now >/dev/null 2>&1 || true
    popd >/dev/null
    echo "[filemanager-split] history-preserving fallback repository prepared at: $OUTPUT_DIR"
    exit 0
  fi

  if [[ "$ALLOW_SNAPSHOT_FALLBACK" != "1" ]]; then
    echo "[filemanager-split] no history-preserving splitter available (git-filter-repo/fast-export)." >&2
    exit 1
  fi

  if [[ "$FORCE_SNAPSHOT_FALLBACK" == "1" ]]; then
    echo "[filemanager-split] manifest paths are not present in committed HEAD; creating snapshot fallback (no history)."
  else
    echo "[filemanager-split] no history-preserving splitter available; creating snapshot fallback (no history)."
  fi
  mkdir -p "$OUTPUT_DIR"
  while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    if [[ -e "$path" ]]; then
      mkdir -p "$OUTPUT_DIR/$(dirname "$path")"
      cp -a "$path" "$OUTPUT_DIR/$path"
    fi
  done < "$PATHS_FILE"

  pushd "$OUTPUT_DIR" >/dev/null
  git init -q
  git config user.name "SLM Build Bot"
  git config user.email "slm-buildbot@local"
  git add .
  git commit -q -m "Initial slm-filemanager snapshot split (fallback, no history)"
  popd >/dev/null

  echo "[filemanager-split] snapshot repository prepared at: $OUTPUT_DIR"
  if [[ "$FORCE_SNAPSHOT_FALLBACK" == "1" ]]; then
    echo "[filemanager-split] commit filemanager paths first, then rerun for history-preserving split."
  else
    echo "[filemanager-split] install git-filter-repo and rerun for full history-preserving split."
  fi
  exit 0
fi

if [[ "$HAS_HISTORY_PATHS" != "1" || "$FORCE_SNAPSHOT_FALLBACK" == "1" ]]; then
  if [[ "$ALLOW_SNAPSHOT_FALLBACK" != "1" ]]; then
    echo "[filemanager-split] manifest paths are not present in HEAD history; cannot perform history-preserving split." >&2
    exit 1
  fi
  echo "[filemanager-split] manifest paths are not present in HEAD history; using snapshot fallback."
  mkdir -p "$OUTPUT_DIR"
  while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    if [[ -e "$path" ]]; then
      mkdir -p "$OUTPUT_DIR/$(dirname "$path")"
      cp -a "$path" "$OUTPUT_DIR/$path"
    fi
  done < "$PATHS_FILE"

  pushd "$OUTPUT_DIR" >/dev/null
  git init -q
  git config user.name "SLM Build Bot"
  git config user.email "slm-buildbot@local"
  git add .
  git commit -q -m "Initial slm-filemanager snapshot split (fallback, no history)"
  popd >/dev/null

  echo "[filemanager-split] snapshot repository prepared at: $OUTPUT_DIR"
  echo "[filemanager-split] after filemanager paths are committed, rerun for history-preserving split."
  exit 0
fi

echo "[filemanager-split] cloning current repo to temporary workspace..."
git clone --no-local . "$WORK_DIR/repo"

pushd "$WORK_DIR/repo" >/dev/null

FILTER_ARGS=()
while IFS= read -r path; do
  [[ -z "$path" ]] && continue
  FILTER_ARGS+=(--path "$path")
done < "$OLDPWD/$PATHS_FILE"

echo "[filemanager-split] filtering history by manifest paths..."
git filter-repo "${FILTER_ARGS[@]}" --force

mkdir -p "$OUTPUT_DIR"
cp -a . "$OUTPUT_DIR/"

popd >/dev/null

echo "[filemanager-split] split repository prepared at: $OUTPUT_DIR"
echo "[filemanager-split] next recommended step:"
echo "  cd $OUTPUT_DIR && git remote add origin <new-slm-filemanager-repo-url> && git push -u origin HEAD"
