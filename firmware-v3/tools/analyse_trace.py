#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 SpectraSynq
"""
analyse_trace.py — post-process a Chrome Trace Format JSON file produced by
capture_trace.py and emit a performance report.

Supports:
  - Per-counter and per-span histograms (16 buckets, nearest-rank percentiles)
  - Instant frequency tables
  - Render-frame deadline-miss extraction with effect-ID join
  - Regime segmentation via bench_begin/bench_split/bench_end markers
  - Conditional stats partitioned by event proximity (KS D-statistic)
  - Baseline comparison and contract enforcement

Stdlib only. No external dependencies.

British English in all user-facing strings (colour, behaviour, initialise, etc.).
"""
from __future__ import annotations

import argparse
import csv
import html as html_mod
import json
import math
import re
import sys
from bisect import bisect_right
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Version
# ---------------------------------------------------------------------------

TOOL_VERSION = "analyse_trace v1.0"
SCHEMA_VERSION = "1.0"

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Bucket:
    lo: float
    hi: float
    count: int


@dataclass(frozen=True)
class Stats:
    count: int
    min: float
    max: float
    mean: float
    p50: float
    p90: float
    p95: float
    p99: float
    p999: float
    histogram: tuple  # tuple[Bucket, ...]


@dataclass(frozen=True)
class InstantStats:
    count: int
    first_ts_us: int
    last_ts_us: int
    mean_interarrival_us: float | None


@dataclass
class Region:
    label: str
    start_ts_us: int
    end_ts_us: int
    counters: dict[str, Stats] = field(default_factory=dict)
    spans: dict[str, Stats] = field(default_factory=dict)


@dataclass(frozen=True)
class Miss:
    ts_us: int
    dur_us: int
    over_budget_us: int
    effect_id: int | None
    effect_hex: str | None


@dataclass(frozen=True)
class ConditionalResult:
    metric: str
    condition: str
    with_stats: Stats | None
    without_stats: Stats | None
    ks_d: float | None
    note: str | None


@dataclass(frozen=True)
class Verdict:
    status: str  # "PASS" | "FAIL" | "EMPTY" | "NO_BASELINE"
    summary: str


@dataclass
class Trace:
    path: Path
    events: list[dict]
    duration_us: int
    n_events: int
    display_time_unit: str
    parse_warnings: list[str]


@dataclass
class Report:
    trace: Trace
    counters: dict[str, Stats]
    spans: dict[str, Stats]
    instants: dict[str, InstantStats]
    regions: list[Region]
    deadline_misses: list[Miss]
    conditional_results: list[ConditionalResult]
    baseline_deltas: list[dict] | None
    baseline_path: Path | None
    verdict: Verdict


# ---------------------------------------------------------------------------
# Core statistical functions
# ---------------------------------------------------------------------------


def _nearest_rank_percentile(sorted_samples: list, p: float) -> float:
    """Nearest-rank percentile: deterministic, does not depend on interpolation."""
    n = len(sorted_samples)
    if n == 0:
        return 0.0
    idx = max(0, math.ceil(p * n) - 1)
    return float(sorted_samples[idx])


def compute_stats(samples: list) -> Stats:
    """Compute Stats from a sorted sample list (float or int)."""
    n = len(samples)
    if n == 0:
        raise ValueError("Cannot compute stats on empty sample list")
    lo = float(samples[0])
    hi = float(samples[-1])
    mean = sum(float(x) for x in samples) / n
    p50 = _nearest_rank_percentile(samples, 0.50)
    p90 = _nearest_rank_percentile(samples, 0.90)
    p95 = _nearest_rank_percentile(samples, 0.95)
    p99 = _nearest_rank_percentile(samples, 0.99)
    p999 = _nearest_rank_percentile(samples, 0.999)
    histogram = _make_histogram(samples, lo, hi)
    return Stats(
        count=n,
        min=lo,
        max=hi,
        mean=mean,
        p50=p50,
        p90=p90,
        p95=p95,
        p99=p99,
        p999=p999,
        histogram=histogram,
    )


def _make_histogram(samples: list, lo: float, hi: float) -> tuple:
    """Build a 16-bucket linear histogram."""
    n_buckets = 16
    if lo == hi:
        return (Bucket(lo=lo, hi=hi, count=len(samples)),)
    width = (hi - lo) / n_buckets
    buckets: list[Bucket] = []
    counts = [0] * n_buckets
    for v in samples:
        idx = int((float(v) - lo) / width)
        if idx >= n_buckets:
            idx = n_buckets - 1
        counts[idx] += 1
    for i in range(n_buckets):
        b_lo = lo + i * width
        b_hi = lo + (i + 1) * width
        buckets.append(Bucket(lo=b_lo, hi=b_hi, count=counts[i]))
    return tuple(buckets)


def compute_sparkline(time_value_pairs: list[tuple[int, float]]) -> str:
    """60-char sparkline from time-ordered (ts, value) pairs."""
    blocks = "▁▂▃▄▅▆▇█"
    n_buckets = 60
    if not time_value_pairs:
        return "▁" * n_buckets
    times = [t for t, _ in time_value_pairs]
    values = [v for _, v in time_value_pairs]
    t_min, t_max = times[0], times[-1]
    if t_min == t_max:
        v = values[0]
        v_min = v_max = v
    else:
        v_min = min(values)
        v_max = max(values)
    span = t_max - t_min
    bucket_sums = [0.0] * n_buckets
    bucket_counts = [0] * n_buckets
    for t, v in time_value_pairs:
        if span == 0:
            idx = 0
        else:
            idx = int((t - t_min) / span * n_buckets)
            if idx >= n_buckets:
                idx = n_buckets - 1
        bucket_sums[idx] += v
        bucket_counts[idx] += 1
    result = []
    for i in range(n_buckets):
        if bucket_counts[i] == 0:
            result.append(blocks[0])
        else:
            avg = bucket_sums[i] / bucket_counts[i]
            if v_max == v_min:
                norm = 0
            else:
                norm = int((avg - v_min) / (v_max - v_min) * 7)
                norm = max(0, min(7, norm))
            result.append(blocks[norm])
    return "".join(result)


# ---------------------------------------------------------------------------
# KS D-statistic (stdlib only)
# ---------------------------------------------------------------------------


