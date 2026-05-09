#!/usr/bin/env bash
# scripts/dev/capture-gui-death.sh — Practical GUI session death capture.
#
# Usage:
#   capture-gui-death.sh                start watcher (default)
#   capture-gui-death.sh stop           stop watcher and finalize summary
#   capture-gui-death.sh status         show whether watcher is running
#   capture-gui-death.sh show           print latest summary + forensics
#   capture-gui-death.sh help           this help
#
# Output:
#   ~/slm-debug/gui-death/<timestamp>/
#   ~/slm-debug/gui-death/latest         (symlink to most recent run)
#
# Behavior:
#   * Watcher samples target processes every 0.5s.
#   * Auto-stops the moment slm-session-broker or kwin_wayland disappears
#     (override with SLM_CAPTURE_KEEP_GOING=1).
#   * Captures journalctl/dmesg/loginctl forensics at each death.
#   * Caches sudo upfront so per-sample journalctl calls don't prompt.
#   * Survives terminal close (uses setsid + nohup-like detach).

set -uo pipefail

OUT_ROOT="${SLM_CAPTURE_ROOT:-$HOME/slm-debug/gui-death}"
LATEST_LINK="$OUT_ROOT/latest"
PID_FILE="$OUT_ROOT/.capture.pid"
META_FILE="$OUT_ROOT/.capture.meta"

SAMPLE_INTERVAL="${SLM_CAPTURE_INTERVAL:-0.5}"
MAX_DURATION="${SLM_CAPTURE_MAX:-600}"
TARGETS_REGEX='kwin_wayland|slm-(shell|session-broker|watchdog|desktop|lockd)|greetd|cage'
SESSION_DEATH_REGEX='kwin_wayland|slm-session-broker'
KEEP_GOING="${SLM_CAPTURE_KEEP_GOING:-0}"

print_help() {
    sed -n '2,21p' "$0" | sed 's/^# \?//'
}

