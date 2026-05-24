#!/usr/bin/env bash
set -euo pipefail

LOG_FILE="${1:-log/slm-shell.log}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "[launch-sla-report] missing log file: $LOG_FILE" >&2
    exit 1
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

awk '
/\[launch-sla\] first_window_mapped/ {
    runtime="unknown"
    app="unknown"
    lat=""
    for (i = 1; i <= NF; ++i) {
        if ($i ~ /^runtime=/) { runtime = substr($i, 9) }
        else if ($i ~ /^appId=/) { app = substr($i, 7) }
        else if ($i ~ /^latencyMs=/) { lat = substr($i, 11) }
    }
    if (lat ~ /^[0-9]+$/) {
        printf "%s\t%s\t%s\n", runtime, app, lat
    }
}
' "$LOG_FILE" >"$tmp"

if [[ ! -s "$tmp" ]]; then
    echo "[launch-sla-report] no launch-sla mapped events found in $LOG_FILE"
    exit 0
fi

python3 - "$tmp" <<'PY'
import sys
from collections import defaultdict

path = sys.argv[1]
by_runtime = defaultdict(list)
by_app = defaultdict(list)

with open(path, "r", encoding="utf-8") as f:
    for line in f:
        runtime, app, lat = line.rstrip("\n").split("\t")
        v = int(lat)
        by_runtime[runtime].append(v)
        by_app[(runtime, app)].append(v)

def percentile(sorted_vals, p):
    if not sorted_vals:
        return 0
    idx = int((len(sorted_vals) - 1) * p)
    return sorted_vals[idx]

def summarize(vals):
    s = sorted(vals)
    n = len(s)
    med = s[n // 2] if n % 2 == 1 else (s[n // 2 - 1] + s[n // 2]) // 2
    p95 = percentile(s, 0.95)
    return n, med, p95, s[0], s[-1]

print("== launch-sla runtime summary ==")
for runtime in sorted(by_runtime):
    n, med, p95, mn, mx = summarize(by_runtime[runtime])
    print(f"{runtime}: n={n} median={med}ms p95={p95}ms min={mn}ms max={mx}ms")

print("\n== launch-sla app summary ==")
for (runtime, app) in sorted(by_app):
    n, med, p95, mn, mx = summarize(by_app[(runtime, app)])
    print(f"{runtime}/{app}: n={n} median={med}ms p95={p95}ms min={mn}ms max={mx}ms")
PY