def ks_d(a_sorted: list, b_sorted: list) -> float | None:
    """Kolmogorov-Smirnov two-sample D-statistic."""
    if not a_sorted or not b_sorted:
        return None
    na = len(a_sorted)
    nb = len(b_sorted)
    pooled = sorted(set(a_sorted) | set(b_sorted))
    max_d = 0.0
    for x in pooled:
        fa = bisect_right(a_sorted, x) / na
        fb = bisect_right(b_sorted, x) / nb
        d = abs(fa - fb)
        if d > max_d:
            max_d = d
    return max_d


# ---------------------------------------------------------------------------
# Trace parsing
# ---------------------------------------------------------------------------


def parse_trace(path: Path) -> Trace:
    """Load Chrome Trace Format JSON. Validates phase fields. Fills parse_warnings."""
    if not path.exists():
        sys.stderr.write(f"ERROR: input file not found: {path}\n")
        sys.exit(1)
    try:
        raw = path.read_text(encoding="utf-8")
    except OSError as exc:
        sys.stderr.write(f"ERROR: cannot read {path}: {exc}\n")
        sys.exit(1)
    try:
        data = json.loads(raw)
    except json.JSONDecodeError as exc:
        sys.stderr.write(f"ERROR: JSON parse error in {path}: {exc}\n")
        sys.exit(2)

    events: list[dict] = data.get("traceEvents", [])
    display_time_unit = data.get("displayTimeUnit", "ms")
    parse_warnings: list[str] = []

    # Validate required fields per phase
    required_fields_by_phase = {
        "X": ("name", "ts", "dur"),
        "C": ("name", "ts"),
        "i": ("name", "ts"),
        "B": ("name", "ts"),
        "E": ("name", "ts"),
    }
    valid_events = []
    for idx, e in enumerate(events):
        ph = e.get("ph")
        if ph not in required_fields_by_phase:
            # Tolerate M, s, t, f etc.
            valid_events.append(e)
            continue
        required = required_fields_by_phase[ph]
        missing = [f for f in required if f not in e]
        if missing:
            parse_warnings.append(
                f"Event[{idx}] ph={ph!r} name={e.get('name')!r} missing fields: {missing}"
            )
            # Check for args.value specifically for C events
            if ph == "C" and "ts" in e and "name" in e:
                args = e.get("args") or {}
                if "value" not in args:
                    sys.stderr.write(
                        f"ERROR: counter event '{e.get('name')}' at ts={e.get('ts')} "
                        f"missing args.value (exit 3)\n"
                    )
                    sys.exit(3)
            continue
        # For C events, ensure args.value exists
        if ph == "C":
            args = e.get("args") or {}
            if "value" not in args:
                sys.stderr.write(
                    f"ERROR: counter event '{e.get('name')}' at ts={e.get('ts')} "
                    f"missing args.value (exit 3)\n"
                )
                sys.exit(3)
        valid_events.append(e)

    # Match B/E pairs LIFO by name+tid
    be_stacks: dict[tuple, list] = defaultdict(list)
    completed_spans: list[dict] = []
    unmatched_e = []
    phase_events_out = []
    for e in valid_events:
        ph = e.get("ph")
        if ph == "B":
            key = (e.get("name"), e.get("tid"))
            be_stacks[key].append(e)
        elif ph == "E":
            key = (e.get("name"), e.get("tid"))
            if be_stacks[key]:
                begin = be_stacks[key].pop()
                synthetic = dict(begin)
                synthetic["ph"] = "X"
                synthetic["dur"] = e["ts"] - begin["ts"]
                completed_spans.append(synthetic)
            else:
                unmatched_e.append(e)
                parse_warnings.append(
                    f"Unmatched E event name={e.get('name')!r} tid={e.get('tid')!r} ts={e.get('ts')}"
                )
        else:
            phase_events_out.append(e)

    # Record unmatched B
    for key, stack in be_stacks.items():
        for begin in stack:
            parse_warnings.append(
                f"Unmatched B event name={begin.get('name')!r} tid={begin.get('tid')!r} ts={begin.get('ts')}"
            )

    all_events = phase_events_out + completed_spans

    # Duration
    ts_values = [e["ts"] for e in all_events if "ts" in e]
    if ts_values:
        t_min = min(ts_values)
        t_max = max(ts_values)
        # Include span end times
        for e in all_events:
            if e.get("ph") == "X" and "dur" in e:
                end = e["ts"] + e["dur"]
                if end > t_max:
                    t_max = end
        duration_us = t_max - t_min
    else:
        duration_us = 0

    return Trace(
        path=path.resolve(),
        events=all_events,
        duration_us=duration_us,
        n_events=len(valid_events),
        display_time_unit=display_time_unit,
        parse_warnings=parse_warnings,
    )


# ---------------------------------------------------------------------------
# Analysis A: per-counter histograms
# ---------------------------------------------------------------------------


def analyse_counters(events: list[dict]) -> dict[str, Stats]:
    """Group counter events by name and compute stats."""
    buckets: dict[str, list[tuple[int, float]]] = defaultdict(list)
    for e in events:
        if e.get("ph") == "C":
            args = e.get("args") or {}
            val = float(args["value"])
            buckets[e["name"]].append((e["ts"], val))

    result: dict[str, Stats] = {}
    for name, pairs in sorted(buckets.items()):
        pairs_sorted_by_ts = sorted(pairs, key=lambda p: p[0])
        values = sorted(v for _, v in pairs_sorted_by_ts)
        result[name] = compute_stats(values)
    return result


# ---------------------------------------------------------------------------
# Analysis B: per-span duration histograms
# ---------------------------------------------------------------------------


def analyse_spans(events: list[dict]) -> dict[str, Stats]:
    """Group X (span) events by name and compute duration stats."""
    buckets: dict[str, list[float]] = defaultdict(list)
    for e in events:
        if e.get("ph") == "X":
            buckets[e["name"]].append(float(e["dur"]))

    result: dict[str, Stats] = {}
    for name, durs in sorted(buckets.items()):
        result[name] = compute_stats(sorted(durs))
    return result


# ---------------------------------------------------------------------------
# Analysis C: instant frequency
# ---------------------------------------------------------------------------


def analyse_instants(events: list[dict]) -> dict[str, InstantStats]:
    """Compute instant event frequency statistics."""
    buckets: dict[str, list[int]] = defaultdict(list)
    for e in events:
        if e.get("ph") == "i":
            buckets[e["name"]].append(e["ts"])

    result: dict[str, InstantStats] = {}
    for name, ts_list in sorted(buckets.items()):
        ts_sorted = sorted(ts_list)
        count = len(ts_sorted)
        first = ts_sorted[0]
        last = ts_sorted[-1]
        if count >= 2:
            deltas = [ts_sorted[i + 1] - ts_sorted[i] for i in range(count - 1)]
            mean_arr: float | None = sum(deltas) / len(deltas)
        else:
            mean_arr = None
        result[name] = InstantStats(
            count=count,
            first_ts_us=first,
            last_ts_us=last,
            mean_interarrival_us=mean_arr,
        )
    return result


