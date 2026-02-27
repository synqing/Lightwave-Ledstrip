#!/usr/bin/env python3
"""
Generate real effect source catalogue artefacts for the K1 composer dashboard.

Outputs:
- src/code-catalog/generated/index.json
- src/code-catalog/generated/effects/<EID_...>.json
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


SCHEMA_VERSION = "2.0.0"


CLASS_RE = re.compile(r"^\s*class\s+([A-Za-z_]\w*)\b")
KID_RE = re.compile(r"kId\s*=\s*lightwaveos::(EID_[A-Z0-9_]+)")
RENDER_SIG_RE = re.compile(
    r"^\s*void\s+([A-Za-z_]\w*)::render\s*\(\s*(?:lightwaveos::)?plugins::EffectContext&\s*ctx\s*\)\s*\{"
)
METADATA_SIG_RE = re.compile(
    r"^\s*const\s+(?:lightwaveos::)?plugins::EffectMetadata&\s+([A-Za-z_]\w*)::getMetadata\s*\(\s*\)\s+const\s*\{"
)
EFFECT_ID_RE = re.compile(
    r"^\s*constexpr\s+EffectId\s+(EID_[A-Z0-9_]+)\s*=\s*0x([0-9A-Fa-f]{4})\s*;"
)


PHASES = ("input", "mapping", "modulation", "render", "post", "output")


@dataclass
class ClassDecl:
    class_name: str
    effect_id: str
    header_path: Path


def rel(root: Path, path: Path) -> str:
    return str(path.relative_to(root)).replace("\\", "/")


def load_effect_id_hex_map(effect_ids_path: Path) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for line in effect_ids_path.read_text(encoding="utf-8").splitlines():
        m = EFFECT_ID_RE.match(line)
        if not m:
            continue
        out[m.group(1)] = f"0x{m.group(2).upper()}"
    return out


def discover_classes(headers: List[Path]) -> List[ClassDecl]:
    decls: List[ClassDecl] = []
    for header in headers:
        current_class: Optional[str] = None
        for line in header.read_text(encoding="utf-8").splitlines():
            class_match = CLASS_RE.match(line)
            if class_match:
                current_class = class_match.group(1)
                continue
            kid_match = KID_RE.search(line)
            if kid_match and current_class:
                decls.append(
                    ClassDecl(
                        class_name=current_class,
                        effect_id=kid_match.group(1),
                        header_path=header,
                    )
                )
    return decls


def build_render_source_map(cpp_files: List[Path]) -> Dict[str, Path]:
    out: Dict[str, Path] = {}
    for cpp_path in cpp_files:
        for line in cpp_path.read_text(encoding="utf-8").splitlines():
            m = RENDER_SIG_RE.match(line)
            if m:
                out[m.group(1)] = cpp_path
    return out


def find_block_end(lines: List[str], start_idx: int) -> Optional[int]:
    depth = 0
    seen_open = False
    for idx in range(start_idx, len(lines)):
        line = lines[idx]
        for ch in line:
            if ch == "{":
                depth += 1
                seen_open = True
            elif ch == "}":
                if seen_open:
                    depth -= 1
                    if depth == 0:
                        return idx
    return None


def find_function_range(lines: List[str], signature: re.Pattern[str], class_name: str) -> Optional[Tuple[int, int]]:
    for idx, line in enumerate(lines):
        m = signature.match(line)
        if not m:
            continue
        if m.group(1) != class_name:
            continue
        end_idx = find_block_end(lines, idx)
        if end_idx is None:
            return None
        return (idx + 1, end_idx + 1)
    return None


def find_display_name(lines: List[str], class_name: str) -> str:
    fn_range = find_function_range(lines, METADATA_SIG_RE, class_name)
    if not fn_range:
        return class_name
    start_line, end_line = fn_range
    body = "\n".join(lines[start_line - 1 : end_line])
    m = re.search(r'"([^"]+)"', body)
    return m.group(1) if m else class_name


def to_ranges(line_numbers: List[int]) -> List[List[int]]:
    if not line_numbers:
        return []
    sorted_lines = sorted(set(line_numbers))
    ranges: List[List[int]] = []
    start = sorted_lines[0]
    prev = sorted_lines[0]
    for ln in sorted_lines[1:]:
        if ln == prev + 1:
            prev = ln
            continue
        ranges.append([start, prev])
        start = ln
        prev = ln
    ranges.append([start, prev])
    return ranges


def fallback_phase_ranges(start_line: int, end_line: int) -> Dict[str, List[List[int]]]:
    span = max(1, end_line - start_line + 1)
    offsets = {
        "input": (0.00, 0.12),
        "mapping": (0.12, 0.28),
        "modulation": (0.28, 0.46),
        "render": (0.46, 0.88),
        "post": (0.88, 0.95),
        "output": (0.95, 1.00),
    }
    out: Dict[str, List[List[int]]] = {}
    for phase in PHASES:
        begin_pct, end_pct = offsets[phase]
        begin = start_line + int(begin_pct * span)
        end = start_line + int(end_pct * span) - 1
        if phase == "output":
            end = end_line
        begin = max(start_line, min(begin, end_line))
        end = max(begin, min(end, end_line))
        out[phase] = [[begin, end]]
    return out


def infer_phase_ranges(lines: List[str], start_line: int, end_line: int) -> Tuple[Dict[str, List[List[int]]], str, List[str]]:
    line_hits: Dict[str, List[int]] = {phase: [] for phase in PHASES}
    comment_hits = set()

    comment_patterns = {
        "input": ("audio analysis", "input", "analysis", "source"),
        "mapping": ("mapping", "semantic", "normal", "map"),
        "modulation": ("modulation", "phase offset", "smooth", "beat", "flux"),
        "render": ("rendering", "render loop", "drawing"),
        "post": ("post", "postfx", "gamma", "correction"),
        "output": ("output", "strip output", "led output"),
    }
    code_patterns = {
        "input": ("ctx.audio", ".audio."),
        "mapping": ("target", "norm", "map", "semantic"),
        "modulation": ("smooth", "alpha", "beat", "flux", "phase"),
        "render": ("for (", "while (", "ctx.leds", "setcentre", "setcenter", "palette"),
        "post": ("gamma", "bloom", "post", "fade"),
        "output": ("ctx.leds[", "strip", "output"),
    }

    for line_no in range(start_line, end_line + 1):
        text = lines[line_no - 1]
        lowered = text.lower()
        stripped = lowered.strip()
        is_comment = (
            stripped.startswith("//")
            or stripped.startswith("/*")
            or stripped.startswith("*")
            or "*/" in stripped
        )

        if is_comment:
            for phase, patterns in comment_patterns.items():
                if any(p in lowered for p in patterns):
                    line_hits[phase].append(line_no)
                    comment_hits.add(phase)

        for phase, patterns in code_patterns.items():
            if any(p in lowered for p in patterns):
                line_hits[phase].append(line_no)

        if "ctx.leds[" in lowered and "=" in lowered:
            line_hits["output"].append(line_no)
            line_hits["render"].append(line_no)

    warnings: List[str] = []
    phase_ranges: Dict[str, List[List[int]]] = {phase: to_ranges(hits) for phase, hits in line_hits.items()}
    hit_count = sum(1 for phase in PHASES if phase_ranges[phase])

    if hit_count == len(PHASES) and len(comment_hits) >= 3:
        confidence = "high"
    elif hit_count >= 4:
        confidence = "medium"
    else:
        confidence = "low"
        warnings.append("Phase inference used fallback segmentation due to sparse signal.")

    fallback = fallback_phase_ranges(start_line, end_line)
    for phase in PHASES:
        if not phase_ranges[phase]:
            phase_ranges[phase] = fallback[phase]
            warnings.append(f"Phase '{phase}' inferred from fallback segmentation.")

    return phase_ranges, confidence, warnings


def firmware_hash(repo_root: Path, paths: List[Path]) -> str:
    sha = hashlib.sha256()
    for path in sorted(paths):
        relative = rel(repo_root, path)
        sha.update(relative.encode("utf-8"))
        sha.update(b"\n")
        sha.update(path.read_bytes())
        sha.update(b"\n")
    return sha.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate real effect source catalogue artefacts.")
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
        help="Repository root path.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Output directory for generated catalogue artefacts.",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    effects_dir = repo_root / "firmware-v3/src/effects/ieffect"
    effect_ids_h = repo_root / "firmware-v3/src/config/effect_ids.h"
    out_dir = (
        args.output_dir.resolve()
        if args.output_dir
        else repo_root / "k1-composer/src/code-catalog/generated"
    )
    out_effects = out_dir / "effects"
    out_effects.mkdir(parents=True, exist_ok=True)

    headers = sorted(effects_dir.glob("*.h"))
    cpps = sorted(effects_dir.glob("*.cpp"))
    if not headers or not cpps:
        raise RuntimeError("No effect headers/cpp files discovered.")

    effect_hex = load_effect_id_hex_map(effect_ids_h)
    decls = discover_classes(headers)
    render_source_map = build_render_source_map(cpps)

    warnings: List[str] = []
    generated_effects: List[Dict[str, object]] = []
    seen_effect_ids = set()

    hash_inputs = list(headers) + list(cpps) + [effect_ids_h]
    source_hash = firmware_hash(repo_root, hash_inputs)
    generated_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")

    for decl in decls:
        if decl.effect_id in seen_effect_ids:
            warnings.append(
                f"{decl.class_name}: duplicate effectId {decl.effect_id} skipped (first declaration kept)."
            )
            continue
        seen_effect_ids.add(decl.effect_id)

        source_path = render_source_map.get(decl.class_name)
        if source_path is None:
            maybe = decl.header_path.with_suffix(".cpp")
            if maybe.exists():
                source_path = maybe

        if source_path is None:
            warnings.append(f"{decl.class_name}: no source file with render() implementation found.")
            continue

        source_lines = source_path.read_text(encoding="utf-8").splitlines()
        source_text = source_path.read_text(encoding="utf-8")

        render_range = find_function_range(source_lines, RENDER_SIG_RE, decl.class_name)
        if not render_range:
            warnings.append(f"{decl.class_name}: render() not found or unbalanced braces.")
            continue

        phase_ranges, confidence, mapping_warnings = infer_phase_ranges(
            source_lines, render_range[0], render_range[1]
        )
        display_name = find_display_name(source_lines, decl.class_name)
        eid_hex = effect_hex.get(decl.effect_id, "0x0000")

        payload = {
            "schemaVersion": SCHEMA_VERSION,
            "generatedAtUtc": generated_at,
            "firmwareSourceHash": source_hash,
            "effectId": decl.effect_id,
            "effectIdHex": eid_hex,
            "className": decl.class_name,
            "displayName": display_name,
            "headerPath": rel(repo_root, decl.header_path),
            "sourcePath": rel(repo_root, source_path),
            "renderRange": [render_range[0], render_range[1]],
            "phaseRanges": phase_ranges,
            "mappingConfidence": confidence,
            "mappingWarnings": mapping_warnings,
            "sourceText": source_text,
        }

        effect_path = out_effects / f"{decl.effect_id}.json"
        effect_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        generated_effects.append(
            {
                "schemaVersion": SCHEMA_VERSION,
                "generatedAtUtc": generated_at,
                "firmwareSourceHash": source_hash,
                "effectId": decl.effect_id,
                "effectIdHex": eid_hex,
                "className": decl.class_name,
                "displayName": display_name,
                "headerPath": rel(repo_root, decl.header_path),
                "sourcePath": rel(repo_root, source_path),
                "renderRange": [render_range[0], render_range[1]],
                "phaseRanges": phase_ranges,
                "mappingConfidence": confidence,
                "mappingWarnings": mapping_warnings,
                "artifactPath": f"effects/{decl.effect_id}.json",
            }
        )

    generated_effects.sort(key=lambda e: int(str(e["effectIdHex"])[2:], 16))

    index_payload = {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAtUtc": generated_at,
        "firmwareSourceHash": source_hash,
        "effectCount": len(generated_effects),
        "effects": generated_effects,
    }
    (out_dir / "index.json").write_text(
        json.dumps(index_payload, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    print(f"Generated {len(generated_effects)} effect artefacts at {rel(repo_root, out_dir)}")
    if warnings:
        print(f"Warnings: {len(warnings)}")
        for w in warnings[:20]:
            print(f" - {w}")
        if len(warnings) > 20:
            print(f" - ... {len(warnings) - 20} more warnings")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
