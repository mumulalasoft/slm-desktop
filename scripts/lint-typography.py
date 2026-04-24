#!/usr/bin/env python3
"""
lint-typography.py — Typography token consistency linter for QML source files.

Checks                                     Rule ID
─────────────────────────────────────────────────────────────────────────────
1. font.family assigned a string literal   hardcoded-font-family
2. font.letterSpacing assigned a number    hardcoded-letter-spacing
3. Theme.fontSize() with unknown token     unknown-font-size-token
4. Theme.fontWeight() with unknown token   unknown-font-weight-token
5. Theme.textStyle() with unknown role     unknown-text-style-role
6. font.family + pixelSize + weight        triple-font-smell
   in the same block (TypeLabel candidate)

Exit codes:
  0 — no violations (or all in allowlist)
  1 — violations found
  2 — usage error

Usage:
  python3 scripts/lint-typography.py [all|staged] [--no-allowlist]
"""

from __future__ import annotations

import re
import sys
import subprocess
import argparse
from pathlib import Path
from dataclasses import dataclass, field


# ── Root ──────────────────────────────────────────────────────────────────────

ROOT = Path(__file__).resolve().parent.parent


# ── Known-good token sets ─────────────────────────────────────────────────────

VALID_FONT_SIZE_TOKENS: frozenset[str] = frozenset({
    # Canonical macOS HIG role names (preferred)
    "micro", "tiny", "xs", "sm", "small", "menu", "smallPlus",
    "body", "regular", "bodyLarge", "bodySmall",
    "subtitle", "caption", "caption1",
    "title", "title2", "titleLarge",
    "hero", "display", "jumbo",
    # Backward-compat aliases (discouraged in new code)
    "h2", "h3", "xl", "lg", "xxs",
})

VALID_FONT_WEIGHT_TOKENS: frozenset[str] = frozenset({
    "thin", "light", "normal", "regular",  # regular = alias for normal
    "medium", "semibold", "semiBold",       # semiBold = alias for semibold
    "bold",
})

VALID_TEXT_STYLE_ROLES: frozenset[str] = frozenset({
    # macOS HIG standard roles
    "largeTitle",
    "title1", "title2", "title3",
    "headline", "body", "callout", "subheadline",
    "footnote", "caption1", "caption2",
    # Shell-specific extensions
    "menuItem", "tooltip", "badge", "code", "label",
})


# ── Patterns ──────────────────────────────────────────────────────────────────

# Check 1 — font.family: "literal string"  (not Theme.fontFamily*)
_RE_FAMILY_LITERAL = re.compile(r'\bfont\.family\s*:\s*"[^"]*"')

# Check 2 — font.letterSpacing: <number>
_RE_LETTERSPACING_LITERAL = re.compile(
    r'\bfont\.letterSpacing\s*:\s*-?\d+(?:\.\d+)?(?!\s*\*|\s*[A-Za-z_])'
)

# Check 3 — Theme.fontSize("token")
_RE_FONTSIZE_CALL = re.compile(r'Theme\.fontSize\(\s*"([^"]+)"\s*\)')

# Check 4 — Theme.fontWeight("token")
_RE_FONTWEIGHT_CALL = re.compile(r'Theme\.fontWeight\(\s*"([^"]+)"\s*\)')

# Check 5 — Theme.textStyle("role")
_RE_TEXTSTYLE_CALL = re.compile(r'Theme\.textStyle\(\s*"([^"]+)"\s*\)')

# Check 6 — triple-font-smell component patterns
_RE_FONT_FAMILY    = re.compile(r'\bfont\.family\s*:')
_RE_FONT_PIXELSIZE = re.compile(r'\bfont\.pixelSize\s*:')
_RE_FONT_WEIGHT    = re.compile(r'\bfont\.weight\s*:')
_TRIPLE_HALF_WIN   = 7   # lines above and below the anchor line to search


# ── File exclusions ───────────────────────────────────────────────────────────

_EXCLUDE_SEGMENTS = (
    "third_party/",
    "/build/",
    "node_modules/",
)

_EXCLUDE_SUFFIXES = (
    "/SlmStyle/TypeLabel.qml",  # defines the pattern intentionally
    "/SlmStyle/Theme.qml",      # defines the tokens
)

def _is_excluded(path: Path) -> bool:
    s = str(path).replace("\\", "/")
    return (
        any(seg in s for seg in _EXCLUDE_SEGMENTS)
        or any(s.endswith(suf) for suf in _EXCLUDE_SUFFIXES)
    )


