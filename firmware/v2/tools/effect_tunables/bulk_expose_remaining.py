#!/usr/bin/env python3
"""Bulk expose generic tunables for effects that still report zero parameters.

This sprint utility is intentionally conservative and idempotent:
- targets effects that are still zero-exposed in the manifest
- also repairs effects previously touched by AUTO_TUNABLES_BULK markers
- removes stale injected blocks/methods before re-inserting clean scaffolding
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


AUTO_TAG = "AUTO_TUNABLES_BULK"


@dataclass
class EffectEntry:
    effect_id: str
    family: str
    class_name: str
    header_path: Path
    cpp_path: Path
    exposed_count: int


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--manifest",
        default="firmware/v2/docs/effects/tunable_manifest.json",
        help="Path to tunable manifest",
    )
    p.add_argument(
        "--family",
        action="append",
        default=[],
        help="Only process this family (repeatable)",
    )
    p.add_argument(
        "--effect-id",
        action="append",
        default=[],
        help="Only process this effect id (repeatable)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Report planned changes without writing files",
    )
    return p.parse_args()


def load_entries(repo_root: Path, manifest_path: Path) -> List[EffectEntry]:
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    out: List[EffectEntry] = []
    for e in data.get("effects", []):
        source_files = e.get("source_files", [])
        header = next((s for s in source_files if s.endswith(".h")), None)
        cpp = next((s for s in source_files if s.endswith(".cpp")), None)
        if not header or not cpp:
            continue
        out.append(
            EffectEntry(
                effect_id=e["effect_id"],
                family=e["family"],
                class_name=e["class_name"],
                header_path=repo_root / header,
                cpp_path=repo_root / cpp,
                exposed_count=len(e.get("exposed_parameters", [])),
            )
        )
    return out


def find_matching_brace(text: str, open_idx: int) -> int:
    depth = 0
    i = open_idx
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def class_block_span(header_text: str, class_name: str) -> Optional[Tuple[int, int, int, int]]:
    pat = re.compile(
        rf"class\s+{re.escape(class_name)}\s*(?:final\s*)?:\s*public\s+plugins::IEffect\s*\{{",
        re.S,
    )
    m = pat.search(header_text)
    if not m:
        return None
    open_brace = header_text.find("{", m.start())
    close_brace = find_matching_brace(header_text, open_brace)
    if open_brace < 0 or close_brace < 0:
        return None
    semi = header_text.find(";", close_brace)
    if semi < 0:
        return None
    return (m.start(), open_brace, close_brace, semi)


def class_snake_name(class_name: str) -> str:
    # Keep "effect" in the key so canonical name matches k<Class>Effect* constants.
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", class_name).lower()
    return s.strip("_")


def ensure_cstring_include(text: str) -> str:
    if "#include <cstring>" in text:
        return text
    include_matches = list(re.finditer(r"^\s*#include\s+[^\n]+\n", text, re.M))
    if not include_matches:
        return text
    insert_at = include_matches[-1].end()
    return text[:insert_at] + "#include <cstring>\n" + text[insert_at:]


def remove_function_definition(text: str, class_name: str, method: str) -> Tuple[str, bool]:
    # Match the method token; we'll remove from the beginning of the signature line (or
    # a prior return-type-only line) through the matching closing brace.
    pat = re.compile(rf"\b{re.escape(class_name)}::{re.escape(method)}\s*\(")
    changed = False
    while True:
        m = pat.search(text)
        if not m:
            break
        line_start = text.rfind("\n", 0, m.start()) + 1

        # If the previous line is only a return type token (e.g. "bool"), include it.
        prev_line_start = text.rfind("\n", 0, max(line_start - 1, 0)) + 1
        prev_line = text[prev_line_start:line_start].strip()
        if prev_line and "::" not in prev_line and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_:<>\s\*&]*", prev_line):
            start = prev_line_start
        else:
            start = line_start

        brace_idx = text.find("{", m.start())
        if brace_idx < 0:
            break
        end_idx = find_matching_brace(text, brace_idx)
        if end_idx < 0:
            break

        cut_end = end_idx + 1
        while cut_end < len(text) and text[cut_end] in " \t\r\n":
            cut_end += 1

        text = text[:start] + "\n" + text[cut_end:]
        changed = True
    return text, changed


def remove_old_auto_blocks(text: str, class_name: str) -> str:
    # Remove new-style block markers if present.
    text = re.sub(
        rf"\n?// {AUTO_TAG}_BEGIN:{re.escape(class_name)}\n.*?// {AUTO_TAG}_END:{re.escape(class_name)}\n?",
        "\n",
        text,
        flags=re.S,
    )
    text = re.sub(
        rf"\n?// {AUTO_TAG}_METHODS_BEGIN:{re.escape(class_name)}\n.*?// {AUTO_TAG}_METHODS_END:{re.escape(class_name)}\n?",
        "\n",
        text,
        flags=re.S,
    )

    # Remove old reset blocks.
    text = re.sub(
        rf"\n\s*// {AUTO_TAG}_RESET_BEGIN:{re.escape(class_name)}\n.*?\n\s*// {AUTO_TAG}_RESET_END:{re.escape(class_name)}\n",
        "\n",
        text,
        flags=re.S,
    )
    text = re.sub(
        rf"\n\s*// {AUTO_TAG}:{re.escape(class_name)} reset\n(?:\s*g{re.escape(class_name)}[A-Za-z0-9_]+\s*=\s*[^;]+;\n)+",
        "\n",
        text,
    )

    # Remove old namespace block directly tied to old single marker.
    legacy_marker = f"// {AUTO_TAG}:{class_name}"
    while legacy_marker in text:
        marker_idx = text.find(legacy_marker)
        ns_idx = text.rfind("namespace {", 0, marker_idx)
        if ns_idx >= 0:
            marker_line_end = text.find("\n", marker_idx)
            if marker_line_end < 0:
                marker_line_end = len(text)
            remove_start = ns_idx
            while remove_start > 0 and text[remove_start - 1] == "\n":
                remove_start -= 1
            text = text[:remove_start] + text[marker_line_end + 1 :]
        else:
            marker_line_end = text.find("\n", marker_idx)
            if marker_line_end < 0:
                marker_line_end = len(text)
            text = text[:marker_idx] + text[marker_line_end + 1 :]

    return text


def insert_constants_block(text: str, class_name: str, key_prefix: str) -> str:
    include_matches = list(re.finditer(r"^\s*#include\s+[^\n]+\n", text, re.M))
    insert_at = include_matches[-1].end() if include_matches else 0

    block = f"""

