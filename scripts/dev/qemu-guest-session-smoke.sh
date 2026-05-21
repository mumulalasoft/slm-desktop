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
CRASH_REPORT_FILE="${SESSION_HOME}/.config/slm-desktop/last-crash.json"
RUNTIME_DIR="/run/user/$(id -u "$SESSION_USER")"
mkdir -p "$ARTIFACT_DIR"
SMOKE_STARTED_AT="$(date --iso-8601=seconds)"

fail=0
warn=0
COMPAT_LOG="$ARTIFACT_DIR/compatibility-checklist.log"
COMPAT_JSON="$ARTIFACT_DIR/compatibility-report.json"

ok()   { echo "[OK] $*" | tee -a "$ARTIFACT_DIR/summary.log"; }
ng()   { echo "[FAIL] $*" | tee -a "$ARTIFACT_DIR/summary.log"; fail=$((fail + 1)); }
warnf(){ echo "[WARN] $*" | tee -a "$ARTIFACT_DIR/summary.log"; warn=$((warn + 1)); }

: >"$COMPAT_LOG"
compat_ok() {
    printf '[OK] %s :: %s\n' "$1" "$2" | tee -a "$COMPAT_LOG"
    ok "compat[$1] $2"
}
compat_fail() {
    printf '[FAIL] %s :: %s\n' "$1" "$2" | tee -a "$COMPAT_LOG"
    ng "compat[$1] $2"
}
compat_warn() {
    printf '[WARN] %s :: %s\n' "$1" "$2" | tee -a "$COMPAT_LOG"
    warnf "compat[$1] $2"
}

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

# Process timeline — sample GUI process group every 1s for the wait window so we can
# see exactly which process disappears first when the GUI session dies. Runs in
# background and is reaped before log collection.
PROC_WATCH_LOG="$ARTIFACT_DIR/proc-timeline.log"
# Watch duration must be long enough to capture the GUI death window we are
# investigating (sessions die ~10s post-handoff). Default 30s is well past that.
PROC_WATCH_DURATION="${SLM_QEMU_PROC_WATCH_SEC:-30}"
PROC_WATCH_PID=
PROC_WATCH_START=
start_proc_watch() {
    : >"$PROC_WATCH_LOG"
    PROC_WATCH_START=$SECONDS
    (
        end=$((SECONDS + PROC_WATCH_DURATION))
        while (( SECONDS < end )); do
            ts=$(date '+%H:%M:%S.%3N')
            {
                printf '[%s] sessions:\n' "$ts"
                loginctl list-sessions --no-pager 2>/dev/null \
                    | awk 'NR>1 && NF { print "  " $0 }'
                printf '[%s] processes:\n' "$ts"
                ps -eo pid,ppid,pgid,sid,stat,etime,comm,args --no-headers \
                    2>/dev/null \
                    | grep -E 'kwin_wayland|slm-(shell|session-broker|watchdog|desktop|lockd)|greetd|cage' \
                    | grep -v 'grep\|proc-timeline' \
                    | awk '{ print "  " $0 }'
                printf '\n'
            } >>"$PROC_WATCH_LOG"
            sleep 1
        done
    ) &
    PROC_WATCH_PID=$!
}
stop_proc_watch() {
    if [[ -n "$PROC_WATCH_PID" ]]; then
        kill "$PROC_WATCH_PID" 2>/dev/null || true
        wait "$PROC_WATCH_PID" 2>/dev/null || true
        PROC_WATCH_PID=
    fi
}
start_proc_watch
trap 'stop_proc_watch' EXIT

