#!/usr/bin/env bash
# scripts/dev/qemu-guest-session-smoke.sh — Kumpulkan oracle login/session smoke di guest.

set -euo pipefail

SESSION_USER=""
TIMEOUT_SEC="${SLM_QEMU_SESSION_SMOKE_TIMEOUT_SEC:-90}"
ARTIFACT_DIR=""
STRICT_PROCESS=0

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-guest-session-smoke.sh --session-user USER [options]

Options:
  --session-user USER   User desktop yang state.json-nya diperiksa
  --timeout SEC         Batas tunggu state healthy. Default: $TIMEOUT_SEC
  --artifact-dir PATH   Direktori artefak di guest
  --strict-process      Gagal bila shell/greeter/broker/watchdog tidak terlihat
  --help                Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --session-user)
            SESSION_USER="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT_SEC="$2"
            shift 2
            ;;
        --artifact-dir)
            ARTIFACT_DIR="$2"
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
            echo "[qemu-guest-session-smoke] unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$SESSION_USER" ]]; then
    echo "[qemu-guest-session-smoke] ERROR: --session-user wajib." >&2
    exit 1
fi

if [[ $EUID -ne 0 ]]; then
    echo "[qemu-guest-session-smoke] ERROR: jalankan via sudo/root." >&2
    exit 1
fi

SESSION_HOME="$(getent passwd "$SESSION_USER" | cut -d: -f6)"
if [[ -z "$SESSION_HOME" || ! -d "$SESSION_HOME" ]]; then
    echo "[qemu-guest-session-smoke] ERROR: home user tidak ditemukan untuk $SESSION_USER" >&2
    exit 1
fi

if [[ -z "$ARTIFACT_DIR" ]]; then
    TS="$(date -u +%Y%m%dT%H%M%SZ)"
    ARTIFACT_DIR="/tmp/slm-qemu-session-smoke-${TS}"
fi

STATE_FILE="${SESSION_HOME}/.config/slm-desktop/state.json"
mkdir -p "$ARTIFACT_DIR"

fail=0
warn=0

ok()   { echo "[OK] $*" | tee -a "$ARTIFACT_DIR/summary.log"; }
ng()   { echo "[FAIL] $*" | tee -a "$ARTIFACT_DIR/summary.log"; fail=$((fail + 1)); }
warnf(){ echo "[WARN] $*" | tee -a "$ARTIFACT_DIR/summary.log"; warn=$((warn + 1)); }

capture_cmd() {
    local name="$1"
    shift
    if "$@" >"$ARTIFACT_DIR/${name}.log" 2>&1; then
        return 0
    fi
    return 1
}

wait_for_healthy_state() {
    local deadline=$((SECONDS + TIMEOUT_SEC))
    while (( SECONDS < deadline )); do
        if [[ -f "$STATE_FILE" ]] \
           && python3 - "$STATE_FILE" >"$ARTIFACT_DIR/state-eval.log" 2>&1 <<'PY'
import json, sys
path = sys.argv[1]
with open(path, "r", encoding="utf-8") as fh:
    data = json.load(fh)
status = data.get("last_boot_status")
pending = data.get("config_pending")
print(f"last_boot_status={status}")
print(f"config_pending={pending}")
sys.exit(0 if status == "healthy" and pending is False else 1)
PY
        then
            return 0
        fi
        sleep 2
    done
    return 1
}

echo "[qemu-guest-session-smoke] session-user=${SESSION_USER}" | tee "$ARTIFACT_DIR/summary.log"
echo "[qemu-guest-session-smoke] state-file=${STATE_FILE}" | tee -a "$ARTIFACT_DIR/summary.log"
echo "[qemu-guest-session-smoke] timeout=${TIMEOUT_SEC}s" | tee -a "$ARTIFACT_DIR/summary.log"

capture_cmd "uname" uname -a || true
capture_cmd "os-release" cat /etc/os-release || true
capture_cmd "greetd-status" systemctl status greetd --no-pager --full || true
capture_cmd "greetd-enabled" systemctl is-enabled greetd || true
capture_cmd "greetd-active" systemctl is-active greetd || true
capture_cmd "journal-greetd" journalctl -b -u greetd --no-pager || true
capture_cmd "journal-user" journalctl -b "_UID=$(id -u "$SESSION_USER")" --no-pager || true
capture_cmd "ps-ef" ps -ef || true
capture_cmd "loginctl" loginctl list-sessions || true
capture_cmd "state-initial" cat "$STATE_FILE" || true

if systemctl is-active greetd >/dev/null 2>&1; then
    ok "greetd active"
else
    ng "greetd tidak aktif"
fi

if [[ -f /etc/greetd/config.toml ]]; then
    ok "/etc/greetd/config.toml ada"
    cp /etc/greetd/config.toml "$ARTIFACT_DIR/greetd-config.toml"
else
    ng "/etc/greetd/config.toml hilang"
fi

if [[ -f /usr/share/wayland-sessions/slm.desktop ]]; then
    ok "/usr/share/wayland-sessions/slm.desktop ada"
    cp /usr/share/wayland-sessions/slm.desktop "$ARTIFACT_DIR/slm.desktop"
else
    ng "/usr/share/wayland-sessions/slm.desktop hilang"
fi

if wait_for_healthy_state; then
    ok "state healthy tercapai"
else
    ng "state healthy tidak tercapai dalam ${TIMEOUT_SEC}s"
fi

if [[ -f "$STATE_FILE" ]]; then
    cp "$STATE_FILE" "$ARTIFACT_DIR/state.json"
else
    ng "state.json tidak ditemukan di ${STATE_FILE}"
fi

PROCESS_MATCHES=0
for pattern in greetd slm-greeter slm-session-broker slm-watchdog appSlm_Desktop slm-shell; do
    if pgrep -af "$pattern" >"$ARTIFACT_DIR/proc-${pattern}.log" 2>&1; then
        ok "process terlihat: $pattern"
        PROCESS_MATCHES=1
    else
        if [[ "$STRICT_PROCESS" -eq 1 ]]; then
            ng "process tidak terlihat: $pattern"
        else
            warnf "process tidak terlihat: $pattern"
        fi
    fi
done

if [[ "$PROCESS_MATCHES" -eq 0 ]]; then
    warnf "tidak ada process oracle yang match; lihat ps-ef.log"
fi

WAYLAND_SOCKET=""
if [[ -d "/run/user/$(id -u "$SESSION_USER")" ]]; then
    WAYLAND_SOCKET="$(find "/run/user/$(id -u "$SESSION_USER")" -maxdepth 1 -type s -name 'wayland-*' | head -n 1 || true)"
fi
if [[ -n "$WAYLAND_SOCKET" ]]; then
    ok "wayland socket terdeteksi: $WAYLAND_SOCKET"
    printf '%s\n' "$WAYLAND_SOCKET" >"$ARTIFACT_DIR/wayland-socket.txt"
else
    warnf "wayland socket tidak terdeteksi"
fi

echo "[qemu-guest-session-smoke] summary fail=${fail} warn=${warn}" | tee -a "$ARTIFACT_DIR/summary.log"
if [[ "$fail" -gt 0 ]]; then
    exit 1
fi
