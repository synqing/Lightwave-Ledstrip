#!/usr/bin/env python3
"""
Semantic Anchor Extractor for Context Pack

Extracts key symbols, interfaces, and effect IDs from the codebase to provide
semantic context anchors in prompts without including full file contents.

Usage:
    from anchor_extractor import extract_anchors
    anchors = extract_anchors(repo_root, diff_files)
"""

import re
from pathlib import Path
from typing import Dict, List, Set, Tuple


def extract_cpp_symbols(file_path: Path) -> Dict[str, List[str]]:
    """
    Extract C++ symbols (class, struct, enum, #define) from a header file.
    
    Returns dict with keys: 'classes', 'structs', 'enums', 'defines'
    """
    if not file_path.exists() or not file_path.suffix in ['.h', '.hpp', '.cpp']:
        return {'classes': [], 'structs': [], 'enums': [], 'defines': []}
    
    symbols = {'classes': [], 'structs': [], 'enums': [], 'defines': []}
    
    try:
        content = file_path.read_text(encoding='utf-8', errors='ignore')
    except IOError:
        return symbols
    
    # Extract class definitions
    class_pattern = r'class\s+(\w+)(?:\s*:\s*[^{]+)?\s*{'
    for match in re.finditer(class_pattern, content):
        symbols['classes'].append(match.group(1))
    
    # Extract struct definitions
    struct_pattern = r'struct\s+(\w+)(?:\s*:\s*[^{]+)?\s*{'
    for match in re.finditer(struct_pattern, content):
        symbols['structs'].append(match.group(1))
    
    # Extract enum definitions (enum class and plain enum)
    enum_class_pattern = r'enum\s+class\s+(\w+)'
    for match in re.finditer(enum_class_pattern, content):
        symbols['enums'].append(match.group(1))
    
    enum_pattern = r'enum\s+(\w+)(?:\s*\{)'
    for match in re.finditer(enum_pattern, content):
        if match.group(1) not in symbols['enums']:
            symbols['enums'].append(match.group(1))
    
    # Extract #define constants (simple ones like #define NAME value)
    define_pattern = r'#define\s+([A-Z_][A-Z0-9_]+)'
    for match in re.finditer(define_pattern, content):
        symbols['defines'].append(match.group(1))
    
    return symbols


def extract_effect_ids(repo_root: Path) -> List[Tuple[int, str]]:
    """
    Extract effect IDs and names from PatternRegistry.cpp.
    
    Returns list of (effect_id, effect_name) tuples.
    """
    pattern_registry = repo_root / "firmware" / "v2" / "src" / "effects" / "PatternRegistry.cpp"
    
    if not pattern_registry.exists():
        return []
    
    effect_ids = []
    
    try:
        content = pattern_registry.read_text(encoding='utf-8', errors='ignore')
    except IOError:
        return effect_ids
    
    # Pattern: {PM_STR("EffectName"), PatternFamily::..., ...}
    # Pattern metadata array entries with effect names
    pattern = r'\{PM_STR\("([^"]+)"\),'
    matches = re.finditer(pattern, content)
    
    for idx, match in enumerate(matches):
        effect_name = match.group(1)
        effect_ids.append((idx, effect_name))
    
    return effect_ids