capture_cmd "uname" uname -a || true
capture_cmd "os-release" cat /etc/os-release || true
capture_cmd "pam-slm" cat /etc/pam.d/slm || true
capture_cmd "pam-login" cat /etc/pam.d/login || true
capture_cmd "slm-greeter-unit" systemctl cat slm-greeter || true
capture_cmd "greetd-status" systemctl status greetd --no-pager --full || true
capture_cmd "greetd-enabled" systemctl is-enabled greetd || true
capture_cmd "greetd-active" systemctl is-active greetd || true
capture_cmd "journal-greetd" journalctl -b -u greetd --no-pager || true
capture_cmd "slm-greeter-service-status" systemctl status slm-greeter --no-pager --full || true
capture_cmd "slm-greeter-service-enabled" systemctl is-enabled slm-greeter || true
capture_cmd "slm-greeter-service-active" systemctl is-active slm-greeter || true
capture_cmd "journal-slm-greeter-service" journalctl -b -u slm-greeter --no-pager || true
capture_cmd "journal-logind" journalctl -b -u systemd-logind --no-pager || true
capture_cmd "journal-user" journalctl -b "_UID=$(id -u "$SESSION_USER")" --no-pager || true
capture_cmd "journal-kwin" journalctl -b _COMM=kwin_wayland --no-pager || true
capture_cmd "journal-slm-portald" journalctl -b "_UID=$(id -u "$SESSION_USER")" -u slm-portald --no-pager || true
capture_cmd "journal-xdg-desktop-portal" journalctl -b "_UID=$(id -u "$SESSION_USER")" -u xdg-desktop-portal --no-pager || true
capture_cmd "journal-xdg-desktop-portal-gtk" journalctl -b "_UID=$(id -u "$SESSION_USER")" -u xdg-desktop-portal-gtk --no-pager || true
capture_cmd "journal-kernel" journalctl -b -k --since "$SMOKE_STARTED_AT" --no-pager || true
capture_cmd "dmesg" dmesg -T --no-pager || true
capture_cmd "coredumpctl-list" coredumpctl list --no-pager || true
capture_cmd "coredumpctl-info-kwin" coredumpctl info kwin_wayland --no-pager || true
capture_cmd "core-pattern" cat /proc/sys/kernel/core_pattern || true
capture_cmd "ulimit-c" bash -lc "ulimit -c" || true
capture_cmd "greetd-log-dir" bash -lc "ls -la /var/lib/greetd/logs 2>/dev/null || true" || true
capture_cmd "greetd-slm-greeter-log" cat /var/lib/greetd/logs/slm-greeter.log || true
capture_cmd "greetd-slm-cage-log" cat /var/lib/greetd/logs/slm-greeter-cage.log || true
capture_cmd "ps-ef" ps -ef || true
capture_cmd "loginctl" loginctl list-sessions || true
capture_cmd "state-initial" cat "$STATE_FILE" || true
capture_cmd "tmp-slm-logs" bash -lc "ls -la /tmp/slm* 2>/dev/null || true" || true
capture_cmd "portal-user-status" runuser -u "$SESSION_USER" -- env XDG_RUNTIME_DIR="$RUNTIME_DIR" systemctl --user status slm-portald xdg-desktop-portal --no-pager --full || true
capture_cmd "portal-user-enabled" runuser -u "$SESSION_USER" -- env XDG_RUNTIME_DIR="$RUNTIME_DIR" systemctl --user is-enabled slm-portald xdg-desktop-portal || true
capture_cmd "portal-config" bash -lc "set -x; cat '$SESSION_HOME/.config/xdg-desktop-portal/portals.conf' 2>/dev/null || true; cat '$SESSION_HOME/.config/xdg-desktop-portal/slm-portals.conf' 2>/dev/null || true; cat '$SESSION_HOME/.local/share/xdg-desktop-portal/portals/slm.portal' 2>/dev/null || true; cat '$SESSION_HOME/.local/share/dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service' 2>/dev/null || true" || true
if [[ -d "$RUNTIME_DIR" ]]; then
    capture_cmd "runtime-dir" find "$RUNTIME_DIR" -maxdepth 1 -printf "%M %u %g %s %p\n" || true
fi

if systemctl is-active slm-greeter >/dev/null 2>&1; then
    ok "slm-greeter service active"
elif systemctl is-active greetd >/dev/null 2>&1; then
    ok "greetd active"
else
    ng "display manager tidak aktif (slm-greeter/greetd)"
fi

if systemctl is-active greetd >/dev/null 2>&1 && [[ -f /etc/greetd/config.toml ]]; then
    ok "/etc/greetd/config.toml ada"
    cp /etc/greetd/config.toml "$ARTIFACT_DIR/greetd-config.toml"
elif systemctl is-active slm-greeter >/dev/null 2>&1; then
    ok "direct-PAM slm-greeter mode aktif"
else
    ng "konfigurasi display manager tidak lengkap"
fi

if [[ -f /usr/share/wayland-sessions/slm.desktop ]]; then
    ok "/usr/share/wayland-sessions/slm.desktop ada"
    cp /usr/share/wayland-sessions/slm.desktop "$ARTIFACT_DIR/slm.desktop"
