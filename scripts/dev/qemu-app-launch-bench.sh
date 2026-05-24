#!/usr/bin/env bash
set -euo pipefail

RUNS="${1:-3}"

uid="$(id -u)"
export XDG_RUNTIME_DIR="/run/user/$uid"
export DISPLAY="${DISPLAY:-:0}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$uid/bus}"

measure() {
    local name="$1"
    local cmd="$2"
    local pattern="$3"
    local timeout_steps="${4:-300}" # 300 * 100ms = 30s
    local i

    echo "== $name =="
    for i in $(seq 1 "$RUNS"); do
        pkill -f "$pattern" >/dev/null 2>&1 || true
        sleep 1

        local start_ms end_ms elapsed_ms found=0 n
        start_ms="$(python3 -c 'import time; print(int(time.monotonic()*1000))')"
        nohup bash -lc "$cmd" >"/tmp/slm-bench-${name}-${i}.log" 2>&1 &

        for n in $(seq 1 "$timeout_steps"); do
            if pgrep -af "$pattern" >/dev/null 2>&1; then
                found=1
                break
            fi
            sleep 0.1
        done

        end_ms="$(python3 -c 'import time; print(int(time.monotonic()*1000))')"
        elapsed_ms="$((end_ms - start_ms))"
        if [[ "$found" -eq 1 ]]; then
            echo "run${i}=${elapsed_ms}ms"
        else
            echo "run${i}=TIMEOUT(${elapsed_ms}ms)"
        fi

        pkill -f "$pattern" >/dev/null 2>&1 || true
        sleep 1
    done
}

wait_window_ready() {
    local matcher="$1"
    local timeout_steps="${2:-450}" # 45s
    local i
    for i in $(seq 1 "$timeout_steps"); do
        if command -v qdbus6 >/dev/null 2>&1; then
            if timeout 0.3s qdbus6 org.kde.KWin /KWin org.kde.KWin.queryWindowInfo 2>/dev/null | grep -Eiq "$matcher"; then
                return 0
            fi
        fi
        sleep 0.1
    done
    return 1
}

measure_window_ready() {
    local name="$1"
    local cmd="$2"
    local proc_pattern="$3"
    local window_pattern="$4"
    local i

    echo "== ${name}_window_ready =="
    for i in $(seq 1 "$RUNS"); do
        pkill -f "$proc_pattern" >/dev/null 2>&1 || true
        sleep 1

        local start_ms end_ms elapsed_ms ok=0
        start_ms="$(python3 -c 'import time; print(int(time.monotonic()*1000))')"
        nohup bash -lc "$cmd" >"/tmp/slm-bench-${name}-window-${i}.log" 2>&1 &

        if wait_window_ready "$window_pattern" 450; then
            ok=1
        fi

        end_ms="$(python3 -c 'import time; print(int(time.monotonic()*1000))')"
        elapsed_ms="$((end_ms - start_ms))"
        if [[ "$ok" -eq 1 ]]; then
            echo "run${i}=${elapsed_ms}ms"
        else
            echo "run${i}=TIMEOUT(${elapsed_ms}ms)"
        fi

        pkill -f "$proc_pattern" >/dev/null 2>&1 || true
        sleep 1
    done
}

measure "flatpak_onlyoffice" \
    "flatpak run org.onlyoffice.desktopeditors" \
    "desktopeditors|onlyoffice" \
    450

measure "snap_firefox" \
    "snap run firefox --new-window about:blank" \
    "firefox" \
    450

measure_window_ready "flatpak_onlyoffice" \
    "flatpak run org.onlyoffice.desktopeditors" \
    "desktopeditors|onlyoffice" \
    "onlyoffice|desktopeditors"

measure_window_ready "snap_firefox" \
    "snap run firefox --new-window about:blank" \
    "firefox" \
    "firefox"