def extract_interface_methods(file_path: Path, interface_name: str) -> List[str]:
    """
    Extract method signatures from an interface/class definition.
    
    Returns list of method signatures as strings.
    """
    if not file_path.exists():
        return []
    
    methods = []
    
    try:
        content = file_path.read_text(encoding='utf-8', errors='ignore')
    except IOError:
        return methods
    
    # Find the class/interface block
    class_pattern = r'class\s+' + re.escape(interface_name) + r'[^{]*\{'
    class_match = re.search(class_pattern, content, re.DOTALL)
    
    if not class_match:
        return methods
    
    # Extract virtual methods (interface methods)
    # Pattern: virtual return_type method_name(params) = 0; or virtual return_type method_name(params);
    method_pattern = r'virtual\s+[^{;]+\([^)]*\)(?:\s*=\s*0)?;'
    
    start_pos = class_match.end()
    # Find matching closing brace for class
    brace_count = 1
    i = start_pos
    while i < len(content) and brace_count > 0:
        if content[i] == '{':
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
        i += 1
    
    class_content = content[start_pos:i-1]
    
    for match in re.finditer(method_pattern, class_content):
        method_sig = match.group(0).strip()
        # Simplify: remove 'virtual', keep method name and params
        method_sig = re.sub(r'virtual\s+', '', method_sig)
        methods.append(method_sig)
    
    return methods


def extract_effectcontext_fields(file_path: Path) -> List[str]:
    """
    Extract field names from EffectContext struct.
    
    Returns list of field names with types.
    """
    if not file_path.exists():
        return []
    
    fields = []
    
    try:
        content = file_path.read_text(encoding='utf-8', errors='ignore')
    except IOError:
        return fields
    
    # Find EffectContext struct
    struct_pattern = r'struct\s+EffectContext[^{]*\{'
    struct_match = re.search(struct_pattern, content, re.DOTALL)
    
    if not struct_match:
        return fields
    
    # Extract field declarations (simple pattern: type name; or type name = value;)
    # Pattern: Type name; or Type name = value;
    field_pattern = r'([A-Za-z_][A-Za-z0-9_<>*&:]*\s+)?([A-Za-z_][A-Za-z0-9_]+)(?:\s*=\s*[^;]+)?;'
    
    start_pos = struct_match.end()
    # Find matching closing brace
    brace_count = 1
    i = start_pos
    while i < len(content) and brace_count > 0:
        if content[i] == '{':
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
        i += 1
    
    struct_content = content[start_pos:i-1]
    
    for match in re.finditer(field_pattern, struct_content):
        # Skip comments, access specifiers
        line = match.group(0)
        if line.strip().startswith('//') or line.strip() in ['public:', 'private:', 'protected:']:
            continue
        
        field_name = match.group(2)
        if field_name and field_name not in ['public', 'private', 'protected']:
            fields.append(line.strip())
    
    return fields[:20]  # Limit to first 20 fields to avoid bloat


def get_changed_modules(diff_files: List[str]) -> Set[str]:
    """
    Extract changed module paths from diff file list.
    
    Returns set of module directory paths.
    """
    modules = set()
    
    for file_path in diff_files:
        path = Path(file_path)
        # Get parent directory as module
        if path.parts:
            # For firmware/v2/src/effects/CoreEffects.cpp -> firmware/v2/src/effects
            if len(path.parts) >= 2:
                module = str(Path(*path.parts[:-1]))
                modules.add(module)
    
    return modules


def extract_unchanged_modules(repo_root: Path, changed_modules: Set[str]) -> List[str]:
    """
    Identify unchanged module directories (not in changed_modules).
    
    For Context Pack, we report top-level directories that have no changes.
    """
    # Key directories to check
    key_dirs = [
        'firmware/v2/src/audio',
        'firmware/v2/src/palettes',
        'lightwave-dashboard',
        'docs',
    ]
    
    unchanged = []
    
    for dir_path in key_dirs:
        full_path = repo_root / dir_path
        if full_path.exists() and dir_path not in changed_modules:
            # Check if any subdirectory matches changed_modules
            is_changed = any(
                str(Path(dir_path)) in changed_mod or changed_mod.startswith(dir_path)
                for changed_mod in changed_modules
            )
            if not is_changed:
                unchanged.append(dir_path)
    
    return unchanged