elif systemctl is-active slm-greeter >/dev/null 2>&1; then
    warnf "/usr/share/wayland-sessions/slm.desktop hilang, direct-PAM tidak memakainya"
else
    ng "/usr/share/wayland-sessions/slm.desktop hilang"
fi

if [[ -f /usr/share/applications/slm-shell.desktop ]]; then
    ok "/usr/share/applications/slm-shell.desktop ada"
    cp /usr/share/applications/slm-shell.desktop "$ARTIFACT_DIR/slm-shell.desktop"
else
    ng "/usr/share/applications/slm-shell.desktop hilang"
fi

if wait_for_healthy_state; then
    ok "state healthy tercapai"
else
    ng "state healthy tidak tercapai dalam ${TIMEOUT_SEC}s"
fi

verify_screenshot_smoke() {
    local required="/tmp/slm-smoke-screenshot-required"
    local result="/tmp/slm-smoke-screenshot-result.json"
    local output="/tmp/slm-smoke-screenshot.png"
    [[ -f "$required" ]] || return 0

    local deadline=$((SECONDS + 30))
    while (( SECONDS < deadline )); do
        [[ -s "$result" ]] && break
        sleep 1
    done

    if [[ ! -s "$result" ]]; then
        ng "screenshot smoke result tidak muncul: $result"
        return 0
    fi
    cp "$result" "$ARTIFACT_DIR/screenshot-smoke-result.json" || true

    if python3 - "$result" "$output" >"$ARTIFACT_DIR/screenshot-smoke-eval.log" 2>&1 <<'PY'
import json
import os
import sys

result_path, output_path = sys.argv[1:3]
with open(result_path, "r", encoding="utf-8") as fh:
    payload = json.load(fh)
if not payload.get("ok"):
    raise SystemExit(f"screenshot failed: {payload.get('error') or payload}")
actual_path = payload.get("path") or output_path
if not os.path.exists(actual_path):
    raise SystemExit(f"screenshot output missing: {actual_path}")
if os.path.getsize(actual_path) <= 0:
    raise SystemExit(f"screenshot output empty: {actual_path}")
print(f"ok path={actual_path} size={os.path.getsize(actual_path)}")
PY
    then
        ok "screenshot smoke berhasil"
        [[ -f "$output" ]] && cp "$output" "$ARTIFACT_DIR/slm-smoke-screenshot.png" || true
    else
        ng "screenshot smoke gagal"
    fi
}

verify_screenshot_smoke

# Compatibility policy validation (session/env/wayland/x11/portal/dbus)
pick_best_session_id() {
    local ids id best="" best_score=-1
    ids="$(loginctl list-sessions --no-legend 2>/dev/null | awk -v user="$SESSION_USER" '$3 == user {print $1}')"
    for id in $ids; do
        local props type active remote state class seat score
        props="$(loginctl show-session "$id" -p Type -p Active -p Remote -p State -p Class -p Seat 2>/dev/null || true)"
        type="$(awk -F= '$1=="Type"{print $2}' <<<"$props" | head -n1)"
        active="$(awk -F= '$1=="Active"{print $2}' <<<"$props" | head -n1)"
        remote="$(awk -F= '$1=="Remote"{print $2}' <<<"$props" | head -n1)"
        state="$(awk -F= '$1=="State"{print $2}' <<<"$props" | head -n1)"
        class="$(awk -F= '$1=="Class"{print $2}' <<<"$props" | head -n1)"
        seat="$(awk -F= '$1=="Seat"{print $2}' <<<"$props" | head -n1)"

        score=0
        # Prefer real graphical user sessions over manager sessions, even when
        # they are in transition state (e.g. Active=no/State=closing).
        [[ "$class" == "user" ]] && score=$((score + 30))
        [[ "$type" == "wayland" ]] && score=$((score + 30))
        [[ "$seat" == "seat0" ]] && score=$((score + 20))
        [[ "$remote" == "no" ]] && score=$((score + 5))
        [[ "$active" == "yes" ]] && score=$((score + 5))
        [[ "$state" == "active" ]] && score=$((score + 5))
        [[ "$id" == c* ]] && score=$((score + 2))

        if (( score > best_score )); then
            best_score=$score
            best="$id"
        fi
    done
    printf '%s' "$best"
}