is_running() {
    [[ -f "$PID_FILE" ]] || return 1
    local pid
    pid=$(cat "$PID_FILE" 2>/dev/null) || return 1
    [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null
}

cmd_status() {
    if is_running; then
        local pid out
        pid=$(cat "$PID_FILE")
        out=$(readlink -f "$LATEST_LINK" 2>/dev/null || echo "?")
        echo "[capture] running pid=$pid out=$out"
        return 0
    fi
    echo "[capture] not running"
    if [[ -L "$LATEST_LINK" ]]; then
        echo "  latest run: $(readlink -f "$LATEST_LINK")"
    fi
    return 1
}

cmd_stop() {
    if ! is_running; then
        echo "[capture] not running" >&2
        return 1
    fi
    local pid
    pid=$(cat "$PID_FILE")
    echo "[capture] stopping pid=$pid"
    kill -TERM "$pid" 2>/dev/null || true
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        sleep 0.3
    done
    if kill -0 "$pid" 2>/dev/null; then
        kill -KILL "$pid" 2>/dev/null || true
    fi
    rm -f "$PID_FILE"
    local out
    out=$(readlink -f "$LATEST_LINK" 2>/dev/null || true)
    if [[ -n "$out" && -d "$out" ]]; then
        generate_summary "$out/proc-timeline.log" "$out/proc-timeline-summary.log"
        echo "[capture] summary: $out/proc-timeline-summary.log"
        echo "[capture] deaths : $out/death-forensics.log"
    fi
}

cmd_show() {
    local out
    out=$(readlink -f "$LATEST_LINK" 2>/dev/null || true)
    if [[ -z "$out" || ! -d "$out" ]]; then
        echo "[capture] no previous run found at $LATEST_LINK" >&2
        return 1
    fi
    echo "=== $out ==="
    echo
    echo "----- proc-timeline-summary.log -----"
    cat "$out/proc-timeline-summary.log" 2>/dev/null || echo "(no summary; run '$0 stop' to generate)"
    echo
    echo "----- death-forensics.log -----"
    cat "$out/death-forensics.log" 2>/dev/null || echo "(no forensics yet)"
}

# Forensic snapshot — runs whenever a tracked process disappears.
snapshot_death() {
    local comm="$1" pid="$2" last_ts="$3" out_dir="$4" sudo_p="$5"
    {
        echo "================================================================"
        echo "DEATH: comm=$comm pid=$pid last_seen=$last_ts (now=$(date '+%H:%M:%S.%3N'))"
        echo "----------------------------------------------------------------"
        echo "[journal-kernel last 20]"
        $sudo_p journalctl -k --no-pager -n 20 2>&1 | sed 's/^/  /'
        echo "[journal-logind last 10]"
        $sudo_p journalctl -u systemd-logind --no-pager -n 10 2>&1 | sed 's/^/  /'
        echo "[journal-greetd last 10]"
        $sudo_p journalctl -u greetd --no-pager -n 10 2>&1 | sed 's/^/  /'
        echo "[journal-user last 20]"
        $sudo_p journalctl _UID="$(id -u)" --no-pager -n 20 2>&1 | sed 's/^/  /'
        echo "[dmesg last 10]"
        $sudo_p dmesg --ctime --no-pager 2>/dev/null | tail -10 | sed 's/^/  /'
        echo "[audit signal last 10 (if auditd)]"
        $sudo_p ausearch -k sigterm_target --raw 2>/dev/null | tail -10 | sed 's/^/  /' || echo "  audit not configured"
        echo "[loginctl sessions]"
        loginctl list-sessions --no-pager 2>/dev/null | sed 's/^/  /'
        echo
    } >>"$out_dir/death-forensics.log"
}

generate_summary() {
    local timeline="$1" summary="$2"
    [[ -f "$timeline" ]] || return 0
    python3 - "$timeline" >"$summary" 2>&1 <<'PY'
import re, sys
ts_re = re.compile(r'^\[(\d{2}:\d{2}:\d{2}\.\d+)\]')
cmd_re = re.compile(r'\b(kwin_wayland|slm-shell|slm-session-broker|slm-watchdog|slm-desktop|slm-lockd|greetd|cage)\b')
first, last = {}, {}
with open(sys.argv[1]) as fh:
    cur_ts = None
    for line in fh:
        m = ts_re.match(line)
        if m:
            cur_ts = m.group(1); continue
        if not cur_ts: continue
        m = cmd_re.search(line)
        if not m: continue
        parts = line.split()
        pid = parts[0] if parts and parts[0].isdigit() else None
        if not pid: continue
        key = (m.group(1), pid)
        first.setdefault(key, cur_ts)
        last[key] = cur_ts
print("comm                pid     first_seen     last_seen      lifespan")
print("------------------- ------- -------------- -------------- ---------")
def to_sec(t):
    h, m, rest = t.split(':'); s, ms = rest.split('.')
    return int(h)*3600 + int(m)*60 + int(s) + int(ms)/1000.0
for (comm, pid), t0 in sorted(first.items(), key=lambda kv: to_sec(kv[1])):
    t1 = last[(comm, pid)]
    span = to_sec(t1) - to_sec(t0)
    print(f"{comm:<19} {pid:<7} {t0:<14} {t1:<14} {span:6.1f}s")
PY
}

watch_loop() {
    local out_dir="$1" sudo_p="$2"
    local timeline="$out_dir/proc-timeline.log"
    local deaths="$out_dir/death-forensics.log"
    local run_log="$out_dir/capture.log"
    : >"$timeline"; : >"$deaths"
    echo "[capture] started pid=$$ out=$out_dir target='$TARGETS_REGEX' keep_going=$KEEP_GOING" >>"$run_log"

    declare -A LAST_SEEN FIRST_SEEN MISSES
    local end ts pid comm key misses session_dead=0
    end=$(awk -v d="$MAX_DURATION" 'BEGIN{print systime()+d}')

    while (( $(date +%s) < end )); do
        ts=$(date '+%H:%M:%S.%3N')
        declare -A CURRENT=()

        while IFS= read -r line; do
            [[ -z "$line" ]] && continue
            pid=$(awk '{print $1}' <<<"$line")
            comm=$(awk '{print $7}' <<<"$line")
            [[ -z "$pid" || -z "$comm" ]] && continue
            CURRENT["$comm:$pid"]=1
            FIRST_SEEN["$comm:$pid"]="${FIRST_SEEN[$comm:$pid]:-$ts}"
            LAST_SEEN["$comm:$pid"]="$ts"
            MISSES["$comm:$pid"]=0
        done < <(
            ps -eo pid,ppid,pgid,sid,stat,etime,comm,args --no-headers 2>/dev/null \
                | grep -E "$TARGETS_REGEX" \
                | grep -v 'grep\|capture-gui-death\|proc-timeline'
        )

        {
            printf '[%s] sessions:\n' "$ts"
            loginctl list-sessions --no-pager 2>/dev/null | awk 'NR>1 && NF { print "  " $0 }'
            printf '[%s] processes:\n' "$ts"
            ps -eo pid,ppid,pgid,sid,stat,etime,comm,args --no-headers 2>/dev/null \
                | grep -E "$TARGETS_REGEX" \
                | grep -v 'grep\|capture-gui-death\|proc-timeline' \
                | awk '{ print "  " $0 }'
            printf '\n'
        } >>"$timeline"

        for key in "${!LAST_SEEN[@]}"; do
            if [[ -z "${CURRENT[$key]:-}" ]]; then
                misses=${MISSES[$key]:-0}
                misses=$((misses + 1))
                MISSES["$key"]=$misses
                if (( misses == 2 )); then
                    comm="${key%%:*}"; pid="${key##*:}"
                    snapshot_death "$comm" "$pid" "${LAST_SEEN[$key]}" "$out_dir" "$sudo_p"
                    if [[ "$comm" =~ ^($SESSION_DEATH_REGEX)$ ]]; then
                        echo "[capture] session-critical process '$comm' (pid=$pid) died — auto-stop" >>"$run_log"
                        session_dead=1
                    fi
                fi
            fi
        done

        if [[ "$session_dead" == "1" && "$KEEP_GOING" != "1" ]]; then
            sleep 1
            break
        fi

        sleep "$SAMPLE_INTERVAL"
    done

    generate_summary "$timeline" "$out_dir/proc-timeline-summary.log"
    echo "[capture] done at $(date '+%H:%M:%S')" >>"$run_log"
    rm -f "$PID_FILE"
}

cmd_start() {
    if is_running; then
        echo "[capture] already running (pid $(cat "$PID_FILE"))" >&2
        echo "  use '$0 stop' to terminate it first" >&2
        return 1
    fi
    rm -f "$PID_FILE"

    local sudo_p=""
    if ! journalctl -k -n 1 >/dev/null 2>&1; then
        echo "[capture] journalctl/dmesg need privileges — caching sudo..."
        if ! sudo -v; then
            echo "[capture] sudo unavailable — aborting" >&2
            return 1
        fi
        sudo_p="sudo -n"
        # Background sudo refresher: keeps timestamp warm while watcher runs.
        (
            while [[ -f "$PID_FILE" ]]; do
                sudo -n -v 2>/dev/null || exit 0
                sleep 60
            done
        ) </dev/null >/dev/null 2>&1 &
        disown 2>/dev/null || true
    fi

    local ts out_dir
    ts=$(date '+%Y%m%dT%H%M%SZ')
    out_dir="$OUT_ROOT/$ts"
    mkdir -p "$out_dir"
    ln -snf "$out_dir" "$LATEST_LINK"

    {
        echo "ts=$ts"
        echo "out_dir=$out_dir"
        echo "sudo=$([[ -n "$sudo_p" ]] && echo yes || echo no)"
        echo "keep_going=$KEEP_GOING"
    } >"$META_FILE"

    setsid bash -c "
        echo \$\$ >'$PID_FILE'
        trap 'rm -f \"$PID_FILE\"' EXIT
        $(declare -f snapshot_death)
        $(declare -f generate_summary)
        $(declare -f watch_loop)
        TARGETS_REGEX=$(printf %q "$TARGETS_REGEX")
        SESSION_DEATH_REGEX=$(printf %q "$SESSION_DEATH_REGEX")
        SAMPLE_INTERVAL=$(printf %q "$SAMPLE_INTERVAL")
        MAX_DURATION=$(printf %q "$MAX_DURATION")
        KEEP_GOING=$(printf %q "$KEEP_GOING")
        PID_FILE=$(printf %q "$PID_FILE")
        watch_loop $(printf %q "$out_dir") $(printf %q "$sudo_p")
    " </dev/null >>"$out_dir/capture.log" 2>&1 &
    disown 2>/dev/null || true

    # Wait briefly for the child to write its PID.
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        [[ -f "$PID_FILE" ]] && break
        sleep 0.1
    done

    cat <<EOF
[capture-gui-death] started
  out:        $out_dir
  latest:     $LATEST_LINK
  pid:        $(cat "$PID_FILE" 2>/dev/null || echo '?')
  max watch:  ${MAX_DURATION}s (auto-stops sooner on broker/kwin death)

Now do whatever triggers the GUI session (login, VT switch, etc.).
The watcher will stop itself the moment broker/kwin die.

When done:
  $0 show       # print summary + forensics
  $0 stop       # stop early (also generates summary)
  $0 status     # check if still running
EOF
}

cmd="${1:-start}"
case "$cmd" in
    -h|--help|help)  print_help ;;
    start|"")        cmd_start ;;
    stop)            cmd_stop ;;
    status)          cmd_status ;;
    show)            cmd_show ;;
    *) echo "unknown command: $cmd (try '$0 help')" >&2; exit 1 ;;
esac
