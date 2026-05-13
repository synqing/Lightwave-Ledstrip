#!/usr/bin/env python3
"""
Zone Composer Cross-Transport Equivalence Test Runner (E5).

Phase 0 status: SCAFFOLDING. Discovers test files in tests/, runs each against
all configured transports, aggregates pass/fail report.

Phase 1 work: populate tests/ with concrete equivalence cases as commands ship.

Usage:
    python3 run_all.py \\
        --rest-host 192.168.4.1 \\
        --ws-host 192.168.4.1 \\
        --serial-port /dev/cu.usbmodem2101

Exit code 0 = all transports converge; 1 = divergence detected.
"""

import argparse
import importlib.util
import sys
from pathlib import Path

from lib.transports import make_transports


def discover_tests(tests_dir: Path):
    """Yield (name, module) for every test_*.py in tests/."""
    for test_file in sorted(tests_dir.glob("test_*.py")):
        spec = importlib.util.spec_from_file_location(test_file.stem, test_file)
        if spec is None or spec.loader is None:
            continue
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        if not hasattr(module, "run"):
            print(f"[SKIP] {test_file.name}: no run() function")
            continue
        yield test_file.stem, module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rest-host", help="K1 hostname/IP for REST (e.g. 192.168.4.1)")
    parser.add_argument("--ws-host", help="K1 hostname/IP for WS (default: same as REST)")
    parser.add_argument("--serial-port", help="Serial port (e.g. /dev/cu.usbmodem2101)")
    parser.add_argument("--filter", default="", help="Run only tests whose name contains this string")
    args = parser.parse_args()

    if not args.ws_host:
        args.ws_host = args.rest_host

    transports = make_transports(args.rest_host, args.ws_host, args.serial_port)
    if not transports:
        print("[FATAL] no transports configured (need --rest-host or --ws-host or --serial-port)")
        sys.exit(2)

    print(f"Equivalence harness — transports: {', '.join(transports.keys())}")
    print("(Phase 0: scaffolding-only; tests/ is empty until Phase 1 commands ship)\n")

    tests_dir = Path(__file__).parent / "tests"
    if not tests_dir.exists():
        tests_dir.mkdir()

    failures = []
    test_count = 0
    for name, module in discover_tests(tests_dir):
        if args.filter and args.filter not in name:
            continue
        test_count += 1
        print(f"[{name}]")
        try:
            ok, message = module.run(transports)
        except Exception as exc:
            ok, message = False, f"raised {type(exc).__name__}: {exc}"
        if ok:
            print(f"  PASS")
        else:
            print(f"  FAIL: {message}")
            failures.append(name)
        print()

    for transport in transports.values():
        transport.close()

    if test_count == 0:
        print("NO TESTS DISCOVERED — Phase 0 scaffolding only. Add test_*.py files in tests/.")
        sys.exit(0)
    if failures:
        print(f"FAILED ({len(failures)}/{test_count}): {', '.join(failures)}")
        sys.exit(1)
    print(f"ALL TESTS PASSED ({test_count})")
    sys.exit(0)


if __name__ == "__main__":
    main()