pick_policy_compliant_session_id() {
    local ids id
    ids="$(loginctl list-sessions --no-legend 2>/dev/null | awk -v user="$SESSION_USER" '$3 == user {print $1}')"
    for id in $ids; do
        local props type active remote state class seat
        props="$(loginctl show-session "$id" -p Type -p Active -p Remote -p State -p Class -p Seat 2>/dev/null || true)"
        type="$(awk -F= '$1=="Type"{print $2}' <<<"$props" | head -n1)"
        active="$(awk -F= '$1=="Active"{print $2}' <<<"$props" | head -n1)"
        remote="$(awk -F= '$1=="Remote"{print $2}' <<<"$props" | head -n1)"
        state="$(awk -F= '$1=="State"{print $2}' <<<"$props" | head -n1)"
        class="$(awk -F= '$1=="Class"{print $2}' <<<"$props" | head -n1)"
        seat="$(awk -F= '$1=="Seat"{print $2}' <<<"$props" | head -n1)"
        if [[ "$type" == "wayland" && "$active" == "yes" && "$remote" == "no" \
           && "$state" == "active" && "$class" == "user" && "$seat" == "seat0" ]]; then
            printf '%s' "$id"
            return 0
        fi
    done
    return 1
}

wait_for_policy_session_id() {
    local timeout_sec=20
    local elapsed=0
    local id=""
    while (( elapsed < timeout_sec )); do
        if id="$(pick_policy_compliant_session_id)"; then
            printf '%s' "$id"
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    return 1
}

SESSION_ID="$(wait_for_policy_session_id || true)"
if [[ -z "$SESSION_ID" ]]; then
    SESSION_ID="$(pick_best_session_id)"
fi
if [[ -z "$SESSION_ID" ]]; then
    compat_fail "session.logind.id" "tidak menemukan loginctl session untuk user $SESSION_USER"
else
    compat_ok "session.logind.id" "session id terdeteksi: $SESSION_ID"
    capture_cmd "loginctl-show-session" loginctl show-session "$SESSION_ID" || true
    capture_cmd "loginctl-show-session-props" \
        loginctl show-session "$SESSION_ID" \
            -p Type -p Active -p Remote -p State -p Class -p Seat || true

    SESSION_PROPS_FILE="$ARTIFACT_DIR/loginctl-show-session-props.log"
    prop() { awk -F= -v k="$1" '$1 == k {print $2}' "$SESSION_PROPS_FILE" 2>/dev/null | head -n 1; }
    TYPE_VAL="$(prop Type)"
    ACTIVE_VAL="$(prop Active)"
    REMOTE_VAL="$(prop Remote)"
    STATE_VAL="$(prop State)"
    CLASS_VAL="$(prop Class)"
    SEAT_VAL="$(prop Seat)"

    [[ "$TYPE_VAL" == "wayland" ]] && compat_ok "session.logind.type" "Type=wayland" \
        || compat_fail "session.logind.type" "Type=$TYPE_VAL (expected wayland)"
    [[ "$ACTIVE_VAL" == "yes" ]] && compat_ok "session.logind.active" "Active=yes" \
        || compat_fail "session.logind.active" "Active=$ACTIVE_VAL (expected yes)"
    [[ "$REMOTE_VAL" == "no" ]] && compat_ok "session.logind.remote" "Remote=no" \
        || compat_fail "session.logind.remote" "Remote=$REMOTE_VAL (expected no)"
    [[ "$STATE_VAL" == "active" ]] && compat_ok "session.logind.state" "State=active" \
        || compat_fail "session.logind.state" "State=$STATE_VAL (expected active)"
    [[ "$CLASS_VAL" == "user" ]] && compat_ok "session.logind.class" "Class=user" \
        || compat_fail "session.logind.class" "Class=$CLASS_VAL (expected user)"
    [[ "$SEAT_VAL" == "seat0" ]] && compat_ok "session.logind.seat" "Seat=seat0" \
        || compat_fail "session.logind.seat" "Seat=$SEAT_VAL (expected seat0)"
fi