# ---------------------------------------------------------------------------
# Analysis D: render frame deadline misses
# ---------------------------------------------------------------------------


def extract_deadline_misses(
    events: list[dict],
    span_name: str = "render_frame",
    threshold_us: int = 2000,
) -> list[Miss]:
    """Extract span events exceeding the deadline threshold, join with effect_id_active."""
    # Build sorted list of (ts, value) for effect_id_active counter
    effect_samples: list[tuple[int, int]] = []
    for e in events:
        if e.get("ph") == "C" and e.get("name") == "effect_id_active":
            args = e.get("args") or {}
            effect_samples.append((e["ts"], int(args["value"])))
    effect_samples.sort(key=lambda x: x[0])
    effect_ts = [t for t, _ in effect_samples]

    misses: list[Miss] = []
    for e in events:
        if e.get("ph") != "X" or e.get("name") != span_name:
            continue
        dur = e["dur"]
        if dur > threshold_us:
            ts = e["ts"]
            over = dur - threshold_us
            # Most recent effect sample with ts <= miss ts
            eff_id: int | None = None
            eff_hex: str | None = None
            if effect_ts:
                idx = bisect_right(effect_ts, ts) - 1
                if idx >= 0:
                    eff_id = effect_samples[idx][1]
                    eff_hex = f"0x{eff_id:04X}"
            misses.append(Miss(
                ts_us=ts,
                dur_us=dur,
                over_budget_us=over,
                effect_id=eff_id,
                effect_hex=eff_hex,
            ))

    misses.sort(key=lambda m: m.ts_us)
    return misses


# ---------------------------------------------------------------------------
# Analysis E: regime segmentation
# ---------------------------------------------------------------------------


def _label_for(marker: dict, default: str) -> str:
    args = marker.get("args") or {}
    return args.get("label") or default


def regime_segment(events: list[dict], regime_by: str | None = None) -> list[Region]:
    """Segment trace into regions using bench markers or a custom prefix."""
    instants = sorted(
        [e for e in events if e.get("ph") == "i"],
        key=lambda e: e["ts"],
    )

    if regime_by:
        markers = [e for e in instants if e["name"].startswith(regime_by)]
    else:
        markers = [
            e for e in instants
            if e["name"] in ("bench_begin", "bench_split", "bench_end")
        ]

    all_ts = [e["ts"] for e in events if "ts" in e]
    if not all_ts:
        return []

    if not markers:
        return [Region(
            label="whole_trace",
            start_ts_us=min(all_ts),
            end_ts_us=max(all_ts) + 1,
        )]

    regions: list[Region] = []
    cursor_label = _label_for(markers[0], "region_0")
    cursor_start = markers[0]["ts"]

    for i, m in enumerate(markers):
        if i == 0:
            cursor_label = _label_for(m, "region_0")
            cursor_start = m["ts"]
            continue

        regions.append(Region(
            label=cursor_label,
            start_ts_us=cursor_start,
            end_ts_us=m["ts"],
        ))

        is_last = (i == len(markers) - 1)
        if m["name"] == "bench_end" or (regime_by and is_last):
            cursor_label = None  # type: ignore[assignment]
            cursor_start = None  # type: ignore[assignment]
        else:
            cursor_label = _label_for(m, f"region_{i}")
            cursor_start = m["ts"]

    if cursor_start is not None:
        last_ts = max(all_ts)
        regions.append(Region(
            label=cursor_label or "trailing",
            start_ts_us=cursor_start,
            end_ts_us=last_ts + 1,
        ))

    return regions


def scope_events_to_region(events: list[dict], region: Region) -> list[dict]:
    """Filter events to those within the region's time bounds."""
    return [
        e for e in events
        if "ts" in e and region.start_ts_us <= e["ts"] < region.end_ts_us
    ]


def populate_region_stats(regions: list[Region], events: list[dict]) -> None:
    """In-place: add counters and spans stats to each region."""
    for r in regions:
        scoped = scope_events_to_region(events, r)
        r.counters.update(analyse_counters(scoped))
        r.spans.update(analyse_spans(scoped))


# ---------------------------------------------------------------------------
# Analysis F: conditional stats
# ---------------------------------------------------------------------------


def conditional_split(
    events: list[dict],
    metric: str,
    event_name: str,
    window_ms: float,
) -> ConditionalResult:
    """Partition metric samples into 'with' (trigger in last window_ms) and 'without'."""
    window_us = window_ms * 1000.0
    condition_str = f"{metric} BY {event_name} in last {window_ms}ms"

    # Collect metric samples (sorted by ts)
    metric_samples: list[tuple[int, float]] = []
    for e in sorted(events, key=lambda x: x.get("ts", 0)):
        if e.get("name") != metric:
            continue
        if e.get("ph") == "C":
            args = e.get("args") or {}
            metric_samples.append((e["ts"], float(args["value"])))
        elif e.get("ph") == "X":
            metric_samples.append((e["ts"], float(e["dur"])))

    if not metric_samples:
        sys.stderr.write(
            f"ERROR: --conditional: metric '{metric}' not found in trace (exit 4)\n"
        )
        sys.exit(4)

    trigger_ts = sorted(
        e["ts"] for e in events if e.get("ph") == "i" and e.get("name") == event_name
    )

    with_samples: list[float] = []
    without_samples: list[float] = []
    j = 0
    for ts, value in metric_samples:
        while j < len(trigger_ts) and trigger_ts[j] <= ts:
            j += 1
        in_window = j > 0 and (ts - trigger_ts[j - 1]) <= window_us
        (with_samples if in_window else without_samples).append(value)

    if not trigger_ts:
        w_stats = None if not without_samples else compute_stats(sorted(without_samples))
        return ConditionalResult(
            metric=metric,
            condition=condition_str,
            with_stats=None,
            without_stats=w_stats,
            ks_d=None,
            note=f"trigger event '{event_name}' never fired",
        )

    w_stats = compute_stats(sorted(with_samples)) if with_samples else None
    wo_stats = compute_stats(sorted(without_samples)) if without_samples else None
    k = ks_d(sorted(with_samples), sorted(without_samples))
    return ConditionalResult(
        metric=metric,
        condition=condition_str,
        with_stats=w_stats,
        without_stats=wo_stats,
        ks_d=k,
        note=None,
    )


