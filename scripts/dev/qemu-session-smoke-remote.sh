#!/usr/bin/env bash
# scripts/dev/qemu-session-smoke-remote.sh — Jalankan smoke login/session di guest QEMU.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SESSION_USER="${SLM_QEMU_SESSION_USER:-$SSH_USER}"
TIMEOUT_SEC="${SLM_QEMU_SESSION_SMOKE_TIMEOUT_SEC:-90}"
HOST_ARTIFACT_ROOT="${SLM_QEMU_SESSION_SMOKE_ARTIFACT_ROOT:-$PWD/artifacts/qemu-session-smoke}"
REMOTE_SCRIPT="/tmp/qemu-guest-session-smoke.sh"
STRICT_PROCESS=0
STATE_DIR="$(qemu_dev_state_dir)"

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-session-smoke-remote.sh [options]

Options:
  --user USER           SSH user guest. Default: $SSH_USER
  --port PORT           SSH port host->guest. Default: $SSH_PORT
  --session-user USER   User desktop yang state.json-nya diperiksa. Default: $SESSION_USER
  --timeout SEC         Batas tunggu state healthy. Default: $TIMEOUT_SEC
  --artifact-root PATH  Direktori artefak di host. Default: $HOST_ARTIFACT_ROOT
  --strict-process      Gagal bila process oracle tidak terlihat
  --help                Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)
            SSH_USER="$2"
            shift 2
            ;;
        --port)
            SSH_PORT="$2"
            shift 2
            ;;
        --session-user)
            SESSION_USER="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT_SEC="$2"
            shift 2
            ;;
        --artifact-root)
            HOST_ARTIFACT_ROOT="$2"
            shift 2
            ;;
        --strict-process)
            STRICT_PROCESS=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-session-smoke-remote] unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

mkdir -p "$HOST_ARTIFACT_ROOT"
TS="$(date -u +%Y%m%dT%H%M%SZ)"
HOST_ARTIFACT_DIR="${HOST_ARTIFACT_ROOT}/${TS}"
REMOTE_ARTIFACT_DIR="/tmp/slm-qemu-session-smoke-${TS}"
mkdir -p "$HOST_ARTIFACT_DIR"

"$SCRIPT_DIR/qemu-scp.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$SCRIPT_DIR/qemu-guest-session-smoke.sh" \
    "$REMOTE_SCRIPT"

REMOTE_CMD="chmod +x '$REMOTE_SCRIPT' && sudo '$REMOTE_SCRIPT'"
REMOTE_CMD+=" --session-user $(printf '%q' "$SESSION_USER")"
REMOTE_CMD+=" --timeout $(printf '%q' "$TIMEOUT_SEC")"
REMOTE_CMD+=" --artifact-dir $(printf '%q' "$REMOTE_ARTIFACT_DIR")"
if [[ "$STRICT_PROCESS" -eq 1 ]]; then
    REMOTE_CMD+=" --strict-process"
fi

set +e
"$SCRIPT_DIR/qemu-ssh.sh" \
    --tty \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "$REMOTE_CMD"
REMOTE_STATUS=$?
set -e

"$SCRIPT_DIR/qemu-ssh.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "test -d '$REMOTE_ARTIFACT_DIR'"

"$SCRIPT_DIR/qemu-ssh.sh" \
    --user "$SSH_USER" \
    --port "$SSH_PORT" \
    "cat '$REMOTE_ARTIFACT_DIR/summary.log'" \
    >"$HOST_ARTIFACT_DIR/summary.log" || true

scp -r \
    -o StrictHostKeyChecking=accept-new \
    -o UserKnownHostsFile="$(qemu_dev_known_hosts)" \
    -P "$SSH_PORT" \
    "$SSH_USER@127.0.0.1:$REMOTE_ARTIFACT_DIR" \
    "$HOST_ARTIFACT_DIR" >/dev/null

echo "[qemu-session-smoke-remote] summary"
cat "$HOST_ARTIFACT_DIR/summary.log" || true
echo "[qemu-session-smoke-remote] artifacts: $HOST_ARTIFACT_DIR/$(basename "$REMOTE_ARTIFACT_DIR")"

exit "$REMOTE_STATUS"
