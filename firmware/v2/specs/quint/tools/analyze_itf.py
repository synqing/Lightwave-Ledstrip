#!/usr/bin/env python3
"""
Beat Tracker ITF Trace Analyzer

Parses Quint ITF traces and ranks tunings based on performance metrics.
Implements trust gates for minimum evidence and safety requirements.
"""

import json
import sys
import os
from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Optional, Any
from pathlib import Path
from datetime import datetime
import statistics

# ============================================================================
# Configuration Constants
# ============================================================================

# Trust Gate 1: Minimum traces per tuning
MIN_TRACES_PER_TUNING = 3

# Trust Gate 2: Safety thresholds
MAX_FALSE_LOCK_RATE = 0.20  # 20% - relaxed to allow convergence period

# Trust Gate 3: Scenario buckets (if available)
REQUIRED_SCENARIO_BUCKETS = ["baseline", "jitter", "miss", "false", "octave", "tempo_step", "burst"]

# Scoring weights
WEIGHT_LOCK_SUCCESS = 1000.0
WEIGHT_TIME_TO_LOCK = 0.1
WEIGHT_POST_LOCK_MAE = 10.0
WEIGHT_FALSE_LOCK = 500.0
WEIGHT_THRASH = 50.0
WEIGHT_DRIFT = 100.0
WEIGHT_JITTER = 50.0

# Constants
DT_MS = 20  # Time step in milliseconds
BPM_BUCKET_STEP = 2
BPM_MIN = 60

# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class TuningParams:
    refractory_ticks: int
    conf_gate: int
    alpha_attack: int
    alpha_release: int
    hold_ticks: int
    octave_mode: str
    phase_nudge: int
    
    def to_tuple(self) -> Tuple:
        return (self.refractory_ticks, self.conf_gate, self.alpha_attack,
                self.alpha_release, self.hold_ticks, self.octave_mode, self.phase_nudge)
    
    def to_dict(self) -> Dict:
        return {
            "refractory_ticks": self.refractory_ticks,
            "refractory_ms": self.refractory_ticks * DT_MS,
            "conf_gate": self.conf_gate / 10.0,
            "alpha_attack": self.alpha_attack / 100.0,
            "alpha_release": self.alpha_release / 100.0,
            "hold_ticks": self.hold_ticks,
            "hold_ms": self.hold_ticks * DT_MS,
            "octave_mode": self.octave_mode,
            "phase_nudge": self.phase_nudge / 100.0
        }

@dataclass
class TraceMetrics:
    """Metrics extracted from a single trace."""
    tuning: TuningParams
    env_true_bpm: int
    env_jitter_ms: int
    
    # Lock metrics
    locked: bool
    first_lock_tick: int
    locked_ticks: int
    total_ticks: int
    
    # Post-lock metrics (Phase 2)
    post_lock_ticks: int = 0
    post_lock_mae: float = 0.0
    post_lock_false_ticks: int = 0
    post_lock_bpm_samples: List[int] = field(default_factory=list)
    
    # Error metrics
    bpm_error_sum: int = 0
    thrash_count: int = 0
    double_trigger_count: int = 0
    false_lock_ticks: int = 0
    
    # Drift and jitter (Phase 2)
    drift_rate: float = 0.0  # BPM change per second while locked
    lock_jitter: float = 0.0  # Variance of BPM while locked
    
    def time_to_lock_ms(self) -> int:
        if self.first_lock_tick < 0:
            return 9999 * DT_MS  # Never locked
        return self.first_lock_tick * DT_MS
    
    def lock_success_rate(self) -> float:
        return self.locked_ticks / self.total_ticks if self.total_ticks > 0 else 0.0
    
    def post_lock_false_rate(self) -> float:
        return self.post_lock_false_ticks / self.post_lock_ticks if self.post_lock_ticks > 0 else 0.0
    
    def to_summary(self) -> Dict:
        return {
            "tuning": self.tuning.to_dict(),
            "env_true_bpm": self.env_true_bpm,
            "env_jitter_ms": self.env_jitter_ms,
            "locked": self.locked,
            "time_to_lock_ms": self.time_to_lock_ms(),
            "lock_success_rate": self.lock_success_rate(),
            "post_lock_ticks": self.post_lock_ticks,
            "post_lock_mae": self.post_lock_mae,
            "post_lock_false_rate": self.post_lock_false_rate(),
            "drift_rate": self.drift_rate,
            "lock_jitter": self.lock_jitter,
            "bpm_mae": self.bpm_error_sum / self.total_ticks if self.total_ticks > 0 else 0,
            "thrash_count": self.thrash_count,
            "double_trigger_count": self.double_trigger_count,
            "false_lock_ticks": self.false_lock_ticks
        }
    
    def compute_score(self) -> float:
        """Compute ranking score (lower is better)."""
        score = 0.0
        
        # Penalize not locking
        score += WEIGHT_LOCK_SUCCESS * (1.0 - self.lock_success_rate())
        
        # Penalize slow lock
        score += WEIGHT_TIME_TO_LOCK * self.time_to_lock_ms()
        
        # Penalize post-lock error
        score += WEIGHT_POST_LOCK_MAE * self.post_lock_mae
        
        # Penalize false locks
        score += WEIGHT_FALSE_LOCK * self.post_lock_false_rate()
        
        # Penalize thrashing
        score += WEIGHT_THRASH * self.thrash_count
        
        # Penalize drift
        score += WEIGHT_DRIFT * abs(self.drift_rate)
        
        # Penalize jitter
        score += WEIGHT_JITTER * self.lock_jitter
        
        return score