if [[ -d "$RUNTIME_DIR" ]]; then
    perm="$(stat -c '%a' "$RUNTIME_DIR" 2>/dev/null || echo '?')"
    owner_uid="$(stat -c '%u' "$RUNTIME_DIR" 2>/dev/null || echo '?')"
    owner_gid="$(stat -c '%g' "$RUNTIME_DIR" 2>/dev/null || echo '?')"
    expected_uid="$(id -u "$SESSION_USER")"
    expected_path="/run/user/${expected_uid}"
    [[ "$RUNTIME_DIR" == "$expected_path" ]] \
        && compat_ok "env.runtime.path" "XDG_RUNTIME_DIR=$RUNTIME_DIR" \
        || compat_fail "env.runtime.path" "XDG_RUNTIME_DIR=$RUNTIME_DIR (expected $expected_path)"
    [[ "$perm" == "700" ]] \
        && compat_ok "env.runtime.perm" "permission=0700" \
        || compat_fail "env.runtime.perm" "permission=$perm (expected 700)"
    [[ "$owner_uid" == "$expected_uid" ]] \
        && compat_ok "env.runtime.owner" "owner uid=$owner_uid gid=$owner_gid" \
        || compat_fail "env.runtime.owner" "owner uid=$owner_uid (expected $expected_uid)"
else
    compat_fail "env.runtime.exists" "runtime dir tidak ada: $RUNTIME_DIR"
fi

WAYLAND0="$RUNTIME_DIR/wayland-0"
if [[ -S "$WAYLAND0" ]]; then
    compat_ok "wayland.socket" "$WAYLAND0 adalah UNIX socket"
elif [[ -L "$WAYLAND0" ]]; then
    target="$(readlink -f "$WAYLAND0" 2>/dev/null || true)"
    if [[ -n "$target" && -S "$target" ]]; then
        compat_ok "wayland.socket" "$WAYLAND0 symlink valid -> $target"
    else
        compat_fail "wayland.socket" "$WAYLAND0 symlink invalid -> ${target:-<empty>}"
    fi
elif [[ -e "$WAYLAND0" ]]; then
    compat_fail "wayland.socket" "$WAYLAND0 ada tapi bukan socket"
else
    compat_fail "wayland.socket" "$WAYLAND0 tidak ditemukan"
fi

if [[ -S "/tmp/.X11-unix/X0" ]]; then
    compat_ok "x11.socket" "/tmp/.X11-unix/X0 tersedia"
else
    compat_fail "x11.socket" "/tmp/.X11-unix/X0 tidak tersedia"
fi

XAUTH_FILE="$(find "$RUNTIME_DIR" -maxdepth 1 -type f \( -name '.mutter-Xwaylandauth.*' -o -name 'xauth_*' -o -name '.Xauthority' \) 2>/dev/null | head -n 1 || true)"
if [[ -z "$XAUTH_FILE" ]]; then
    XAUTH_FILE="$(find /tmp -maxdepth 1 -type f -user "$SESSION_USER" \( -name '.mutter-Xwaylandauth.*' -o -name 'xauth_*' -o -name '.Xauthority' \) 2>/dev/null | head -n 1 || true)"
fi
if [[ -z "$XAUTH_FILE" && -f "$SESSION_HOME/.Xauthority" ]]; then
    XAUTH_FILE="$SESSION_HOME/.Xauthority"
fi
if [[ -n "$XAUTH_FILE" ]]; then
    compat_ok "x11.xauthority" "xauth file terdeteksi: $XAUTH_FILE"
else
    declare -a x11_displays=(":0")
    broker_display="$(grep -Eo 'DISPLAY=:[0-9]+' /tmp/slm-session-broker.log 2>/dev/null | tail -n 1 | cut -d= -f2 || true)"
    if [[ -n "$broker_display" ]]; then
        x11_displays=("$broker_display" "${x11_displays[@]}")
    fi
    while read -r sock_path; do
        sock_name="$(basename "$sock_path")"
        disp=":${sock_name#X}"
        exists=0
        for known in "${x11_displays[@]}"; do
            if [[ "$known" == "$disp" ]]; then
                exists=1
                break
            fi
        done
        if [[ "$exists" -eq 0 ]]; then
            x11_displays+=("$disp")
        fi
    done < <(find /tmp/.X11-unix -maxdepth 1 -type s -name 'X*' 2>/dev/null | sort)

    X11_PROBE_DISPLAY=""
    for disp in "${x11_displays[@]}"; do
        if runuser -u "$SESSION_USER" -- env DISPLAY="$disp" XDG_RUNTIME_DIR="$RUNTIME_DIR" \
                bash -lc "xset q >/dev/null 2>&1"; then
            X11_PROBE_DISPLAY="$disp"
            break
        fi
    done
    if [[ -n "$X11_PROBE_DISPLAY" ]]; then
        compat_ok "x11.xauthority" "xauth file tidak ditemukan, tetapi probe X11 auth berhasil pada DISPLAY=$X11_PROBE_DISPLAY"
    else
        compat_fail "x11.xauthority" "xauth file tidak ditemukan dan probe X11 auth gagal (candidates=${x11_displays[*]})"
    fi
