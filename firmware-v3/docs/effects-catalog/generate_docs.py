#!/usr/bin/env python3
"""
LightwaveOS Effects Documentation Pipeline -- Synthesis Agent
Generates four Markdown documents from upstream JSON analysis outputs.
"""

import json
import os
import sys
from datetime import datetime

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
GENERATION_DATE = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
DOC_VERSION = "1.0.0"


# ─────────────────────────────────────────────────────────────────────
# Load all inputs
# ─────────────────────────────────────────────────────────────────────

def load_json(filename):
    path = os.path.join(OUTPUT_DIR, filename)
    if not os.path.exists(path):
        print(f"FATAL: Missing input file: {path}", file=sys.stderr)
        sys.exit(1)
    with open(path) as f:
        try:
            return json.load(f)
        except json.JSONDecodeError as e:
            print(f"FATAL: Cannot parse {filename}: {e}", file=sys.stderr)
            sys.exit(1)


manifest = load_json("manifest.json")
inventory = load_json("inventory.json")
math_appendix = load_json("math-appendix.json")
taxonomy = load_json("taxonomy.json")

print("All four JSON inputs loaded successfully.")


# ─────────────────────────────────────────────────────────────────────
# Terminology normalisation
# ─────────────────────────────────────────────────────────────────────

NORMALISATION_MAP = {
    "center-origin": "centre-origin",
    "center origin": "centre-origin",
    "centre origin": "centre-origin",
    "chromagram bin": "chroma bin",
    "chroma channel": "chroma bin",
    "circular mean": "circular weighted mean",
    "weighted circular average": "circular weighted mean",
    "circular exponential moving average": "circular EMA",
    "angular EMA": "circular EMA",
    "light guide plate": "Light Guide Plate (LGP)",
    "lightguide": "Light Guide Plate (LGP)",
    "ws2812": "WS2812",
    "WS2812B": "WS2812",
    "controlBus": "ControlBus",
    "control bus": "ControlBus",
    "audio bus": "ControlBus",
    "hue value": "palette hue",
    "HSV hue": "palette hue",
    "tau": "time constant (tau)",
    "time_constant": "time constant (tau)",
    "smoothing constant": "time constant (tau)",
    # British English
    "color": "colour",
    "center": "centre",
    "initialize": "initialise",
    "behavior": "behaviour",
    "artifact": "artefact",
}


def normalise_text(text):
    """Apply terminology normalisation to text."""
    if not isinstance(text, str):
        return text
    result = text
    for old, new in NORMALISATION_MAP.items():
        # Case-sensitive replacements only for specific terms
        if old[0].isupper():
            result = result.replace(old, new)
        else:
            # Case-insensitive for lowercase terms, but only at word boundaries
            import re
            result = re.sub(r'\b' + re.escape(old) + r'\b', new, result,
                            flags=re.IGNORECASE)
    return result


# ─────────────────────────────────────────────────────────────────────
# Step 1: Cross-Reference Validation
# ─────────────────────────────────────────────────────────────────────

print("\n=== Cross-Reference Validation ===")

contradictions = []
missing_items = {
    "effects_not_found": [],
    "functions_not_found": [],
    "files_not_found": [],
}
unclassified_effects = []
unresolved_xrefs = []

# Build lookup maps
inv_effects = {e["id"]: e for e in inventory["effects"]}
inv_ids = set(inv_effects.keys())

# 1. Effect ID consistency: every inventory ID should appear in taxonomy
tax_effect_ids = set()
for rp in taxonomy["rendering_patterns"]:
    for e in rp["effects_using"]:
        tax_effect_ids.add(e["id"])

inv_only = inv_ids - tax_effect_ids
tax_only = tax_effect_ids - inv_ids

for eid in sorted(inv_only):
    eff = inv_effects[eid]
    if eff.get("removed"):
        # Removed effects may not appear in taxonomy -- INFO level
        contradictions.append({
            "type": "removed_effect_not_in_taxonomy",
            "severity": "INFO",
            "effect_id": eid,
            "effect_name": eff["display_name"],
            "detail": f"Removed effect {eid} ({eff['display_name']}) not classified "
                      f"in any rendering pattern. Expected for removed slots.",
        })
    else:
        contradictions.append({
            "type": "effect_id_mismatch",
            "severity": "WARNING",
            "effect_id": eid,
            "effect_name": eff["display_name"],
            "detail": f"Effect {eid} ({eff['display_name']}) exists in inventory "
                      f"but not in any taxonomy rendering pattern.",
        })

for eid in sorted(tax_only):
    contradictions.append({
        "type": "effect_id_mismatch",
        "severity": "WARNING",
        "effect_id": eid,
        "detail": f"Effect {eid} appears in taxonomy rendering patterns "
                  f"but not in inventory.",
    })

print(f"  Effect ID check: {len(inv_only)} in inventory only, "
      f"{len(tax_only)} in taxonomy only")

# 2. Function usage consistency
for fn in math_appendix["functions"]:
    for used_by_entry in fn.get("used_by", []):
        if isinstance(used_by_entry, dict):
            eid = used_by_entry.get("id")
        elif isinstance(used_by_entry, int):
            eid = used_by_entry
        else:
            continue
        if eid is not None and eid not in inv_ids:
            contradictions.append({
                "type": "phantom_function_reference",
                "severity": "WARNING",
                "function": fn["name"],
                "referenced_effect_id": eid,
                "detail": f"Function {fn['name']} references effect ID {eid} "
                          f"in used_by, but this ID is not in inventory.",
            })

print(f"  Function usage check: "
      f"{sum(1 for c in contradictions if c['type'] == 'phantom_function_reference')} "
      f"phantom references")

# 3. File path consistency
# file_index may be a list of dicts with 'path' key, or a dict keyed by path
fi_raw = manifest.get("file_index", [])
if isinstance(fi_raw, list):
    file_index = set(entry.get("path", "") for entry in fi_raw)
else:
    file_index = set(fi_raw.keys())

# The manifest uses paths relative to firmware/v2/src/ while inventory and
# math-appendix may use full repo-relative paths. Build a normalised set
# that includes both forms for matching.
PREFIX = "firmware/v2/src/"
file_index_normalised = set(file_index)
for fp in list(file_index):
    file_index_normalised.add(PREFIX + fp)

