#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build/toppanel-Debug}"
CTEST_REGEX="${CTEST_REGEX:-sessionstateservice_dbus_test}"
RUN_BUILD=0

usage() {
  cat <<'EOF'
Usage:
  scripts/test-session-unlock.sh [--build] [--build-dir <dir>] [--regex <ctest_regex>]

Behavior:
  - Runs DBus unlock test(s) with SLM_TEST_UNLOCK_PASSWORD injected only for test process.
  - If SLM_TEST_UNLOCK_PASSWORD is not set, prompts securely (read -s).

Examples:
  scripts/test-session-unlock.sh
  scripts/test-session-unlock.sh --build
  scripts/test-session-unlock.sh --build-dir build/toppanel-Debug --regex sessionstateservice_dbus_test
EOF
}

while (($# > 0)); do
  case "$1" in
    --build)
      RUN_BUILD=1
      shift
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      if [[ -z "${BUILD_DIR}" ]]; then
        echo "error: --build-dir requires a value" >&2
        exit 2
      fi
      shift 2
      ;;
    --regex)
      CTEST_REGEX="${2:-}"
      if [[ -z "${CTEST_REGEX}" ]]; then
        echo "error: --regex requires a value" >&2
        exit 2
      fi
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ ${RUN_BUILD} -eq 1 ]]; then
  cmake --build "${BUILD_DIR}" --target sessionstateservice_dbus_test -j8
fi

PASSWORD="${SLM_TEST_UNLOCK_PASSWORD:-}"
if [[ -z "${PASSWORD}" ]]; then
  if [[ ! -t 0 ]]; then
    echo "error: non-interactive shell and SLM_TEST_UNLOCK_PASSWORD not set" >&2
    exit 2
  fi
  read -r -s -p "Enter current user password for SLM_TEST_UNLOCK_PASSWORD: " PASSWORD
  echo
fi

if [[ -z "${PASSWORD}" ]]; then
  echo "error: empty password is not allowed" >&2
  exit 2
fi

cleanup() {
  unset PASSWORD
}
trap cleanup EXIT

echo "Running ctest regex: ${CTEST_REGEX}"
SLM_TEST_UNLOCK_PASSWORD="${PASSWORD}" \
ctest --test-dir "${BUILD_DIR}" -R "${CTEST_REGEX}" --output-on-failure

