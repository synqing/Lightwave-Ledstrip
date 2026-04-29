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

FADE_PATTERN = re.compile(
    r'\bfadeToBlackBy\s*\(([^)]+)\)'
)
INCLUDE_LINE = '#include "effects/PersistenceHelpers.h"'


def migrate_text(text: str, filename: str) -> tuple[str, int]:
    """Returns (new_text, replacement_count)."""
    count = 0

    def replacer(m: re.Match) -> str:
        nonlocal count
        args = m.group(1)
        # Skip if this is already fadeToBlackByDt (shouldn't match but be safe)
        count += 1
        return f'fadeToBlackByDt({args}, ctx.dt)'

    new_text = FADE_PATTERN.sub(replacer, text)

    if count > 0 and INCLUDE_LINE not in new_text:
        # Insert after the first #include line
        lines = new_text.splitlines(keepends=True)
        insert_at = 0
        for i, line in enumerate(lines):
            if line.startswith('#include'):
                insert_at = i + 1
        lines.insert(insert_at, INCLUDE_LINE + '\n')
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
