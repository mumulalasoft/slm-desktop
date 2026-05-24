#!/bin/sh
set -eu

LOG_PATH="${SLM_KWIN_LOG_PATH:-/tmp/slm-compositor.log}"
SOCKET_NAME="${SLM_KWIN_SOCKET:-slm-wayland-0}"

if ! command -v kwin_wayland >/dev/null 2>&1; then
    echo "kwin_only_debug: kwin_wayland not found in PATH" >&2
    echo "kwin_only_debug: install it with: sudo apt install kwin-wayland" >&2
    exit 127
fi

runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export XDG_RUNTIME_DIR="$runtime_dir"
export XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-wayland}"
export XDG_CURRENT_DESKTOP="${XDG_CURRENT_DESKTOP:-SLM}"
export LIBSEAT_BACKEND="${LIBSEAT_BACKEND:-logind}"

if [ -z "${KWIN_DRM_DEVICES:-}" ]; then
    for card in /dev/dri/card0 /dev/dri/card1 /dev/dri/card2 /dev/dri/card3; do
        if [ -e "$card" ]; then
            export KWIN_DRM_DEVICES="$card"
            break
        fi
    done
fi

socket_flag=""
if kwin_wayland --help 2>&1 | grep -q -- '--socket'; then
    socket_flag="--socket"
elif kwin_wayland --help 2>&1 | grep -q -- '--wayland-display'; then
    socket_flag="--wayland-display"
fi

rm -f "$runtime_dir/$SOCKET_NAME"
: > "$LOG_PATH"

echo "kwin_only_debug: XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
echo "kwin_only_debug: KWIN_DRM_DEVICES=${KWIN_DRM_DEVICES:-<unset>}"
echo "kwin_only_debug: log=$LOG_PATH"

if [ -n "$socket_flag" ]; then
    echo "kwin_only_debug: exec kwin_wayland --no-lockscreen $socket_flag $SOCKET_NAME"
    exec kwin_wayland --no-lockscreen "$socket_flag" "$SOCKET_NAME" >>"$LOG_PATH" 2>&1
fi

echo "kwin_only_debug: exec kwin_wayland --no-lockscreen"
exec kwin_wayland --no-lockscreen >>"$LOG_PATH" 2>&1