fi

if [[ -S "$RUNTIME_DIR/bus" ]]; then
    compat_ok "dbus.user_socket" "user bus socket tersedia: $RUNTIME_DIR/bus"
else
    compat_fail "dbus.user_socket" "user bus socket tidak tersedia: $RUNTIME_DIR/bus"
fi

if runuser -u "$SESSION_USER" -- env XDG_RUNTIME_DIR="$RUNTIME_DIR" systemctl --user is-active xdg-desktop-portal >/dev/null 2>&1; then
    compat_ok "portal.desktop.active" "xdg-desktop-portal active"
else
    compat_fail "portal.desktop.active" "xdg-desktop-portal tidak active"
fi

PORTAL_BACKEND_ACTIVE=0
for backend_unit in slm-portald xdg-desktop-portal-gtk xdg-desktop-portal-kde; do
    if runuser -u "$SESSION_USER" -- env XDG_RUNTIME_DIR="$RUNTIME_DIR" systemctl --user is-active "$backend_unit" >/dev/null 2>&1; then
        compat_ok "portal.backend.${backend_unit}" "$backend_unit active"
        PORTAL_BACKEND_ACTIVE=1
    fi
done
if [[ "$PORTAL_BACKEND_ACTIVE" -eq 0 ]]; then
    compat_fail "portal.backend.any" "tidak ada backend portal yang active (slm-portald/gtk/kde)"
fi

if runuser -u "$SESSION_USER" -- env XDG_RUNTIME_DIR="$RUNTIME_DIR" busctl --user tree org.freedesktop.portal.Desktop >"$ARTIFACT_DIR/portal-bus-tree.log" 2>&1; then
    compat_ok "portal.dbus.tree" "org.freedesktop.portal.Desktop terdaftar di user bus"
else
    compat_fail "portal.dbus.tree" "org.freedesktop.portal.Desktop tidak tersedia di user bus"
fi

if grep -E 'apparmor="DENIED"|audit\([^)]*\):.*\bDENIED\b' "$ARTIFACT_DIR/journal-kernel.log" >"$ARTIFACT_DIR/kernel-denied.log" 2>/dev/null; then
    # Split known ambient denials vs compatibility-critical denials so smoke
    # output stays actionable and does not drown in baseline kernel noise.
    grep -E 'profile="(fusermount3|unprivileged_userns)"' "$ARTIFACT_DIR/kernel-denied.log" \
        >"$ARTIFACT_DIR/kernel-denied-known-noise.log" 2>/dev/null || true
    grep -Ev 'profile="(fusermount3|unprivileged_userns)"' "$ARTIFACT_DIR/kernel-denied.log" \
        >"$ARTIFACT_DIR/kernel-denied-critical.log" 2>/dev/null || true

    if [[ -s "$ARTIFACT_DIR/kernel-denied-critical.log" ]]; then
        denied_profile=$(sed -n 's/.*profile="\([^"]*\)".*/\1/p' "$ARTIFACT_DIR/kernel-denied-critical.log" | head -n1)
        compat_warn "security.denied" "ditemukan DENIED compatibility-critical (profile=${denied_profile:-unknown}, cek kernel-denied-critical.log)"
    else
        compat_ok "security.denied" "hanya DENIED known-noise (fusermount3/unprivileged_userns)"
    fi
else
    compat_ok "security.denied" "tidak ada AppArmor/audit DENIED di journal kernel"
fi

# Don't stop the watcher early — let it run for its full duration so we capture
# the GUI death event that happens ~10s after handoff (well after healthy).
elapsed=$((SECONDS - PROC_WATCH_START))
remaining=$((PROC_WATCH_DURATION - elapsed))
if (( remaining > 0 )); then
    echo "[qemu-guest-session-smoke] holding ${remaining}s for proc-timeline to capture death window..." \
        | tee -a "$ARTIFACT_DIR/summary.log"
    sleep "$remaining"
fi
stop_proc_watch
trap - EXIT

