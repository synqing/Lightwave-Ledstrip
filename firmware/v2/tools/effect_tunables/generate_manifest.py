#!/usr/bin/env python3
"""Generate and validate effect tunable manifest coverage."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

ALLOWED_EXCLUSION_REASONS = {"structural", "layout", "memory-only", "state-only"}
DEFAULT_ENFORCED_FAMILIES = ("INTERFERENCE", "QUANTUM", "ADVANCED_OPTICAL")

REGISTER_EFFECT_RE = re.compile(r"registerEffect\s*\(\s*(EID_[A-Z0-9_]+)\s*,")
REGISTER_EFFECT_WITH_INSTANCE_RE = re.compile(r"registerEffect\s*\(\s*(EID_[A-Z0-9_]+)\s*,\s*&([A-Za-z_]\w*)\s*\)")
EFFECT_ID_RE = re.compile(r"constexpr\s+EffectId\s+(EID_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;")
FAMILY_RE = re.compile(r"constexpr\s+uint8_t\s+FAMILY_([A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;")
CLASS_RE = re.compile(r"class\s+([A-Za-z_]\w*)\s*(?:final\s*)?:\s*public\s+[^{]+\{(.*?)\n\};", re.S)
KID_RE = re.compile(r"kId\s*=\s*(?:lightwaveos::)?(EID_[A-Z0-9_]+)\s*;")
STATIC_INSTANCE_DECL_RE = re.compile(
    r"\bstatic\s+([A-Za-z_][A-Za-z0-9_:<>]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:;|=)"
)
PARAM_NAME_STRCMP_RE = re.compile(r"(?:std::)?str(?:n)?cmp\s*\(\s*name\s*,\s*\"([a-z][a-z0-9_]*)\"")
PARAM_NAME_CTOR_RE = re.compile(r"EffectParameter\s*\(\s*\"([a-z][a-z0-9_]*)\"")
CONST_DECL_RE = re.compile(
    r"^\s*(?:static\s+)?(?:constexpr|const)\s+[A-Za-z_][A-Za-z0-9_:<>,\s\*&]*\s+"
    r"(k[A-Za-z0-9_]+)\s*(?:\[[^\]]*\])?\s*=\s*[^;]+;"
)

STRUCTURAL_TOKENS = {
    "size", "sizes", "count", "counts", "length", "lengths", "depth", "depths",
    "max", "min", "capacity", "slot", "slots", "index", "indices", "idx",
    "buffer", "buffers", "array", "arrays", "led", "leds", "strip", "strips",
    "width", "height", "sample", "samples", "dim", "dims", "historydepth",
    "bin", "bins",
}
LAYOUT_TOKENS = {
    "centre", "center", "origin", "pair", "left", "right", "top", "bottom",
    "channel", "channels", "topology", "row", "rows", "column", "columns",
}
MEMORY_TOKENS = {
    "psram", "heap", "alloc", "malloc", "cache", "dma", "stack", "mem", "memory",
}
STATE_TOKENS = {
    "phase", "timer", "cursor", "history", "frame", "frames", "prev", "curr", "next",
    "state", "seed", "accum", "accumulator", "env", "envelope", "smoothed", "target",
    "last", "write", "read", "runtime", "story", "clock", "introphase",
}


@dataclass(frozen=True)
class ClassInfo:
    class_name: str
    header_path: Path
    cpp_path: Optional[Path]


@dataclass(frozen=True)
class ConstantDecl:
    name: str
    path: Path
    line: int


def parse_int_literal(value: str) -> int:
    return int(value, 16) if value.lower().startswith("0x") else int(value)


def remove_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    out_lines = []
    for line in text.splitlines():
        if "//" in line:
            line = line.split("//", 1)[0]
        out_lines.append(line)
    return "\n".join(out_lines)


def to_repo_relative(path: Path, repo_root: Path) -> str:
    return path.resolve().relative_to(repo_root.resolve()).as_posix()


def split_identifier_tokens(identifier: str) -> List[str]:
    if identifier.startswith("k_"):
        identifier = identifier[2:]
    elif identifier.startswith("k") and len(identifier) > 1 and identifier[1].isupper():
        identifier = identifier[1:]

    identifier = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", identifier)
    tokens = [tok for tok in re.split(r"[^A-Za-z0-9]+", identifier.lower()) if tok]
    return tokens


def canonicalise_name(name: str, *, is_constant: bool = False) -> str:
    if is_constant:
        if name.startswith("k_"):
            name = name[2:]
        elif name.startswith("k") and len(name) > 1 and name[1].isupper():
            name = name[1:]
    return "".join(ch for ch in name.lower() if ch.isalnum())


def classify_exclusion_reason(constant_name: str) -> Optional[str]:
    if constant_name in {"kTwoPi", "kBandMap", "kWaveformPoints"}:
        return "structural"

    tokens = split_identifier_tokens(constant_name)
    if not tokens:
        return None

    token_set = set(tokens)
    if token_set & MEMORY_TOKENS:
        return "memory-only"
    if token_set & STRUCTURAL_TOKENS:
        return "structural"
    if token_set & LAYOUT_TOKENS:
        return "layout"
    if token_set & STATE_TOKENS:
        return "state-only"
    return None


def discover_active_effect_ids(core_effects_path: Path) -> List[str]:
    text = remove_cpp_comments(core_effects_path.read_text(encoding="utf-8", errors="replace"))
    seen = set()
    active: List[str] = []
    for effect_id in REGISTER_EFFECT_RE.findall(text):
        if effect_id not in seen:
            seen.add(effect_id)
            active.append(effect_id)
    return active


def discover_active_effect_registrations(core_effects_path: Path) -> List[Tuple[str, str]]:
    text = remove_cpp_comments(core_effects_path.read_text(encoding="utf-8", errors="replace"))
    seen = set()
    registrations: List[Tuple[str, str]] = []
    for effect_id, instance_name in REGISTER_EFFECT_WITH_INSTANCE_RE.findall(text):
        if effect_id in seen:
            continue
        seen.add(effect_id)
        registrations.append((effect_id, instance_name))
    return registrations


def discover_active_families(core_effects_path: Path, effect_ids_path: Path) -> List[str]:
    active_effect_ids = discover_active_effect_ids(core_effects_path)
    effect_id_values = parse_effect_ids(effect_ids_path)
    family_codes = parse_family_codes(effect_ids_path)

    seen = set()
    families: List[str] = []
    for effect_id in active_effect_ids:
        effect_id_value = effect_id_values.get(effect_id)
        if effect_id_value is None:
            continue
        family_code = (effect_id_value >> 8) & 0xFF
        family_name = family_codes.get(family_code, "UNKNOWN")
        if family_name in seen:
            continue
        seen.add(family_name)
        families.append(family_name)
    return families


def discover_instance_class_map(core_effects_path: Path) -> Dict[str, str]:
    text = remove_cpp_comments(core_effects_path.read_text(encoding="utf-8", errors="replace"))
    mapping: Dict[str, str] = {}
    for class_name, instance_name in STATIC_INSTANCE_DECL_RE.findall(text):
        mapping.setdefault(instance_name, class_name.split("::")[-1])
    return mapping


def parse_effect_ids(effect_ids_path: Path) -> Dict[str, int]:
    text = effect_ids_path.read_text(encoding="utf-8", errors="replace")
    ids: Dict[str, int] = {}
    for effect_id, value in EFFECT_ID_RE.findall(text):
        ids[effect_id] = parse_int_literal(value)
    return ids


def parse_family_codes(effect_ids_path: Path) -> Dict[int, str]:
    text = effect_ids_path.read_text(encoding="utf-8", errors="replace")
    family_codes: Dict[int, str] = {}
    for family_name, value in FAMILY_RE.findall(text):
        family_codes[parse_int_literal(value)] = family_name
    return family_codes


def discover_cpp_files(ieffect_root: Path) -> List[Path]:
    return sorted(ieffect_root.rglob("*.cpp"))


def discover_class_map(ieffect_root: Path) -> Tuple[Dict[str, ClassInfo], Dict[str, ClassInfo]]:
    headers = sorted(ieffect_root.rglob("*.h"))
    cpp_files = discover_cpp_files(ieffect_root)
    cpp_text_cache: Dict[Path, str] = {
        cpp_path: cpp_path.read_text(encoding="utf-8", errors="replace") for cpp_path in cpp_files
    }

    class_map_by_id: Dict[str, ClassInfo] = {}
    class_map_by_name: Dict[str, ClassInfo] = {}
    for header_path in headers:
        text = header_path.read_text(encoding="utf-8", errors="replace")
        for class_match in CLASS_RE.finditer(text):
            class_name = class_match.group(1)
            class_body = class_match.group(2)
            kid_match = KID_RE.search(class_body)
            if not kid_match:
                continue

            effect_id = kid_match.group(1)
            cpp_path = header_path.with_suffix(".cpp")
            resolved_cpp: Optional[Path] = None

            if cpp_path.exists():
                resolved_cpp = cpp_path
            else:
                needle = f"{class_name}::"
                candidates = [path for path, source in cpp_text_cache.items() if needle in source]
                if candidates:
                    resolved_cpp = sorted(candidates)[0]

            info = ClassInfo(class_name=class_name, header_path=header_path, cpp_path=resolved_cpp)
            class_map_by_id[effect_id] = info
            class_map_by_name[class_name] = info
    return class_map_by_id, class_map_by_name


def extract_exposed_parameter_names(paths: Sequence[Path]) -> List[str]:
    names = set()
    for path in paths:
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in PARAM_NAME_STRCMP_RE.findall(text):
            names.add(match)
        for match in PARAM_NAME_CTOR_RE.findall(text):
            names.add(match)
    return sorted(names)


def extract_named_constants(paths: Sequence[Path]) -> List[ConstantDecl]:
    constants: Dict[str, ConstantDecl] = {}
    for path in paths:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for line_no, line in enumerate(handle, start=1):
                match = CONST_DECL_RE.match(line)
                if not match:
                    continue
                name = match.group(1)
                if name == "kId":
                    continue
                if not (name.startswith("k_") or (name.startswith("k") and len(name) > 1 and name[1].isupper())):
                    continue
                constants.setdefault(name, ConstantDecl(name=name, path=path, line=line_no))
    return sorted(constants.values(), key=lambda item: item.name)


def build_effect_entry(
    effect_id: str,
    effect_id_value: int,
    family_name: str,
    class_info: Optional[ClassInfo],
    instance_name: Optional[str],
    registered_class_name: Optional[str],
    repo_root: Path,
) -> Dict[str, object]:
    source_paths: List[Path] = []
    source_files: List[str] = []
    class_name = registered_class_name or "<unresolved>"

    if class_info:
        if class_name == "<unresolved>":
            class_name = class_info.class_name
        source_paths.append(class_info.header_path)
        source_files.append(to_repo_relative(class_info.header_path, repo_root))
        if class_info.cpp_path:
            source_paths.append(class_info.cpp_path)
            source_files.append(to_repo_relative(class_info.cpp_path, repo_root))

    exposed_parameters = extract_exposed_parameter_names(source_paths)
    exposed_canonical: Dict[str, List[str]] = {}
    for param_name in exposed_parameters:
        exposed_canonical.setdefault(canonicalise_name(param_name), []).append(param_name)

    constants = extract_named_constants(source_paths)
    discovered_named_tunables: List[Dict[str, object]] = []
    excluded_named_constants: List[Dict[str, object]] = []
    matched_param_names = set()

    for constant in constants:
        canonical = canonicalise_name(constant.name, is_constant=True)
        if canonical in exposed_canonical:
            matched_names = sorted(exposed_canonical[canonical])
            matched_param_names.update(matched_names)
            discovered_named_tunables.append(
                {
                    "name": constant.name,
                    "canonical": canonical,
                    "source": "constant",
                    "file": to_repo_relative(constant.path, repo_root),
                    "line": constant.line,
                    "exposed": True,
                    "matched_parameters": matched_names,
                }
            )
            continue

        exclusion_reason = classify_exclusion_reason(constant.name)
        if exclusion_reason:
            excluded_named_constants.append(
                {
                    "name": constant.name,
                    "canonical": canonical,
                    "reason": exclusion_reason,
                    "file": to_repo_relative(constant.path, repo_root),
                    "line": constant.line,
                }
            )
            continue

        discovered_named_tunables.append(
            {
                "name": constant.name,
                "canonical": canonical,
                "source": "constant",
                "file": to_repo_relative(constant.path, repo_root),
                "line": constant.line,
                "exposed": False,
                "matched_parameters": [],
            }
        )

    for param_name in exposed_parameters:
        if param_name in matched_param_names:
            continue
        discovered_named_tunables.append(
            {
                "name": param_name,
                "canonical": canonicalise_name(param_name),
                "source": "api",
                "file": None,
                "line": None,
                "exposed": True,
                "matched_parameters": [param_name],
            }
        )

    discovered_named_tunables.sort(key=lambda item: (item["name"], item["source"]))
    excluded_named_constants.sort(key=lambda item: item["name"])

    missing_tunable_count = sum(1 for item in discovered_named_tunables if not item["exposed"])

    return {
        "effect_id": effect_id,
        "effect_id_hex": f"0x{effect_id_value:04X}",
        "family": family_name,
        "class_name": class_name,
        "registered_instance": instance_name,
        "source_files": source_files,
        "exposed_parameters": exposed_parameters,
        "discovered_named_tunables": discovered_named_tunables,
        "excluded_named_constants": excluded_named_constants,
        "missing_tunable_count": missing_tunable_count,
    }


def generate_manifest(
    repo_root: Path,
    enforced_families: Sequence[str],
    coverage_mode: str,
) -> Dict[str, object]:
    core_effects_path = repo_root / "firmware/v2/src/effects/CoreEffects.cpp"
    effect_ids_path = repo_root / "firmware/v2/src/config/effect_ids.h"
    ieffect_root = repo_root / "firmware/v2/src/effects/ieffect"

    active_effect_registrations = discover_active_effect_registrations(core_effects_path)
    effect_id_values = parse_effect_ids(effect_ids_path)
    family_codes = parse_family_codes(effect_ids_path)
    class_map_by_id, class_map_by_name = discover_class_map(ieffect_root)
    instance_class_map = discover_instance_class_map(core_effects_path)

    effects: List[Dict[str, object]] = []
    family_missing: Dict[str, int] = {}

    for effect_id, instance_name in active_effect_registrations:
        effect_id_value = effect_id_values.get(effect_id)
        if effect_id_value is None:
            continue
        family_code = (effect_id_value >> 8) & 0xFF
        family_name = family_codes.get(family_code, "UNKNOWN")
        registered_class_name = instance_class_map.get(instance_name)
        class_info = None
        if registered_class_name:
            class_info = class_map_by_name.get(registered_class_name)
        if class_info is None:
            class_info = class_map_by_id.get(effect_id)
        entry = build_effect_entry(
            effect_id=effect_id,
            effect_id_value=effect_id_value,
            family_name=family_name,
            class_info=class_info,
            instance_name=instance_name,
            registered_class_name=registered_class_name,
            repo_root=repo_root,
        )
        effects.append(entry)
        family_missing[family_name] = family_missing.get(family_name, 0) + int(entry["missing_tunable_count"])

    total_exposed = sum(len(effect["exposed_parameters"]) for effect in effects)
    total_discovered = sum(len(effect["discovered_named_tunables"]) for effect in effects)
    total_missing = sum(int(effect["missing_tunable_count"]) for effect in effects)

    return {
        "manifest_version": 1,
        "coverage_mode": coverage_mode,
        "enforced_families": list(enforced_families),
        "active_effect_source": "firmware/v2/src/effects/CoreEffects.cpp:registerAllEffects/registerEffect",
        "effects": effects,
        "summary": {
            "active_effect_count": len(effects),
            "effects_with_api_parameters": sum(1 for effect in effects if effect["exposed_parameters"]),
            "total_exposed_parameters": total_exposed,
            "total_discovered_named_tunables": total_discovered,
            "total_missing_named_tunables": total_missing,
            "missing_named_tunables_by_family": {
                family: count for family, count in sorted(family_missing.items()) if count > 0
            },
        },
    }


def coverage_failures(manifest: Dict[str, object]) -> List[str]:
    enforced_families = set(manifest.get("enforced_families", []))
    failures: List[str] = []

    for effect in manifest.get("effects", []):
        family = effect.get("family")
        if family not in enforced_families:
            continue

        for tunable in effect.get("discovered_named_tunables", []):
            if tunable.get("exposed"):
                continue
            failures.append(
                f"{effect.get('effect_id')} [{family}] missing API exposure for {tunable.get('name')}"
            )

        for exclusion in effect.get("excluded_named_constants", []):
            reason = exclusion.get("reason")
            if reason not in ALLOWED_EXCLUSION_REASONS:
                failures.append(
                    f"{effect.get('effect_id')} has invalid exclusion reason '{reason}' for {exclusion.get('name')}"
                )

    return failures


def load_json(path: Path) -> Dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_manifest(path: Path, manifest: Dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(manifest, indent=2, sort_keys=False)
    path.write_text(text + "\n", encoding="utf-8")


def validate_manifest(
    path: Path,
    generated_manifest: Dict[str, object],
) -> Tuple[bool, List[str]]:
    errors: List[str] = []

    if not path.exists():
        errors.append(f"Manifest missing: {path}")
        return False, errors

    existing = load_json(path)
    if existing != generated_manifest:
        errors.append("Manifest is stale. Re-run generator to update committed JSON.")

    errors.extend(coverage_failures(generated_manifest))

    return len(errors) == 0, errors


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest-path",
        default="firmware/v2/docs/effects/tunable_manifest.json",
        help="Path to tunable manifest JSON",
    )
    parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate existing manifest and enforce family coverage",
    )
    parser.add_argument(
        "--print",
        dest="print_manifest",
        action="store_true",
        help="Print generated JSON to stdout",
    )
    parser.add_argument(
        "--enforce-family",
        action="append",
        default=[],
        help="Family name to enforce coverage for (repeatable)",
    )
    parser.add_argument(
        "--no-default-enforced-families",
        action="store_true",
        help="Disable built-in enforced families",
    )
    parser.add_argument(
        "--enforce-all-families",
        action="store_true",
        help="Enforce coverage for all currently active effect families",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    script_path = Path(__file__).resolve()
    repo_root = script_path.parents[4]
    manifest_path = (repo_root / args.manifest_path).resolve()
    core_effects_path = repo_root / "firmware/v2/src/effects/CoreEffects.cpp"
    effect_ids_path = repo_root / "firmware/v2/src/config/effect_ids.h"
    active_families = discover_active_families(core_effects_path, effect_ids_path)

    enforced_families: List[str] = []
    if not args.no_default_enforced_families:
        enforced_families.extend(DEFAULT_ENFORCED_FAMILIES)
    if args.enforce_all_families:
        enforced_families.extend(active_families)
    enforced_families.extend(args.enforce_family)

    seen_families = set()
    deduped_families: List[str] = []
    for family in enforced_families:
        if family in seen_families:
            continue
        seen_families.add(family)
        deduped_families.append(family)

    active_family_set = set(active_families)
    enforced_family_set = set(deduped_families)
    coverage_mode = (
        "full" if active_family_set and active_family_set.issubset(enforced_family_set) else "phased"
    )

    manifest = generate_manifest(
        repo_root=repo_root,
        enforced_families=deduped_families,
        coverage_mode=coverage_mode,
    )

    if args.print_manifest:
        json.dump(manifest, sys.stdout, indent=2)
        sys.stdout.write("\n")

    if args.validate:
        ok, errors = validate_manifest(path=manifest_path, generated_manifest=manifest)
        if not ok:
            for error in errors:
                print(f"[FAIL] {error}", file=sys.stderr)
            return 1
        print("[OK] Tunable manifest is up to date and coverage gate passed.")
        return 0

    write_manifest(path=manifest_path, manifest=manifest)
    print(f"[OK] Wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
