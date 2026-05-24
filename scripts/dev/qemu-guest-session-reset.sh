#!/usr/bin/env bash
# scripts/dev/qemu-guest-session-reset.sh — Reset sesi grafis guest secara deterministik.
#
# Tujuan:
# - hentikan sesi lama user target (mencegah sesi ganda)
# - pastikan greetd aktif lagi
# - tunggu sesi logind policy-compliant (wayland/user/seat0 active)
# - tunggu socket /run/user/<uid>/wayland-0 siap

set -euo pipefail

SESSION_USER=""
TIMEOUT_SEC="${SLM_QEMU_SESSION_RESET_TIMEOUT_SEC:-45}"
CHECK_ONLY=0

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-guest-session-reset.sh --session-user USER [options]

Options:
  --session-user USER   User sesi desktop target (wajib)
  --timeout SEC         Batas tunggu sesi aktif + socket wayland-0. Default: $TIMEOUT_SEC
  --check-only          Hanya cek sesi policy-compliant + socket wayland tanpa reset
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
        --check-only)
            CHECK_ONLY=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[qemu-session-reset] unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

log() {
    echo "[qemu-session-reset] $*"
}

if [[ $EUID -ne 0 ]]; then
    echo "[qemu-session-reset] ERROR: jalankan via sudo/root." >&2
    exit 1
fi

if [[ -z "$SESSION_USER" ]]; then
    echo "[qemu-session-reset] ERROR: --session-user wajib." >&2
    exit 1
fi

if ! id -u "$SESSION_USER" >/dev/null 2>&1; then
    echo "[qemu-session-reset] ERROR: user tidak ditemukan: $SESSION_USER" >&2
    exit 1
fi

SESSION_UID="$(id -u "$SESSION_USER")"
RUNTIME_DIR="/run/user/${SESSION_UID}"

session_prop() {
    local sid="$1"
    local key="$2"
    loginctl show-session "$sid" -p "$key" --value 2>/dev/null | head -n 1
}

pick_policy_session_id() {
    local sid type active remote state class seat
    while read -r sid; do
        [[ -n "$sid" ]] || continue
        type="$(session_prop "$sid" Type)"
        active="$(session_prop "$sid" Active)"
        remote="$(session_prop "$sid" Remote)"
        state="$(session_prop "$sid" State)"
        class="$(session_prop "$sid" Class)"
        seat="$(session_prop "$sid" Seat)"
        if [[ "$type" == "wayland" && "$active" == "yes" && "$remote" == "no" \
           && "$state" == "active" && "$class" == "user" && "$seat" == "seat0" ]]; then
            printf '%s' "$sid"
            return 0
        fi
    done < <(loginctl list-sessions --no-legend 2>/dev/null | awk -v user="$SESSION_USER" '$3 == user {print $1}')
    return 1
}

wait_policy_session_id() {
    local timeout="$1"
    local elapsed=0
    local sid=""
    while (( elapsed < timeout )); do
        if sid="$(pick_policy_session_id)"; then
            printf '%s' "$sid"
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    return 1
}

check_policy_session_ready() {
    local sid=""
    sid="$(pick_policy_session_id || true)"
    if [[ -z "$sid" ]]; then
        return 1
    fi
    if [[ ! -S "$RUNTIME_DIR/wayland-0" ]]; then
        return 1
    fi
    log "check-only: session ready id=$sid"
    loginctl show-session "$sid" -p Type -p Active -p Remote -p State -p Class -p Seat --no-pager || true
    return 0
}

is_local_graphical_session() {
    local sid="$1"
    local remote class type seat
    remote="$(session_prop "$sid" Remote)"
    class="$(session_prop "$sid" Class)"
    type="$(session_prop "$sid" Type)"
    seat="$(session_prop "$sid" Seat)"
    [[ "$remote" == "no" && "$class" == "user" && ( "$type" == "wayland" || "$seat" == "seat0" ) ]]
}

ensure_greetd_initial_session_for_smoke() {
    local cfg="/etc/greetd/config.toml"
    local smoke_env=""
    [[ -f "$cfg" ]] || return 1
    if grep -Eq '^[[:space:]]*\[initial_session\][[:space:]]*$' "$cfg"; then
        return 1
    fi
    if [[ -f /tmp/slm-smoke-screenshot-required ]]; then
        smoke_env=" SLM_SMOKE_SCREENSHOT_RESULT=/tmp/slm-smoke-screenshot-result.json SLM_SMOKE_SCREENSHOT_OUTPUT=/tmp/slm-smoke-screenshot.png"
    fi

    log "injecting temporary greetd [initial_session] for smoke autologin"
    cat >>"$cfg" <<EOF

[initial_session]
command = "/usr/bin/env XDG_SESSION_TYPE=wayland XDG_SESSION_CLASS=user XDG_CURRENT_DESKTOP=SLM XDG_SESSION_DESKTOP=SLM DESKTOP_SESSION=slm XDG_SEAT=seat0${smoke_env} /usr/local/libexec/slm-session-broker-launch --mode normal"
user = "$SESSION_USER"
EOF
    return 0
}