# Extract first/last sighting of each GUI process so summary shows life span.
if [[ -s "$PROC_WATCH_LOG" ]]; then
    python3 - "$PROC_WATCH_LOG" >"$ARTIFACT_DIR/proc-timeline-summary.log" 2>&1 <<'PY'
import re, sys, collections
ts_re = re.compile(r'^\[(\d{2}:\d{2}:\d{2}\.\d+)\]')
cmd_re = re.compile(r'\b(kwin_wayland|slm-shell|slm-session-broker|slm-watchdog|slm-desktop|slm-lockd|greetd|cage)\b')
first, last = {}, {}
with open(sys.argv[1]) as fh:
    cur_ts = None
    for line in fh:
        m = ts_re.match(line)
        if m:
            cur_ts = m.group(1)
            continue
        if not cur_ts:
            continue
        m = cmd_re.search(line)
        if not m:
            continue
        # Extract PID (first column after leading whitespace)
        parts = line.split()
        pid = parts[0] if parts and parts[0].isdigit() else None
        if not pid:
            continue
        key = (m.group(1), pid)
        first.setdefault(key, cur_ts)
        last[key] = cur_ts
print("comm                pid     first_seen    last_seen     ")
print("------------------- ------- ------------- --------------")
for (comm, pid), t0 in sorted(first.items(), key=lambda kv: kv[1]):
    t1 = last[(comm, pid)]
    print(f"{comm:<19} {pid:<7} {t0:<13} {t1:<13}")
PY
    ok "proc timeline captured ($(wc -l <"$PROC_WATCH_LOG") lines)"
else
    warnf "proc timeline log kosong"
fi

if [[ -f "$STATE_FILE" ]]; then
    cp "$STATE_FILE" "$ARTIFACT_DIR/state.json"
else
    ng "state.json tidak ditemukan di ${STATE_FILE}"
fi

if [[ -f "$CRASH_REPORT_FILE" ]]; then
    cp "$CRASH_REPORT_FILE" "$ARTIFACT_DIR/last-crash.json"
else
    warnf "last-crash.json tidak ditemukan di ${CRASH_REPORT_FILE}"
fi

COMPAT_REPORT_FILE="${SESSION_HOME}/.config/slm-desktop/compatibility-report.json"
if [[ -f "$COMPAT_REPORT_FILE" ]]; then
    cp "$COMPAT_REPORT_FILE" "$ARTIFACT_DIR/slm-compatibility-report.json"
    ok "compatibility report terkumpul: $COMPAT_REPORT_FILE"
else
    warnf "compatibility report tidak ditemukan di ${COMPAT_REPORT_FILE}"
fi

REQUIRED_LOGS=(
    /tmp/slm-compositor.log
    /tmp/slm-shell.log
    /tmp/slm-session-broker.log
    /tmp/slm-session-broker-launch.log
)
OPTIONAL_LOGS=(
    /tmp/slm-smoke-runtime.log
    /tmp/slm-greeter.log
    /tmp/slm-greeter-service.log
    /tmp/slm-portald.log
    /tmp/slm-cache-prewarm.log
)
for log_path in "${REQUIRED_LOGS[@]}" "${OPTIONAL_LOGS[@]}"; do
    if [[ -f "$log_path" ]]; then
        log_name="$(basename "$log_path")"
        cp "$log_path" "$ARTIFACT_DIR/$log_name"
        tail -n 200 "$log_path" >"$ARTIFACT_DIR/${log_name%.log}-tail.log" || true
        ok "log terkumpul: $log_path"
    else
        is_required_log=0
        for required_log in "${REQUIRED_LOGS[@]}"; do
            if [[ "$required_log" == "$log_path" ]]; then
                is_required_log=1
                break
            fi
        done
        if [[ "$is_required_log" -eq 1 ]]; then
            warnf "log wajib tidak ditemukan: $log_path"
        else
            ok "log opsional tidak ditemukan: $log_path"
        fi
    fi
done

