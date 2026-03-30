#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

if [[ ! -d .git ]]; then
  echo "[install-git-hooks] .git directory not found at ${ROOT_DIR}" >&2
  exit 2
fi

mkdir -p .githooks
if [[ -f .githooks/pre-commit ]]; then
  chmod +x .githooks/pre-commit
fi
if [[ -f .githooks/pre-push ]]; then
  chmod +x .githooks/pre-push
fi

git config core.hooksPath .githooks
echo "[install-git-hooks] core.hooksPath set to .githooks"
echo "[install-git-hooks] installed hooks:"
ls -1 .githooks | sed 's/^/  - /'
