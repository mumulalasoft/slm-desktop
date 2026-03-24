#!/usr/bin/env python3
"""
check-cups-leakage.py — CI guard for print framework CUPS isolation.

Verifies that no file outside src/printing/adapter/ references CUPS/IPP
implementation details in ways that would violate the backend isolation contract.

Exit codes:
  0 — no violations found
  1 — violations found (details printed to stdout)

Usage:
  python3 scripts/check-cups-leakage.py [--source-root PATH]
"""

import sys
import os
import re
import argparse
from pathlib import Path


# ── Configuration ─────────────────────────────────────────────────────────────

# These patterns are forbidden in any C++ file outside adapter/.
BANNED_INCLUDES = [
    "cups/cups.h",
    "cups/ipp.h",
    "cups/ppd.h",
    "cups/backend.h",
    "cups/cups-private.h",
]

# These type/function identifiers from libcups must not appear outside adapter/.
BANNED_CPP_TOKENS = [
    r"\bcups_dest_t\b",
    r"\bipp_t\b",
    r"\bipp_status_e\b",
    r"\bppd_file_t\b",
    r"\bppd_attr_t\b",
    r"\bcupsGetDests\b",
    r"\bcupsFreeDests\b",
    r"\bcupsPrintFile\b",
    r"\bcupsDoRequest\b",
    r"\bippNew\b",
    r"\bippDelete\b",
    r"\bppdOpenFile\b",
    r"\bppdClose\b",
    r"\bHTTP_t\b",
]

# These strings must not appear in PUBLIC header names, Q_PROPERTY names,
# method names, or signal/slot names (i.e. in .h files outside adapter/).
# Pattern: word appears as part of an identifier in a Q_PROPERTY, signal, or
# method declaration.
BANNED_API_TOKENS_IN_HEADERS = [
    r"\bcups[A-Za-z]",          # any camelCase starting with "cups"
    r"\bipp[A-Za-z]",           # any camelCase starting with "ipp"
    r"\bppd[A-Za-z]",           # any camelCase starting with "ppd"
    r"isCupsAvailable",
    r"cupsAvailabilityChanged",
    r"cupsJobId",
    r"ippState",
    r"ppdPath",
]

# ── Helpers ───────────────────────────────────────────────────────────────────

def is_adapter_path(path: Path, adapter_dir: Path) -> bool:
    try:
        path.relative_to(adapter_dir)
        return True
    except ValueError:
        return False


def check_file_cpp(path: Path, adapter_dir: Path) -> list[str]:
    """Check a .cpp/.h file for banned CUPS symbols (only if outside adapter/)."""
    if is_adapter_path(path, adapter_dir):
        return []

    violations = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    lines = text.splitlines()

    # Check banned includes
    for lineno, line in enumerate(lines, start=1):
        stripped = line.strip()
        for banned in BANNED_INCLUDES:
            if f'"{banned}"' in stripped or f"<{banned}>" in stripped:
                violations.append(
                    f"{path}:{lineno}: banned include <{banned}>"
                )

    # Check banned C++ token patterns
    for pattern in BANNED_CPP_TOKENS:
        for m in re.finditer(pattern, text):
            lineno = text[:m.start()].count("\n") + 1
            violations.append(
                f"{path}:{lineno}: banned CUPS symbol '{m.group()}'"
            )

    return violations


def check_file_header_api(path: Path, adapter_dir: Path) -> list[str]:
    """Check a .h file (outside adapter/) for CUPS-leaking API names."""
    if is_adapter_path(path, adapter_dir):
        return []
    if path.suffix != ".h":
        return []

    violations = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    for pattern in BANNED_API_TOKENS_IN_HEADERS:
        for m in re.finditer(pattern, text):
            lineno = text[:m.start()].count("\n") + 1
            violations.append(
                f"{path}:{lineno}: CUPS leak in public API identifier: '{m.group()}'"
            )

    return violations


def check_file_qml(path: Path) -> list[str]:
    """Check a .qml file for CUPS-leaking display text or property bindings."""
    BANNED_QML_PATTERNS = [
        (r'\bCUPS\b',                     "UI text contains 'CUPS'"),
        (r'\bIPP\b',                      "UI text contains 'IPP'"),
        (r'\bppd\b',                      "UI text contains 'ppd'"),
        (r'\bscheduler\b',                "UI text contains 'scheduler'"),
        (r'isCupsAvailable',              "property binding uses 'isCupsAvailable'"),
        (r'cupsAvailabilityChanged',      "signal uses 'cupsAvailabilityChanged'"),
        # Banned as display values (not as comments)
        (r'value:\s*"monochrome"',        "combo value uses IPP 'monochrome' — use 'grayscale'"),
        (r'value:\s*"one-sided"',         "combo value uses IPP 'one-sided' — use 'off'"),
        (r'value:\s*"two-sided-long-edge"',  "combo value uses IPP 'two-sided-long-edge' — use 'long-edge'"),
        (r'value:\s*"two-sided-short-edge"', "combo value uses IPP 'two-sided-short-edge' — use 'short-edge'"),
    ]

    violations = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    for pattern, description in BANNED_QML_PATTERNS:
        for m in re.finditer(pattern, text):
            lineno = text[:m.start()].count("\n") + 1
            violations.append(f"{path}:{lineno}: {description}")

    return violations


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-root",
        default=".",
        help="Root directory of the repository (default: current dir)",
    )
    args = parser.parse_args()

    root = Path(args.source_root).resolve()
    printing_root = root / "src" / "printing"
    adapter_dir = printing_root / "adapter"

    if not printing_root.exists():
        print(f"ERROR: src/printing/ not found under {root}")
        return 1

    all_violations: list[str] = []

    # Walk all C++ files under src/printing/
    for path in printing_root.rglob("*.cpp"):
        all_violations.extend(check_file_cpp(path, adapter_dir))
    for path in printing_root.rglob("*.h"):
        all_violations.extend(check_file_cpp(path, adapter_dir))
        all_violations.extend(check_file_header_api(path, adapter_dir))

    # Walk all QML files under Qml/ and src/
    for qml_dir in [root / "Qml", root / "src"]:
        if qml_dir.exists():
            for path in qml_dir.rglob("*.qml"):
                all_violations.extend(check_file_qml(path))

    if all_violations:
        print("CUPS leakage check FAILED. Violations found:\n")
        for v in all_violations:
            print(f"  {v}")
        print(f"\n{len(all_violations)} violation(s) found.")
        return 1

    print("CUPS leakage check passed. No violations found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
