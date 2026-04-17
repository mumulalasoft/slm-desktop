#!/usr/bin/env bash
set -euo pipefail

SERVICE_BIN="${SLM_PACKAGE_POLICY_SERVICE_BIN:-/usr/bin/slm-package-policy-service}"
STRICT="${SLM_PACKAGE_POLICY_WRAPPER_SMOKE_STRICT:-1}"

usage() {
  cat <<'EOF'
usage: scripts/smoke-package-policy-wrapper.sh [--service-bin PATH] [--strict 0|1]

Environment gate:
  SLM_PACKAGE_POLICY_RUNTIME_SMOKE=1   run runtime smoke check
  default (unset/0)                    skip (exit 0)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --service-bin)
      shift
      SERVICE_BIN="${1:-}"
      ;;
    --strict)
      shift
      STRICT="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[smoke-package-policy] unknown arg: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift || true
done

if [[ "${SLM_PACKAGE_POLICY_RUNTIME_SMOKE:-0}" != "1" ]]; then
  echo "[smoke-package-policy] SKIP (set SLM_PACKAGE_POLICY_RUNTIME_SMOKE=1 to run)"
  exit 0
fi

if [[ "${STRICT}" != "0" && "${STRICT}" != "1" ]]; then
  echo "[smoke-package-policy][FAIL] invalid strict flag: ${STRICT}" >&2
  exit 2
fi

for cmd in command readlink grep sed mktemp; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "[smoke-package-policy][FAIL] missing required command: $cmd" >&2
    exit 2
  fi
done

if [[ ! -x "${SERVICE_BIN}" ]]; then
  echo "[smoke-package-policy][FAIL] service binary not found: ${SERVICE_BIN}" >&2
  exit 1
fi

check_wrapper() {
  local tool="$1"
  local cmd_path resolved
  cmd_path="$(command -v "$tool" 2>/dev/null || true)"
  if [[ -z "$cmd_path" ]]; then
    echo "[smoke-package-policy][FAIL] ${tool} not found in PATH" >&2
    return 1
  fi
  resolved="$(readlink -f "$cmd_path" 2>/dev/null || echo "$cmd_path")"
  echo "[smoke-package-policy] ${tool} command=${cmd_path} resolved=${resolved}"
  if [[ "${resolved}" == *"/slm-package-policy/wrappers/${tool}" ]]; then
    return 0
  fi
  if [[ "${STRICT}" == "1" ]]; then
    echo "[smoke-package-policy][FAIL] ${tool} does not resolve to SLM wrapper" >&2
    return 1
  fi
  echo "[smoke-package-policy][WARN] ${tool} not resolved to SLM wrapper (strict=0)"
  return 0
}

check_wrapper "apt"
check_wrapper "apt-get"
check_wrapper "dpkg"

dpkg_cmd="$(command -v dpkg)"
dpkg_resolved="$(readlink -f "$dpkg_cmd" 2>/dev/null || echo "$dpkg_cmd")"
if [[ "${dpkg_resolved}" != *"/slm-package-policy/wrappers/dpkg" ]]; then
  echo "[smoke-package-policy][FAIL] refusing block-sample check because dpkg is not wrapper" >&2
  exit 1
fi

echo "[smoke-package-policy] running one-shot baseline check"
set +e
oneshot_out="$("${SERVICE_BIN}" --check --json --tool dpkg -- --remove libc6 2>&1)"
oneshot_rc=$?
set -e
echo "${oneshot_out}" | sed 's/^/[oneshot] /'
if [[ "${oneshot_rc}" -ne 42 ]]; then
  echo "[smoke-package-policy][FAIL] expected one-shot exit 42 for protected remove, got ${oneshot_rc}" >&2
  exit 1
fi

echo "[smoke-package-policy] running wrapper block sample: dpkg --remove libc6"
set +e
block_out="$(dpkg --remove libc6 2>&1)"
block_rc=$?
set -e
echo "${block_out}" | sed 's/^/[wrapper] /'
if [[ "${block_rc}" -ne 100 ]]; then
  echo "[smoke-package-policy][FAIL] expected wrapper exit 100 (blocked), got ${block_rc}" >&2
  exit 1
fi
if ! echo "${block_out}" | grep -qi "blocked by policy"; then
  echo "[smoke-package-policy][FAIL] expected \"blocked by policy\" message from wrapper" >&2
  exit 1
fi

echo "[smoke-package-policy][PASS] wrapper interception and policy block verified."