# ── Comment stripping ─────────────────────────────────────────────────────────

def _strip_comment(line: str) -> str:
    """Remove // single-line comments (naive — adequate for QML property lines)."""
    idx = line.find("//")
    return line[:idx] if idx >= 0 else line


# ── Violation dataclass ───────────────────────────────────────────────────────

@dataclass
class Violation:
    path: Path
    line: int
    rule: str
    message: str

    def __str__(self) -> str:
        rel = self.path.relative_to(ROOT)
        return f"{rel}:{self.line}: [{self.rule}] {self.message}"


# ── Per-file analysis ─────────────────────────────────────────────────────────

def check_file(path: Path) -> list[Violation]:
    try:
        text = path.read_text(errors="replace")
    except OSError:
        return []

    raw_lines = text.splitlines()
    stripped   = [_strip_comment(l) for l in raw_lines]
    violations: list[Violation] = []

    def add(lineno: int, rule: str, msg: str) -> None:
        violations.append(Violation(path, lineno, rule, msg))

    for i, line in enumerate(stripped, start=1):

        # ── Check 1: hardcoded font.family ───────────────────────────────────
        if _RE_FAMILY_LITERAL.search(line):
            add(i, "hardcoded-font-family",
                "font.family uses a string literal — "
                "use Theme.fontFamilyUi / fontFamilyDisplay / fontFamilyMonospace")

        # ── Check 2: hardcoded letterSpacing ─────────────────────────────────
        if _RE_LETTERSPACING_LITERAL.search(line):
            add(i, "hardcoded-letter-spacing",
                "font.letterSpacing uses a raw number — "
                "use a Theme.letterSpacing* token "
                "(letterSpacingNormal / Wide / Wider / Display / Title1 …)")

        # ── Check 3: unknown fontSize token ──────────────────────────────────
        for m in _RE_FONTSIZE_CALL.finditer(line):
            tok = m.group(1)
            if tok not in VALID_FONT_SIZE_TOKENS:
                add(i, "unknown-font-size-token",
                    f'Theme.fontSize("{tok}") — unknown token; '
                    f'valid: {", ".join(sorted(VALID_FONT_SIZE_TOKENS))}')

        # ── Check 4: unknown fontWeight token ────────────────────────────────
        for m in _RE_FONTWEIGHT_CALL.finditer(line):
            tok = m.group(1)
            if tok not in VALID_FONT_WEIGHT_TOKENS:
                add(i, "unknown-font-weight-token",
                    f'Theme.fontWeight("{tok}") — unknown token; '
                    f'valid: {", ".join(sorted(VALID_FONT_WEIGHT_TOKENS))}')

        # ── Check 5: unknown textStyle role ──────────────────────────────────
        for m in _RE_TEXTSTYLE_CALL.finditer(line):
            role = m.group(1)
            if role not in VALID_TEXT_STYLE_ROLES:
                add(i, "unknown-text-style-role",
                    f'Theme.textStyle("{role}") — unknown role; '
                    f'valid: {", ".join(sorted(VALID_TEXT_STYLE_ROLES))}')

    # ── Check 6: triple-font-smell ────────────────────────────────────────────
    # When font.family, font.pixelSize, and font.weight all appear within a
    # tight window, it's a candidate for TypeLabel { textStyle: "..." }.
    reported_ends: list[int] = []   # track reported regions to avoid duplicates

    for i, line in enumerate(stripped):
        if not _RE_FONT_FAMILY.search(line):
            continue
        # Skip if this line is already inside a previously reported region.
        if any(i <= end for end in reported_ends):
            continue
        win_start = max(0, i - _TRIPLE_HALF_WIN)
        win_end   = min(len(stripped), i + _TRIPLE_HALF_WIN + 1)
        window    = stripped[win_start:win_end]
        if (any(_RE_FONT_PIXELSIZE.search(l) for l in window)
                and any(_RE_FONT_WEIGHT.search(l) for l in window)):
            add(i + 1, "triple-font-smell",
                "font.family + font.pixelSize + font.weight set together — "
                'consider TypeLabel { textStyle: "..." } for a single-property style')
            reported_ends.append(win_end)

    return violations


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
    paths = []
    for name in out.splitlines():
        p = ROOT / name
        if p.exists() and not _is_excluded(p):
            paths.append(p)
    return sorted(paths)