// {AUTO_TAG}_BEGIN:{class_name}
namespace {{
constexpr float k{class_name}SpeedScale = 1.0f;
constexpr float k{class_name}OutputGain = 1.0f;
constexpr float k{class_name}CentreBias = 1.0f;

float g{class_name}SpeedScale = k{class_name}SpeedScale;
float g{class_name}OutputGain = k{class_name}OutputGain;
float g{class_name}CentreBias = k{class_name}CentreBias;

const lightwaveos::plugins::EffectParameter k{class_name}Parameters[] = {{
    {{"{key_prefix}_speed_scale", "Speed Scale", 0.25f, 2.0f, k{class_name}SpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false}},
    {{"{key_prefix}_output_gain", "Output Gain", 0.25f, 2.0f, k{class_name}OutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false}},
    {{"{key_prefix}_centre_bias", "Centre Bias", 0.50f, 1.50f, k{class_name}CentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false}},
}};
}} // namespace
// {AUTO_TAG}_END:{class_name}
"""

    return text[:insert_at] + block + text[insert_at:]


def inject_reset_block(text: str, class_name: str) -> str:
    init_re = re.compile(rf"\bbool\s+{re.escape(class_name)}::init\s*\(\s*plugins::EffectContext\s*&\s*ctx\s*\)\s*\{{")
    m = init_re.search(text)
    if not m:
        return text

    init_open = text.find("{", m.start())
    init_close = find_matching_brace(text, init_open)
    if init_open < 0 or init_close < 0:
        return text

    body = text[init_open + 1 : init_close]

    # Remove any pre-existing reset block remnants.
    body = re.sub(
        rf"\n\s*// {AUTO_TAG}_RESET_BEGIN:{re.escape(class_name)}\n.*?\n\s*// {AUTO_TAG}_RESET_END:{re.escape(class_name)}\n",
        "\n",
        body,
        flags=re.S,
    )
    body = re.sub(
        rf"\n\s*// {AUTO_TAG}:{re.escape(class_name)} reset\n(?:\s*g{re.escape(class_name)}[A-Za-z0-9_]+\s*=\s*[^;]+;\n)+",
        "\n",
        body,
    )

    reset_block = (
        f"\n    // {AUTO_TAG}_RESET_BEGIN:{class_name}\n"
        f"    g{class_name}SpeedScale = k{class_name}SpeedScale;\n"
        f"    g{class_name}OutputGain = k{class_name}OutputGain;\n"
        f"    g{class_name}CentreBias = k{class_name}CentreBias;\n"
        f"    // {AUTO_TAG}_RESET_END:{class_name}\n"
    )

    void_ctx = re.search(r"\(void\)\s*ctx\s*;", body)
    if void_ctx:
        insert_rel = void_ctx.end()
        body = body[:insert_rel] + reset_block + body[insert_rel:]
    else:
        body = reset_block + body

    return text[: init_open + 1] + body + text[init_close:]


def insert_methods_block(text: str, class_name: str, key_prefix: str) -> Tuple[str, bool]:
    methods_block = f"""