# ============================================================================
# Trace Parsing
# ============================================================================

def parse_bigint(value: Any) -> int:
    """Parse Quint bigint format."""
    if isinstance(value, dict) and "#bigint" in value:
        return int(value["#bigint"])
    return int(value)

def parse_tuning(tuning_dict: Dict) -> TuningParams:
    """Parse tuning parameters from trace state."""
    return TuningParams(
        refractory_ticks=parse_bigint(tuning_dict.get("refractory_ticks", 7)),
        conf_gate=parse_bigint(tuning_dict.get("conf_gate", 7)),
        alpha_attack=parse_bigint(tuning_dict.get("alpha_attack", 15)),
        alpha_release=parse_bigint(tuning_dict.get("alpha_release", 5)),
        hold_ticks=parse_bigint(tuning_dict.get("hold_ticks", 25)),
        octave_mode=tuning_dict.get("octave_mode", "conservative"),
        phase_nudge=parse_bigint(tuning_dict.get("phase_nudge", 5))
    )

def parse_trace(trace_path: Path) -> Optional[TraceMetrics]:
    """Parse a single ITF trace file."""
    try:
        with open(trace_path) as f:
            trace = json.load(f)
        
        states = trace.get("states", [])
        if not states:
            return None
        
        final_state = states[-1].get("state", {})
        tuning = parse_tuning(final_state.get("tuning", {}))
        env = final_state.get("env", {})
        metrics = final_state.get("metrics", {})
        
        true_bpm = parse_bigint(env.get("true_bpm", 120))
        true_bpm_bucket = (true_bpm - BPM_MIN) // BPM_BUCKET_STEP
        
        # Extract basic metrics
        first_lock_tick = parse_bigint(metrics.get("first_lock_tick", -1))
        locked_ticks = parse_bigint(metrics.get("locked_ticks", 0))
        total_ticks = parse_bigint(final_state.get("tick", 1))
        
        # Compute post-lock metrics by scanning states
        post_lock_ticks = 0
        post_lock_error_sum = 0
        post_lock_false_ticks = 0
        post_lock_bpm_samples = []
        
        for state_entry in states:
            state = state_entry.get("state", {})
            if state.get("locked", False):
                post_lock_ticks += 1
                bpm_hat = parse_bigint(state.get("bpm_hat", 0))
                post_lock_bpm_samples.append(bpm_hat)
                
                error = abs(bpm_hat - true_bpm_bucket) * BPM_BUCKET_STEP
                post_lock_error_sum += error
                
                if error > 10:  # >10 BPM error
                    post_lock_false_ticks += 1
        
        post_lock_mae = post_lock_error_sum / post_lock_ticks if post_lock_ticks > 0 else 0.0
        
        # Compute drift and jitter
        drift_rate = 0.0
        lock_jitter = 0.0
        if len(post_lock_bpm_samples) > 1:
            # Drift: BPM change per second
            bpm_changes = [abs(post_lock_bpm_samples[i+1] - post_lock_bpm_samples[i]) 
                          for i in range(len(post_lock_bpm_samples) - 1)]
            drift_rate = sum(bpm_changes) * BPM_BUCKET_STEP / (len(bpm_changes) * DT_MS / 1000)
            
            # Jitter: standard deviation of BPM while locked
            if len(post_lock_bpm_samples) > 1:
                lock_jitter = statistics.stdev(post_lock_bpm_samples) * BPM_BUCKET_STEP
        
        return TraceMetrics(
            tuning=tuning,
            env_true_bpm=true_bpm,
            env_jitter_ms=parse_bigint(env.get("jitter_ms", 0)),
            locked=final_state.get("locked", False),
            first_lock_tick=first_lock_tick,
            locked_ticks=locked_ticks,
            total_ticks=total_ticks,
            post_lock_ticks=post_lock_ticks,
            post_lock_mae=post_lock_mae,
            post_lock_false_ticks=post_lock_false_ticks,
            post_lock_bpm_samples=post_lock_bpm_samples,
            bpm_error_sum=parse_bigint(metrics.get("bpm_error_sum", 0)),
            thrash_count=parse_bigint(metrics.get("thrash_count", 0)),
            double_trigger_count=parse_bigint(metrics.get("double_trigger_count", 0)),
            false_lock_ticks=parse_bigint(metrics.get("false_lock_ticks", 0)),
            drift_rate=drift_rate,
            lock_jitter=lock_jitter
        )
    except Exception as e:
        print(f"    Warning: Failed to parse {trace_path}: {e}", file=sys.stderr)
        return None