# ── Allowlist ─────────────────────────────────────────────────────────────────

_ALLOWLIST_FILE = ROOT / "config" / "lint" / "typography-allowlist.txt"


def _load_allowlist() -> set[str]:
    if not _ALLOWLIST_FILE.exists():
        return set()
    entries: set[str] = set()
    for raw in _ALLOWLIST_FILE.read_text().splitlines():
        row = raw.split("#", 1)[0].strip()
        if row:
            entries.add(row)
    return entries


def _is_allowlisted(v: Violation, allowlist: set[str]) -> bool:
    """Match by relative path (any suffix) or rule ID."""
    rel = str(v.path.relative_to(ROOT))
    return rel in allowlist or v.rule in allowlist


# ── Main ──────────────────────────────────────────────────────────────────────

def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="QML typography token linter",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "mode",
        nargs="?",
        default="all",
        choices=["all", "--all", "staged", "--staged"],
        help="Scan all QML files (default) or only git-staged ones",
    )
    p.add_argument(
        "--no-allowlist",
        action="store_true",
        help="Ignore the allowlist and report everything",
    )
    return p.parse_args()


def main() -> int:
    args   = _parse_args()
    prefix = "[lint-typography]"

    staged = args.mode in ("staged", "--staged")

    if staged:
        files = _collect_staged()
        if not files:
            print(f"{prefix} no staged QML files to check")
            return 0
    else:
        files = _collect_all()

    if not files:
        print(f"{prefix} no QML files found")
        return 0

    allowlist = set() if args.no_allowlist else _load_allowlist()

    all_violations:        list[Violation] = []
    allowlisted_count = 0

    for f in files:
        for v in check_file(f):
            if _is_allowlisted(v, allowlist):
                allowlisted_count += 1
            else:
                all_violations.append(v)

    # Group by rule for a clean summary
    by_rule: dict[str, list[Violation]] = {}
    for v in all_violations:
        by_rule.setdefault(v.rule, []).append(v)

    rule_order = [
        "hardcoded-font-family",
        "hardcoded-letter-spacing",
        "unknown-font-size-token",
        "unknown-font-weight-token",
        "unknown-text-style-role",
        "triple-font-smell",
    ]

    rule_labels = {
        "hardcoded-font-family":     "hardcoded font.family string",
        "hardcoded-letter-spacing":  "hardcoded font.letterSpacing number",
        "unknown-font-size-token":   "unknown Theme.fontSize() token",
        "unknown-font-weight-token": "unknown Theme.fontWeight() token",
        "unknown-text-style-role":   "unknown Theme.textStyle() role",
        "triple-font-smell":         "triple-font-property block (TypeLabel candidate)",
    }

    hard_rules = {
        "hardcoded-font-family",
        "hardcoded-letter-spacing",
        "unknown-font-size-token",
        "unknown-font-weight-token",
        "unknown-text-style-role",
    }

    failed = False

    for rule in rule_order:
        if rule not in by_rule:
            continue
        label = rule_labels[rule]
        is_hard = rule in hard_rules
        severity = "ERROR" if is_hard else "WARN"

        print(f"\n{prefix} [{severity}] {label}")
        print(f"{prefix} {'─' * (len(label) + 10)}")
        for v in sorted(by_rule[rule], key=lambda x: (x.path, x.line)):
            print(f"  {v}")

        if is_hard:
            failed = True

    if allowlisted_count:
        print(f"\n{prefix} [BASELINE] {allowlisted_count} violation(s) suppressed by allowlist")

    if not all_violations:
        print(f"\n{prefix} OK — {len(files)} file(s) checked")
        return 0

    hard_count = sum(
        len(vs) for rule, vs in by_rule.items() if rule in hard_rules
    )
    warn_count = sum(
        len(vs) for rule, vs in by_rule.items() if rule not in hard_rules
    )

    print()
    if failed:
        print(
            f"{prefix} FAILED — {hard_count} error(s), {warn_count} warning(s).\n"
            f"{prefix} Fix errors; add to config/lint/typography-allowlist.txt only as a "
            "temporary baseline.\n"
            f"{prefix} Use Theme.textStyle() / TypeLabel to address triple-font-smell warnings.",
            file=sys.stderr,
        )
        return 1

    # Only warnings remain
    print(
        f"{prefix} {warn_count} warning(s) — no hard errors. "
        "Consider migrating triple-font blocks to TypeLabel.",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
