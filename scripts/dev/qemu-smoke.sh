#!/usr/bin/env bash
# scripts/dev/qemu-smoke.sh — Build, install, verify, dan smoke-test SLM di QEMU guest.
#
# Build dilakukan di host (cmake lokal), lalu artifact di-rsync ke guest.
# Install, verify, dan smoke tetap berjalan di guest via SSH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"

STATE_DIR="$(qemu_dev_state_dir)"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SESSION_USER="${SLM_QEMU_SESSION_USER:-$SSH_USER}"
REPO_DIR="${SLM_QEMU_GUEST_REPO_DIR:-/mnt/hostshare}"
BUILD_DIR="${SLM_QEMU_GUEST_BUILD_DIR:-/home/${SSH_USER}/.cache/slm-qemu/build/dev}"
JOBS="${SLM_QEMU_GUEST_JOBS:-$(nproc)}"
RUN_SMOKE=1
SMOKE_TIMEOUT="${SLM_QEMU_SESSION_SMOKE_TIMEOUT_SEC:-90}"
BUILD_ONLY=0
STRICT_PROCESS=0
HOST_ARTIFACT_ROOT="${SLM_QEMU_SESSION_SMOKE_ARTIFACT_ROOT:-$PWD/artifacts/qemu-session-smoke}"
HOST_REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOST_BUILD_DIR="${SLM_QEMU_HOST_BUILD_DIR:-$BUILD_DIR}"

RUNTIME_TARGETS=(
    slm-greeter slm-watchdog slm-recovery-app slm-session-broker slm-svcmgrd slm-loggerd
    appSlm_Desktop desktopd slm-svcmgrd slm-loggerd slm-portald
    slm-fileopsd slm-devicesd slm-clipboardd slm-polkit-agent
    slm-envd slm-recoveryd
)

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-smoke.sh [options]

Options:
  --user USER           SSH user. Default: $SSH_USER
  --port PORT           SSH port. Default: $SSH_PORT
  --session-user USER   Desktop session user. Default: $SESSION_USER
  --repo-dir PATH       Repo path in guest. Default: $REPO_DIR
  --build-dir PATH      Build dir in guest (rsync target). Default: $BUILD_DIR
  --host-build-dir PATH Build dir on host. Default: same as --build-dir
  --jobs N              Parallel build jobs. Default: $JOBS
  --timeout SEC         Smoke wait timeout. Default: $SMOKE_TIMEOUT
  --artifact-root PATH  Host artifact dir. Default: $HOST_ARTIFACT_ROOT
  --build-only          Skip configure, only cmake --build
  --skip-smoke          Build + install + verify only
  --strict-process      Smoke fails if oracle processes not visible
  --help                Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)           SSH_USER="$2";           shift 2 ;;
        --port)           SSH_PORT="$2";           shift 2 ;;
        --session-user)   SESSION_USER="$2";       shift 2 ;;
        --repo-dir)       REPO_DIR="$2";           shift 2 ;;
        --build-dir)      BUILD_DIR="$2";          shift 2 ;;
        --host-build-dir) HOST_BUILD_DIR="$2";     shift 2 ;;
        --jobs)           JOBS="$2";               shift 2 ;;
        --timeout)        SMOKE_TIMEOUT="$2";      shift 2 ;;
        --artifact-root)  HOST_ARTIFACT_ROOT="$2"; shift 2 ;;
        --build-only)     BUILD_ONLY=1;            shift   ;;
        --skip-smoke)     RUN_SMOKE=0;             shift   ;;
        --strict-process) STRICT_PROCESS=1;        shift   ;;
        --help|-h)        usage; exit 0                    ;;
        *) echo "[qemu-smoke] unknown arg: $1" >&2; usage >&2; exit 1 ;;
    esac
done

echo "[qemu-smoke] Starting"
printf '  %-16s: %s\n' \
    "user"           "$SSH_USER" \
    "session-user"   "$SESSION_USER" \
    "repo-dir"       "$REPO_DIR" \
    "build-dir"      "$BUILD_DIR" \
    "host-build-dir" "$HOST_BUILD_DIR" \
    "host-repo-dir"  "$HOST_REPO_DIR" \
    "jobs"           "$JOBS" \
    "build-only"     "$BUILD_ONLY" \
    "run-smoke"      "$RUN_SMOKE"
echo ""

# ── SSH connection pool (ControlMaster) ────────────────────────────────────────
mkdir -p "$STATE_DIR"
KNOWN_HOSTS="$(qemu_dev_known_hosts)"
SSH_CTL="$STATE_DIR/ctl-$$"
SSH_HOST="$SSH_USER@127.0.0.1"

echo "[qemu-smoke] Waiting for guest SSH on port $SSH_PORT..."
if ! qemu_dev_wait_ssh "$SSH_PORT" 90; then
    echo "[qemu-smoke] ERROR: guest SSH tidak siap setelah 3 menit." >&2
    echo "[qemu-smoke] Pastikan VM sudah berjalan via: bash scripts/dev/qemu-run.sh" >&2
    exit 1
fi
echo "[qemu-smoke] Guest SSH ready"
echo ""

SSH_OPTS=(
    -o ControlMaster=auto
    -o "ControlPath=$SSH_CTL"
    -o ControlPersist=60
    -o StrictHostKeyChecking=accept-new
    -o "UserKnownHostsFile=$KNOWN_HOSTS"
    -p "$SSH_PORT"
)
SCP_OPTS=(
    -o ControlMaster=auto
    -o "ControlPath=$SSH_CTL"
    -o ControlPersist=60
    -o StrictHostKeyChecking=accept-new
    -o "UserKnownHostsFile=$KNOWN_HOSTS"
    -P "$SSH_PORT"
)