def extract_anchors(repo_root: Path, diff_files: List[str]) -> Dict[str, any]:
    """
    Extract semantic anchors from the codebase.
    
    Args:
        repo_root: Repository root directory
        diff_files: List of changed file paths (from git diff)
    
    Returns:
        Dict with keys: 'effect_ids', 'interfaces', 'changed_modules', 'unchanged_modules'
    """
    repo_root = Path(repo_root)
    
    anchors = {
        'effect_ids': [],
        'interfaces': {},
        'changed_modules': [],
        'unchanged_modules': [],
    }
    
    # Extract effect IDs from PatternRegistry
    effect_ids = extract_effect_ids(repo_root)
    anchors['effect_ids'] = effect_ids
    
    # Extract interface signatures
    ieffect_path = repo_root / "firmware" / "v2" / "src" / "plugins" / "api" / "IEffect.h"
    effectcontext_path = repo_root / "firmware" / "v2" / "src" / "plugins" / "api" / "EffectContext.h"
    
    if ieffect_path.exists():
        methods = extract_interface_methods(ieffect_path, "IEffect")
        anchors['interfaces']['IEffect'] = methods
    
    if effectcontext_path.exists():
        fields = extract_effectcontext_fields(effectcontext_path)
        anchors['interfaces']['EffectContext'] = fields[:10]  # Limit to key fields
    
    # Extract changed modules
    changed_modules = get_changed_modules(diff_files)
    anchors['changed_modules'] = sorted(list(changed_modules))
    
    # Extract unchanged modules
    unchanged_modules = extract_unchanged_modules(repo_root, changed_modules)
    anchors['unchanged_modules'] = sorted(unchanged_modules)
    
    return anchors


def format_anchors_markdown(anchors: Dict[str, any]) -> str:
    """
    Format extracted anchors as Markdown for inclusion in packet.md.
    
    Returns Markdown string.
    """
    lines = [
        "## Semantic Anchors",
        "",
    ]
    
    # Effect Registry
    if anchors.get('effect_ids'):
        lines.extend([
            "### Effect Registry (IDs stable)",
            "",
        ])
        for effect_id, effect_name in anchors['effect_ids'][:30]:  # Limit to first 30
            lines.append(f"- {effect_id}: {effect_name}")
        
        if len(anchors['effect_ids']) > 30:
            lines.append(f"- ... and {len(anchors['effect_ids']) - 30} more")
        
        lines.append("")
    
    # Key Interfaces
    if anchors.get('interfaces'):
        lines.extend([
            "### Key Interfaces",
            "",
        ])
        
        if 'IEffect' in anchors['interfaces']:
            methods = anchors['interfaces']['IEffect']
            methods_str = ", ".join([m.split('(')[0].split()[-1] for m in methods[:5]])
            lines.append(f"- **IEffect**: {methods_str}")
        
        if 'EffectContext' in anchors['interfaces']:
            fields = anchors['interfaces']['EffectContext']
            fields_str = ", ".join([f.split()[1].rstrip(';') if len(f.split()) >= 2 else f for f in fields[:8]])
            lines.append(f"- **EffectContext**: {fields_str}")
        
        lines.append("")
    
    # Changed Modules
    if anchors.get('changed_modules'):
        lines.extend([
            "### Changed Modules",
            "",
        ])
        for module in anchors['changed_modules']:
            lines.append(f"- `{module}`")
        lines.append("")
    
    # Unchanged Modules
    if anchors.get('unchanged_modules'):
        lines.extend([
            "### Unchanged (omitted from diff)",
            "",
        ])
        for module in anchors['unchanged_modules']:
            lines.append(f"- `{module}` (no changes)")
        lines.append("")
    
    return "\n".join(lines)


if __name__ == "__main__":
    # CLI test mode
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python anchor_extractor.py <repo_root> [diff_file1] [diff_file2] ...")
        sys.exit(1)
    
    repo_root = Path(sys.argv[1])
    diff_files = sys.argv[2:] if len(sys.argv) > 2 else []
    
    anchors = extract_anchors(repo_root, diff_files)
    markdown = format_anchors_markdown(anchors)
    
    print(markdown)
