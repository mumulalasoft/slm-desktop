#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Force kwin-wayland by default for compatibility wrapper.
if [[ "${1:-}" != "--backend" ]]; then
  export DS_WINDOWING_BACKEND="${DS_WINDOWING_BACKEND:-kwin-wayland}"
fi

exec "${SCRIPT_DIR}/run-nested.sh" "$@"
