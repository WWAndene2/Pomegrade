#!/usr/bin/env python3
"""Checks dp and sp values against CLAUDE.md §7's PerfectSuite scale (31 October, owner's request).

Usage: check_scale.py FILE...   (XML layouts, values, drawables, Kotlin)
       check_scale.py --branch   (the Pomegrade-written files changed since origin/main)

Prints `file:line: value -> suggested | line` for each value off the scale, exit 1 if any.
Rules, as CLAUDE.md writes them:
- up to 16: one of 1 2 3 4 6 8 12 14 16 (§7), ties by §7.1 (primary, secondary, tertiary);
- above 16: a scale value, or the nearest primary at or below it plus one primary (§7.2: 20 = 16 + 4, 96 = 64 + 32).
Not checked: corner radii (§7.3, 0.24 x the element's height: needs the height), aspect ratios (§7.4), values in a
core's own upstream layouts (§7's note: they keep theirs), comments that quote a size.
"""
import re
import subprocess
import sys

PRIMARY = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]
SECONDARY = [3, 6, 12, 24, 48, 96, 192, 384, 768]
TERTIARY = [14, 30, 62, 126, 254, 510, 1022]
SCALE = set(PRIMARY + SECONDARY + TERTIARY)
VALUE = re.compile(r'(?<![\w.])(-?\d+(?:\.\d+)?)\s*(?:dp|sp)\b|(?<![\w.])(-?\d+(?:\.\d+)?)\.(?:dp|sp)\b|\bdp\((-?\d+)\)')


def on_scale(v: int) -> bool:
    v = abs(v)
    if v == 0 or v in SCALE:
        return True
    if v <= 16:
        return False
    base = max(p for p in PRIMARY if p <= v)
    return any(base + p == v for p in PRIMARY)


def suggest(v: int) -> int:
    sign, v = (-1 if v < 0 else 1), abs(v)
    if v <= 16:
        rank = lambda x: (abs(x - v), 0 if x in PRIMARY else 1 if x in SECONDARY else 2)
        return sign * min(SCALE, key=rank)
    base = max(p for p in PRIMARY if p <= v)
    return sign * min([base + p for p in PRIMARY] + [base], key=lambda x: (abs(x - v), x))


def branch_files() -> list:
    out = subprocess.run(["git", "diff", "--name-only", "origin/main...HEAD"], capture_output=True, text=True).stdout
    return [f for f in out.split() if re.search(r'(res/(layout|values|drawable)[^/]*/.*\.xml|\.kt)$', f)
            and "values-b+" not in f]


def main(args: list) -> int:
    files = branch_files() if args == ["--branch"] else args
    found = 0
    for name in files:
        try:
            lines = open(name, encoding="utf-8", errors="ignore").read().splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, 1):
            if line.lstrip().startswith(("//", "*", "<!--", "/*")):
                continue
            for match in VALUE.finditer(line):
                text = next(g for g in match.groups() if g)
                value = float(text)
                if value != int(value) or not on_scale(int(value)):
                    print(f"{name}:{number}: {text} -> {suggest(round(value))} | {line.strip()[:110]}")
                    found += 1
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
