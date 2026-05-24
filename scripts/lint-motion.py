#!/usr/bin/env python3
"""
lint-motion.py — MotionController consistency linter for QML source files.

Checks                                           Rule ID
─────────────────────────────────────────────────────────────────────────────
1. MotionController.preset() with unknown role   unknown-motion-preset
2. layer.enabled: true without effectsEnabled    unguarded-layer-effect  (WARN)
3. SpringAnimation without Theme physics tokens  raw-spring-params

Exit codes:
  0 — no violations (or all in allowlist)
  1 — violations found
  2 — usage error

Usage:
  python3 scripts/lint-motion.py [all|staged] [--no-allowlist]
"""

from __future__ import annotations

import re
import sys
import subprocess
import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


# ── Valid preset names (must mirror MotionController.qml) ─────────────────────

VALID_PRESETS: frozenset[str] = frozenset({
    "micro", "hover",
    "enter", "exit",
    "standard", "emphasized",
    "spring",
    "page", "workspace",
    "instant",
})


# ── Exclusions ────────────────────────────────────────────────────────────────

_EXCLUDE_SEGMENTS = ("third_party/", "/build/", "node_modules/")
_EXCLUDE_NAMES    = ("MotionController.qml",)

def _is_excluded(path: Path) -> bool:
    s = str(path).replace("\\", "/")
    return (
        any(seg in s for seg in _EXCLUDE_SEGMENTS)
        or any(s.endswith(n) for n in _EXCLUDE_NAMES)
    )


# ── Patterns ──────────────────────────────────────────────────────────────────

_RE_PRESET_CALL        = re.compile(r'MotionController\.preset\(\s*"([^"]+)"\s*\)')
_RE_LAYER_ENABLED_TRUE = re.compile(r'\blayer\.enabled\s*:\s*true\b')
_RE_LAYER_ENABLED_GUARD= re.compile(r'MotionController\.effectsEnabled')
_RE_SPRING_VELOCITY    = re.compile(r'\bSpringAnimation\b')
_RE_SPRING_THEME       = re.compile(r'Theme\.physicsSpring|Theme\.physicsDamping|Theme\.physicsMass')


# ── Comment stripping ─────────────────────────────────────────────────────────

def _strip_comment(line: str) -> str:
    idx = line.find("//")
    return line[:idx] if idx >= 0 else line


# ── Violation ─────────────────────────────────────────────────────────────────

class Violation:
    def __init__(self, path: Path, line: int, rule: str, message: str):
        self.path    = path
        self.line    = line
        self.rule    = rule
        self.message = message

    def __str__(self) -> str:
        return f"{self.path.relative_to(ROOT)}:{self.line}: [{self.rule}] {self.message}"


# ── Per-file analysis ─────────────────────────────────────────────────────────

def check_file(path: Path) -> list[Violation]:
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return []

    raw   = text.splitlines()
    lines = [_strip_comment(l) for l in raw]
    vs: list[Violation] = []

    def add(i: int, rule: str, msg: str) -> None:
        vs.append(Violation(path, i + 1, rule, msg))

    # Track if file uses MotionController.effectsEnabled anywhere
    file_has_effects_guard = any(
        _RE_LAYER_ENABLED_GUARD.search(l) for l in lines
    )

    # Accumulate SpringAnimation blocks for check 3
    in_spring = False
    spring_start = 0
    spring_lines: list[str] = []

    for i, line in enumerate(lines):

        # ── Check 1: unknown preset name ────────────────────────────────────
        for m in _RE_PRESET_CALL.finditer(line):
            role = m.group(1)
            if role not in VALID_PRESETS:
                add(i, "unknown-motion-preset",
                    f'MotionController.preset("{role}") — unknown role; '
                    f'valid: {", ".join(sorted(VALID_PRESETS))}')

        # ── Check 2: unguarded layer.enabled: true ──────────────────────────
        if _RE_LAYER_ENABLED_TRUE.search(line):
            # Only warn if the file does NOT use effectsEnabled at all.
            # Per-line guard check would have too many false positives.
            if not file_has_effects_guard:
                add(i, "unguarded-layer-effect",
                    "layer.enabled: true without MotionController.effectsEnabled guard — "
                    "heavy effects should be conditional on quality tier: "
                    "layer.enabled: MotionController.effectsEnabled")

        # ── Check 3: SpringAnimation with hardcoded numeric spring/damping ──────
        # Flag only when spring/damping/mass are assigned raw numbers.
        # Using a local property that references Theme.physics* is acceptable.
        if _RE_SPRING_VELOCITY.search(line):
            in_spring    = True
            spring_start = i
            spring_lines = [line]
        elif in_spring:
            spring_lines.append(line)
            if "}" in line:
                block = "\n".join(spring_lines)
                raw_spring  = re.search(r'\bspring\s*:\s*[0-9]', block)
                raw_damping = re.search(r'\bdamping\s*:\s*[0-9]', block)
                raw_mass    = re.search(r'\bmass\s*:\s*[0-9]', block)
                if raw_spring or raw_damping or raw_mass:
                    add(spring_start, "raw-spring-params",
                        "SpringAnimation with hardcoded spring/damping/mass — "
                        "use Theme.physicsSpring*/physicsDamping*/physicsMass* tokens")
                in_spring    = False
                spring_lines = []

    return vs