// {AUTO_TAG}_METHODS_BEGIN:{class_name}
uint8_t {class_name}::getParameterCount() const {{
    return static_cast<uint8_t>(sizeof(k{class_name}Parameters) / sizeof(k{class_name}Parameters[0]));
}}

const plugins::EffectParameter* {class_name}::getParameter(uint8_t index) const {{
    if (index >= getParameterCount()) return nullptr;
    return &k{class_name}Parameters[index];
}}

bool {class_name}::setParameter(const char* name, float value) {{
    if (!name) return false;
    if (strcmp(name, "{key_prefix}_speed_scale") == 0) {{
        g{class_name}SpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }}
    if (strcmp(name, "{key_prefix}_output_gain") == 0) {{
        g{class_name}OutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }}
    if (strcmp(name, "{key_prefix}_centre_bias") == 0) {{
        g{class_name}CentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }}
    return false;
}}

float {class_name}::getParameter(const char* name) const {{
    if (!name) return 0.0f;
    if (strcmp(name, "{key_prefix}_speed_scale") == 0) return g{class_name}SpeedScale;
    if (strcmp(name, "{key_prefix}_output_gain") == 0) return g{class_name}OutputGain;
    if (strcmp(name, "{key_prefix}_centre_bias") == 0) return g{class_name}CentreBias;
    return 0.0f;
}}
// {AUTO_TAG}_METHODS_END:{class_name}

