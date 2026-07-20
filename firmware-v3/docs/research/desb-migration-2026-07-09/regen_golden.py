#!/usr/bin/env python3
"""Re-bless named goldens after the de-SB rename (name-only drift).

Usage: regen_golden.py <worktree_root> <golden_basename> [<golden_basename> ...]
  e.g. regen_golden.py /private/tmp/k1_edgemixer_on_im73d serial_struct smart_director

For each name: import capture() from scripts/regression-harness/golden/oracle_<name>.py,
write tests/golden/<name>.golden.jsonl, then recompute that line in MANIFEST.sha256.
Prints old->new sha and a unified-ish diff summary so drift can be eyeballed as
NAME-ONLY (sb_->k1_ / SB_->K1_) before trusting the re-bless.
"""
import os, sys, hashlib, importlib.util, re, difflib

def load_capture(oracle_path):
    spec = importlib.util.spec_from_file_location("oracle_mod", oracle_path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.capture

def main():
    root = sys.argv[1]
    names = sys.argv[2:]
    gdir = os.path.join(root, "tests", "golden")
    odir = os.path.join(root, "scripts", "regression-harness", "golden")
    man = os.path.join(gdir, "MANIFEST.sha256")
    manlines = open(man).read().splitlines() if os.path.exists(man) else []
    for name in names:
        gpath = os.path.join(gdir, f"{name}.golden.jsonl")
        opath = os.path.join(odir, f"oracle_{name}.py")
        if not os.path.exists(opath):
            print(f"  ✗ no oracle for {name} ({opath})"); continue
        old = open(gpath).read() if os.path.exists(gpath) else ""
        # run oracle in its own dir so relative FIRMWARE paths resolve
        cwd = os.getcwd(); os.chdir(odir)
        try:
            cap = load_capture(opath)
            new = cap()
        finally:
            os.chdir(cwd)
        if not new.endswith("\n"):
            new += "\n"
        # drift analysis: strip sb/SB renames from both, compare — should be identical
        norm = lambda s: re.sub(r"\b[sS][bB]_?", "", s)
        name_only = norm(old) == norm(new)
        oldsha = hashlib.sha256(old.encode()).hexdigest()
        newsha = hashlib.sha256(new.encode()).hexdigest()
        # sample the first differing lines
        diff = [l for l in difflib.unified_diff(old.splitlines(), new.splitlines(),
                                                lineterm="", n=0)][:8]
        open(gpath, "w").write(new)
        # update MANIFEST line
        base = f"{name}.golden.jsonl"
        updated = False
        for i, ln in enumerate(manlines):
            if ln.strip().endswith(base):
                manlines[i] = f"{newsha}  {base}"; updated = True; break
        if not updated:
            manlines.append(f"{newsha}  {base}")
        tag = "NAME-ONLY drift ✓" if name_only else "⚠ VALUE drift — INSPECT"
        print(f"  {name}: {tag}  sha {oldsha[:8]}->{newsha[:8]}")
        for d in diff:
            print(f"      {d[:120]}")
    open(man, "w").write("\n".join(manlines) + "\n")
    print("MANIFEST.sha256 updated.")

if __name__ == "__main__":
    main()
