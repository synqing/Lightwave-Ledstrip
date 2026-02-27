#!/usr/bin/env python3
"""
Validate generated effect catalogue artefacts for the K1 composer dashboard.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, List, Set, Tuple


PHASES = ("input", "mapping", "modulation", "render", "post", "output")
SCHEMA_VERSION = "2.0.0"


def rel(root: Path, path: Path) -> str:
    return str(path.relative_to(root)).replace("\\", "/")


def discover_expected_effect_ids(repo_root: Path) -> Set[str]:
    from generate_effect_catalog import discover_classes  # local script import

    headers = sorted((repo_root / "firmware-v3/src/effects/ieffect").glob("*.h"))
    decls = discover_classes(headers)
    return {d.effect_id for d in decls}


def current_firmware_hash(repo_root: Path) -> str:
    from generate_effect_catalog import firmware_hash  # local script import

    headers = sorted((repo_root / "firmware-v3/src/effects/ieffect").glob("*.h"))
    cpps = sorted((repo_root / "firmware-v3/src/effects/ieffect").glob("*.cpp"))
    effect_ids = repo_root / "firmware-v3/src/config/effect_ids.h"
    return firmware_hash(repo_root, list(headers) + list(cpps) + [effect_ids])


def valid_range_pair(value: object) -> bool:
    if not isinstance(value, list) or len(value) != 2:
        return False
    if not isinstance(value[0], int) or not isinstance(value[1], int):
        return False
    return value[0] > 0 and value[1] >= value[0]


def check_ranges_within(
    ranges: List[List[int]], parent_start: int, parent_end: int
) -> Tuple[bool, str]:
    for pair in ranges:
        if not valid_range_pair(pair):
            return False, f"Invalid range pair: {pair!r}"
        start, end = pair
        if start < parent_start or end > parent_end:
            return False, f"Range {pair!r} not within renderRange [{parent_start}, {parent_end}]"
    return True, ""


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate generated effect catalogue.")
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
        help="Repository root path.",
    )
    parser.add_argument(
        "--catalog-dir",
        type=Path,
        default=None,
        help="Generated catalogue directory (default: src/code-catalog/generated).",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail on any validation error.",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    catalog_dir = (
        args.catalog_dir.resolve()
        if args.catalog_dir
        else repo_root / "k1-composer/src/code-catalog/generated"
    )
    index_path = catalog_dir / "index.json"

    errors: List[str] = []
    warnings: List[str] = []

    if not index_path.exists():
        errors.append(f"Missing generated index: {rel(repo_root, index_path)}")
        for e in errors:
            print(f"ERROR: {e}")
        return 1

    index = json.loads(index_path.read_text(encoding="utf-8"))
    if index.get("schemaVersion") != SCHEMA_VERSION:
        errors.append(
            f"index.json schemaVersion must be {SCHEMA_VERSION}, got {index.get('schemaVersion')!r}"
        )

    expected_ids = discover_expected_effect_ids(repo_root)
    listed = index.get("effects")
    if not isinstance(listed, list):
        errors.append("index.json effects must be a list")
        listed = []

    seen_ids: Set[str] = set()
    current_hash = current_firmware_hash(repo_root)
    if index.get("firmwareSourceHash") != current_hash:
        errors.append("firmwareSourceHash mismatch: generated artefacts are stale")

    for entry in listed:
        if not isinstance(entry, dict):
            errors.append("index entry is not an object")
            continue

        eid = entry.get("effectId")
        if not isinstance(eid, str) or not eid.startswith("EID_"):
            errors.append(f"Invalid effectId in index entry: {eid!r}")
            continue

        if eid in seen_ids:
            errors.append(f"Duplicate effectId in index: {eid}")
            continue
        seen_ids.add(eid)

        artifact = entry.get("artifactPath")
        if not isinstance(artifact, str) or not artifact.endswith(".json"):
            errors.append(f"{eid}: invalid artifactPath {artifact!r}")
            continue

        effect_path = catalog_dir / artifact
        if not effect_path.exists():
            errors.append(f"{eid}: missing artefact file {rel(repo_root, effect_path)}")
            continue

        payload = json.loads(effect_path.read_text(encoding="utf-8"))
        if payload.get("schemaVersion") != SCHEMA_VERSION:
            errors.append(f"{eid}: schemaVersion mismatch in artefact")

        if payload.get("effectId") != eid:
            errors.append(f"{eid}: effectId mismatch between index and artefact")

        source_text = payload.get("sourceText")
        if not isinstance(source_text, str) or source_text.strip() == "":
            errors.append(f"{eid}: empty sourceText")

        render_range = payload.get("renderRange")
        if not valid_range_pair(render_range):
            errors.append(f"{eid}: invalid renderRange {render_range!r}")
            continue
        render_start, render_end = render_range

        phase_ranges = payload.get("phaseRanges")
        if not isinstance(phase_ranges, dict):
            errors.append(f"{eid}: phaseRanges must be an object")
            continue

        for phase in PHASES:
            ranges = phase_ranges.get(phase)
            if not isinstance(ranges, list) or not ranges:
                errors.append(f"{eid}: phase '{phase}' missing or empty")
                continue
            ok, msg = check_ranges_within(ranges, render_start, render_end)
            if not ok:
                errors.append(f"{eid}: phase '{phase}' invalid: {msg}")

        mapping_warnings = payload.get("mappingWarnings", [])
        if isinstance(mapping_warnings, list) and mapping_warnings:
            warnings.append(f"{eid}: {len(mapping_warnings)} mapping warning(s)")

    missing = sorted(expected_ids - seen_ids)
    extra = sorted(seen_ids - expected_ids)
    if missing:
        errors.append(f"Missing effect IDs in generated index: {', '.join(missing[:15])}")
        if len(missing) > 15:
            errors.append(f"... and {len(missing) - 15} more missing IDs")
    if extra:
        warnings.append(f"Extra effect IDs not found in headers: {', '.join(extra[:15])}")

    print(f"Validated catalogue: {len(seen_ids)} entries")
    print(f"Warnings: {len(warnings)}")
    for w in warnings[:15]:
        print(f"WARN: {w}")
    if len(warnings) > 15:
        print(f"WARN: ... {len(warnings) - 15} more warnings")

    if errors:
        print(f"Errors: {len(errors)}")
        for e in errors:
            print(f"ERROR: {e}")
        return 1 if args.strict else 0

    print("Validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