# ============================================================================
# Aggregation and Ranking
# ============================================================================

def aggregate_by_tuning(traces: List[TraceMetrics]) -> Dict[Tuple, Dict]:
    """Aggregate trace metrics by tuning."""
    by_tuning: Dict[Tuple, List[TraceMetrics]] = {}
    
    for trace in traces:
        key = trace.tuning.to_tuple()
        if key not in by_tuning:
            by_tuning[key] = []
        by_tuning[key].append(trace)
    
    aggregated = {}
    for tuning_tuple, tuning_traces in by_tuning.items():
        n = len(tuning_traces)
        
        # Aggregate metrics
        lock_success_rate = sum(t.lock_success_rate() for t in tuning_traces) / n
        time_to_lock_mean = sum(t.time_to_lock_ms() for t in tuning_traces) / n
        time_to_lock_p95 = sorted([t.time_to_lock_ms() for t in tuning_traces])[int(n * 0.95)] if n > 1 else tuning_traces[0].time_to_lock_ms()
        
        post_lock_mae_mean = sum(t.post_lock_mae for t in tuning_traces) / n
        post_lock_false_rate_mean = sum(t.post_lock_false_rate() for t in tuning_traces) / n
        drift_rate_mean = sum(t.drift_rate for t in tuning_traces) / n
        lock_jitter_mean = sum(t.lock_jitter for t in tuning_traces) / n
        
        bpm_mae_mean = sum(t.bpm_error_sum / t.total_ticks for t in tuning_traces) / n
        thrash_rate = sum(t.thrash_count for t in tuning_traces) / sum(t.total_ticks for t in tuning_traces) * (1000 / DT_MS)
        double_trigger_count = sum(t.double_trigger_count for t in tuning_traces)
        
        # Compute aggregate score
        score = sum(t.compute_score() for t in tuning_traces) / n
        
        aggregated[tuning_tuple] = {
            "tuning": tuning_traces[0].tuning.to_dict(),
            "trace_count": n,
            "lock_success_rate": lock_success_rate,
            "time_to_lock_mean_ms": time_to_lock_mean,
            "time_to_lock_p95_ms": time_to_lock_p95,
            "post_lock_mae_mean": post_lock_mae_mean,
            "post_lock_false_rate_mean": post_lock_false_rate_mean,
            "drift_rate_mean": drift_rate_mean,
            "lock_jitter_mean": lock_jitter_mean,
            "bpm_mae_mean": bpm_mae_mean,
            "thrash_rate_per_sec": thrash_rate,
            "double_trigger_count": double_trigger_count,
            "score": score
        }
    
    return aggregated