# ── File collection ───────────────────────────────────────────────────────────

def _collect_all() -> list[Path]:
    return sorted(
        p for p in ROOT.rglob("*.qml")
        if not _is_excluded(p)
    )


def _collect_staged() -> list[Path]:
    try:
        out = subprocess.check_output(
            ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "--", "*.qml"],
            cwd=ROOT, text=True,
        )
    except subprocess.CalledProcessError:
        return []
    return sorted(
        ROOT / name for name in out.splitlines()
        if (ROOT / name).exists() and not _is_excluded(ROOT / name)
    )


# ── Allowlist ─────────────────────────────────────────────────────────────────

_ALLOWLIST_FILE = ROOT / "config" / "lint" / "motion-allowlist.txt"

def _load_allowlist() -> set[str]:
    if not _ALLOWLIST_FILE.exists():
        return set()
    entries: set[str] = set()
    for raw in _ALLOWLIST_FILE.read_text().splitlines():
        row = raw.split("#", 1)[0].strip()
        if row:
            entries.add(row)
    return entries

def _is_allowlisted(v: Violation, al: set[str]) -> bool:
    return str(v.path.relative_to(ROOT)) in al or v.rule in al


# ── Main ──────────────────────────────────────────────────────────────────────

_HARD_RULES = {"unknown-motion-preset", "raw-spring-params"}
_WARN_RULES = {"unguarded-layer-effect"}

_RULE_LABELS = {
    "unknown-motion-preset":  "unknown MotionController.preset() role",
    "unguarded-layer-effect": "layer.enabled: true without effectsEnabled guard",
    "raw-spring-params":      "SpringAnimation without Theme physics tokens",
}

def main() -> int:
    p = argparse.ArgumentParser(description="QML motion token linter")
    p.add_argument("mode", nargs="?", default="all",
                   choices=["all", "--all", "staged", "--staged"])
    p.add_argument("--no-allowlist", action="store_true")
    args = p.parse_args()

    prefix = "[lint-motion]"
    staged = args.mode in ("staged", "--staged")

    files = _collect_staged() if staged else _collect_all()
    if not files:
        print(f"{prefix} no QML files {'staged' if staged else 'found'}")
        return 0

    allowlist = set() if args.no_allowlist else _load_allowlist()

    all_vs:            list[Violation] = []
    allowlisted_count: int = 0

    for f in files:
        for v in check_file(f):
            if _is_allowlisted(v, allowlist):
                allowlisted_count += 1
            else:
                all_vs.append(v)

    by_rule: dict[str, list[Violation]] = {}
    for v in all_vs:
        by_rule.setdefault(v.rule, []).append(v)

    rule_order = ["unknown-motion-preset", "raw-spring-params", "unguarded-layer-effect"]
    failed = False

    for rule in rule_order:
        if rule not in by_rule:
            continue
        label    = _RULE_LABELS[rule]
        is_hard  = rule in _HARD_RULES
        severity = "ERROR" if is_hard else "WARN"

        print(f"\n{prefix} [{severity}] {label}")
        print(f"{prefix} {'─' * (len(label) + 10)}")
        for v in sorted(by_rule[rule], key=lambda x: (x.path, x.line)):
            print(f"  {v}")

        if is_hard:
            failed = True

    if allowlisted_count:
        print(f"\n{prefix} [BASELINE] {allowlisted_count} violation(s) suppressed by allowlist")

    if not all_vs:
        print(f"\n{prefix} OK — {len(files)} file(s) checked")
        return 0

    hard_n = sum(len(vs) for r, vs in by_rule.items() if r in _HARD_RULES)
    warn_n = sum(len(vs) for r, vs in by_rule.items() if r in _WARN_RULES)
    print()

    if failed:
        print(
            f"{prefix} FAILED — {hard_n} error(s), {warn_n} warning(s).\n"
            f"{prefix} Use MotionController.preset() role names and Theme physics tokens.",
            file=sys.stderr,
        )
        return 1

    print(f"{prefix} {warn_n} warning(s) — no hard errors.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
