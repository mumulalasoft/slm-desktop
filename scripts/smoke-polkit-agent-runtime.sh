#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="${SLM_POLKIT_AGENT_SERVICE:-slm-polkit-agent.service}"
TRIGGER_CMD="${SLM_POLKIT_TRIGGER_CMD:-/usr/bin/id -u}"
TIMEOUT_SEC="${SLM_POLKIT_TRIGGER_TIMEOUT_SEC:-8}"

usage() {
  cat <<'EOF'
usage: scripts/smoke-polkit-agent-runtime.sh [--service UNIT] [--timeout SEC] [--trigger-cmd CMD]

Environment gate:
  SLM_POLKIT_RUNTIME_SMOKE=1   run real-session smoke.
  default (unset/0)            skip (exit 0).
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --service)
      shift
      SERVICE_NAME="${1:-}"
      ;;
    --timeout)
      shift
      TIMEOUT_SEC="${1:-}"
      ;;
    --trigger-cmd)
      shift
      TRIGGER_CMD="${1:-}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[smoke-polkit-agent] unknown arg: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift || true
done

if [[ "${SLM_POLKIT_RUNTIME_SMOKE:-0}" != "1" ]]; then
  echo "[smoke-polkit-agent] SKIP (set SLM_POLKIT_RUNTIME_SMOKE=1 to run real-session smoke)"
  exit 0
fi

for cmd in systemctl journalctl pkexec timeout date; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "[smoke-polkit-agent][FAIL] missing required command: $cmd" >&2
    exit 2
  fi
done

if ! systemctl --user list-unit-files --type=service --no-pager | rg -q "^${SERVICE_NAME}\b"; then
  echo "[smoke-polkit-agent][FAIL] user service not installed: ${SERVICE_NAME}" >&2
  echo "[smoke-polkit-agent][HINT] run: scripts/install-polkit-agent-user-service.sh" >&2
  exit 1
fi

echo "[smoke-polkit-agent] ensuring service active: ${SERVICE_NAME}"
systemctl --user start "${SERVICE_NAME}"
if ! systemctl --user is-active --quiet "${SERVICE_NAME}"; then
  echo "[smoke-polkit-agent][FAIL] service inactive: ${SERVICE_NAME}" >&2
  systemctl --user --no-pager --full status "${SERVICE_NAME}" || true
  exit 1
fi

since="$(date --iso-8601=seconds)"
tmp_log="$(mktemp)"
trap 'rm -f "$tmp_log"' EXIT

echo "[smoke-polkit-agent] trigger command: pkexec --disable-internal-agent ${TRIGGER_CMD}"
set +e
timeout "${TIMEOUT_SEC}s" pkexec --disable-internal-agent ${TRIGGER_CMD} >"$tmp_log" 2>&1
rc=$?
set -e

echo "[smoke-polkit-agent] trigger rc=${rc}"
if [[ -s "$tmp_log" ]]; then
  echo "[smoke-polkit-agent] trigger output:"
  sed 's/^/[pkexec] /' "$tmp_log"
fi

logs="$(journalctl --user -u "${SERVICE_NAME}" --since "${since}" --no-pager || true)"
echo "$logs" | sed 's/^/[agent-log] /'

if echo "$logs" | rg -q "auth request accepted action="; then
  echo "[smoke-polkit-agent][PASS] auth request reached SLM polkit agent."
  exit 0
fi

if [[ "${rc}" -eq 127 ]] && rg -q "No authentication agent found" "$tmp_log"; then
  echo "[smoke-polkit-agent][FAIL] pkexec reports no authentication agent found." >&2
  exit 1
fi

echo "[smoke-polkit-agent][FAIL] no auth request log detected after trigger." >&2
exit 1