all_referenced_files = set()
for e in inventory["effects"]:
    for fkey in ("header", "source"):
        fp = e.get("files", {}).get(fkey)
        if fp:
            all_referenced_files.add(fp)
for fn in math_appendix["functions"]:
    fp = fn.get("file")
    if fp:
        all_referenced_files.add(fp)

orphan_files = all_referenced_files - file_index_normalised
for fp in sorted(orphan_files):
    missing_items["files_not_found"].append({
        "severity": "INFO",
        "path": fp,
        "detail": f"File '{fp}' referenced in analysis but not in manifest file_index.",
    })

print(f"  File path check: {len(orphan_files)} orphan file references")

# 4. Chroma migration consistency
tax_migration_effects = {}
for mp in taxonomy["migration_patterns"]:
    pid = mp["pattern_id"]
    for e in mp["effects_using"]:
        tax_migration_effects[e["id"]] = pid

for e in inventory["effects"]:
    inv_pattern = e.get("chroma", {}).get("migration_pattern")
    if inv_pattern is None:
        continue
    # Map inventory pattern letter to taxonomy pattern_id
    tax_pattern_id = f"chroma_pattern_{inv_pattern}" if len(str(inv_pattern)) == 1 else None
    if tax_pattern_id is None:
        # Non-standard pattern like "additional_user"
        continue
    tax_entry = tax_migration_effects.get(e["id"])
    if tax_entry is None:
        contradictions.append({
            "type": "chroma_migration_mismatch",
            "severity": "WARNING",
            "effect_id": e["id"],
            "effect_name": e["display_name"],
            "detail": f"Effect {e['id']} ({e['display_name']}) has migration_pattern "
                      f"'{inv_pattern}' in inventory but is not listed in taxonomy "
                      f"migration pattern {tax_pattern_id}.",
        })
    elif tax_entry != tax_pattern_id:
        contradictions.append({
            "type": "chroma_migration_mismatch",
            "severity": "CRITICAL",
            "effect_id": e["id"],
            "effect_name": e["display_name"],
            "detail": f"Effect {e['id']} ({e['display_name']}) is pattern "
                      f"'{inv_pattern}' in inventory but '{tax_entry}' in taxonomy.",
        })

print(f"  Chroma migration check: "
      f"{sum(1 for c in contradictions if c['type'] == 'chroma_migration_mismatch')} "
      f"mismatches")

# 5. Audio-reactive consistency
tax_audio_effects = set()
for am in taxonomy["audio_coupling_modes"]:
    for e in am["effects_using"]:
        tax_audio_effects.add(e["id"])

for e in inventory["effects"]:
    is_audio = e.get("audio_reactive", False)
    in_tax_audio = e["id"] in tax_audio_effects
    if is_audio and not in_tax_audio:
        contradictions.append({
            "type": "audio_reactive_mismatch",
            "severity": "WARNING",
            "effect_id": e["id"],
            "effect_name": e["display_name"],
            "detail": f"Effect {e['id']} ({e['display_name']}) is audio_reactive in "
                      f"inventory but has no audio coupling mode in taxonomy.",
        })
    elif not is_audio and in_tax_audio:
        contradictions.append({
            "type": "audio_reactive_mismatch",
            "severity": "WARNING",
            "effect_id": e["id"],
            "effect_name": e["display_name"],
            "detail": f"Effect {e['id']} ({e['display_name']}) has audio coupling mode "
                      f"in taxonomy but is not audio_reactive in inventory.",
        })

print(f"  Audio-reactive check: "
      f"{sum(1 for c in contradictions if c['type'] == 'audio_reactive_mismatch')} "
      f"mismatches")

# Collect all gaps from upstream
all_upstream_gaps = []
for g in inventory.get("gaps", []):
    g["source"] = "inventory"
    all_upstream_gaps.append(g)
for g in math_appendix.get("gaps", []):
    g["source"] = "math-appendix"
    all_upstream_gaps.append(g)
for g in taxonomy.get("gaps", []):
    g["source"] = "taxonomy"
    all_upstream_gaps.append(g)
for g in manifest.get("gaps", []):
    g["source"] = "manifest"
    all_upstream_gaps.append(g)

print(f"  Total upstream gaps: {len(all_upstream_gaps)}")
print(f"  Total contradictions found: {len(contradictions)}")

# Check for unclassified effects
tax_xref = taxonomy.get("cross_reference_validation", {})
for ue in tax_xref.get("unclassified_effects", []):
    unclassified_effects.append(ue)


# ─────────────────────────────────────────────────────────────────────
# Helper: write file
# ─────────────────────────────────────────────────────────────────────

def write_doc(filename, content):
    path = os.path.join(OUTPUT_DIR, filename)
    with open(path, "w") as f:
        f.write(content)
    size = os.path.getsize(path)
    print(f"  Wrote {filename}: {size:,} bytes")
    return size


# ─────────────────────────────────────────────────────────────────────
# Step 3: Generate EFFECTS_INVENTORY.md
# ─────────────────────────────────────────────────────────────────────

print("\n=== Generating EFFECTS_INVENTORY.md ===")


