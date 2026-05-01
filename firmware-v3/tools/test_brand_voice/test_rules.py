#!/usr/bin/env python3
"""
Test runner for the brand-voice extensions to check_effect_contracts.py.

Imports the rule check functions from the parent lint and applies them to
the synthetic fixtures in ./fixtures/, asserting expected pass/fail/warn
behaviour per fixture.

Run:
    python3 firmware-v3/tools/test_brand_voice/test_rules.py

Exit codes:
    0 — all fixtures behaved as expected
    1 — at least one fixture produced unexpected output
"""

from __future__ import annotations

import sys
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
TOOLS_DIR = THIS_DIR.parent
FIXTURES_DIR = THIS_DIR / "fixtures"

sys.path.insert(0, str(TOOLS_DIR))

# Import the rule checks from the main lint module.
import check_effect_contracts as cec  # noqa: E402


# Per-fixture expected outcomes.
# Format: filename -> (expected_violation_count, expected_warning_count)
EXPECTED = {
    "EXAMPLE_tempo_bank_violator.cpp":     (1, 0),  # 1 tempo-bank violation
    "EXAMPLE_tempo_scalar_clean.cpp":      (0, 0),  # compliant
    "EXAMPLE_CircularRingViolator.cpp":    (2, 0),  # filename + class match
    "EXAMPLE_AsymmetricDriftOrigin.cpp":   (1, 0),  # filename match
    "EXAMPLE_pendulum_chain_warner.cpp":   (0, 1),  # WARN (not FAIL)
    "EXAMPLE_continuum_clean.cpp":         (0, 0),  # compliant
}


def run_one_fixture(fixture_path: Path) -> tuple[list[str], list[str]]:
    """Run all 3 brand-voice rules against a single fixture file.

    We isolate each fixture by temporarily redirecting the rule's effect_dir
    to a per-fixture temporary directory containing only that file.
    """
    import shutil
    import tempfile

    violations: list[str] = []
    warnings: list[str] = []

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        shutil.copy(fixture_path, tmp_path / fixture_path.name)

        cec.check_tempo_bank_in_render(violations, {}, effect_dir=tmp_path)
        cec.check_geo_kill_patterns(violations, {}, effect_dir=tmp_path)
        cec.check_fragmentation_patterns(warnings, {}, effect_dir=tmp_path)

    return violations, warnings


def main() -> int:
    failures = 0
    print("=" * 72)
    print("Brand-voice rule fixture test runner")
    print("=" * 72)
    print()

    for fixture_path in sorted(FIXTURES_DIR.glob("*.cpp")):
        name = fixture_path.name
        violations, warnings = run_one_fixture(fixture_path)
        actual = (len(violations), len(warnings))
        expected = EXPECTED.get(name, ("?", "?"))

        status = "OK  " if actual == expected else "FAIL"
        if actual != expected:
            failures += 1

        print(f"[{status}] {name}")
        print(f"        expected: violations={expected[0]} warnings={expected[1]}")
        print(f"        actual:   violations={actual[0]} warnings={actual[1]}")

        if violations:
            for v in violations:
                print(f"          violation> {v}")
        if warnings:
            for w in warnings:
                print(f"          warning>   {w}")
        print()

    print("=" * 72)
    if failures:
        print(f"FAIL: {failures} fixture(s) did not match expected output.")
        return 1
    print(f"PASS: all {len(EXPECTED)} fixtures matched expected output.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
