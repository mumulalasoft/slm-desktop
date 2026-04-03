#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <test-binary> [args...]" >&2
  exit 2
fi

TEST_BIN="$1"
shift || true

if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
  export QT_QPA_PLATFORM=offscreen
fi

if command -v dbus-run-session >/dev/null 2>&1; then
  if [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]]; then
    exec "${TEST_BIN}" "$@"
  fi

  if output="$(dbus-run-session -- "${TEST_BIN}" "$@" 2>&1)"; then
    printf '%s\n' "${output}"
    exit 0
  fi

  status=$?
  printf '%s\n' "${output}" >&2
  if printf '%s' "${output}" | grep -qi "Failed to bind socket"; then
    echo "[run-dbus-test] dbus-run-session unavailable in sandbox; fallback to direct run" >&2
    exec "${TEST_BIN}" "$@"
  fi
  exit "${status}"
fi

echo "[run-dbus-test] dbus-run-session not found, running test directly" >&2
exec "${TEST_BIN}" "$@"
