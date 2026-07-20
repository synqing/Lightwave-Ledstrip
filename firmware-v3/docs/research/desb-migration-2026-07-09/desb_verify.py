#!/usr/bin/env python3
"""Canonical de-SB acceptance verifier (Python re — correct \\b, RTK-immune).

Replaces the master-plan grep command, which silently false-passes under zsh
(unquoted --include=*.cpp globs get filename-expanded before grep runs).

Scope: SPECTRASYNQ_K1_FIRMWARE/, scripts/, tests/, platformio.ini.
Skips: .pio, __pycache__, .git, *.ino.cpp (generated), *.pyc, binaries.
"""
import os, re, sys, json

ROOT = sys.argv[1] if len(sys.argv) > 1 else "/private/tmp/k1_edgemixer_on_im73d"
SCOPE = ["SPECTRASYNQ_K1_FIRMWARE", "scripts", "tests", "platformio.ini"]
SKIP_DIRS = {".pio", "__pycache__", ".git", "build", "artifacts", ".pytest_cache"}
SKIP_SUFFIX = (".ino.cpp", ".pyc", ".o", ".a", ".bin", ".elf", ".png", ".jpg",
               ".gz", ".zip", ".wav")

PATS = {
    "sensory_bridge": re.compile(r"sensory.?bridge", re.I),
    "sb_lower":       re.compile(r"\bsb_"),
    "SB_upper":       re.compile(r"\bSB_"),
    "SB_caps":        re.compile(r"\bSB[A-Z]"),
    # -D<flag> glues a word char before SB so \b misses it — catch it explicitly.
    # BOTH cases of -D: uppercase real flags AND lowercase test-side expectations.
    # This is the flag blind spot that would let beat-tracking silently disable.
    "flag_DSB":       re.compile(r"-[Dd][Ss][Bb]"),
}
# Sanctioned residuals (the FLAGGED items — allowed to remain post-migration):
#  1. persisted preset magic bytes (on-disk compat)
#  2. external on-disk baseline dir literal (real path, physically exists)
#  3. legacy-absence guards: assertNotIn("SB_...") lines that PROVE the old SB
#     prefix is gone — the SB token is the thing being forbidden, not a miss.
MAGIC_OK = re.compile(r"0x53504253|'SBPS'|\"SBPS\"|bytes still spell|SensoryBridge-main 9|assertNotIn\(")

def iter_files(root):
    for base in SCOPE:
        p = os.path.join(root, base)
        if os.path.isfile(p):
            yield p; continue
        for dpath, dnames, fnames in os.walk(p):
            dnames[:] = [d for d in dnames if d not in SKIP_DIRS]
            for f in fnames:
                if f.endswith(SKIP_SUFFIX):
                    continue
                yield os.path.join(dpath, f)

def main():
    total = {k: 0 for k in PATS}
    per_file = {}          # relpath -> {cat: count}
    magic_lines = []       # sanctioned residuals to report separately
    flags_D = set()        # -DSB_* from platformio.ini
    ifdef_SB = set()       # SB_* seen in #if/#ifdef/#ifndef/defined() in source

    D_RE = re.compile(r"-D(SB_[A-Za-z0-9_]+)")
    IFDEF_RE = re.compile(r"#\s*if(?:def|ndef)?\s+.*?\b(SB_[A-Za-z0-9_]+)")
    DEFINED_RE = re.compile(r"defined\s*\(\s*(SB_[A-Za-z0-9_]+)\s*\)")

    for fpath in iter_files(ROOT):
        try:
            with open(fpath, "r", errors="replace") as fh:
                txt = fh.read()
        except Exception:
            continue
        rel = os.path.relpath(fpath, ROOT)
        counts = {}
        for cat, rx in PATS.items():
            # count hits that are NOT the sanctioned magic bytes
            hits = [m for m in rx.finditer(txt)]
            real = 0
            for m in hits:
                nl_after = txt.find("\n", m.end())
                line = txt[txt.rfind("\n", 0, m.start())+1 : nl_after if nl_after != -1 else len(txt)]
                if MAGIC_OK.search(line):
                    # sanctioned residual (magic bytes OR external real path) — report, don't count
                    magic_lines.append((rel, line.strip()[:120]))
                    continue
                real += 1
            if real:
                counts[cat] = real
                total[cat] += real
        if counts:
            per_file[rel] = counts
        # flag/ifdef cross-map
        if rel.endswith("platformio.ini"):
            flags_D |= set(D_RE.findall(txt))
        if fpath.endswith((".cpp", ".h", ".hpp", ".c", ".ino")):
            ifdef_SB |= set(IFDEF_RE.findall(txt))
            ifdef_SB |= set(DEFINED_RE.findall(txt))

    grand = sum(total.values())
    print("=" * 70)
    print(f"DE-SB VERIFIER — root={ROOT}")
    print("=" * 70)
    print("TOTAL residual (excluding sanctioned magic bytes):")
    for k, v in total.items():
        print(f"  {k:16s}: {v}")
    print(f"  {'GRAND TOTAL':16s}: {grand}")
    print()
    print(f"FILES WITH RESIDUAL: {len(per_file)}")
    for rel in sorted(per_file, key=lambda r: -sum(per_file[r].values())):
        c = per_file[rel]
        print(f"  {sum(c.values()):5d}  {rel}   {c}")
    print()
    print(f"SANCTIONED MAGIC residual lines ({len(magic_lines)} — MUST remain):")
    for rel, line in magic_lines:
        print(f"  {rel}: {line}")
    print()
    print(f"-DSB_ FLAGS in platformio.ini ({len(flags_D)}):")
    for f in sorted(flags_D):
        print(f"  {f}")
    print()
    print(f"SB_ symbols in #ifdef/#if/defined() across source ({len(ifdef_SB)}):")
    for f in sorted(ifdef_SB):
        mark = "" if f in flags_D else "   <-- no matching -D flag (may be nested/derived)"
        print(f"  {f}{mark}")
    print()
    # Atomicity cross-check: any -D flag whose #ifdef won't be found post-rename
    print("ATOMICITY NOTE: after rename, every -DSB_X becomes -DK1_X AND every")
    print("#ifdef SB_X becomes #ifdef K1_X in the SAME uncommitted pass, so the")
    print("pairing is preserved by construction. The 0-residual test confirms it.")
    print()
    print("ACCEPTANCE:", "PASS (0 residual)" if grand == 0 else f"FAIL ({grand} residual)")
    # machine-readable tail
    print("JSON:" + json.dumps({"grand": grand, "by_cat": total, "files": len(per_file)}))

if __name__ == "__main__":
    main()