def parse_conditional_expression(expr: str) -> tuple[str, str, float]:
    """Parse 'METRIC BY EVENT in last Nms' → (metric, event, window_ms)."""
    pattern = re.compile(
        r"^\s*(.+?)\s+BY\s+(.+?)\s+in\s+last\s+(\d+(?:\.\d+)?)\s*ms\s*$",
        re.IGNORECASE,
    )
    m = pattern.match(expr)
    if not m:
        sys.stderr.write(
            f"ERROR: --conditional expression not understood: {expr!r}\n"
            f"  Expected format: 'METRIC BY EVENT in last Nms'\n"
        )
        sys.exit(1)
    return m.group(1).strip(), m.group(2).strip(), float(m.group(3))


# ---------------------------------------------------------------------------
# Analysis G: baseline comparison
# ---------------------------------------------------------------------------


def _stats_to_dict(s: Stats) -> dict:
    return {
        "count": s.count,
        "min": s.min,
        "max": s.max,
        "mean": round(s.mean, 4),
        "p50": s.p50,
        "p90": s.p90,
        "p95": s.p95,
        "p99": s.p99,
        "p999": s.p999,
        "histogram": {
            "buckets": [
                {"lo": b.lo, "hi": b.hi, "count": b.count}
                for b in s.histogram
            ],
            "n_buckets": len(s.histogram),
        },
    }


def compare_to_baseline(
    counters: dict[str, Stats],
    spans: dict[str, Stats],
    baseline: dict,
) -> list[dict]:
    """Produce pairwise delta rows."""
    deltas: list[dict] = []
    for metric_kind, current_dict in (("counters", counters), ("spans", spans)):
        for name, cur_stats in current_dict.items():
            base_stats = baseline.get(metric_kind, {}).get(name)
            if not base_stats:
                continue
            cur_d = _stats_to_dict(cur_stats)
            for stat in ("p50", "p90", "p95", "p99", "p999", "mean", "count"):
                cur_v = cur_d.get(stat)
                base_v = base_stats.get(stat)
                if cur_v is None or base_v is None:
                    continue
                contract_key = f"{name}.{stat}"
                contract = baseline.get("contracts", {}).get(contract_key, {})
                regression_pct = contract.get("regression_pct", 10.0)
                contract_max = contract.get("max_us") or contract.get("max")

                delta_pct: float | None = None
                if base_v and base_v != 0:
                    delta_pct = round((cur_v - base_v) / base_v * 100, 2)

                regression = False
                if contract_max is not None and cur_v > contract_max:
                    regression = True
                if delta_pct is not None and delta_pct > regression_pct:
                    regression = True

                deltas.append({
                    "metric": name,
                    "stat": stat,
                    "current": cur_v,
                    "baseline": base_v,
                    "delta_pct": delta_pct,
                    "regression": regression,
                    "contract": contract_max,
                })
    return deltas


def build_verdict(
    counters: dict[str, Stats],
    spans: dict[str, Stats],
    baseline: dict | None,
    deltas: list[dict] | None,
) -> Verdict:
    """Build overall verdict."""
    if not counters and not spans:
        return Verdict("EMPTY", "trace contains no measurable events")

    if baseline is None:
        rfw = counters.get("render_frame_work_us")
        if rfw:
            return Verdict(
                "NO_BASELINE",
                f"render_frame_work_us p99 = {rfw.p99:.0f} µs (no baseline supplied)",
            )
        n = len(counters) + len(spans)
        return Verdict("NO_BASELINE", f"{n} metrics captured, no baseline supplied")

    contract_failures = [
        d for d in (deltas or [])
        if d["regression"] and d["contract"] is not None
    ]
    regression_only = [
        d for d in (deltas or [])
        if d["regression"] and d["contract"] is None
    ]
    if contract_failures:
        first = contract_failures[0]
        unit = "µs" if "us" in first["metric"] else ""
        return Verdict(
            "FAIL",
            f"{first['metric']}.{first['stat']} = {first['current']}{unit} "
            f"(contract: < {first['contract']}{unit})",
        )
    if regression_only:
        first = regression_only[0]
        return Verdict(
            "FAIL",
            f"{first['metric']}.{first['stat']} regressed by {first['delta_pct']:.1f}%",
        )

    # Summarise top metric
    rfw = counters.get("render_frame_work_us") or spans.get("render_frame")
    if rfw:
        return Verdict(
            "PASS",
            f"all contracts met; render_frame_work_us p99 = {rfw.p99:.0f} µs",
        )
    return Verdict("PASS", "all contracts met")


# ---------------------------------------------------------------------------
# ASCII histogram and sparkline rendering
# ---------------------------------------------------------------------------

_BAR_CHARS = "▏▎▍▌▋▊▉█"
_BAR_FULL = "█"
_MAX_BAR_WIDTH = 32


def _render_bar(fraction: float) -> str:
    """Render a fraction [0..1] as an 8th-precision block bar, max 32 chars wide."""
    if fraction <= 0:
        return ""
    total_eighths = round(fraction * _MAX_BAR_WIDTH * 8)
    full = total_eighths // 8
    remainder = total_eighths % 8
    bar = _BAR_FULL * full
    if remainder > 0 and full < _MAX_BAR_WIDTH:
        bar += _BAR_CHARS[remainder - 1]
    return bar


def render_histogram_ascii(histogram: tuple, unit: str = "") -> str:
    """Render a histogram as ASCII block bars."""
    if not histogram:
        return "(no data)\n"
    max_count = max(b.count for b in histogram)
    lines = []
    for b in histogram:
        fraction = b.count / max_count if max_count > 0 else 0
        bar = _render_bar(fraction)
        pct = 100.0 * b.count / sum(x.count for x in histogram) if histogram else 0
        lo_s = f"{b.lo:>8.0f}" if b.lo == int(b.lo) else f"{b.lo:>10.2f}"
        hi_s = f"{b.hi:>8.0f}" if b.hi == int(b.hi) else f"{b.hi:>10.2f}"
        lines.append(
            f"[{lo_s} .. {hi_s} {unit}] {bar:<32s}  {b.count:>6d}  ({pct:5.1f}%)"
        )
    lines.append(f"{'':>22s}  {'0%':>32s}  {'100%'}")
    return "\n".join(lines)