PROCESS_MATCHES=0
REQUIRED_PROCESSES=(greetd slm-session-broker slm-watchdog slm-shell slm-portald slm-sharingd xdg-desktop-portal)
OPTIONAL_PROCESSES=(cage slm-greeter slm-desktop)
for pattern in "${REQUIRED_PROCESSES[@]}" "${OPTIONAL_PROCESSES[@]}"; do
    if pgrep -af "$pattern" >"$ARTIFACT_DIR/proc-${pattern}.log" 2>&1; then
        ok "process terlihat: $pattern"
        PROCESS_MATCHES=1
    else
        is_required_process=0
        for required_process in "${REQUIRED_PROCESSES[@]}"; do
            if [[ "$required_process" == "$pattern" ]]; then
                is_required_process=1
                break
            fi
        done
        if [[ "$STRICT_PROCESS" -eq 1 && "$is_required_process" -eq 1 ]]; then
            ng "process wajib tidak terlihat: $pattern"
        elif [[ "$is_required_process" -eq 1 ]]; then
            warnf "process wajib tidak terlihat: $pattern"
        else
            ok "process opsional tidak terlihat: $pattern"
        fi
    fi
done

if [[ "$PROCESS_MATCHES" -eq 0 ]]; then
    warnf "tidak ada process oracle yang match; lihat ps-ef.log"
fi

WAYLAND_SOCKET=""
if [[ -d "$RUNTIME_DIR" ]]; then
    WAYLAND_SOCKET="$(find "$RUNTIME_DIR" -maxdepth 1 -type s -name 'wayland-*' 2>/dev/null | head -n 1 || true)"
fi
if [[ -n "$WAYLAND_SOCKET" ]]; then
    ok "wayland socket terdeteksi: $WAYLAND_SOCKET"
    printf '%s\n' "$WAYLAND_SOCKET" >"$ARTIFACT_DIR/wayland-socket.txt"
else
    warnf "wayland socket tidak terdeteksi"
fi

python3 - "$COMPAT_LOG" "$COMPAT_JSON" <<'PY'
import json
import sys
from datetime import datetime, timezone

src, dst = sys.argv[1], sys.argv[2]
entries = []
counts = {"ok": 0, "warn": 0, "fail": 0}

with open(src, "r", encoding="utf-8") as fh:
    for raw in fh:
        line = raw.strip()
        if not line:
            continue
        # Format: [OK] check.id :: message
        if not line.startswith("[") or "]" not in line:
            continue
        status = line[1:line.index("]")].lower()
        rest = line[line.index("]") + 1 :].strip()
        if "::" in rest:
            check_id, message = [part.strip() for part in rest.split("::", 1)]
        else:
            check_id, message = rest, ""
        if status in counts:
            counts[status] += 1
        entries.append(
            {
                "status": status,
                "checkId": check_id,
                "message": message,
            }
        )

matrix = [
    {"category": "GTK3 app", "example": "gedit", "status": "manual-test-required"},
    {"category": "GTK4 app", "example": "gnome-text-editor", "status": "manual-test-required"},
    {"category": "Qt6 app", "example": "qt6ct", "status": "manual-test-required"},
    {"category": "Electron app", "example": "Slack/Discord", "status": "manual-test-required"},
    {"category": "Chromium", "example": "chromium", "status": "manual-test-required"},
    {"category": "VSCode", "example": "code", "status": "manual-test-required"},
    {"category": "Steam", "example": "steam", "status": "manual-test-required"},
    {"category": "Snap Firefox", "example": "firefox", "status": "manual-test-required"},
    {"category": "Flatpak app", "example": "org.onlyoffice.desktopeditors", "status": "manual-test-required"},
    {"category": "X11 legacy app", "example": "xterm/xeyes", "status": "manual-test-required"},
]

payload = {
    "policy": "SLM Desktop Compatibility Policy",
    "timestampUtc": datetime.now(timezone.utc).isoformat(),
    "summary": {
        "ok": counts["ok"],
        "warn": counts["warn"],
        "fail": counts["fail"],
    },
    "checks": entries,
    "matrix": matrix,
    "requiredManualChecks": [
        "launch",
        "render",
        "input",
        "clipboard",
        "file-picker",
        "drag-drop",
        "notifications",
    ],
}

with open(dst, "w", encoding="utf-8") as out:
    json.dump(payload, out, indent=2)
PY
if [[ -f "$COMPAT_JSON" ]]; then
    ok "compatibility JSON report generated: $COMPAT_JSON"
else
    warnf "gagal generate compatibility JSON report"
fi

echo "[qemu-guest-session-smoke] summary fail=${fail} warn=${warn}" | tee -a "$ARTIFACT_DIR/summary.log"
if [[ "$fail" -gt 0 ]]; then
    exit 1
fi
