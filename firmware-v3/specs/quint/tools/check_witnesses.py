#!/usr/bin/env python3
"""
Witness Checker: Verify lock witnesses exist in traces.

Checks for:
- LockAchieved: At least one trace achieves lock
- LockWithin5Seconds: Lock achieved within 250 ticks
- AccurateLock: Lock with <10 BPM error
- StableLock: Lock maintained for 100+ ticks
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple

DT_MS = 20
BPM_MIN = 60
BPM_BUCKET_STEP = 2

def parse_bigint(value) -> int:
    if isinstance(value, dict) and "#bigint" in value:
        return int(value["#bigint"])
    return int(value)

def check_witnesses(traces_dir: Path) -> Dict[str, Tuple[int, int]]:
    """Check witness properties across all traces.
    
    Returns dict of witness_name -> (count_found, total_traces)
    """
    witnesses = {
        "LockAchieved": 0,
        "LockWithin5Seconds": 0,
        "AccurateLock": 0,
        "StableLock": 0
    }
    
    trace_files = []
    # Check traces subdirectory first
    traces_subdir = traces_dir / "traces"
    if traces_subdir.exists():
        for f in traces_subdir.iterdir():
            if f.is_file() and not f.name.startswith(".") and (f.suffix == ".json" or f.name.isdigit()):
                trace_files.append(f)
    
    # Fall back to root directory if no traces subdir
    if not trace_files:
        for f in traces_dir.iterdir():
            if f.is_file() and not f.name.startswith(".") and (f.suffix == ".json" or f.name.isdigit()):
                # Skip non-trace JSON files
                if f.name in ["summary.json", "top_k.json", "run_manifest.json"]:
                    continue
                trace_files.append(f)
    
    trace_files = list(set(trace_files))
    total = len(trace_files)
    
    for tf in trace_files:
        try:
            with open(tf) as f:
                trace = json.load(f)
            
            states = trace.get("states", [])
            if not states:
                continue
            
            # Check each state for witness properties
            lock_achieved = False
            lock_within_5s = False
            accurate_lock = False
            stable_lock = False
            
            for state_entry in states:
                state = state_entry.get("state", {})
                locked = state.get("locked", False)
                tick = parse_bigint(state.get("tick", 0))
                
                if locked:
                    lock_achieved = True
                    
                    if tick <= 250:
                        lock_within_5s = True
                    
                    # Check accuracy
                    env = state.get("env", {})
                    true_bpm = parse_bigint(env.get("true_bpm", 120))
                    true_bpm_bucket = (true_bpm - BPM_MIN) // BPM_BUCKET_STEP
                    bpm_hat = parse_bigint(state.get("bpm_hat", 0))
                    error = abs(bpm_hat - true_bpm_bucket) * BPM_BUCKET_STEP
                    
                    if error <= 10:
                        accurate_lock = True
                
                # Check stable lock
                metrics = state.get("metrics", {})
                locked_ticks = parse_bigint(metrics.get("locked_ticks", 0))
                if locked_ticks >= 100:
                    stable_lock = True
            
            if lock_achieved:
                witnesses["LockAchieved"] += 1
            if lock_within_5s:
                witnesses["LockWithin5Seconds"] += 1
            if accurate_lock:
                witnesses["AccurateLock"] += 1
            if stable_lock:
                witnesses["StableLock"] += 1
                
        except Exception as e:
            print(f"Warning: Failed to parse {tf}: {e}", file=sys.stderr)
    
    return {k: (v, total) for k, v in witnesses.items()}

def main():
    if len(sys.argv) < 2:
        print("Usage: check_witnesses.py <traces_dir>")
        sys.exit(1)
    
    traces_dir = Path(sys.argv[1])
    
    print("==> Checking witness properties...")
    results = check_witnesses(traces_dir)
    
    print("\n==> Witness Observed Rates:")
    for witness, (found, total) in results.items():
        pct = found / total * 100 if total > 0 else 0
        print(f"    {witness}: {found}/{total} traces ({pct:.1f}%)")
    
    print()
    print("==> Witness check complete (observed rates reported above)")
    print("    Note: Hard thresholds are enforced in analyzer trust gates, not here.")
    sys.exit(0)

if __name__ == "__main__":
    main()