def generate_effects_inventory():
    lines = []
    L = lines.append

    L(f"# LightwaveOS Effects Inventory")
    L(f"")
    L(f"**Version:** {DOC_VERSION}")
    L(f"**Generated:** {GENERATION_DATE}")
    L(f"")
    L(f"---")
    L(f"")

    # Summary Statistics
    total = inventory["total_effects"]
    registered = inventory["registered_effects"]
    removed = inventory["removed_slots"]
    audio_count = sum(1 for e in inventory["effects"] if e.get("audio_reactive"))
    non_audio = total - audio_count
    families = {}
    for e in inventory["effects"]:
        fam = e.get("family", "UNKNOWN")
        families[fam] = families.get(fam, 0) + 1

    L(f"## Summary Statistics")
    L(f"")
    L(f"| Metric | Value |")
    L(f"|--------|-------|")
    L(f"| Total effect slots | {total} |")
    L(f"| Registered (active) | {registered} |")
    L(f"| Removed slots | {len(removed)} (IDs: {', '.join(str(r) for r in removed)}) |")
    L(f"| Audio-reactive | {audio_count} |")
    L(f"| Non-audio-reactive | {non_audio} |")
    L(f"| Effect families | {len(families)} |")
    L(f"")

    # Chroma migration summary
    chroma_patterns = {}
    for e in inventory["effects"]:
        p = e.get("chroma", {}).get("migration_pattern")
        key = str(p) if p is not None else "none"
        chroma_patterns[key] = chroma_patterns.get(key, 0) + 1

    L(f"### Chroma Migration Summary")
    L(f"")
    L(f"| Migration Pattern | Count | Status |")
    L(f"|-------------------|-------|--------|")
    pattern_status = {
        "A": "Migrated -- static utility functions replaced by "
             "[`circularChromaHueSmoothed()`](MATH_APPENDIX.md#circularchromahuesmoothed)",
        "B": "Migrated -- linear EMA on circular quantity replaced by "
             "[`circularEma()`](MATH_APPENDIX.md#circularema)",
        "C": "Migrated -- raw inline argmax replaced by "
             "[`circularChromaHue()`](MATH_APPENDIX.md#circularchromahue)",
        "D": "Native implementation -- Schmitt trigger with "
             "[`circularChromaHueSmoothed()`](MATH_APPENDIX.md#circularchromahuesmoothed)",
        "additional_user": "Additional effects using ChromaUtils",
        "none": "No chroma processing",
    }
    for p in ["A", "B", "C", "D", "additional_user", "none"]:
        cnt = chroma_patterns.get(p, 0)
        status = pattern_status.get(p, "Unknown")
        L(f"| {p} | {cnt} | {status} |")
    L(f"")

    # Effects by Family
    L(f"## Effects by Family")
    L(f"")
    for fam_name in sorted(families.keys()):
        fam_effects = [e for e in inventory["effects"] if e.get("family") == fam_name]
        fam_effects.sort(key=lambda e: e["id"])
        L(f"### {fam_name} ({len(fam_effects)} effects)")
        L(f"")
        L(f"| ID | Name | Class | Audio | Chroma | Centre-Origin |")
        L(f"|----|------|-------|-------|--------|---------------|")
        for e in fam_effects:
            eid = e["id"]
            name = e["display_name"]
            cls = e["class_name"]
            audio = "Yes" if e.get("audio_reactive") else "No"
            chroma_p = e.get("chroma", {}).get("migration_pattern")
            chroma_str = f"Pattern {chroma_p}" if chroma_p else "--"
            centre = "Yes" if e.get("centre_origin") else "No"
            removed_mark = " (REMOVED)" if e.get("removed") else ""
            L(f"| {eid} | {name}{removed_mark} | `{cls}` | {audio} | "
              f"{chroma_str} | {centre} |")
        L(f"")

    # Audio-Reactive Effects
    L(f"## Audio-Reactive Effects")
    L(f"")
    L(f"Effects that respond to audio input via the "
      f"[ControlBus](MATH_APPENDIX.md#audio-feature-access) system.")
    L(f"")

    L(f"### Effects by Migration Pattern")
    L(f"")
    for pattern_letter in ["A", "B", "C", "D"]:
        pattern_effects = [
            e for e in inventory["effects"]
            if e.get("chroma", {}).get("migration_pattern") == pattern_letter
        ]
        if not pattern_effects:
            continue
        L(f"#### Pattern {pattern_letter}")
        L(f"")
        L(f"See [Pattern Taxonomy -- Pattern {pattern_letter}]"
          f"(PATTERN_TAXONOMY.md#pattern-{pattern_letter.lower()}-"
          f"{'static-utility-functions' if pattern_letter == 'A' else ''}"
          f"{'linear-ema-on-circular-quantity' if pattern_letter == 'B' else ''}"
          f"{'raw-inline-argmax' if pattern_letter == 'C' else ''}"
          f"{'experimental-pack-schmitt-trigger' if pattern_letter == 'D' else ''}"
          f") for migration details.")
        L(f"")
        for e in sorted(pattern_effects, key=lambda x: x["id"]):
            uses_cu = "Yes" if e.get("chroma", {}).get("uses_chromautils") else "No"
            L(f"- **{e['display_name']}** (ID {e['id']}, `{e['class_name']}`) "
              f"-- uses ChromaUtils: {uses_cu}")
        L(f"")

    # Additional audio users
    additional = [
        e for e in inventory["effects"]
        if e.get("chroma", {}).get("migration_pattern") == "additional_user"
    ]
    if additional:
        L(f"#### Additional ChromaUtils Users")
        L(f"")
        for e in sorted(additional, key=lambda x: x["id"]):
            L(f"- **{e['display_name']}** (ID {e['id']}, `{e['class_name']}`)")
        L(f"")

    # Complete Effects Table (alphabetical)
    L(f"## Complete Effects Table (Alphabetical)")
    L(f"")
    L(f"| ID | Name | Class | Family | Audio | Chroma | Files |")
    L(f"|----|------|-------|--------|-------|--------|-------|")
    for e in sorted(inventory["effects"], key=lambda x: x["display_name"].lower()):
        eid = e["id"]
        name = e["display_name"]
        cls = e["class_name"]
        fam = e.get("family", "?")
        audio = "Yes" if e.get("audio_reactive") else "No"
        chroma_p = e.get("chroma", {}).get("migration_pattern")
        chroma_str = f"Pattern {chroma_p}" if chroma_p else "--"
        header = e.get("files", {}).get("header", "")
        source = e.get("files", {}).get("source", "")
        # Shorten paths for table readability
        header_short = header.split("/")[-1] if header else "--"
        source_short = source.split("/")[-1] if source else "--"
        removed_mark = " **(REMOVED)**" if e.get("removed") else ""
        L(f"| {eid} | {name}{removed_mark} | `{cls}` | {fam} | {audio} | "
          f"{chroma_str} | `{header_short}`, `{source_short}` |")
    L(f"")

    # Appendix: Effect ID Allocation Map
    L(f"## Appendix: Effect ID Allocation Map")
    L(f"")
    L(f"The following table shows the full ID allocation sequence. "
      f"Gaps indicate unused or removed slots.")
    L(f"")

    all_ids = sorted(e["id"] for e in inventory["effects"])
    max_id = max(all_ids) if all_ids else 0
    id_set = set(all_ids)
    gaps_in_sequence = []
    for i in range(max_id + 1):
        if i not in id_set:
            gaps_in_sequence.append(i)

    L(f"- **ID range:** 0 -- {max_id}")
    L(f"- **Allocated:** {len(all_ids)}")
    L(f"- **Gaps in sequence:** {len(gaps_in_sequence)}")
    if gaps_in_sequence:
        L(f"- **Unused IDs:** {', '.join(str(g) for g in gaps_in_sequence)}")
    L(f"")

    # Compact allocation map
    L(f"```")
    L(f"ID    Effect Name")
    L(f"----  --------------------------------------------------")
    for e in sorted(inventory["effects"], key=lambda x: x["id"]):
        removed_mark = " [REMOVED]" if e.get("removed") else ""
        L(f"{e['id']:<5} {e['display_name']}{removed_mark}")
    L(f"```")
    L(f"")

    return "\n".join(lines)