reset_greetd_initial_session_guard() {
    # greetd may skip [initial_session] when this run marker exists.
    rm -f /run/greetd.run 2>/dev/null || true
}

if [[ "$CHECK_ONLY" -eq 1 ]]; then
    if check_policy_session_ready; then
        exit 0
    fi
    echo "[qemu-session-reset] check-only: policy session/socket belum siap." >&2
    loginctl list-sessions --no-pager >&2 || true
    exit 1
fi

log "stopping greetd to avoid autologin race"
systemctl stop greetd.service 2>/dev/null || true
if systemctl list-unit-files --no-legend 2>/dev/null | awk '{print $1}' | grep -qx "display-manager.service"; then
    systemctl stop display-manager.service 2>/dev/null || true
fi

log "terminating existing sessions for user '$SESSION_USER'"
while read -r sid; do
    [[ -n "$sid" ]] || continue
    if is_local_graphical_session "$sid"; then
        log "terminate graphical session $sid"
        loginctl terminate-session "$sid" 2>/dev/null || true
    else
        log "skip non-graphical session $sid"
    fi
done < <(loginctl list-sessions --no-legend 2>/dev/null | awk -v user="$SESSION_USER" '$3 == user {print $1}')

sleep 1

for proc_name in slm-session-broker slm-shell slm-shell.real slm-watchdog slm-desktop slm-lockd kwin_wayland; do
    pkill -TERM -u "$SESSION_USER" -x "$proc_name" 2>/dev/null || true
done
sleep 0.5
for proc_name in slm-session-broker slm-shell slm-shell.real slm-watchdog slm-desktop slm-lockd kwin_wayland; do
    pkill -KILL -u "$SESSION_USER" -x "$proc_name" 2>/dev/null || true
done

if [[ -d "$RUNTIME_DIR" ]]; then
    rm -f \
        "$RUNTIME_DIR/wayland-0" \
        "$RUNTIME_DIR/wayland-0.lock" \
        "$RUNTIME_DIR"/slm-wayland-* \
        "$RUNTIME_DIR"/slm-wayland-*.lock 2>/dev/null || true
fi

log "starting greetd"
reset_greetd_initial_session_guard
systemctl reset-failed greetd.service 2>/dev/null || true
systemctl start greetd.service
if ! systemctl is-active greetd >/dev/null 2>&1; then
    echo "[qemu-session-reset] ERROR: greetd gagal aktif setelah restart." >&2
    systemctl status greetd --no-pager --full >&2 || true
    exit 1
fi

log "waiting for policy-compliant session (timeout=${TIMEOUT_SEC}s)"
SESSION_ID="$(wait_policy_session_id "$TIMEOUT_SEC" || true)"
if [[ -z "$SESSION_ID" ]]; then
    if ensure_greetd_initial_session_for_smoke; then
        log "retrying with greetd initial_session autologin enabled"
        reset_greetd_initial_session_guard
        systemctl reset-failed greetd.service 2>/dev/null || true
        systemctl restart greetd.service
        sleep 1
        SESSION_ID="$(wait_policy_session_id "$TIMEOUT_SEC" || true)"
    fi
fi
if [[ -z "$SESSION_ID" ]]; then
    echo "[qemu-session-reset] ERROR: sesi wayland/user/seat0 aktif tidak muncul dalam ${TIMEOUT_SEC}s." >&2
    loginctl list-sessions --no-pager >&2 || true
    exit 1
fi

deadline=$((SECONDS + TIMEOUT_SEC))
while (( SECONDS < deadline )); do
    if [[ -S "$RUNTIME_DIR/wayland-0" ]]; then
        break
    fi
    sleep 1
done
if [[ ! -S "$RUNTIME_DIR/wayland-0" ]]; then
    echo "[qemu-session-reset] ERROR: socket $RUNTIME_DIR/wayland-0 tidak siap." >&2
    ls -la "$RUNTIME_DIR" >&2 || true
    exit 1
fi

log "session ready id=$SESSION_ID"
loginctl show-session "$SESSION_ID" -p Type -p Active -p Remote -p State -p Class -p Seat --no-pager || true
log "wayland socket ready: $RUNTIME_DIR/wayland-0"