g_ssh()  { ssh  "${SSH_OPTS[@]}" "$SSH_HOST" "$@"; }
g_scp()  { scp  "${SCP_OPTS[@]}" "$1" "$SSH_HOST:$2"; }
g_scpr() { scp  "${SCP_OPTS[@]}" -r "$SSH_HOST:$1" "$2"; }

# Buka master connection terlebih dahulu; semua g_ssh/g_scp berikutnya reuse ini.
# ControlPersist=3600 supaya master tidak expire selama build lokal berlangsung.
ssh -o ControlMaster=yes -o "ControlPath=$SSH_CTL" -o ControlPersist=3600 \
    -o StrictHostKeyChecking=accept-new -o "UserKnownHostsFile=$KNOWN_HOSTS" \
    -p "$SSH_PORT" -N -f "$SSH_HOST"

cleanup() { ssh -O exit -o "ControlPath=$SSH_CTL" "$SSH_HOST" 2>/dev/null || true; }
trap cleanup EXIT

# ── Mount hostshare di guest (butuh untuk jalur script install/verify) ────────
echo "[qemu-smoke] Mounting hostshare di guest..."
g_scp "$SCRIPT_DIR/qemu-guest-bootstrap.sh" /tmp/qemu-guest-bootstrap.sh
g_ssh -tt "chmod +x /tmp/qemu-guest-bootstrap.sh && sudo /tmp/qemu-guest-bootstrap.sh --mount-only"

# ── Build di host ────────────────────────────────────────────────────────────
echo "[qemu-smoke] Building on host..."
HOST_BUILD_ARGS=(
    --repo-dir "$HOST_REPO_DIR"
    --build-dir "$HOST_BUILD_DIR"
    --jobs "$JOBS"
    --no-run
)
for t in "${RUNTIME_TARGETS[@]}"; do
    HOST_BUILD_ARGS+=(--target "$t")
done
[[ "$BUILD_ONLY" == "1" ]] && HOST_BUILD_ARGS+=(--build-only)
"$SCRIPT_DIR/qemu-guest-build.sh" "${HOST_BUILD_ARGS[@]}"

# ── Sync artifacts ke guest ───────────────────────────────────────────────────
echo "[qemu-smoke] Syncing artifacts ke guest ($BUILD_DIR)..."
rsync -az --delete \
    -e "ssh -p $SSH_PORT -o ControlPath=$SSH_CTL -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$KNOWN_HOSTS" \
    "$HOST_BUILD_DIR/" \
    "$SSH_HOST:$BUILD_DIR/"

# ── Install → Verify ─────────────────────────────────────────────────────────
echo "[qemu-smoke] Install → verify..."
RCMD="sudo SLM_TARGET_USER=$(printf '%q' "$SESSION_USER") bash $(printf '%q' "$REPO_DIR/scripts/login/install-slm-desktop-runtime.sh") $(printf '%q' "$BUILD_DIR")"
RCMD+=" && sudo SLM_TARGET_USER=$(printf '%q' "$SESSION_USER") bash $(printf '%q' "$REPO_DIR/scripts/login/verify-slm-desktop-runtime.sh")"
RCMD+=" && sudo bash $(printf '%q' "$REPO_DIR/scripts/login/verify-greetd-slm.sh")"
g_ssh -tt "$RCMD"

# ── Smoke ─────────────────────────────────────────────────────────────────────
if [[ "$RUN_SMOKE" -eq 0 ]]; then
    echo "[qemu-smoke] install+verify selesai (--skip-smoke)."
    exit 0
fi

echo "[qemu-smoke] Running smoke test..."
TS="$(date -u +%Y%m%dT%H%M%SZ)"
REMOTE_ARTIFACT_DIR="/tmp/slm-qemu-session-smoke-${TS}"
HOST_ARTIFACT_DIR="${HOST_ARTIFACT_ROOT}/${TS}"
mkdir -p "$HOST_ARTIFACT_DIR"

# Upload smoke runner ke guest sebelum dipanggil via sudo.
g_scp "$SCRIPT_DIR/qemu-guest-session-smoke.sh" /tmp/qemu-guest-session-smoke.sh
g_ssh "chmod +x /tmp/qemu-guest-session-smoke.sh"

SCMD="sudo /tmp/qemu-guest-session-smoke.sh"
SCMD+=" --session-user $(printf '%q' "$SESSION_USER")"
SCMD+=" --timeout   $(printf '%q' "$SMOKE_TIMEOUT")"
SCMD+=" --artifact-dir $(printf '%q' "$REMOTE_ARTIFACT_DIR")"
[[ "$STRICT_PROCESS" -eq 1 ]] && SCMD+=" --strict-process"

SMOKE_EXIT=0
g_ssh -tt "$SCMD" || SMOKE_EXIT=$?

g_scpr "$REMOTE_ARTIFACT_DIR" "$HOST_ARTIFACT_DIR/"

ARTIFACT_SUBDIR="$HOST_ARTIFACT_DIR/$(basename "$REMOTE_ARTIFACT_DIR")"
echo ""
echo "[qemu-smoke] Summary:"
cat "$ARTIFACT_SUBDIR/summary.log" 2>/dev/null || true
echo ""
echo "[qemu-smoke] Artifacts: $ARTIFACT_SUBDIR"

exit "$SMOKE_EXIT"
