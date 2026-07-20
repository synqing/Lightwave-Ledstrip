#!/usr/bin/env python3
"""Fork-wide de-SB -> K1 CONTENT transform (run AFTER git mv of sb_* files).

Ordered, word-boundary-anchored, with two protected classes preserved verbatim:
  1. persisted preset magic  'SBPS' / 0x53504253  (on-disk byte compat)
  2. external on-disk baseline dir literal  'SensoryBridge-main 9'  (real path)

Deterministic + reversible (git checkout . restores). Reports per-file counts.
"""
import os, re, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "/private/tmp/k1_edgemixer_on_im73d"
SCOPE = ["SPECTRASYNQ_K1_FIRMWARE", "scripts", "tests", "platformio.ini"]
SKIP_DIRS = {".pio","__pycache__",".git","build","artifacts",".pytest_cache"}
SKIP_SUF = (".ino.cpp",".pyc",".o",".a",".bin",".elf",".png",".jpg",".gz",".zip",".wav")

# sentinels (chars that never occur in source)
SENT_MAGIC = "\x00MAGICSBPS\x00"
SENT_PATH  = "\x00EXTPATH\x00"

# branding: anything that reads 'sensory bridge' (any sep/case, incl. ALL-CAPS
# serial headers like "SENSORY BRIDGE | VER:") -> K1
RX_BRAND = re.compile(r"sensory[ _\-]?bridge", re.I)

# legacy-absence guards: a NEGATIVE assertion names the OLD SB token as the thing
# being forbidden — that token must STAY SB (renaming it inverts the guard against
# a live K1 symbol → red test, while 0-residual still passes because no SB survives).
# Mirrors desb_verify.py's MAGIC_OK sanction of assertNotIn(...) lines. Protects the
# QUOTED NEEDLE only (first arg); the haystack expression still renames normally.
RX_GUARD = re.compile(r"""(assert(?:NotIn|NotRegex)\(\s*['"])(SB[A-Za-z0-9_]*)(['"])""")

# ordered identifier subs (double-k1 collapse BEFORE generic)
# NOTE: the -D lookbehind rules MUST come first — a compiler define flag
# `-DSB_X` glues `-D` (word chars) before `SB`, so `\bSB_` can NOT match it.
# Without these, platformio `-DSB_TEMPO_CONF_V2` stays SB while source
# `#ifdef SB_TEMPO_CONF_V2` renames to K1 -> flag silently undefined ->
# beat-tracking compiled out with a GREEN build. This is the whole reason
# the 0-residual test exists.
IDENT_SUBS = [
    # compiler-flag forms: -D glues a word char before SB/sb so \b can't match.
    # Handle BOTH cases of -D: uppercase -DSB_ (real flags) AND lowercase -dsb_
    # (test-side lowercased flag expectations that must mirror the renamed flag).
    (re.compile(r"(?<=-[Dd])SB_K1_"), "K1_"),   # -DSB_K1_HARDWARE -> -DK1_HARDWARE
    (re.compile(r"(?<=-[Dd])SB_"),    "K1_"),   # -DSB_TEMPO_CONF_V2 -> -DK1_TEMPO_CONF_V2
    (re.compile(r"(?<=-[Dd])SB([A-Z])"), r"K1\1"),
    (re.compile(r"(?<=-[Dd])sb_k1_"), "k1_"),
    (re.compile(r"(?<=-[Dd])sb_"),    "k1_"),   # -dsb_led_task_core -> -dk1_led_task_core
    (re.compile(r"\bsb_k1_"), "k1_"),
    (re.compile(r"\bSB_K1_"), "K1_"),
    (re.compile(r"\bsb_"),    "k1_"),
    (re.compile(r"\bSB_"),    "K1_"),
    (re.compile(r"\bSB([A-Z])"), r"K1\1"),
]

def iter_files(root):
    for b in SCOPE:
        p = os.path.join(root, b)
        if os.path.isfile(p):
            yield p; continue
        for dp, dn, fn in os.walk(p):
            dn[:] = [d for d in dn if d not in SKIP_DIRS]
            for f in fn:
                if f.endswith(SKIP_SUF):
                    continue
                yield os.path.join(dp, f)

def transform(txt):
    # 1. protect external real path literal (functional dependency)
    txt = txt.replace("SensoryBridge-main 9", SENT_PATH)
    # 2. protect persisted magic byte comment 'SBPS'
    txt = txt.replace("'SBPS'", SENT_MAGIC)
    # 3. protect legacy-absence guard needles: assertNotIn("SB_…") keeps its SB
    #    token. Use opaque numbered sentinels (no SB chars) so IDENT_SUBS can't
    #    reach the token; restore verbatim afterwards.
    guards = []
    def _stash(m):
        guards.append(m.group(2))
        return f"{m.group(1)}\x00GUARD{len(guards)-1}\x00{m.group(3)}"
    txt = RX_GUARD.sub(_stash, txt)
    # 4. branding sweep (remaining sensory-bridge prose/serial text) -> K1
    txt = RX_BRAND.sub("K1", txt)
    # 5. ordered identifier renames
    for rx, repl in IDENT_SUBS:
        txt = rx.sub(repl, txt)
    # 6. restore protected literals verbatim
    for i, tok in enumerate(guards):
        txt = txt.replace(f"\x00GUARD{i}\x00", tok)
    txt = txt.replace(SENT_MAGIC, "'SBPS'")
    txt = txt.replace(SENT_PATH, "SensoryBridge-main 9")
    return txt

def main():
    changed = []
    total_files = 0
    for fp in iter_files(ROOT):
        total_files += 1
        try:
            with open(fp, "r", errors="surrogateescape") as fh:
                orig = fh.read()
        except Exception as e:
            print(f"  SKIP (read err): {fp} {e}"); continue
        new = transform(orig)
        if new != orig:
            with open(fp, "w", errors="surrogateescape") as fh:
                fh.write(new)
            # count changed lines for report
            delta = sum(1 for a, b in zip(orig.splitlines(), new.splitlines()) if a != b)
            changed.append((os.path.relpath(fp, ROOT), delta))
    print(f"scanned: {total_files} files")
    print(f"changed: {len(changed)} files")
    for rel, d in sorted(changed, key=lambda x: -x[1]):
        print(f"  {d:5d}  {rel}")

if __name__ == "__main__":
    main()