inv_doc = generate_effects_inventory()
inv_size = write_doc("EFFECTS_INVENTORY.md", inv_doc)


# ─────────────────────────────────────────────────────────────────────
# Step 4: Generate MATH_APPENDIX.md
# ─────────────────────────────────────────────────────────────────────

print("\n=== Generating MATH_APPENDIX.md ===")


def generate_math_appendix():
    lines = []
    L = lines.append

    L(f"# LightwaveOS Effects -- Mathematical Functions Reference")
    L(f"")
    L(f"**Version:** {DOC_VERSION}")
    L(f"**Generated:** {GENERATION_DATE}")
    L(f"")
    L(f"---")
    L(f"")

    # Overview
    L(f"## Overview")
    L(f"")
    L(f"This document catalogues all mathematical functions, constants, and "
      f"lookup tables used by the")
    L(f"LightwaveOS effects engine. Functions are organised by domain and "
      f"cross-referenced to the")
    L(f"[Effects Inventory](EFFECTS_INVENTORY.md) and "
      f"[Pattern Taxonomy](PATTERN_TAXONOMY.md).")
    L(f"")
    L(f"| Category | Functions | Description |")
    L(f"|----------|-----------|-------------|")
    for cat_id, cat_data in math_appendix["utility_categories"].items():
        L(f"| {cat_id.replace('_', ' ').title()} | {cat_data['function_count']} "
          f"| {normalise_text(cat_data['description'])} |")
    L(f"")
    L(f"**Total functions documented:** {len(math_appendix['functions'])}")
    L(f"")
    L(f"**Total constants documented:** {len(math_appendix['constants'])}")
    L(f"")

    # Organise functions by category
    categories_order = [
        "chroma_processing",
        "smoothing_primitives",
        "rendering_helpers",
        "audio_feature_access",
        "beat_processing",
        "noise",
        "interpolation",
        "geometry",
    ]

    category_display = {
        "chroma_processing": "Chroma Processing Functions",
        "smoothing_primitives": "Smoothing Primitives",
        "rendering_helpers": "Rendering Helpers",
        "audio_feature_access": "Audio Feature Access",
        "beat_processing": "Beat Processing",
        "noise": "Noise and Pseudo-Random",
        "interpolation": "Interpolation",
        "geometry": "Geometry and Spatial",
    }

    functions_by_cat = {}
    for fn in math_appendix["functions"]:
        cat = fn.get("category", "uncategorised")
        functions_by_cat.setdefault(cat, []).append(fn)

    for cat_id in categories_order:
        cat_fns = functions_by_cat.get(cat_id, [])
        if not cat_fns:
            continue

        cat_display = category_display.get(cat_id, cat_id.replace("_", " ").title())
        L(f"## {cat_display}")
        L(f"")

        cat_meta = math_appendix["utility_categories"].get(cat_id, {})
        if cat_meta.get("description"):
            L(f"{normalise_text(cat_meta['description'])}")
            L(f"")

        for fn in cat_fns:
            # Anchor-friendly name
            anchor_name = fn["name"].lower().replace("::", "").replace("_", "")
            L(f"### {fn['name']}")
            L(f"")
            line_info = f" (line {fn['line']})" if fn.get('line') else ""
            L(f"**File:** `{fn.get('file', 'unknown')}`{line_info}")
            L(f"")
            if fn.get("namespace"):
                L(f"**Namespace:** `{fn['namespace']}`")
                L(f"")

            # Signature
            L(f"**Signature:**")
            L(f"")
            L(f"```cpp")
            L(f"{fn.get('signature', fn['name'] + '()')}")
            L(f"```")
            L(f"")

            # Parameters
            params = fn.get("parameters", [])
            if params:
                L(f"**Parameters:**")
                L(f"")
                L(f"| Name | Type | Description |")
                L(f"|------|------|-------------|")
                for p in params:
                    desc = normalise_text(p.get("description", "--"))
                    L(f"| `{p.get('name', '?')}` | `{p.get('type', '?')}` | {desc} |")
                L(f"")

            # Return type
            if fn.get("return_type"):
                L(f"**Returns:** `{fn['return_type']}`")
                L(f"")

            # Formula
            if fn.get("formula"):
                L(f"**Formula:**")
                L(f"")
                L(f"```")
                formula = normalise_text(fn["formula"])
                # Wrap long formulas
                if len(formula) > 110:
                    words = formula.split()
                    current_line = ""
                    formula_lines = []
                    for w in words:
                        if len(current_line) + len(w) + 1 > 110:
                            formula_lines.append(current_line)
                            current_line = w
                        else:
                            current_line = (current_line + " " + w).strip()
                    if current_line:
                        formula_lines.append(current_line)
                    for fl in formula_lines:
                        L(fl)
                else:
                    L(formula)
                L(f"```")
                L(f"")

            # Domain
            if fn.get("domain"):
                L(f"**Domain:** {normalise_text(fn['domain'])}")
                L(f"")

            # Computational cost
            cost = fn.get("computational_cost", {})
            if cost:
                L(f"**Computational Cost:**")
                L(f"")
                parts = []
                if cost.get("trig_calls"):
                    parts.append(f"{cost['trig_calls']} trig call(s)")
                if cost.get("exp_calls"):
                    parts.append(f"{cost['exp_calls']} exp call(s)")
                if cost.get("multiply_accumulates"):
                    parts.append(
                        f"{cost['multiply_accumulates']} multiply-accumulate(s)")
                if cost.get("estimated_cycles"):
                    parts.append(f"~{cost['estimated_cycles']} estimated cycles")
                if cost.get("branches"):
                    parts.append(f"{cost['branches']} branch(es)")
                if cost.get("divisions"):
                    parts.append(f"{cost['divisions']} division(s)")
                if cost.get("sqrt_calls"):
                    parts.append(f"{cost['sqrt_calls']} sqrt call(s)")
                if cost.get("sin_cos_calls"):
                    parts.append(f"{cost['sin_cos_calls']} sin/cos call(s)")
                if cost.get("loop_iterations"):
                    parts.append(f"{cost['loop_iterations']} loop iteration(s)")
                if cost.get("table_lookups"):
                    parts.append(f"{cost['table_lookups']} table lookup(s)")
                L(f"- {'; '.join(parts)}")
                L(f"")

            # Frame-rate independence
            fri = fn.get("frame_rate_independent")
            if fri is not None:
                L(f"**Frame-rate independent:** {'Yes' if fri else 'No'}")
                L(f"")

            # Used by
            used_by = fn.get("used_by", [])
            if used_by:
                L(f"**Used by:**")
                L(f"")
                for ub in used_by:
                    if isinstance(ub, dict):
                        ub_id = ub.get("id", "?")
                        ub_name = ub.get("name", ub.get("class", "?"))
                    else:
                        ub_id = ub
                        ub_name = inv_effects.get(ub, {}).get("display_name", "?")
                    L(f"- [{ub_name}](EFFECTS_INVENTORY.md) (ID {ub_id})")
                L(f"")

            # Notes
            if fn.get("notes"):
                L(f"**Notes:** {normalise_text(fn['notes'])}")
                L(f"")

            L(f"---")
            L(f"")

    # Handle any uncategorised functions
    uncategorised = functions_by_cat.get("uncategorised", [])
    if uncategorised:
        L(f"## Uncategorised Functions")
        L(f"")
        for fn in uncategorised:
            L(f"### {fn['name']}")
            L(f"")
            L(f"**File:** `{fn.get('file', 'unknown')}`")
            L(f"")
            if fn.get("signature"):
                L(f"```cpp")
                L(f"{fn['signature']}")
                L(f"```")
                L(f"")
            if fn.get("notes"):
                L(f"{normalise_text(fn['notes'])}")
                L(f"")
            L(f"---")
            L(f"")

    # Constants and Lookup Tables
    L(f"## Constants and Lookup Tables")
    L(f"")
    L(f"| Name | Type | File | Domain |")
    L(f"|------|------|------|--------|")
    for c in math_appendix["constants"]:
        L(f"| `{c['name']}` | `{c.get('type', '?')}` | `{c.get('file', '?')}` "
          f"| {normalise_text(c.get('domain', '--'))} |")
    L(f"")

    # Detailed constant entries
    for c in math_appendix["constants"]:
        L(f"### {c['name']}")
        L(f"")
        c_line_info = f" (line {c['line']})" if c.get('line') else ""
        L(f"**File:** `{c.get('file', 'unknown')}`{c_line_info}")
        L(f"")
        L(f"**Type:** `{c.get('type', '?')}`")
        L(f"")
        if c.get("formula"):
            L(f"**Formula:** `{c['formula']}`")
            L(f"")
        if c.get("values") is not None:
            vals = c["values"]
            if isinstance(vals, list):
                if len(vals) <= 20:
                    L(f"**Values:** `[{', '.join(str(v) for v in vals)}]`")
                else:
                    L(f"**Values:** `[{', '.join(str(v) for v in vals[:10])}, "
                      f"... ({len(vals)} total)]`")
            else:
                L(f"**Value:** `{vals}`")
            L(f"")
        if c.get("domain"):
            L(f"**Domain:** {normalise_text(c['domain'])}")
            L(f"")
        L(f"")

    # Domain Cross-Reference
    L(f"## Domain Cross-Reference")
    L(f"")
    L(f"Functions grouped by domain, for quick lookup when implementing "
      f"or debugging effects.")
    L(f"")
    for domain_name, fn_names in math_appendix.get("domain_index", {}).items():
        display_name = domain_name.replace("_", " ").title()
        L(f"### {display_name}")
        L(f"")
        for fn_name in fn_names:
            anchor = fn_name.lower().replace("::", "").replace("_", "")
            L(f"- [`{fn_name}`](#{anchor})")
        L(f"")

    # Computational Cost Summary
    L(f"## Computational Cost Summary")
    L(f"")
    L(f"Estimated per-call cost for each function, to aid in per-frame "
      f"budget planning.")
    L(f"Target: keep total per-frame effect code under ~2 ms at 120 FPS.")
    L(f"")
    L(f"| Function | Trig | Exp | MACs | Est. Cycles |")
    L(f"|----------|------|-----|------|-------------|")
    for fn in math_appendix["functions"]:
        cost = fn.get("computational_cost", {})
        trig = cost.get("trig_calls", 0)
        exp = cost.get("exp_calls", 0)
        macs = cost.get("multiply_accumulates", 0)
        cycles = cost.get("estimated_cycles", "?")
        L(f"| `{fn['name']}` | {trig} | {exp} | {macs} | {cycles} |")
    L(f"")

    return "\n".join(lines)