"""

    cleanup_re = re.compile(rf"\bvoid\s+{re.escape(class_name)}::cleanup\s*\(")
    meta_re = re.compile(rf"\bconst\s+plugins::EffectMetadata&\s+{re.escape(class_name)}::getMetadata\s*\(")
    m_cleanup = cleanup_re.search(text)
    m_meta = meta_re.search(text)
    insert_idx = -1
    if m_cleanup:
        insert_idx = m_cleanup.start()
    elif m_meta:
        insert_idx = m_meta.start()
    else:
        ns_close = text.rfind("} // namespace")
        if ns_close > 0:
            insert_idx = ns_close
    if insert_idx < 0:
        return text, False

    return text[:insert_idx] + methods_block + text[insert_idx:], True


def patch_header(header_path: Path, class_name: str, dry_run: bool) -> Tuple[bool, str]:
    text = header_path.read_text(encoding="utf-8", errors="replace")
    span = class_block_span(text, class_name)
    if not span:
        return False, "class-block-missing"
    _, open_brace, close_brace, _ = span

    body = text[open_brace + 1 : close_brace]

    required = [
        "uint8_t getParameterCount() const override;",
        "const plugins::EffectParameter* getParameter(uint8_t index) const override;",
        "bool setParameter(const char* name, float value) override;",
        "float getParameter(const char* name) const override;",
    ]
    missing = [decl for decl in required if decl not in body]
    if not missing:
        return False, "already-declared"

    insert_at = body.find("private:")
    if insert_at < 0:
        insert_at = len(body)

    block = "\n"
    for decl in missing:
        block += f"    {decl}\n"

    new_body = body[:insert_at] + block + body[insert_at:]
    new_text = text[: open_brace + 1] + new_body + text[close_brace:]

    if not dry_run:
        header_path.write_text(new_text, encoding="utf-8")
    return True, "patched"


def patch_cpp(cpp_path: Path, class_name: str, dry_run: bool) -> Tuple[bool, str]:
    text = cpp_path.read_text(encoding="utf-8", errors="replace")
    original = text

    key_prefix = class_snake_name(class_name)

    text = ensure_cstring_include(text)
    text = remove_old_auto_blocks(text, class_name)

    # Remove any existing parameter method definitions for this class to avoid duplicates.
    for method in ["getParameterCount", "getParameter", "setParameter"]:
        text, _ = remove_function_definition(text, class_name, method)
    # Remove getParameter(name) after getParameter(uint8_t) sweep.
    text, _ = remove_function_definition(text, class_name, "getParameter")

    # Fix broken split signatures left by older injector: "bool\nClass::init" etc.
    text = re.sub(
        rf"\n\s*([A-Za-z_][A-Za-z0-9_:<>\s\*&]*)\s*\n\s*({re.escape(class_name)}::)",
        r"\n\1 \2",
        text,
    )

    text = insert_constants_block(text, class_name, key_prefix)
    text = inject_reset_block(text, class_name)
    text, inserted_methods = insert_methods_block(text, class_name, key_prefix)
    if not inserted_methods:
        return False, "method-insert-point-missing"

    # Collapse excessive blank lines introduced by repeated repairs.
    text = re.sub(r"\n{4,}", "\n\n\n", text)

    changed = text != original
    if changed and not dry_run:
        cpp_path.write_text(text, encoding="utf-8")

    return changed, ("patched" if changed else "already-patched")


def select_targets(entries: List[EffectEntry]) -> Dict[str, EffectEntry]:
    targets: Dict[str, EffectEntry] = {}
    for e in entries:
        if e.exposed_count == 0:
            targets[e.effect_id] = e
    for e in entries:
        if not e.cpp_path.exists():
            continue
        txt = e.cpp_path.read_text(encoding="utf-8", errors="replace")
        if AUTO_TAG in txt:
            targets[e.effect_id] = e
    return targets


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[4]
    manifest_path = (repo_root / args.manifest).resolve()

    entries = load_entries(repo_root, manifest_path)
    selected = select_targets(entries)

    families = set(args.family)
    effect_ids = set(args.effect_id)

    targets = []
    for effect_id in sorted(selected.keys()):
        e = selected[effect_id]
        if families and e.family not in families:
            continue
        if effect_ids and e.effect_id not in effect_ids:
            continue
        targets.append(e)

    print(f"[INFO] Target effects: {len(targets)}")

    patched_headers = 0
    patched_cpp = 0
    failures: List[Tuple[str, str]] = []

    for e in targets:
        if not e.header_path.exists() or not e.cpp_path.exists():
            failures.append((e.effect_id, "source-file-missing"))
            continue

        h_changed, h_msg = patch_header(e.header_path, e.class_name, args.dry_run)
        c_changed, c_msg = patch_cpp(e.cpp_path, e.class_name, args.dry_run)

        hard_fail_tokens = ("class-block-missing", "method-insert-point-missing")
        if h_msg in hard_fail_tokens or c_msg in hard_fail_tokens:
            failures.append((e.effect_id, f"header={h_msg},cpp={c_msg}"))

        if h_changed:
            patched_headers += 1
        if c_changed:
            patched_cpp += 1

        print(f"[EFFECT] {e.effect_id} ({e.class_name}) -> header:{h_msg} cpp:{c_msg}")

    print(f"[SUMMARY] patched headers={patched_headers}, patched cpp={patched_cpp}, failures={len(failures)}")
    for eid, msg in failures:
        print(f"[FAIL] {eid}: {msg}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
