#!/usr/bin/env python3
"""REST contract drift detector for LightwaveOS.

Reads ``firmware-v3/src/network/webserver/V1ApiRoutes.cpp`` (the firmware-side
registration list) and ``docs/protocol/k1-rest-contract.yaml`` (the consumer-
facing contract), then surfaces any drift between the two.

Modes
-----
* default            — print drift report, exit 0 always.
* ``--strict``       — exit non-zero if any drift is found (CI-friendly).
* ``--update-skeleton`` — write ``<contract>.regen.yaml`` next to the existing
  contract with FIXME-stubbed entries for every firmware-only route. Does NOT
  modify the canonical YAML.

Dependencies are stdlib only, plus PyYAML. British English in all comments,
log strings, and user-facing output — see CLAUDE.md hard constraints.

Background: see BACKLOG.md § F-1 — the contract YAML is a regeneratable
artefact, not a hand-curated source of truth.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Sequence, Set, TextIO, Tuple

import yaml


# ---------------------------------------------------------------------------
# Defaults — used when the caller does not pass --cpp / --yaml explicitly.
# Resolved relative to the repo root, which is two directories above this file.
# ---------------------------------------------------------------------------

_THIS_FILE = Path(__file__).resolve()
_REPO_ROOT = _THIS_FILE.parents[3]  # firmware-v3/tools/contract-regen/regen_rest.py → repo root
DEFAULT_CPP = _REPO_ROOT / "firmware-v3" / "src" / "network" / "webserver" / "V1ApiRoutes.cpp"
DEFAULT_YAML = _REPO_ROOT / "docs" / "protocol" / "k1-rest-contract.yaml"


# ---------------------------------------------------------------------------
# Data shapes
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Route:
    """A single (path, method) pair, plus where the entry came from."""

    path: str
    method: str
    source_line: int = 0  # 0 = unknown / contract-derived
    raw_path: str = ""    # original regex literal, useful for skeleton output


@dataclass
class DriftReport:
    firmware_only: List[Route] = field(default_factory=list)
    contract_only: List[Route] = field(default_factory=list)
    total_firmware: int = 0
    total_contract: int = 0

    @property
    def drift_count(self) -> int:
        return len(self.firmware_only) + len(self.contract_only)


# ---------------------------------------------------------------------------
# C++ parser
# ---------------------------------------------------------------------------

# Plain registrations: registry.onGet("/api/v1/...", ...)
_PLAIN_RE = re.compile(
    r"""
    ^\s*registry\.on
    (Get|Post|Put|Patch|Delete)        # 1: HTTP verb (capitalised in firmware)
    \s*\(\s*
    "                                  # opening quote
    (/api/v1/[^"]*)                    # 2: path literal
    "
    """,
    re.VERBOSE,
)

# Regex registrations: registry.onGetRegex("^\\/api\\/v1\\/zones\\/([0-3])$", ...)
_REGEX_RE = re.compile(
    r"""
    ^\s*registry\.on
    (Get|Post|Put|Patch|Delete)        # 1: HTTP verb
    Regex
    \s*\(\s*
    "                                  # opening quote
    (\^[^"]+)                          # 2: regex literal (starts with ^)
    "
    """,
    re.VERBOSE,
)

# Single-line C++ comment detector — strips before checking for registrations.
_LINE_COMMENT_RE = re.compile(r"^\s*//")


def _normalise_regex_path(regex: str) -> str:
    """Convert an ESPAsync regex route into the YAML's parameterised form.

    Examples
    --------
    ``^\\/api\\/v1\\/zones\\/([0-3])$``         → ``/api/v1/zones/{id}``
    ``^\\/api\\/v1\\/zones\\/([0-3])\\/audio$`` → ``/api/v1/zones/{id}/audio``
    ``^\\/api\\/v1\\/presets\\/([^/]+)$``       → ``/api/v1/presets/{name}``
    ``^\\/api\\/v1\\/presets\\/effects\\/([0-9]+)$`` → ``/api/v1/presets/effects/{id}``

    Heuristic: a numeric character class (``[0-9...]`` or ``[0-3]``-style ranges)
    becomes ``{id}``; everything else becomes ``{name}``. This matches the
    convention used in ``k1-rest-contract.yaml`` as of 2026-05-01.
    """

    # Strip the anchors first.
    body = regex.strip()
    if body.startswith("^"):
        body = body[1:]
    if body.endswith("$"):
        body = body[:-1]

    # Unescape escaped slashes. In the C++ source they appear as "\\/" (two
    # backslashes plus a forward slash, because the regex literal itself is
    # inside a C++ string). Strip every backslash that precedes a slash.
    body = re.sub(r"\\+/", "/", body)

    # Replace each capture group with {id} or {name} based on its character set.
    def _sub(match: re.Match) -> str:
        inner = match.group(1)
        # Treat "[0-9]+", "[0-3]", "[0-9]*" etc. as numeric → {id}.
        if re.fullmatch(r"\[[\d\-]+\]\+?\*?", inner):
            return "{id}"
        return "{name}"

    body = re.sub(r"\(([^)]+)\)", _sub, body)
    return body


def parse_cpp_routes(path: str | os.PathLike[str]) -> List[Route]:
    """Extract every REST registration from ``V1ApiRoutes.cpp``.

    The parser only inspects the first line of each registration — multi-line
    lambda bodies do not affect what we capture.
    """

    routes: List[Route] = []
    seen: Set[Tuple[str, str]] = set()

    with open(path, "r", encoding="utf-8") as fh:
        for line_no, line in enumerate(fh, start=1):
            if _LINE_COMMENT_RE.match(line):
                continue

            plain = _PLAIN_RE.search(line)
            if plain:
                method = plain.group(1).upper()
                route_path = plain.group(2)
                key = (route_path, method)
                if key not in seen:
                    seen.add(key)
                    routes.append(
                        Route(
                            path=route_path,
                            method=method,
                            source_line=line_no,
                            raw_path=route_path,
                        )
                    )
                continue

            regex_match = _REGEX_RE.search(line)
            if regex_match:
                method = regex_match.group(1).upper()
                raw = regex_match.group(2)
                normalised = _normalise_regex_path(raw)
                key = (normalised, method)
                if key not in seen:
                    seen.add(key)
                    routes.append(
                        Route(
                            path=normalised,
                            method=method,
                            source_line=line_no,
                            raw_path=raw,
                        )
                    )

    routes.sort(key=lambda r: (r.path, r.method))
    return routes


# ---------------------------------------------------------------------------
# YAML parser
# ---------------------------------------------------------------------------


def _expand_methods(value: object) -> List[str]:
    """Normalise the ``method:`` field into a list of upper-case verbs."""

    if isinstance(value, str):
        return [value.strip().upper()]
    if isinstance(value, list):
        out: List[str] = []
        for entry in value:
            if isinstance(entry, str):
                out.append(entry.strip().upper())
        return out
    return []


def parse_contract_routes(path: str | os.PathLike[str]) -> List[Route]:
    """Extract every REST endpoint from the contract YAML.

    The contract uses YAML mappings keyed by path. Because YAML mappings only
    allow one entry per key, the contract files in this repo sometimes list the
    same path twice (different methods); ``yaml.safe_load`` will silently keep
    the last one. We therefore walk the file with yaml's loader API directly so
    we can capture *every* path/method pair, even duplicates.
    """

    routes: List[Route] = []
    seen: Set[Tuple[str, str]] = set()

    with open(path, "r", encoding="utf-8") as fh:
        text = fh.read()

    loader = yaml.SafeLoader(text)
    try:
        node = loader.get_single_node()
    finally:
        loader.dispose()

    if node is None:
        return routes

    # Walk the top-level mapping looking for a key called either "endpoints"
    # or "paths" — both styles appear in the wild.
    if not isinstance(node, yaml.MappingNode):
        return routes

    target_node: yaml.MappingNode | None = None
    for key_node, value_node in node.value:
        key_value = key_node.value if isinstance(key_node, yaml.ScalarNode) else None
        if key_value in ("endpoints", "paths") and isinstance(value_node, yaml.MappingNode):
            target_node = value_node
            break

    if target_node is None:
        return routes

    for key_node, value_node in target_node.value:
        if not isinstance(key_node, yaml.ScalarNode):
            continue
        path_value = key_node.value
        if not path_value or not path_value.startswith("/"):
            continue

        # Construct the entry as plain Python so we can read ``method``.
        entry: object
        if isinstance(value_node, yaml.MappingNode):
            entry = _construct_mapping(value_node)
        else:
            entry = None

        methods: List[str] = []
        if isinstance(entry, dict):
            methods = _expand_methods(entry.get("method"))

        if not methods:
            # No usable method — record once as UNKNOWN so the drift report
            # surfaces it rather than silently dropping the entry.
            methods = ["UNKNOWN"]

        for method in methods:
            key = (path_value, method)
            if key in seen:
                continue
            seen.add(key)
            routes.append(Route(path=path_value, method=method, raw_path=path_value))

    routes.sort(key=lambda r: (r.path, r.method))
    return routes


def _construct_mapping(node: yaml.MappingNode) -> dict:
    """Cheap-and-cheerful YAML mapping → Python dict, scalars only."""

    out: dict = {}
    for key_node, value_node in node.value:
        if not isinstance(key_node, yaml.ScalarNode):
            continue
        key = key_node.value
        if isinstance(value_node, yaml.ScalarNode):
            out[key] = value_node.value
        elif isinstance(value_node, yaml.SequenceNode):
            out[key] = [
                child.value
                for child in value_node.value
                if isinstance(child, yaml.ScalarNode)
            ]
        else:
            # Nested mappings — we don't need their contents, but record presence.
            out[key] = "<mapping>"
    return out


# ---------------------------------------------------------------------------
# Drift computation
# ---------------------------------------------------------------------------


def compute_drift(firmware: Sequence[Route], contract: Sequence[Route]) -> DriftReport:
    """Diff two route sets, returning the drift report."""

    firmware_keys = {(r.path, r.method): r for r in firmware}
    contract_keys = {(r.path, r.method): r for r in contract}

    firmware_only = [
        firmware_keys[k] for k in sorted(firmware_keys.keys() - contract_keys.keys())
    ]
    contract_only = [
        contract_keys[k] for k in sorted(contract_keys.keys() - firmware_keys.keys())
    ]

    return DriftReport(
        firmware_only=firmware_only,
        contract_only=contract_only,
        total_firmware=len(firmware_keys),
        total_contract=len(contract_keys),
    )


# ---------------------------------------------------------------------------
# Output formatting
# ---------------------------------------------------------------------------


def format_report(report: DriftReport, *, cpp_path: Path, yaml_path: Path) -> str:
    """Return a human-readable drift report as a single string."""

    lines: List[str] = []
    lines.append("LightwaveOS REST contract drift report")
    lines.append("=" * 72)
    lines.append(f"  firmware source : {cpp_path}")
    lines.append(f"  contract YAML   : {yaml_path}")
    lines.append("")

    lines.append(f"Firmware-only routes ({len(report.firmware_only)}):")
    if report.firmware_only:
        for route in report.firmware_only:
            suffix = ""
            if route.raw_path and route.raw_path != route.path:
                suffix = f"   [from regex: {route.raw_path}]"
            lines.append(f"  - {route.method:<7} {route.path}{suffix}")
    else:
        lines.append("  (none)")
    lines.append("")

    lines.append(f"Contract-only routes ({len(report.contract_only)}):")
    if report.contract_only:
        for route in report.contract_only:
            lines.append(f"  - {route.method:<7} {route.path}")
    else:
        lines.append("  (none)")
    lines.append("")

    lines.append("Summary:")
    lines.append(f"  total firmware  : {report.total_firmware}")
    lines.append(f"  total contract  : {report.total_contract}")
    lines.append(f"  drift count     : {report.drift_count}")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Skeleton writer
# ---------------------------------------------------------------------------


_SKELETON_HEADER = (
    "# Auto-generated drift skeleton — DO NOT commit blindly.\n"
    "# Each entry below corresponds to a firmware-only REST route detected by\n"
    "# tools/contract-regen/regen_rest.py. Fill in the description and consumers,\n"
    "# then merge the relevant entries into k1-rest-contract.yaml.\n"
    "#\n"
    "# This file is safe to delete — it does not affect firmware behaviour.\n"
)


def write_skeleton(report: DriftReport, target_path: Path) -> None:
    """Write a YAML skeleton with FIXME entries for every firmware-only route.

    The output goes to ``<target_path>``. Caller is responsible for choosing a
    path that does NOT clobber the canonical contract.
    """

    payload: dict = {"version": "1.0-skeleton", "endpoints": {}}
    endpoints = payload["endpoints"]

    for route in report.firmware_only:
        # If a path appears with multiple methods it will overwrite — collapse
        # the methods into a list to preserve all of them.
        existing = endpoints.get(route.path)
        if existing is None:
            entry: dict = {
                "method": route.method,
                "description": f"FIXME: document {route.method} {route.path}",
                "consumers": [],
            }
            if route.raw_path and route.raw_path != route.path:
                entry["source_regex"] = route.raw_path
            endpoints[route.path] = entry
        else:
            current = existing["method"]
            if isinstance(current, str):
                if current != route.method:
                    existing["method"] = sorted([current, route.method])
            elif isinstance(current, list):
                if route.method not in current:
                    existing["method"] = sorted([*current, route.method])

    target_path.parent.mkdir(parents=True, exist_ok=True)
    with open(target_path, "w", encoding="utf-8") as fh:
        fh.write(_SKELETON_HEADER)
        yaml.safe_dump(payload, fh, sort_keys=False, default_flow_style=False)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _build_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="regen_rest.py",
        description="Detect drift between V1ApiRoutes.cpp and k1-rest-contract.yaml.",
    )
    parser.add_argument(
        "--cpp",
        type=Path,
        default=DEFAULT_CPP,
        help=f"Path to V1ApiRoutes.cpp (default: {DEFAULT_CPP})",
    )
    parser.add_argument(
        "--yaml",
        type=Path,
        default=DEFAULT_YAML,
        help=f"Path to k1-rest-contract.yaml (default: {DEFAULT_YAML})",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero when drift is detected (for CI).",
    )
    parser.add_argument(
        "--update-skeleton",
        action="store_true",
        help="Write <yaml>.regen.yaml with FIXME stubs for firmware-only routes.",
    )
    return parser


def main(
    argv: Sequence[str] | None = None,
    *,
    stdout: TextIO | None = None,
) -> int:
    parser = _build_argparser()
    args = parser.parse_args(argv[1:] if argv is not None else None)
    out = stdout if stdout is not None else sys.stdout

    cpp_path: Path = args.cpp
    yaml_path: Path = args.yaml

    if not cpp_path.exists():
        print(f"error: cpp source not found: {cpp_path}", file=out)
        return 2
    if not yaml_path.exists():
        print(f"error: contract YAML not found: {yaml_path}", file=out)
        return 2

    firmware_routes = parse_cpp_routes(cpp_path)
    contract_routes = parse_contract_routes(yaml_path)
    report = compute_drift(firmware_routes, contract_routes)

    out.write(format_report(report, cpp_path=cpp_path, yaml_path=yaml_path))

    if args.update_skeleton:
        skeleton = Path(str(yaml_path) + ".regen.yaml")
        write_skeleton(report, skeleton)
        out.write(f"\nWrote skeleton: {skeleton}\n")

    if args.strict and report.drift_count > 0:
        return 1
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main(sys.argv))
