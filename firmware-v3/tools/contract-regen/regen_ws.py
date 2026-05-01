#!/usr/bin/env python3
"""regen_ws.py — WebSocket contract drift detector.

Sister tool to ``regen_rest.py``. Compares the set of commands registered in
``firmware-v3/src/network/webserver/ws/Ws*Commands.cpp`` against the set of
commands declared in ``docs/protocol/k1-ws-contract.yaml`` and reports any
drift in either direction.

Modes
-----
default
    Print a human-readable drift report. Exit 0 regardless of drift.
``--strict``
    Print the report and exit non-zero if any drift exists. Suitable for CI.
``--update-skeleton``
    Write ``<contract>.regen.yaml`` with placeholder entries for every
    firmware-only command, ready for a human to fill in.

Constraints
-----------
* Stdlib + PyYAML only.
* British English.
* Run from any working directory; defaults are anchored to the repo layout
  so a no-argument invocation works from the repository root.

Examples
--------
    python3 firmware-v3/tools/contract-regen/regen_ws.py
    python3 firmware-v3/tools/contract-regen/regen_ws.py --strict
    python3 firmware-v3/tools/contract-regen/regen_ws.py --update-skeleton
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

try:
    import yaml
except ImportError as exc:  # pragma: no cover — dependency error path
    sys.stderr.write(
        "regen_ws: PyYAML is required. Install with `pip install pyyaml`.\n"
    )
    raise SystemExit(2) from exc


# ---------------------------------------------------------------------------
# Path defaults — anchored from this file's location so no-argument invocation
# works from anywhere inside the repository.
# ---------------------------------------------------------------------------
THIS_FILE = Path(__file__).resolve()
TOOL_DIR = THIS_FILE.parent
FIRMWARE_DIR = TOOL_DIR.parent.parent  # firmware-v3/
REPO_ROOT = FIRMWARE_DIR.parent
DEFAULT_WS_DIR = FIRMWARE_DIR / "src" / "network" / "webserver" / "ws"
DEFAULT_CONTRACT = REPO_ROOT / "docs" / "protocol" / "k1-ws-contract.yaml"
WS_GLOB = "Ws*Commands.cpp"

# ---------------------------------------------------------------------------
# Source parsing
# ---------------------------------------------------------------------------
REGISTER_RE = re.compile(
    r'WsCommandRouter::registerCommand\(\s*"([^"]+)"\s*,'
)


def _strip_block_comments(source: str) -> str:
    """Remove ``/* ... */`` block comments while preserving line numbers.

    Line numbers are preserved so any future "show me the file:line" reporting
    stays accurate. Newline characters inside the block are kept; everything
    else inside is replaced with spaces.
    """
    out: list[str] = []
    i = 0
    in_block = False
    n = len(source)
    while i < n:
        ch = source[i]
        nxt = source[i + 1] if i + 1 < n else ""
        if not in_block and ch == "/" and nxt == "*":
            in_block = True
            out.append("  ")
            i += 2
            continue
        if in_block and ch == "*" and nxt == "/":
            in_block = False
            out.append("  ")
            i += 2
            continue
        if in_block:
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def parse_registered_commands(source: str) -> list[str]:
    """Return command names from a single source-file string.

    * Strips ``/* ... */`` block comments first.
    * Skips any line whose first non-whitespace characters are ``//``.
    * Preserves source order for deterministic output.
    """
    cleaned = _strip_block_comments(source)
    commands: list[str] = []
    for raw_line in cleaned.splitlines():
        stripped = raw_line.lstrip()
        if stripped.startswith("//"):
            continue
        for match in REGISTER_RE.finditer(raw_line):
            commands.append(match.group(1))
    return commands


def collect_firmware_commands(ws_dir: Path) -> dict[str, Path]:
    """Collect every registered command across ``Ws*Commands.cpp`` files.

    Returns a dict mapping each command name to the file it was first seen in.
    Aliases register independently — both names appear in the result.
    """
    out: dict[str, Path] = {}
    for cpp in sorted(ws_dir.glob(WS_GLOB)):
        text = cpp.read_text(encoding="utf-8", errors="replace")
        for name in parse_registered_commands(text):
            out.setdefault(name, cpp)
    return out


# ---------------------------------------------------------------------------
# Contract parsing
# ---------------------------------------------------------------------------
def collect_contract_commands(contract_path: Path) -> set[str]:
    """Return the set of command names declared in the YAML contract.

    The YAML structure has commands at ``commands.<name>``. Each entry IS a
    command name. Falls back gracefully if the file is empty or malformed.
    """
    with contract_path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f)
    if not isinstance(data, dict):
        return set()
    cmds = data.get("commands")
    if not isinstance(cmds, dict):
        return set()
    return {str(k) for k in cmds.keys()}


# ---------------------------------------------------------------------------
# Drift comparison
# ---------------------------------------------------------------------------
@dataclass
class DriftReport:
    """Structured drift result.

    Lists are alphabetically sorted for stable diffs.
    """
    firmware_only: list[str] = field(default_factory=list)
    contract_only: list[str] = field(default_factory=list)
    firmware_total: int = 0
    contract_total: int = 0

    @property
    def total_drift(self) -> int:
        return len(self.firmware_only) + len(self.contract_only)

    @property
    def has_drift(self) -> bool:
        return self.total_drift > 0


def compute_drift(firmware: Iterable[str], contract: Iterable[str]) -> DriftReport:
    fw = set(firmware)
    co = set(contract)
    return DriftReport(
        firmware_only=sorted(fw - co),
        contract_only=sorted(co - fw),
        firmware_total=len(fw),
        contract_total=len(co),
    )


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------
def format_report(
    report: DriftReport,
    *,
    firmware_sources: dict[str, Path] | None = None,
    ws_dir: Path | None = None,
    contract_path: Path | None = None,
) -> str:
    lines: list[str] = []
    lines.append("WebSocket contract drift report")
    lines.append("=" * 32)
    if ws_dir is not None:
        lines.append(f"Firmware glob   : {ws_dir}/{WS_GLOB}")
    if contract_path is not None:
        lines.append(f"Contract        : {contract_path}")
    lines.append(f"Firmware total  : {report.firmware_total}")
    lines.append(f"Contract total  : {report.contract_total}")
    lines.append(f"Drift count     : {report.total_drift}")
    lines.append("")

    if report.firmware_only:
        lines.append(
            f"Firmware-only — registered in C++ but missing from contract "
            f"({len(report.firmware_only)}):"
        )
        for name in report.firmware_only:
            origin = ""
            if firmware_sources is not None:
                src = firmware_sources.get(name)
                if src is not None:
                    origin = f"  ({src.name})"
            lines.append(f"  + {name}{origin}")
        lines.append("")
    else:
        lines.append("Firmware-only   : none")
        lines.append("")

    if report.contract_only:
        lines.append(
            f"Contract-only — declared in YAML but no matching "
            f"registerCommand call ({len(report.contract_only)}):"
        )
        for name in report.contract_only:
            lines.append(f"  - {name}")
        lines.append("")
    else:
        lines.append("Contract-only   : none")
        lines.append("")

    if report.has_drift:
        lines.append("Result: DRIFT DETECTED")
    else:
        lines.append("Result: contract is in sync")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Skeleton writer
# ---------------------------------------------------------------------------
SKELETON_HEADER = (
    "# regen_ws skeleton — placeholder entries for firmware-only commands.\n"
    "# Generated by firmware-v3/tools/contract-regen/regen_ws.py\n"
    "# Review each entry, fill in real fields, then merge into "
    "k1-ws-contract.yaml.\n"
)


def write_skeleton(
    contract_path: Path,
    firmware_only: Sequence[str],
    firmware_sources: dict[str, Path],
) -> Path:
    """Write ``<contract>.regen.yaml`` with placeholder YAML for firmware-only
    commands. Returns the path written."""
    skeleton = contract_path.with_suffix(".regen.yaml")
    lines: list[str] = [SKELETON_HEADER, "commands:"]
    if not firmware_only:
        lines.append("  {}  # no firmware-only commands — contract in sync")
    for name in firmware_only:
        src = firmware_sources.get(name)
        origin = f"  # registered in {src.name}" if src is not None else ""
        lines.append(f'  "{name}":{origin}')
        lines.append('    direction: "client -> K1"  # TODO: confirm')
        lines.append('    description: "TODO: fill in"')
        lines.append("    request: {}  # TODO: enumerate request fields")
        lines.append('    response: "TODO: describe response shape"')
        lines.append("    consumers: []  # TODO: tab5 / ios / web / etc.")
    skeleton.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return skeleton


# ---------------------------------------------------------------------------
# CLI entry
# ---------------------------------------------------------------------------
def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="regen_ws",
        description=(
            "Compare WebSocket commands registered in firmware C++ against "
            "the YAML contract and report drift."
        ),
    )
    parser.add_argument(
        "--ws-dir",
        type=Path,
        default=DEFAULT_WS_DIR,
        help=f"Directory containing {WS_GLOB} (default: {DEFAULT_WS_DIR})",
    )
    parser.add_argument(
        "--contract",
        type=Path,
        default=DEFAULT_CONTRACT,
        help=f"Path to k1-ws-contract.yaml (default: {DEFAULT_CONTRACT})",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero if any drift is detected.",
    )
    parser.add_argument(
        "--update-skeleton",
        action="store_true",
        help=(
            "Write <contract>.regen.yaml with placeholder entries for every "
            "firmware-only command."
        ),
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress the human-readable report (still exits per --strict).",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    ws_dir: Path = args.ws_dir
    contract_path: Path = args.contract

    if not ws_dir.is_dir():
        sys.stderr.write(f"regen_ws: ws-dir not found: {ws_dir}\n")
        return 2
    if not contract_path.is_file():
        sys.stderr.write(f"regen_ws: contract not found: {contract_path}\n")
        return 2

    firmware_sources = collect_firmware_commands(ws_dir)
    firmware = set(firmware_sources.keys())
    contract = collect_contract_commands(contract_path)
    report = compute_drift(firmware, contract)

    if not args.quiet:
        print(
            format_report(
                report,
                firmware_sources=firmware_sources,
                ws_dir=ws_dir,
                contract_path=contract_path,
            )
        )

    if args.update_skeleton:
        skeleton = write_skeleton(
            contract_path, report.firmware_only, firmware_sources
        )
        if not args.quiet:
            print(f"\nSkeleton written: {skeleton}")

    if args.strict and report.has_drift:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