math_doc = generate_math_appendix()
math_size = write_doc("MATH_APPENDIX.md", math_doc)


# ─────────────────────────────────────────────────────────────────────
# Step 5: Generate PATTERN_TAXONOMY.md
# ─────────────────────────────────────────────────────────────────────

print("\n=== Generating PATTERN_TAXONOMY.md ===")


def generate_pattern_taxonomy():
    lines = []
    L = lines.append

    L(f"# LightwaveOS Effects -- Rendering Pattern Taxonomy")
    L(f"")
    L(f"**Version:** {DOC_VERSION}")
    L(f"**Generated:** {GENERATION_DATE}")
    L(f"")
    L(f"---")
    L(f"")

    # Classification System
    L(f"## Classification System")
    L(f"")
    L(f"Every effect in the LightwaveOS library is classified along "
      f"four orthogonal axes:")
    L(f"")
    L(f"1. **Spatial Rendering Pattern** -- How the effect generates its "
      f"visual output across the LED strip")
    L(f"2. **Colour Derivation Method** -- How the effect determines "
      f"which colours to display")
    L(f"3. **Audio Coupling Mode** -- How (if at all) the effect "
      f"responds to audio input")
    L(f"4. **Chroma Migration Pattern** -- Historical classification "
      f"of how chroma-to-hue conversion was implemented")
    L(f"")
    L(f"All effects adhere to the **centre-origin** constraint: "
      f"visual patterns radiate from LED 79/80 outward")
    L(f"(or converge inward to 79/80). No linear sweeps are permitted.")
    L(f"")

    # Spatial Rendering Patterns
    L(f"## Spatial Rendering Patterns")
    L(f"")

    spatial_display = {
        "centre_origin_radial_expand": ("Radial Expansion", "radial"),
        "centre_origin_wave_propagation": ("Wave Propagation", "wave_propagation"),
        "multi_wave_interference": ("Interference", "interference"),
        "discrete_particle_system": ("Particle Systems", "particle"),
        "continuous_field_computation": ("Field Computations", "field"),
        "direct_audio_waveform": ("Waveform Visualisation", "waveform"),
        "mathematical_geometric": ("Geometric", "geometric"),
    }

    for rp in taxonomy["rendering_patterns"]:
        pid = rp["pattern_id"]
        display_info = spatial_display.get(pid, (rp.get("name", pid), pid))
        display_name = display_info[0]

        L(f"### {display_name}")
        L(f"")
        L(f"{normalise_text(rp.get('description', ''))}")
        L(f"")

        markers = rp.get("implementation_markers", [])
        if markers:
            L(f"**Implementation markers:** `{', '.join(markers)}`")
            L(f"")

        L(f"**Centre-origin compliant:** "
          f"{'Yes' if rp.get('compliant_with_centre_origin') else 'No'}")
        L(f"")

        if rp.get("notes"):
            L(f"**Notes:** {normalise_text(rp['notes'])}")
            L(f"")

        effects = rp.get("effects_using", [])
        L(f"**Effects using this pattern ({len(effects)}):**")
        L(f"")
        L(f"| ID | Name | Class |")
        L(f"|----|------|-------|")
        for e in sorted(effects, key=lambda x: x["id"]):
            L(f"| {e['id']} | [{e['name']}](EFFECTS_INVENTORY.md) | "
              f"`{e['class']}` |")
        L(f"")

    # Colour Derivation Methods
    L(f"## Colour Derivation Methods")
    L(f"")

    colour_display = {
        "circular_chroma_smoothed": "Circular Chroma (Smoothed)",
        "chroma_hue": "Chroma Hue (Instantaneous)",
        "palette_position": "Palette Position",
        "global_hue": "Global Hue",
        "static_colour": "Static Colour",
        "heat_color": "Heat Colour Map",
    }

    for cm in taxonomy["colour_derivation_methods"]:
        mid = cm["method_id"]
        display_name = colour_display.get(mid, cm.get("name", mid))

        L(f"### {display_name}")
        L(f"")
        L(f"{normalise_text(cm.get('description', ''))}")
        L(f"")

        if cm.get("function"):
            L(f"**Function:** "
              f"[`{cm['function']}`](MATH_APPENDIX.md#{cm['function'].lower().replace('::', '').replace('()', '')})")
            L(f"")

        effects = cm.get("effects_using", [])
        L(f"**Effects using this method ({len(effects)}):**")
        L(f"")
        # If many effects, use compact list
        if len(effects) > 15:
            L(f"| ID | Name |")
            L(f"|----|------|")
            for e in sorted(effects, key=lambda x: x["id"]):
                ename = e.get("name", e.get("class", "?"))
                L(f"| {e['id']} | {ename} |")
        else:
            for e in sorted(effects, key=lambda x: x["id"]):
                ename = e.get("name", e.get("class", "?"))
                L(f"- **{ename}** (ID {e['id']})")
        L(f"")

    # Audio Coupling Modes
    L(f"## Audio Coupling Modes")
    L(f"")
    L(f"Effects that respond to audio input via the ControlBus system. "
      f"See [Audio Feature Access](MATH_APPENDIX.md#audio-feature-access) "
      f"in the Math Appendix for accessor documentation.")
    L(f"")

    audio_display = {
        "rms_intensity": "RMS Intensity",
        "beat_trigger": "Beat Trigger",
        "frequency_band": "Frequency Band",
        "chroma_hue": "Chroma Hue",
    }

    for am in taxonomy["audio_coupling_modes"]:
        mid = am["mode_id"]
        display_name = audio_display.get(mid, am.get("name", mid))

        L(f"### {display_name}")
        L(f"")
        L(f"{normalise_text(am.get('description', ''))}")
        L(f"")

        if am.get("controlbus_field"):
            L(f"**ControlBus field:** `{am['controlbus_field']}`")
            L(f"")

        effects = am.get("effects_using", [])
        L(f"**Effects using this mode ({len(effects)}):**")
        L(f"")
        L(f"| ID | Name |")
        L(f"|----|------|")
        for e in sorted(effects, key=lambda x: x["id"]):
            ename = e.get("name", e.get("class", "?"))
            L(f"| {e['id']} | {ename} |")
        L(f"")

    # Migration Patterns (Historical)
    L(f"## Migration Patterns (Historical)")
    L(f"")
    L(f"The chroma-to-hue conversion code underwent a systematic "
      f"migration from ad-hoc inline implementations to the centralised "
      f"[ChromaUtils](MATH_APPENDIX.md#chroma-processing-functions) "
      f"library. Four distinct patterns were identified and migrated.")
    L(f"")

    migration_display = {
        "chroma_pattern_A": (
            "Pattern A: Static Utility Functions",
            "Duplicated `dominantChromaBin12()` + `chromaBinToHue()` static functions."
        ),
        "chroma_pattern_B": (
            "Pattern B: Linear EMA on Circular Quantity",
            "Applied standard (linear) exponential moving average to a circular "
            "hue value, causing wrap-around artefacts at the 0/255 boundary."
        ),
        "chroma_pattern_C": (
            "Pattern C: Raw Inline Argmax",
            "Inline argmax over 12 chroma bins with direct bin-to-hue mapping. "
            "Produced jarring hue jumps when two bins had similar magnitudes."
        ),
        "chroma_pattern_D": (
            "Pattern D: Experimental Pack (Schmitt Trigger)",
            "Schmitt trigger hysteresis combined with "
            "`circularChromaHueSmoothed()`. Native implementation, not a migration."
        ),
    }

    for mp in taxonomy["migration_patterns"]:
        pid = mp["pattern_id"]
        display_info = migration_display.get(pid, (mp.get("name", pid), ""))
        display_name = display_info[0]
        display_desc = display_info[1]

        L(f"### {display_name}")
        L(f"")
        L(f"{normalise_text(display_desc)}")
        L(f"")
        L(f"**Status:** {mp.get('status', 'unknown')}")
        L(f"")

        if mp.get("before_code_signature"):
            L(f"**Before:**")
            L(f"")
            L(f"```cpp")
            L(f"{mp['before_code_signature']}")
            L(f"```")
            L(f"")

        if mp.get("after_code_signature"):
            L(f"**After:**")
            L(f"")
            L(f"```cpp")
            L(f"{mp['after_code_signature']}")
            L(f"```")
            L(f"")

        effects = mp.get("effects_using", [])
        L(f"**Affected effects ({len(effects)}):**")
        L(f"")
        for e in sorted(effects, key=lambda x: x["id"]):
            ename = e.get("name", e.get("class", "?"))
            L(f"- [{ename}](EFFECTS_INVENTORY.md) (ID {e['id']})")
        L(f"")

    # Cross-Reference Matrix
    L(f"## Cross-Reference Matrix")
    L(f"")
    L(f"Sparse matrix showing which spatial rendering pattern(s) "
      f"each effect uses.")
    L(f"")

    # Build the matrix
    # Columns: rendering pattern IDs
    rp_ids = [rp["pattern_id"] for rp in taxonomy["rendering_patterns"]]
    rp_short = {
        "continuous_field_computation": "Field",
        "centre_origin_radial_expand": "Radial",
        "centre_origin_wave_propagation": "Wave",
        "discrete_particle_system": "Particle",
        "multi_wave_interference": "Interf.",
        "mathematical_geometric": "Geom.",
        "direct_audio_waveform": "Waveform",
    }

    # Build effect -> patterns map
    effect_patterns = {}
    for rp in taxonomy["rendering_patterns"]:
        pid = rp["pattern_id"]
        for e in rp["effects_using"]:
            effect_patterns.setdefault(e["id"], set()).add(pid)

    # Get all unique effect IDs from taxonomy
    all_tax_effects = {}
    for rp in taxonomy["rendering_patterns"]:
        for e in rp["effects_using"]:
            all_tax_effects[e["id"]] = e.get("name", "?")

    # Header
    cols = " | ".join(rp_short.get(pid, pid[:8]) for pid in rp_ids)
    L(f"| ID | Name | {cols} |")
    sep = " | ".join("---" for _ in rp_ids)
    L(f"|----|------|{sep}|")

    for eid in sorted(all_tax_effects.keys()):
        ename = all_tax_effects[eid]
        pats = effect_patterns.get(eid, set())
        marks = []
        for pid in rp_ids:
            marks.append("X" if pid in pats else "")
        row = " | ".join(marks)
        L(f"| {eid} | {ename} | {row} |")
    L(f"")

    return "\n".join(lines)