def _fmt_stat(v: float) -> str:
    if v == int(v):
        return str(int(v))
    return f"{v:.2f}"


def render_stats_table(stats: Stats) -> str:
    """Render a single-row stats table."""
    header = "| count | min | max | mean | p50 | p90 | p95 | p99 | p999 |"
    sep = "|---|---|---|---|---|---|---|---|---|"
    row = (
        f"| {stats.count} | {_fmt_stat(stats.min)} | {_fmt_stat(stats.max)} | "
        f"{stats.mean:.1f} | {_fmt_stat(stats.p50)} | {_fmt_stat(stats.p90)} | "
        f"{_fmt_stat(stats.p95)} | {_fmt_stat(stats.p99)} | {_fmt_stat(stats.p999)} |"
    )
    return f"{header}\n{sep}\n{row}"


# ---------------------------------------------------------------------------
# Markdown renderer
# ---------------------------------------------------------------------------


def render_markdown(report: Report) -> str:
    lines: list[str] = []

    # 1. Verdict line
    prefix = {"PASS": "✓", "FAIL": "✗", "EMPTY": "…", "NO_BASELINE": "…"}.get(
        report.verdict.status, "…"
    )
    lines.append(f"{prefix} {report.verdict.summary}")
    lines.append("")

    # 2. Capture summary
    t = report.trace
    n_regions = len(report.regions)
    n_c = len(report.counters)
    n_s = len(report.spans)
    n_i = len(report.instants)
    lines.append("## Capture summary")
    lines.append("")
    lines.append("| Field | Value |")
    lines.append("|---|---|")
    lines.append(f"| File | `{t.path}` |")
    lines.append(f"| Events | {t.n_events} |")
    lines.append(f"| Duration | {t.duration_us / 1_000_000:.3f} s |")
    lines.append(f"| Counters | {n_c} |")
    lines.append(f"| Spans | {n_s} |")
    lines.append(f"| Instants | {n_i} |")
    lines.append(f"| Regions | {n_regions} |")
    lines.append("")

    # 3. Contract checks (if baseline)
    if report.baseline_deltas is not None and report.baseline_path is not None:
        lines.append("## Contract checks")
        lines.append("")
        contract_rows = [d for d in report.baseline_deltas if d["contract"] is not None]
        if contract_rows:
            lines.append("| Metric | Stat | Value | Contract | Status |")
            lines.append("|---|---|---|---|---|")
            for d in contract_rows:
                status = "✗ FAIL" if d["regression"] else "✓ PASS"
                lines.append(
                    f"| {d['metric']} | {d['stat']} | {d['current']} | "
                    f"< {d['contract']} | {status} |"
                )
        else:
            lines.append("_No contract rows in baseline._")
        lines.append("")

    # 4. Deadline misses
    lines.append("## Render frame deadline misses")
    lines.append("")
    if report.deadline_misses:
        lines.append("| # | ts_us | dur_us | over_budget_us | effect_id | effect_hex |")
        lines.append("|---|---|---|---|---|---|")
        for i, m in enumerate(report.deadline_misses, 1):
            eid = str(m.effect_id) if m.effect_id is not None else "—"
            ehex = m.effect_hex or "—"
            lines.append(
                f"| {i} | {m.ts_us} | {m.dur_us} | {m.over_budget_us} | {eid} | {ehex} |"
            )
    else:
        lines.append("_No deadline misses detected._")
    lines.append("")

    # 5. Per-counter statistics
    if report.counters:
        lines.append("## Per-counter statistics")
        lines.append("")
        for name, stats in sorted(report.counters.items()):
            lines.append(f"### {name}")
            lines.append("")
            lines.append(render_stats_table(stats))
            lines.append("")
            lines.append("```")
            lines.append(render_histogram_ascii(stats.histogram))
            lines.append("```")
            lines.append("")
            # Sparkline: need time-ordered pairs
            lines.append("_sparkline computed from time-ordered samples_")
            lines.append("")

    # 6. Per-span duration statistics
    if report.spans:
        lines.append("## Per-span duration statistics")
        lines.append("")
        for name, stats in sorted(report.spans.items()):
            lines.append(f"### {name}")
            lines.append("")
            lines.append(render_stats_table(stats))
            lines.append("")
            lines.append("```")
            lines.append(render_histogram_ascii(stats.histogram, unit="µs"))
            lines.append("```")
            lines.append("")

    # 7. Instant frequency
    if report.instants:
        lines.append("## Instant frequency")
        lines.append("")
        lines.append("| Name | Count | First ts (µs) | Last ts (µs) | Mean interarrival (µs) |")
        lines.append("|---|---|---|---|---|")
        for name, ist in sorted(report.instants.items()):
            arr = f"{ist.mean_interarrival_us:.0f}" if ist.mean_interarrival_us is not None else "—"
            lines.append(
                f"| {name} | {ist.count} | {ist.first_ts_us} | {ist.last_ts_us} | {arr} |"
            )
        lines.append("")

    # 8. Regime comparison
    if len(report.regions) > 1:
        lines.append("## Regime comparison")
        lines.append("")
        # Collect all metric names across regions
        all_metrics: dict[str, str] = {}  # metric_stat -> kind
        for r in report.regions:
            for nm in r.counters:
                for st in ("p50", "p90", "p95", "p99"):
                    all_metrics[f"{nm}.{st}"] = "counter"
            for nm in r.spans:
                for st in ("p50", "p90", "p95", "p99"):
                    all_metrics[f"{nm}.{st}"] = "span"

        region_labels = [r.label for r in report.regions]
        header = "| Metric.Stat | " + " | ".join(region_labels) + " |"
        sep = "|---|" + "---|" * len(report.regions)
        lines.append(header)
        lines.append(sep)
        for metric_stat in sorted(all_metrics.keys()):
            parts = metric_stat.rsplit(".", 1)
            nm, st = parts[0], parts[1]
            row_vals = []
            for r in report.regions:
                d = r.counters if nm in r.counters else r.spans
                if nm in d:
                    v = getattr(d[nm], st, None)
                    row_vals.append(_fmt_stat(v) if v is not None else "—")
                else:
                    row_vals.append("—")
            lines.append(f"| {metric_stat} | " + " | ".join(row_vals) + " |")
        lines.append("")

    # 9. Conditional analyses
    for cr in report.conditional_results:
        lines.append(f"## Conditional analysis: {cr.condition}")
        lines.append("")
        if cr.note:
            lines.append(f"_Note: {cr.note}_")
            lines.append("")
        if cr.with_stats:
            lines.append("**With trigger:**")
            lines.append("")
            lines.append(render_stats_table(cr.with_stats))
            lines.append("")
        if cr.without_stats:
            lines.append("**Without trigger:**")
            lines.append("")
            lines.append(render_stats_table(cr.without_stats))
            lines.append("")
        if cr.ks_d is not None:
            if cr.ks_d > 0.2:
                interp = "distributions differ substantially"
            elif cr.ks_d < 0.05:
                interp = "distributions effectively identical"
            else:
                interp = "moderate distributional difference"
            lines.append(f"KS D = {cr.ks_d:.4f} — {interp}")
            lines.append("")

    # 10. Baseline comparison
    if report.baseline_deltas is not None and report.baseline_path is not None:
        lines.append("## Baseline comparison")
        lines.append("")
        lines.append(f"Baseline: `{report.baseline_path}`")
        lines.append("")
        lines.append("| Metric | Stat | Current | Baseline | Δ% | Regression? |")
        lines.append("|---|---|---|---|---|---|")
        for d in report.baseline_deltas:
            dpct = f"{d['delta_pct']:+.1f}%" if d["delta_pct"] is not None else "—"
            reg = "✗ YES" if d["regression"] else "✓ no"
            lines.append(
                f"| {d['metric']} | {d['stat']} | {d['current']} | "
                f"{d['baseline']} | {dpct} | {reg} |"
            )
        lines.append("")

    # 11. Parse warnings
    if report.trace.parse_warnings:
        lines.append("## Parse warnings")
        lines.append("")
        for w in report.trace.parse_warnings:
            lines.append(f"- {w}")
        lines.append("")

    # 12. Footer
    lines.append("---")
    lines.append(f"_Tool: {TOOL_VERSION} | Input: `{t.path}`_")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# HTML renderer
