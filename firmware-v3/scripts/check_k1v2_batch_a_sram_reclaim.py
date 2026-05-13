#!/usr/bin/env python3
"""Check that K1v2 Batch A internal SRAM reclaim symbols stayed reclaimed."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_NM = (
    Path.home()
    / ".platformio/packages/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5/bin/xtensa-esp32s3-elf-nm"
)


@dataclass(frozen=True)
class SymbolLimit:
    label: str
    pattern: str
    max_bytes: int


SYMBOL_LIMITS = (
    SymbolLimit(
        "CaptureStreamer singleton",
        "_ZL15captureStreamer",
        512,
    ),
    SymbolLimit(
        "StaticAssetRoutes launcher buffer",
        "StaticAssetRoutes14registerRoutesERNS1_17HttpRouteRegistryEENKUlP21AsyncWebServerRequestE_clES6_E3buf",
        64,
    ),
    SymbolLimit(
        "WsCommandRouter handler table",
        "_ZN11lightwaveos7network9webserver15WsCommandRouter10s_handlersE",
        64,
    ),
    SymbolLimit(
        "BuiltinEffectRegistry entries",
        "_ZN11lightwaveos7plugins21BuiltinEffectRegistry9s_entriesE",
        64,
    ),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path, help="Path to firmware.elf")
    parser.add_argument(
        "--nm",
        type=Path,
        default=Path(os.environ.get("XTENSA_NM", DEFAULT_NM)),
        help="Path to xtensa-esp32s3-elf-nm",
    )
    return parser.parse_args()


def load_symbols(nm: Path, elf: Path) -> list[tuple[int, str, str]]:
    if not nm.exists():
        raise FileNotFoundError(f"nm tool not found: {nm}")
    if not elf.exists():
        raise FileNotFoundError(f"ELF not found: {elf}")

    proc = subprocess.run(
        [str(nm), "-S", "--size-sort", str(elf)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    rows: list[tuple[int, str, str]] = []
    for line in proc.stdout.splitlines():
        parts = line.split()
        if len(parts) < 4:
            continue
        try:
            size = int(parts[1], 16)
        except ValueError:
            continue
        rows.append((size, parts[2], parts[3]))
    return rows


def matching_size(rows: list[tuple[int, str, str]], pattern: str) -> int:
    total = 0
    for size, _kind, name in rows:
        if pattern in name:
            total += size
    return total


def main() -> int:
    args = parse_args()
    rows = load_symbols(args.nm, args.elf)

    failures: list[str] = []
    print("K1v2 Batch A SRAM reclaim symbol check")
    for limit in SYMBOL_LIMITS:
        size = matching_size(rows, limit.pattern)
        status = "PASS" if size <= limit.max_bytes else "FAIL"
        print(f"{status}: {limit.label}: {size} B <= {limit.max_bytes} B")
        if size > limit.max_bytes:
            failures.append(f"{limit.label} is {size} B, limit {limit.max_bytes} B")

    if failures:
        print("\nFailures:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