tax_doc = generate_pattern_taxonomy()
tax_size = write_doc("PATTERN_TAXONOMY.md", tax_doc)


# ─────────────────────────────────────────────────────────────────────
# Step 6: Generate GAP_REPORT.md
# ─────────────────────────────────────────────────────────────────────

print("\n=== Generating GAP_REPORT.md ===")


def generate_gap_report():
    lines = []
    L = lines.append

    L(f"# Effects Documentation Pipeline -- Gap Report")
    L(f"")
    L(f"**Version:** {DOC_VERSION}")
    L(f"**Generated:** {GENERATION_DATE}")
    L(f"")
    L(f"---")
    L(f"")

    # Summary
    total_gaps = (len(all_upstream_gaps) + len(contradictions)
                  + len(missing_items["effects_not_found"])
                  + len(missing_items["functions_not_found"])
                  + len(missing_items["files_not_found"])
                  + len(unclassified_effects)
                  + len(unresolved_xrefs))

    critical_count = sum(
        1 for c in contradictions if c.get("severity") == "CRITICAL"
    ) + sum(
        1 for g in all_upstream_gaps
        if g.get("impact") == "high" or g.get("severity") == "high"
    )
    warning_count = sum(
        1 for c in contradictions if c.get("severity") == "WARNING"
    ) + sum(
        1 for g in all_upstream_gaps
        if g.get("impact") == "medium" or g.get("severity") == "medium"
    )
    info_count = total_gaps - critical_count - warning_count

    L(f"## Summary")
    L(f"")
    L(f"| Severity | Count |")
    L(f"|----------|-------|")
    L(f"| CRITICAL | {critical_count} |")
    L(f"| WARNING | {warning_count} |")
    L(f"| INFO | {info_count} |")
    L(f"| **Total** | **{total_gaps}** |")
    L(f"")
    L(f"**Sources:**")
    L(f"")
    source_counts = {}
    for g in all_upstream_gaps:
        s = g.get("source", "unknown")
        source_counts[s] = source_counts.get(s, 0) + 1
    source_counts["cross-reference-validation"] = len(contradictions)
    for s, c in sorted(source_counts.items()):
        L(f"- {s}: {c} gap(s)")
    L(f"")

    # Contradictions Between Outputs
    L(f"## Contradictions Between Outputs")
    L(f"")
    if not contradictions:
        L(f"No contradictions found between the three analysis outputs.")
    else:
        L(f"The following inconsistencies were found during cross-reference "
          f"validation of the inventory, taxonomy, and math appendix outputs.")
        L(f"")

        # Group by type
        by_type = {}
        for c in contradictions:
            by_type.setdefault(c["type"], []).append(c)

        for ctype, items in sorted(by_type.items()):
            L(f"### {ctype.replace('_', ' ').title()}")
            L(f"")
            for c in items:
                severity = c.get("severity", "INFO")
                L(f"- **[{severity}]** {c['detail']}")
            L(f"")
    L(f"")

    # Missing Items
    L(f"## Missing Items")
    L(f"")

    L(f"### Effects Not Found")
    L(f"")
    if not missing_items["effects_not_found"]:
        L(f"No missing effects detected.")
    else:
        for item in missing_items["effects_not_found"]:
            L(f"- **[{item.get('severity', 'INFO')}]** {item['detail']}")
    L(f"")

    L(f"### Functions Not Found")
    L(f"")
    if not missing_items["functions_not_found"]:
        L(f"No missing functions detected.")
    else:
        for item in missing_items["functions_not_found"]:
            L(f"- **[{item.get('severity', 'INFO')}]** {item['detail']}")
    L(f"")

    L(f"### Files Not Found")
    L(f"")
    if not missing_items["files_not_found"]:
        L(f"No orphan file references detected.")
    else:
        for item in missing_items["files_not_found"]:
            L(f"- **[{item.get('severity', 'INFO')}]** `{item['path']}` -- "
              f"{item['detail']}")
    L(f"")

    # Unclassified Effects
    L(f"## Unclassified Effects")
    L(f"")
    if not unclassified_effects:
        L(f"All effects are classified in at least one rendering pattern.")
    else:
        for ue in unclassified_effects:
            L(f"- Effect ID {ue.get('id', '?')}: {ue.get('name', '?')}")
    L(f"")

    # Upstream Gaps
    L(f"## Upstream Agent Gaps")
    L(f"")
    L(f"These gaps were reported by the upstream analysis agents.")
    L(f"")

    for source_name in ["manifest", "inventory", "math-appendix", "taxonomy"]:
        source_gaps = [g for g in all_upstream_gaps
                       if g.get("source") == source_name]
        if not source_gaps:
            continue

        L(f"### From {source_name}")
        L(f"")
        for g in source_gaps:
            severity = "INFO"
            if g.get("impact") == "high" or g.get("severity") == "high":
                severity = "CRITICAL"
            elif g.get("impact") == "medium" or g.get("severity") == "medium":
                severity = "WARNING"
            elif g.get("severity"):
                severity = g["severity"].upper()

            desc = g.get("description", g.get("context", "No description"))
            note = g.get("note", "")
            L(f"- **[{severity}]** {normalise_text(desc)}")
            if note:
                L(f"  - Note: {normalise_text(note)}")
        L(f"")

    # Unresolved Cross-References
    L(f"## Unresolved Cross-References")
    L(f"")
    if not unresolved_xrefs:
        L(f"No unresolved cross-references detected.")
    else:
        for xr in unresolved_xrefs:
            L(f"- {xr}")
    L(f"")

    # Recommendations
    L(f"## Recommendations")
    L(f"")

    # Generate recommendations based on findings
    recommendations = []

    # Critical items first
    critical_gaps = [c for c in contradictions if c.get("severity") == "CRITICAL"]
    critical_upstream = [
        g for g in all_upstream_gaps
        if g.get("impact") == "high" or g.get("severity") == "high"
    ]

    if critical_gaps:
        for c in critical_gaps:
            recommendations.append({
                "priority": "CRITICAL",
                "action": f"Resolve contradiction: {c['detail']}",
                "reason": "Data integrity issue between pipeline outputs.",
            })

    if critical_upstream:
        for g in critical_upstream:
            desc = g.get("description", g.get("context", ""))
            recommendations.append({
                "priority": "CRITICAL",
                "action": f"Investigate: {normalise_text(desc)}",
                "reason": f"Reported by {g.get('source', 'unknown')} agent.",
            })

    # Warning items
    audio_mismatches = [
        c for c in contradictions
        if c["type"] == "audio_reactive_mismatch"
    ]
    if audio_mismatches:
        recommendations.append({
            "priority": "WARNING",
            "action": (f"Review {len(audio_mismatches)} audio-reactive "
                       f"classification mismatches between inventory and taxonomy."),
            "reason": "May indicate incomplete audio analysis or "
                      "incorrect audio_reactive flags.",
        })

    id_mismatches = [
        c for c in contradictions
        if c["type"] == "effect_id_mismatch" and c.get("severity") == "WARNING"
    ]
    if id_mismatches:
        recommendations.append({
            "priority": "WARNING",
            "action": (f"Review {len(id_mismatches)} effects that appear in "
                       f"inventory but not in taxonomy rendering patterns."),
            "reason": "These effects may need manual classification.",
        })

    # General recommendations
    recommendations.append({
        "priority": "INFO",
        "action": "Re-run the pipeline after addressing CRITICAL gaps to "
                  "verify consistency.",
        "reason": "Ensures all corrections propagate through all documents.",
    })

    for r in recommendations:
        L(f"### [{r['priority']}] {r['action']}")
        L(f"")
        L(f"{r['reason']}")
        L(f"")

    return "\n".join(lines)


gap_doc = generate_gap_report()
gap_size = write_doc("GAP_REPORT.md", gap_doc)


# ─────────────────────────────────────────────────────────────────────
# Final Summary
# ─────────────────────────────────────────────────────────────────────

print("\n=== SYNTHESIS COMPLETE ===")
total_size = inv_size + math_size + tax_size + gap_size
total_gaps_count = (len(all_upstream_gaps) + len(contradictions)
                    + len(missing_items["files_not_found"]))
print(f"  Documents generated: 4")
print(f"  Total combined size: {total_size:,} bytes")
print(f"  Gaps in gap report: {total_gaps_count}")
print(f"  Cross-reference contradictions: {len(contradictions)}")
print(f"    CRITICAL: {sum(1 for c in contradictions if c.get('severity') == 'CRITICAL')}")
print(f"    WARNING:  {sum(1 for c in contradictions if c.get('severity') == 'WARNING')}")
print(f"    INFO:     {sum(1 for c in contradictions if c.get('severity') == 'INFO')}")
