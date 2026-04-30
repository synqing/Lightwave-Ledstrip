#!/usr/bin/env python3
"""
Migrate fadeToBlackBy → fadeToBlackByDt across ieffect/ .cpp files.

Usage:
    python3 firmware-v3/tools/migrate_fade_to_dt.py [--dry-run] [--file PATH]

Without --file, processes ALL .cpp files under firmware-v3/src/effects/ieffect/.
With --dry-run, prints changes without writing.

Transformation:
    fadeToBlackBy(X, Y, Z)  →  fadeToBlackByDt(X, Y, Z, ctx.dt)

Adds #include "effects/PersistenceHelpers.h" if not already present.
Does NOT add FastLED.h (already included in all effect files).
"""

from __future__ import annotations
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IEFFECT_DIR = ROOT / "src" / "effects" / "ieffect"

# Matches the function name only; argument capture done with paren counting below.
FADE_NAME_PATTERN = re.compile(r'(?<!\.)(?<!\w)\bfadeToBlackBy\s*\(')
INCLUDE_LINE = '#include "effects/PersistenceHelpers.h"'
USING_LINE = 'using lightwaveos::effects::persistence::fadeToBlackByDt;'

# CRGB member call pattern — ctx.leds[i].fadeToBlackBy(X) — NOT migrated
# (it's a FastLED CRGB method, not the free function).
MEMBER_CALL_PATTERN = re.compile(r'\.\s*fadeToBlackBy\b')


def _extract_call(text: str, name_end: int) -> tuple[str, int]:
    """Starting at the '(' at name_end, find the matching ')'.
    Returns (full_arg_string_without_outer_parens, index_after_closing_paren).
    """
    depth = 1
    i = name_end + 1  # skip the opening '('
    while i < len(text) and depth > 0:
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
        i += 1
    args = text[name_end + 1: i - 1]  # between outer parens
    return args, i


def migrate_text(text: str, filename: str) -> tuple[str, int]:
    """Returns (new_text, replacement_count)."""
    count = 0
    result: list[str] = []
    pos = 0

    for m in FADE_NAME_PATTERN.finditer(text):
        # m.end() points to just after the opening '('
        open_paren = m.end() - 1  # index of '('
        args, after = _extract_call(text, open_paren)
        result.append(text[pos:m.start()])
        result.append(f'fadeToBlackByDt({args}, ctx.getSafeDeltaSeconds())')
        pos = after
        count += 1

    result.append(text[pos:])
    new_text = ''.join(result)

    if count > 0:
        lines = new_text.splitlines(keepends=True)
        has_include = any(INCLUDE_LINE in line for line in lines)
        has_using   = any(USING_LINE   in line for line in lines)
        if not has_include or not has_using:
            # Find insertion point: after last existing #include block
            insert_at = 0
            for i, line in enumerate(lines):
                if line.startswith('#include'):
                    insert_at = i + 1
            inject = []
            if not has_include:
                inject.append(INCLUDE_LINE + '\n')
            if not has_using:
                inject.append(USING_LINE + '\n')
            for j, extra in enumerate(inject):
                lines.insert(insert_at + j, extra)
            new_text = ''.join(lines)

    return new_text, count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--file', type=Path, default=None)
    args = parser.parse_args()

    if args.file:
        paths = [args.file]
    else:
        paths = sorted(IEFFECT_DIR.rglob('*.cpp'))

    total_files = 0
    total_replacements = 0

    for path in paths:
        original = path.read_text(encoding='utf-8', errors='replace')
        new_text, count = migrate_text(original, path.name)
        if count == 0:
            continue
        total_files += 1
        total_replacements += count
        print(f'  {path.relative_to(ROOT)} — {count} replacement(s)')
        if not args.dry_run:
            path.write_text(new_text, encoding='utf-8')

    mode = '[DRY RUN] ' if args.dry_run else ''
    print(f'\n{mode}{total_replacements} replacements across {total_files} files.')
    if args.dry_run:
        print('Re-run without --dry-run to apply.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