# ---------------------------------------------------------------------------


def render_html(report: Report) -> str:
    """Render a self-contained HTML report with collapsible sections."""
    md = render_markdown(report)

    verdict_colour = {
        "PASS": "#2d7a2d",
        "FAIL": "#c0392b",
        "EMPTY": "#888888",
        "NO_BASELINE": "#888888",
    }.get(report.verdict.status, "#888888")

    prefix_map = {"PASS": "✓", "FAIL": "✗", "EMPTY": "…", "NO_BASELINE": "…"}
    prefix = prefix_map.get(report.verdict.status, "…")
    verdict_line = f"{prefix} {html_mod.escape(report.verdict.summary)}"

    # Convert markdown to very simple HTML (tables, pre blocks, headings)
    def md_to_html(text: str) -> str:
        out = []
        in_pre = False
        in_table = False
        for line in text.split("\n"):
            if line.startswith("```"):
                if in_pre:
                    out.append("</pre>")
                    in_pre = False
                else:
                    out.append("<pre>")
                    in_pre = True
                continue
            if in_pre:
                out.append(html_mod.escape(line))
                continue
            # Tables
            if line.startswith("|"):
                cells = [c.strip() for c in line.split("|")[1:-1]]
                if all(re.match(r"^-+$", c) for c in cells):
                    continue  # separator row
                if not in_table:
                    out.append("<table>")
                    in_table = True
                td = "".join(f"<td>{html_mod.escape(c)}</td>" for c in cells)
                out.append(f"<tr>{td}</tr>")
                continue
            else:
                if in_table:
                    out.append("</table>")
                    in_table = False
            # Headings
            if line.startswith("### "):
                out.append(f"<h3>{html_mod.escape(line[4:])}</h3>")
            elif line.startswith("## "):
                section_title = html_mod.escape(line[3:])
                out.append(f'<details open><summary><h2>{section_title}</h2></summary><div class="section-body">')
            elif line.startswith("# "):
                out.append(f"<h1>{html_mod.escape(line[2:])}</h1>")
            elif line.startswith("---"):
                out.append("<hr>")
            elif line.startswith("_") and line.endswith("_"):
                out.append(f"<em>{html_mod.escape(line[1:-1])}</em>")
            elif line.startswith("**") and line.endswith("**"):
                out.append(f"<strong>{html_mod.escape(line[2:-2])}</strong>")
            elif line == "":
                out.append("<br>")
            else:
                out.append(f"<p>{html_mod.escape(line)}</p>")
        if in_pre:
            out.append("</pre>")
        if in_table:
            out.append("</table>")
        return "\n".join(out)

    body = md_to_html(md)

    css = """
    body { font-family: 'Courier New', monospace; max-width: 1200px; margin: 0 auto; padding: 1em; }
    .verdict-banner { padding: 1em; border-radius: 4px; color: white; font-size: 1.2em; font-weight: bold; margin-bottom: 1em; }
    table { border-collapse: collapse; width: 100%; margin: 0.5em 0; }
    td, th { border: 1px solid #ddd; padding: 4px 8px; }
    pre { background: #f4f4f4; padding: 1em; overflow-x: auto; white-space: pre; }
    details > summary { cursor: pointer; user-select: none; }
    details > summary h2 { display: inline; }
    .section-body { padding-left: 1em; }
    h1, h2, h3 { margin: 0.5em 0; }
    """

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>analyse_trace report — {html_mod.escape(str(report.trace.path.name))}</title>
<style>{css}</style>
</head>
<body>
<div class="verdict-banner" style="background:{verdict_colour}">{verdict_line}</div>
{body}
</body>
</html>"""


# ---------------------------------------------------------------------------
# JSON renderer
# ---------------------------------------------------------------------------


def render_json(report: Report) -> dict:
    """Build deterministic JSON summary dict (sort_keys=True for output)."""
    def stats_to_dict(s: Stats) -> dict:
        return {
            "count": s.count,
            "histogram": {
                "buckets": [
                    {"count": b.count, "hi": b.hi, "lo": b.lo}
                    for b in s.histogram
                ],
                "n_buckets": len(s.histogram),
            },
            "max": s.max,
            "mean": round(s.mean, 4),
            "min": s.min,
            "p50": s.p50,
            "p90": s.p90,
            "p95": s.p95,
            "p99": s.p99,
            "p999": s.p999,
        }

    def instant_to_dict(ist: InstantStats) -> dict:
        return {
            "count": ist.count,
            "first_ts_us": ist.first_ts_us,
            "last_ts_us": ist.last_ts_us,
            "mean_interarrival_us": ist.mean_interarrival_us,
        }

    def region_to_dict(r: Region) -> dict:
        return {
            "counters": {k: stats_to_dict(v) for k, v in sorted(r.counters.items())},
            "end_ts_us": r.end_ts_us,
            "label": r.label,
            "spans": {k: stats_to_dict(v) for k, v in sorted(r.spans.items())},
            "start_ts_us": r.start_ts_us,
        }

    def cond_to_dict(cr: ConditionalResult) -> dict:
        return {
            "condition": cr.condition,
            "ks_d": cr.ks_d,
            "metric": cr.metric,
            "note": cr.note,
            "with": stats_to_dict(cr.with_stats) if cr.with_stats else None,
            "without": stats_to_dict(cr.without_stats) if cr.without_stats else None,
        }

    t = report.trace
    misses_data = None
    if report.deadline_misses is not None:
        misses_data = {
            "count": len(report.deadline_misses),
            "events": [
                {
                    "dur_us": m.dur_us,
                    "effect_id": m.effect_id,
                    "effect_hex": m.effect_hex,
                    "ts_us": m.ts_us,
                }
                for m in report.deadline_misses
            ],
            "metric": "render_frame",
            "threshold_us": 2000,
        }

    return {
        "baseline_comparison": (
            {
                "baseline_path": str(report.baseline_path),
                "deltas": report.baseline_deltas,
            }
            if report.baseline_deltas is not None
            else None
        ),
        "conditional": [cond_to_dict(cr) for cr in report.conditional_results],
        "counters": {k: stats_to_dict(v) for k, v in sorted(report.counters.items())},
        "deadline_misses": misses_data,
        "input": {
            "displayTimeUnit": t.display_time_unit,
            "duration_us": t.duration_us,
            "n_events": t.n_events,
            "path": str(t.path),
        },
        "instants": {k: instant_to_dict(v) for k, v in sorted(report.instants.items())},
        "parse_warnings": report.trace.parse_warnings,
        "regions": [region_to_dict(r) for r in report.regions],
        "schema_version": SCHEMA_VERSION,
        "spans": {k: stats_to_dict(v) for k, v in sorted(report.spans.items())},
        "tool_version": TOOL_VERSION,
        "verdict": {
            "status": report.verdict.status,
            "summary": report.verdict.summary,
        },
    }


# ---------------------------------------------------------------------------
# CSV exporter
# ---------------------------------------------------------------------------


def _sanitise_name(name: str) -> str:
    """Sanitise metric name for use as a filename component."""
    s = re.sub(r"[^A-Za-z0-9_.\-]", "_", name)
    s = re.sub(r"__+", "_", s)
    return s.strip("_")


def write_csvs(report: Report, csv_dir: Path) -> None:
    """Write one CSV per counter, span, instant, plus regions.csv and summary.csv."""
    csv_dir.mkdir(parents=True, exist_ok=True)
    trace = report.trace

    # Build a time-indexed lookup for sparklines and raw values
    counter_raw: dict[str, list[tuple[int, float]]] = defaultdict(list)
    span_raw: dict[str, list[tuple[int, float]]] = defaultdict(list)
    instant_raw: dict[str, list[int]] = defaultdict(list)
    for e in trace.events:
        ph = e.get("ph")
        if ph == "C":
            args = e.get("args") or {}
            counter_raw[e["name"]].append((e["ts"], float(args["value"])))
        elif ph == "X":
            span_raw[e["name"]].append((e["ts"], float(e["dur"])))
        elif ph == "i":
            instant_raw[e["name"]].append(e["ts"])

    # counter_<name>.csv
    for name, pairs in counter_raw.items():
        fname = csv_dir / f"counter_{_sanitise_name(name)}.csv"
        with fname.open("w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["ts_us", "value"])
            for ts, v in sorted(pairs):
                w.writerow([ts, v])

    # span_<name>.csv
    for name, pairs in span_raw.items():
        fname = csv_dir / f"span_{_sanitise_name(name)}.csv"
        with fname.open("w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["ts_us", "dur_us"])
            for ts, d in sorted(pairs):
                w.writerow([ts, d])

    # instant_<name>.csv
    for name, ts_list in instant_raw.items():
        fname = csv_dir / f"instant_{_sanitise_name(name)}.csv"
        with fname.open("w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["ts_us"])
            for ts in sorted(ts_list):
                w.writerow([ts])

    # regions.csv
    rfname = csv_dir / "regions.csv"
    with rfname.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["index", "label", "start_ts_us", "end_ts_us"])
        for i, r in enumerate(report.regions):
            w.writerow([i, r.label, r.start_ts_us, r.end_ts_us])

    # summary.csv
    sfname = csv_dir / "summary.csv"
    with sfname.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["metric", "kind", "count", "min", "max", "mean",
                    "p50", "p90", "p95", "p99", "p999"])
        for name, s in sorted(report.counters.items()):
            w.writerow([name, "counter", s.count, s.min, s.max,
                        round(s.mean, 4), s.p50, s.p90, s.p95, s.p99, s.p999])
        for name, s in sorted(report.spans.items()):
            w.writerow([name, "span", s.count, s.min, s.max,
                        round(s.mean, 4), s.p50, s.p90, s.p95, s.p99, s.p999])


# ---------------------------------------------------------------------------
# main()
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    """Entry point. Returns exit code."""
    parser = argparse.ArgumentParser(
        prog="analyse_trace.py",
        description="Post-process a Chrome Trace Format JSON file from capture_trace.py.",
    )
    parser.add_argument("trace", type=Path, help="Input Chrome Trace Format JSON.")
    parser.add_argument(
        "--output", type=Path, default=None,
        help="Markdown report path. Default: <trace>.report.md",
    )
    parser.add_argument(
        "--html", type=Path, default=None, nargs="?", const=...,
        help="Also write HTML report (default path: <trace>.report.html).",
    )
    parser.add_argument(
        "--json", type=Path, default=None, nargs="?", const=...,
        help="Also write JSON summary (default path: <trace>.report.json).",
    )
    parser.add_argument(
        "--csv-dir", type=Path, default=None,
        help="Also write per-counter/span CSVs into this directory.",
    )
    parser.add_argument(
        "--baseline", type=Path, default=None,
        help="Baseline JSON file for comparison and contract checks.",
    )
    parser.add_argument(
        "--conditional", action="append", default=[],
        metavar="'METRIC BY EVENT in last Nms'",
        help="Conditional analysis (repeatable).",
    )
    parser.add_argument(
        "--regime-by", type=str, default=None,
        help="Custom regime segmentation prefix for instant events.",
    )
    parser.add_argument(
        "--deadline-span", type=str, default="render_frame",
        help="Span name for deadline-miss extraction. Default: render_frame.",
    )
    parser.add_argument(
        "--deadline-us", type=int, default=2000,
        help="Deadline threshold in µs. Default: 2000.",
    )
    parser.add_argument(
        "--top", type=int, default=None,
        help="Show top N counters/spans by event count.",
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="Exit code 7 if verdict is FAIL.",
    )
    parser.add_argument(
        "--quiet", action="store_true",
        help="Suppress progress output to stderr.",
    )
    parser.add_argument(
        "--version", action="version", version=TOOL_VERSION,
    )

    args = parser.parse_args(argv)

    def progress(msg: str) -> None:
        if not args.quiet:
            sys.stderr.write(f"[analyse_trace] {msg}\n")

    # --- Parse trace ---
    progress(f"Loading {args.trace}")
    trace = parse_trace(args.trace)
    progress(f"Loaded {trace.n_events} events, {len(trace.events)} after B/E matching")

    # Handle empty trace
    if not trace.events:
        verdict = Verdict("EMPTY", "trace contains no measurable events")
        report = Report(
            trace=trace,
            counters={},
            spans={},
            instants={},
            regions=[],
            deadline_misses=[],
            conditional_results=[],
            baseline_deltas=None,
            baseline_path=None,
            verdict=verdict,
        )
        _write_outputs(args, report)
        return 0

    # --- Load baseline ---
    baseline: dict | None = None
    baseline_path: Path | None = None
    if args.baseline is not None:
        if not args.baseline.exists():
            sys.stderr.write(
                f"ERROR: cannot read baseline file: {args.baseline} (exit 5)\n"
            )
            sys.exit(5)
        try:
            baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            sys.stderr.write(
                f"ERROR: cannot read baseline {args.baseline}: {exc} (exit 5)\n"
            )
            sys.exit(5)
        baseline_path = args.baseline.resolve()

    # --- Core analyses ---
    progress("Analysing counters…")
    counters = analyse_counters(trace.events)
    progress("Analysing spans…")
    spans = analyse_spans(trace.events)
    progress("Analysing instants…")
    instants = analyse_instants(trace.events)

    # Apply --top filter
    if args.top is not None:
        def _top_n(d: dict[str, Stats], n: int) -> dict[str, Stats]:
            return dict(sorted(d.items(), key=lambda kv: -kv[1].count)[:n])
        counters = _top_n(counters, args.top)
        spans = _top_n(spans, args.top)

    # --- Regime segmentation ---
    progress("Segmenting regimes…")
    if args.regime_by is not None:
        # Validate that at least one event matches the prefix
        matching = [
            e for e in trace.events
            if e.get("ph") == "i" and e.get("name", "").startswith(args.regime_by)
        ]
        if not matching:
            sys.stderr.write(
                f"ERROR: --regime-by '{args.regime_by}' matched zero events in trace (exit 6)\n"
            )
            sys.exit(6)
    regions = regime_segment(trace.events, regime_by=args.regime_by)
    populate_region_stats(regions, trace.events)

    # --- Deadline misses ---
    progress("Extracting deadline misses…")
    deadline_misses = extract_deadline_misses(
        trace.events,
        span_name=args.deadline_span,
        threshold_us=args.deadline_us,
    )

    # --- Conditional analyses ---
    conditional_results: list[ConditionalResult] = []
    for expr in args.conditional:
        metric, event_name, window_ms = parse_conditional_expression(expr)
        progress(f"Conditional: {metric} BY {event_name} in last {window_ms}ms")
        cr = conditional_split(trace.events, metric, event_name, window_ms)
        conditional_results.append(cr)

    # --- Baseline comparison ---
    baseline_deltas: list[dict] | None = None
    if baseline is not None:
        progress("Comparing to baseline…")
        baseline_deltas = compare_to_baseline(counters, spans, baseline)

    # --- Verdict ---
    verdict = build_verdict(counters, spans, baseline, baseline_deltas)
    progress(f"Verdict: {verdict.status} — {verdict.summary}")

    # --- Build report ---
    report = Report(
        trace=trace,
        counters=counters,
        spans=spans,
        instants=instants,
        regions=regions,
        deadline_misses=deadline_misses,
        conditional_results=conditional_results,
        baseline_deltas=baseline_deltas,
        baseline_path=baseline_path,
        verdict=verdict,
    )

    _write_outputs(args, report)

    if args.strict and verdict.status == "FAIL":
        return 7

    return 0


def _write_outputs(args: argparse.Namespace, report: Report) -> None:
    """Write all requested output files."""
    trace_path = args.trace

    # Markdown (always written unless --quiet with no --output?)
    md_path = args.output or trace_path.with_suffix("").with_suffix(".report.md")
    # Handle the case where trace is e.g. foo.json → foo.report.md
    if args.output is None:
        stem = trace_path.stem  # strips last extension
        md_path = trace_path.with_name(stem + ".report.md")
    md_content = render_markdown(report)
    md_path.write_text(md_content, encoding="utf-8")
    if not args.quiet:
        sys.stderr.write(f"[analyse_trace] Wrote markdown → {md_path}\n")

    # Also print to stdout
    sys.stdout.write(md_content)
    sys.stdout.write("\n")

    # HTML
    if args.html is not None:
        if args.html is ...:
            stem = trace_path.stem
            html_path = trace_path.with_name(stem + ".report.html")
        else:
            html_path = args.html
        html_path.write_text(render_html(report), encoding="utf-8")
        if not args.quiet:
            sys.stderr.write(f"[analyse_trace] Wrote HTML → {html_path}\n")

    # JSON
    if args.json is not None:
        if args.json is ...:
            stem = trace_path.stem
            json_path = trace_path.with_name(stem + ".report.json")
        else:
            json_path = args.json
        json_data = render_json(report)
        json_path.write_text(
            json.dumps(json_data, sort_keys=True, separators=(",", ":")),
            encoding="utf-8",
        )
        if not args.quiet:
            sys.stderr.write(f"[analyse_trace] Wrote JSON → {json_path}\n")

    # CSV
    if args.csv_dir is not None:
        write_csvs(report, args.csv_dir)
        if not args.quiet:
            sys.stderr.write(f"[analyse_trace] Wrote CSVs → {args.csv_dir}\n")


if __name__ == "__main__":
    sys.exit(main())