def apply_trust_gates(aggregated: Dict[Tuple, Dict], traces: List[TraceMetrics]) -> Dict[Tuple, Dict]:
    """Apply trust gates to filter unreliable tunings."""
    
    print("\n==> Filtering Safety Violations (Trust Gate 2):")
    print("    NOTE: 'double_trigger_count' = onsets blocked by refractory (good, not a failure)")
    
    safe_tunings = {}
    disqualified_false_lock = 0
    
    for tuning_tuple, metrics in aggregated.items():
        ref_ms = metrics["tuning"]["refractory_ms"]
        blocks = metrics["double_trigger_count"]
        pl_false = metrics["post_lock_false_rate_mean"]
        pl_mae = metrics["post_lock_mae_mean"]
        
        # Trust Gate 2: Safety violations
        if pl_false > MAX_FALSE_LOCK_RATE:
            print(f"    ❌ DISQ(false-lock {pl_false*100:.1f}%) | ref={ref_ms}ms | blocks={blocks} | pl_false={pl_false:.3f} | pl_mae={pl_mae:.1f}")
            disqualified_false_lock += 1
            continue
        
        safe_tunings[tuning_tuple] = metrics
    
    print(f"    Total tunings: {len(aggregated)}")
    print(f"    Disqualified (false locks >{MAX_FALSE_LOCK_RATE*100:.0f}%): {disqualified_false_lock}")
    print(f"    SAFE tunings remaining: {len(safe_tunings)}")
    
    if len(safe_tunings) == 0:
        print("\nERROR: NO SAFE TUNINGS FOUND!", file=sys.stderr)
        print("All tunings violated safety requirements. Check model/environment.", file=sys.stderr)
        return {}
    
    # Trust Gate 3: Minimum evidence
    print("\n==> Filtering Insufficient Evidence (Trust Gate 3):")
    
    evidence_sufficient = {}
    disqualified_traces = 0
    
    for tuning_tuple, metrics in safe_tunings.items():
        if metrics["trace_count"] < MIN_TRACES_PER_TUNING:
            disqualified_traces += 1
            continue
        
        print(f"    ✅ PASS | traces={metrics['trace_count']} | tuning={tuning_tuple}")
        evidence_sufficient[tuning_tuple] = metrics
    
    print(f"    Total safe tunings: {len(safe_tunings)}")
    print(f"    Disqualified (insufficient traces <{MIN_TRACES_PER_TUNING}): {disqualified_traces}")
    print(f"    EVIDENCE-SUFFICIENT tunings remaining: {len(evidence_sufficient)}")
    
    return evidence_sufficient

def rank_tunings(tunings: Dict[Tuple, Dict]) -> List[Tuple[Tuple, Dict]]:
    """Rank tunings by score (lower is better)."""
    return sorted(tunings.items(), key=lambda x: x[1]["score"])

# ============================================================================
# Output Generation
# ============================================================================

