#!/usr/bin/env python3
"""
gen_fallback_html.py — K1 Fallback Controller HTML build helper
================================================================
Reads  : docs/web/fallback_controller_source.html  (in repo root)
Writes : firmware-v3/src/network/webserver/FallbackControllerHtml.cpp

The generated .cpp embeds the gzip-compressed HTML as a PROGMEM uint8_t
array. The companion header FallbackControllerHtml.h declares the symbols.

Usage (manual, one-shot):
    cd <repo-root>
    python3 firmware-v3/scripts/gen_fallback_html.py

PlatformIO extra_scripts integration (optional):
    Add to platformio.ini under [env:...]:
        extra_scripts = scripts/gen_fallback_html.py
    When invoked by PlatformIO as a SConscript, the script detects the
    'env' variable injected by SCons and runs automatically before build.
    The source .html is resolved relative to PROJECT_DIR.

The script is idempotent: it only re-writes the .cpp when the source HTML
has changed (compared by gzip content, not mtime, to avoid false rebuilds).
"""

import gzip
import hashlib
import os
import sys
import textwrap

# ── Path resolution ─────────────────────────────────────────────────────────

def _find_root():
    """Walk up from this script's location to the repo root (contains docs/)."""
    here = os.path.dirname(os.path.realpath(__file__))  # firmware-v3/scripts/
    # Go up: firmware-v3/ -> repo root
    candidate = os.path.dirname(here)  # firmware-v3/
    candidate = os.path.dirname(candidate)  # repo root
    if os.path.isdir(os.path.join(candidate, 'docs')) and \
       os.path.isdir(os.path.join(candidate, 'firmware-v3')):
        return candidate
    # Fallback: caller may have set PROJECT_DIR (PlatformIO SCons env)
    return None


def _resolve_paths():
    # PlatformIO SCons mode
    if 'env' in dir():  # noqa: F821 — injected by SCons
        project_dir = env['PROJECT_DIR']  # noqa: F821
    else:
        project_dir = _find_root()

    if project_dir is None:
        raise RuntimeError(
            'Cannot determine repo root. Run from repo root or via PlatformIO.'
        )

    html_src = os.path.join(project_dir, 'docs', 'web', 'fallback_controller_source.html')
    cpp_out  = os.path.join(project_dir, 'firmware-v3', 'src', 'network',
                            'webserver', 'FallbackControllerHtml.cpp')
    return html_src, cpp_out


# ── Core generation ─────────────────────────────────────────────────────────

def _bytes_to_c_array(data: bytes, name: str) -> str:
    """Convert raw bytes to a C uint8_t array literal with 16-byte line width."""
    lines = []
    chunk = 16
    for i in range(0, len(data), chunk):
        row = data[i:i+chunk]
        lines.append('    ' + ', '.join('0x{:02x}'.format(b) for b in row))
    body = ',\n'.join(lines)
    return (
        'const uint8_t {name}[] PROGMEM = {{\n'
        '{body}\n'
        '}};\n'
        'const size_t {name}_LEN = {length}u;\n'
    ).format(name=name, body=body, length=len(data))


_CPP_HEADER = (
    "/**\n"
    " * @file FallbackControllerHtml.cpp\n"
    " * @brief Pre-compressed gzip of the K1 fallback controller HTML page.\n"
    " *\n"
    " * AUTO-GENERATED -- do not edit by hand.\n"
    " * Regenerate via: python3 firmware-v3/scripts/gen_fallback_html.py\n"
    " * Source:         docs/web/fallback_controller_source.html\n"
    " *\n"
    " * Source SHA-256: {sha256}\n"
    " * Raw size: {raw_bytes} bytes\n"
    " * Gzip size: {gz_bytes} bytes\n"
    " */\n"
    "// Arduino.h must be included before FallbackControllerHtml.h so that\n"
    "// PROGMEM is defined when the extern declaration is parsed.\n"
    "#ifndef NATIVE_BUILD\n"
    "#include <Arduino.h>\n"
    "#endif\n"
    "#include \"FallbackControllerHtml.h\"\n"
    "\n"
)


def generate(html_src: str, cpp_out: str) -> dict:
    with open(html_src, 'rb') as f:
        html_bytes = f.read()

    gz_bytes = gzip.compress(html_bytes, compresslevel=9)
    sha256 = hashlib.sha256(html_bytes).hexdigest()

    # Check if re-generation is needed
    marker = '// Source SHA-256: ' + sha256
    if os.path.isfile(cpp_out):
        with open(cpp_out, 'r', encoding='utf-8') as f:
            existing = f.read()
        if marker in existing:
            return {
                'regenerated': False,
                'raw_bytes': len(html_bytes),
                'gz_bytes': len(gz_bytes),
                'sha256': sha256,
                'cpp_out': cpp_out,
            }

    header_comment = _CPP_HEADER.format(
        sha256=sha256,
        raw_bytes=len(html_bytes),
        gz_bytes=len(gz_bytes),
    )
    array_body = _bytes_to_c_array(gz_bytes, 'FALLBACK_HTML_GZ')

    cpp_content = header_comment + array_body

    os.makedirs(os.path.dirname(cpp_out), exist_ok=True)
    with open(cpp_out, 'w', encoding='utf-8') as f:
        f.write(cpp_content)

    return {
        'regenerated': True,
        'raw_bytes': len(html_bytes),
        'gz_bytes': len(gz_bytes),
        'sha256': sha256,
        'cpp_out': cpp_out,
    }


# ── PlatformIO SCons entry point ─────────────────────────────────────────────

def run_as_extra_script():
    """Called by PlatformIO when this file is listed in extra_scripts."""
    try:
        html_src, cpp_out = _resolve_paths()
        result = generate(html_src, cpp_out)
        if result['regenerated']:
            print('[gen_fallback_html] Generated {} ({} raw, {} gz)'.format(
                os.path.basename(result['cpp_out']),
                result['raw_bytes'],
                result['gz_bytes'],
            ))
        else:
            print('[gen_fallback_html] Up to date, skipping regeneration.')
    except Exception as e:
        print('[gen_fallback_html] ERROR: {}'.format(e))
        raise


# Detect PlatformIO SCons context (they inject 'env' into globals)
if 'env' in dir():  # noqa: F821
    run_as_extra_script()


# ── CLI entry point ──────────────────────────────────────────────────────────

def main():
    try:
        html_src, cpp_out = _resolve_paths()
    except RuntimeError as e:
        print('Error: {}'.format(e), file=sys.stderr)
        sys.exit(1)

    if not os.path.isfile(html_src):
        print('Error: source not found: {}'.format(html_src), file=sys.stderr)
        sys.exit(1)

    result = generate(html_src, cpp_out)

    if result['regenerated']:
        print('Generated:  {}'.format(result['cpp_out']))
    else:
        print('Up to date: {}'.format(result['cpp_out']))
    print('  Source SHA-256 : {}'.format(result['sha256']))
    print('  Raw size       : {} bytes'.format(result['raw_bytes']))
    print('  Gzip size      : {} bytes'.format(result['gz_bytes']))
    ratio = 100.0 * result['gz_bytes'] / max(result['raw_bytes'], 1)
    print('  Compression    : {:.1f}%'.format(ratio))


if __name__ == '__main__':
    main()