def generate_report(ranked: List[Tuple[Tuple, Dict]], output_dir: Path, manifest: Dict) -> None:
    """Generate analysis report files."""
    
    # Summary CSV
    csv_path = output_dir / "summary.csv"
    with open(csv_path, "w") as f:
        f.write("rank,score,refractory_ms,conf_gate,alpha_attack,alpha_release,hold_ms,octave_mode,phase_nudge,")
        f.write("lock_success_rate,time_to_lock_mean_ms,post_lock_mae,post_lock_false_rate,drift_rate,lock_jitter,trace_count\n")
        for i, (tuning_tuple, metrics) in enumerate(ranked):
            t = metrics["tuning"]
            f.write(f"{i+1},{metrics['score']:.2f},{t['refractory_ms']},{t['conf_gate']},{t['alpha_attack']},{t['alpha_release']},")
            f.write(f"{t['hold_ms']},{t['octave_mode']},{t['phase_nudge']},")
            f.write(f"{metrics['lock_success_rate']:.3f},{metrics['time_to_lock_mean_ms']:.0f},")
            f.write(f"{metrics['post_lock_mae_mean']:.2f},{metrics['post_lock_false_rate_mean']:.3f},")
            f.write(f"{metrics['drift_rate_mean']:.3f},{metrics['lock_jitter_mean']:.3f},{metrics['trace_count']}\n")
    print(f"==> Wrote summary CSV: {csv_path}")
    
    # Summary JSON
    json_path = output_dir / "summary.json"
    with open(json_path, "w") as f:
        json.dump({
            "manifest": manifest,
            "rankings": [{"rank": i+1, "tuning": t, "metrics": m} for i, (t, m) in enumerate(ranked)]
        }, f, indent=2)
    print(f"==> Wrote summary JSON: {json_path}")
    
    # Top-K JSON
    top_k_path = output_dir / "top_k.json"
    top_k = ranked[:20]
    with open(top_k_path, "w") as f:
        json.dump([{"rank": i+1, "tuning": m["tuning"], "score": m["score"]} for i, (t, m) in enumerate(top_k)], f, indent=2)
    print(f"==> Wrote top-20 tunings: {top_k_path}")
    
    # Markdown report
    report_path = output_dir / "report.md"
    with open(report_path, "w") as f:
        f.write("# Beat Tracker Tuning Sweep Results\n\n")
        f.write(f"**Date**: {manifest.get('timestamp', 'unknown')}\n")
        f.write(f"**Quint Version**: {manifest.get('quint_version', 'unknown')}\n")
        f.write(f"**Traces**: {manifest.get('trace_count', 'unknown')}\n")
        f.write(f"**Steps per trace**: {manifest.get('max_steps', 'unknown')}\n")
        f.write(f"**Unique tunings**: {len(ranked)}\n\n")
        
        f.write("---\n\n## Top 20 Tunings\n\n")
        f.write("| Rank | Score | Refractory (ms) | Conf Gate | α_attack | α_release | Hold (ms) | Octave Mode | Phase Nudge |\n")
        f.write("|------|-------|----------------|-----------|----------|-----------|-----------|-------------|-------------|\n")
        for i, (tuning_tuple, metrics) in enumerate(top_k):
            t = metrics["tuning"]
            f.write(f"| {i+1} | {metrics['score']:.1f} | {t['refractory_ms']} | {t['conf_gate']} | {t['alpha_attack']:.2f} | {t['alpha_release']:.2f} | {t['hold_ms']} | {t['octave_mode']} | {t['phase_nudge']:.2f} |\n")
        
        f.write("\n---\n\n## Performance Metrics (Top 5)\n\n")
        for i, (tuning_tuple, metrics) in enumerate(ranked[:5]):
            t = metrics["tuning"]
            f.write(f"### Rank {i+1}\n\n")
            f.write(f"**Tuning**:\n")
            f.write(f"- refractory_ms: {t['refractory_ms']}\n")
            f.write(f"- conf_gate: {t['conf_gate']}\n")
            f.write(f"- alpha_attack: {t['alpha_attack']}\n")
            f.write(f"- alpha_release: {t['alpha_release']}\n")
            f.write(f"- hold_ms: {t['hold_ms']}\n")
            f.write(f"- octave_mode: {t['octave_mode']}\n")
            f.write(f"- phase_nudge: {t['phase_nudge']}\n\n")
            f.write(f"**Metrics**:\n")
            f.write(f"- Lock success rate: {metrics['lock_success_rate']*100:.1f}%\n")
            f.write(f"- Time to lock (mean): {metrics['time_to_lock_mean_ms']:.0f}ms\n")
            f.write(f"- Time to lock (p95): {metrics['time_to_lock_p95_ms']:.0f}ms\n")
            f.write(f"- Post-lock MAE: {metrics['post_lock_mae_mean']:.2f} BPM\n")
            f.write(f"- Post-lock false rate: {metrics['post_lock_false_rate_mean']*100:.1f}%\n")
            f.write(f"- Drift rate: {metrics['drift_rate_mean']:.3f} BPM/sec\n")
            f.write(f"- Lock jitter: {metrics['lock_jitter_mean']:.3f} BPM\n")
            f.write(f"- Thrash rate: {metrics['thrash_rate_per_sec']:.3f}/sec\n")
            f.write(f"- Double-trigger count: {metrics['double_trigger_count']}\n")
            f.write(f"- Score: {metrics['score']:.2f}\n\n")
        
        f.write("---\n\n## Manifest\n\n")
        f.write("```json\n")
        f.write(json.dumps(manifest, indent=2))
        f.write("\n```\n")
    
    print(f"==> Wrote report: {report_path}")

# ============================================================================
# Main Entry Point
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_itf.py <traces_dir> [--manifest <manifest.json>]")
        sys.exit(1)
    
    traces_dir = Path(sys.argv[1])
    manifest_path = None
    
    if "--manifest" in sys.argv:
        idx = sys.argv.index("--manifest")
        if idx + 1 < len(sys.argv):
            manifest_path = Path(sys.argv[idx + 1])
    
    # Find trace files recursively (supports per-tuning subdirectories)
    trace_files = []
    traces_subdir = traces_dir / "traces"
    
    if traces_subdir.exists():
        # Recursively search traces/ directory
        for f in traces_subdir.rglob("*"):
            if f.is_file() and not f.name.startswith("."):
                # Accept .itf.json files or numbered files (Quint output)
                if f.suffix == ".json" or f.name.isdigit():
                    # Skip non-trace JSON files
                    if f.name not in ["summary.json", "top_k.json", "run_manifest.json"]:
                        trace_files.append(f)
    
    # Fallback: check root of traces_dir if no traces subdir
    if not trace_files:
        for f in traces_dir.iterdir():
            if f.is_file() and not f.name.startswith(".") and (f.suffix == ".json" or f.name.isdigit()):
                if f.name not in ["summary.json", "top_k.json", "run_manifest.json"]:
                    trace_files.append(f)
    
    trace_files = list(set(trace_files))  # Dedupe
    print(f"==> Found {len(trace_files)} trace files\n")
    
    if not trace_files:
        print("ERROR: No trace files found!", file=sys.stderr)
        sys.exit(1)
    
    # Parse traces
    print("==> Parsing traces...")
    traces = []
    for tf in trace_files:
        trace = parse_trace(tf)
        if trace:
            traces.append(trace)
    
    print(f"==> Parsed {len(traces)} traces successfully")
    
    # Coverage analysis
    by_tuning = {}
    for t in traces:
        key = t.tuning.to_tuple()
        by_tuning[key] = by_tuning.get(key, 0) + 1
    
    print(f"==> Unique tunings: {len(by_tuning)}")
    
    counts = list(by_tuning.values())
    print(f"\n==> Coverage Analysis (Trust Gate 1):")
    print(f"    Traces per tuning - Min: {min(counts)}, Median: {statistics.median(counts)}, Mean: {statistics.mean(counts):.1f}, Max: {max(counts)}")
    print(f"    Tunings with <{MIN_TRACES_PER_TUNING} traces: {sum(1 for c in counts if c < MIN_TRACES_PER_TUNING)}")
    
    # Coverage uniformity check
    coverage_ratio = max(counts) / min(counts) if min(counts) > 0 else float('inf')
    if len(set(counts)) == 1:
        print(f"    ✓ Coverage is UNIFORM: exactly {counts[0]} traces per tuning")
    elif coverage_ratio > 2:
        print(f"    WARNING: Coverage is NON-UNIFORM (max/min ratio = {coverage_ratio:.1f})")
        print(f"    RECOMMENDATION: Use deterministic sweep (make sweep-deterministic) for uniform coverage")
    else:
        print(f"    ✓ Coverage is reasonably uniform (max/min ratio = {coverage_ratio:.1f})")
    
    # Report per-tuning coverage
    print(f"\n==> Per-Tuning Coverage:")
    for tuning_tuple, count in sorted(by_tuning.items(), key=lambda x: -x[1]):
        ref_ticks = tuning_tuple[0]
        conf_gate = tuning_tuple[1]
        status = "✓" if count >= MIN_TRACES_PER_TUNING else "⚠"
        print(f"    ref={ref_ticks*DT_MS}ms, conf={conf_gate/10:.1f}: {count} traces {status}")
    
    # Aggregate
    print("\n==> Aggregating results...")
    aggregated = aggregate_by_tuning(traces)
    
    # Apply trust gates
    filtered = apply_trust_gates(aggregated, traces)
    
    if not filtered:
        sys.exit(1)
    
    # Rank
    print("\n==> Ranking evidence-sufficient tunings...")
    ranked = rank_tunings(filtered)
    
    # Load or create manifest
    manifest = {
        "timestamp": datetime.now().isoformat(),
        "quint_version": "unknown",
        "trace_count": len(traces),
        "max_steps": max(t.total_ticks for t in traces) if traces else 0,
        "thresholds": {
            "min_traces_per_tuning": MIN_TRACES_PER_TUNING,
            "max_false_lock_rate": MAX_FALSE_LOCK_RATE
        }
    }
    
    if manifest_path and manifest_path.exists():
        with open(manifest_path) as f:
            manifest.update(json.load(f))
    
    # Generate outputs
    print("\n==> Writing outputs...")
    generate_report(ranked, traces_dir, manifest)
    
    print(f"\n==> Analysis complete!")
    print(f"==> Top tuning score: {ranked[0][1]['score']:.2f}")
    print(f"==> Results directory: {traces_dir}")
    
    if len(filtered) < 3:
        print(f"\nWARNING: Very few safe tunings ({len(filtered)}). May need to relax environment.", file=sys.stderr)

if __name__ == "__main__":
    main()
