# Surface 8 — analyse_trace.py post-process tool

## Contract

The analyser is a **pure-stdlib, deterministic, read-only** post-process for Chrome Trace Format JSON files produced by `firmware-v3/tools/capture_trace.py`. Numerical and behavioural invariants:

1. **Determinism.** Two invocations with identical input flags produce byte-identical `--json` output and byte-identical markdown bodies (excluding any `Generated:` timestamp line, which is the single permitted source of non-determinism and MUST be omitted from the JSON summary).
2. **Stdlib only.** Imports limited to `json`, `statistics`, `argparse`, `pathlib`, `dataclasses`, `math`, `re`, `sys`, `csv`, `html`, `typing`, `collections`. No third-party deps. No optional `numpy` fallback.
3. **No mutation.** Never writes to or modifies the input trace file. Output paths are always explicit (default-derived from input path).
4. **Verdict-first output.** First line of every markdown report and the `verdict` field of every JSON summary is a pass/fail line covering every contract metric in the (optional) baseline file.
5. **Empty-trace tolerance.** A trace containing zero `traceEvents` produces a valid (mostly empty) report with exit code 0 and verdict `EMPTY`.
6. **Loud failure on undefined references.** `--conditional` against a missing metric, `--baseline` referencing a missing file, `--regime-by` matching zero events: each fails with a non-zero exit code and a clear stderr message. Never silently ignored.
7. **Single pass per file.** The analyser reads each input file exactly once and holds the parsed `traceEvents` list in memory. No streaming requirement (typical trace ≤ 10 MB).
8. **Bounded output.** Markdown report is capped at ~600 lines (collapsing tail of long histograms). HTML is a single self-contained file (no external assets).

Exit codes:
- `0` success (including `EMPTY` verdict)
- `1` input file missing or unreadable
- `2` JSON parse error
- `3` missing required field in `traceEvents` (e.g. counter without `args.value`)
- `4` `--conditional` references a metric not in trace
- `5` `--baseline` file unreadable or schema-invalid
- `6` `--regime-by` matched zero events
- `7` contract verdict FAIL (only emitted when `--strict` flag is set)

## Input format

Chrome Trace Format JSON. Required top-level shape:

```json
{
  "traceEvents": [ <event>, ... ],
  "displayTimeUnit": "ms",
  "otherData": { "version": "..." }
}
```

`displayTimeUnit` and `otherData` are advisory; only `traceEvents` is required. Per-event contract by phase:

| `ph` | Meaning | Required fields | Optional fields used |
|---|---|---|---|
| `X` | Span (complete event) | `name` (str), `ts` (int µs), `dur` (int µs), `pid`, `tid` | `args` (dict) |
| `C` | Counter sample | `name`, `ts`, `args.value` (int or number), `pid`, `tid` | `args.<other>` (preserved but unused) |
| `i` | Instant | `name`, `ts`, `pid`, `tid` | `s` (scope), `args` |
| `B`/`E` | Begin/End duration pair | `name`, `ts`, `pid`, `tid` | matched by `name`+`tid` |

Other phases (`M`, `s`, `t`, `f`, etc.) are tolerated but ignored. A `B` without matching `E` (or vice versa) is recorded under `parse_warnings` and excluded from span statistics.

Bench markers (Surface 7 framework):
- `bench_begin` (instant) — region 0 starts
- `bench_split` (instant, repeatable) — closes current region, opens next; `args.label` (optional str) names the new region
- `bench_end` (instant) — closes final region

If `bench_begin` is absent, the trace is a single anonymous region named `whole_trace`.

## Output formats

### Markdown report (default)

Single `.md` file. Section order is fixed:

```
1. Verdict line                          (1 line, ✓ or ✗ summary)
2. Capture summary                       (table: file, n_events, duration, n_counters, n_spans, n_instants, n_regions)
3. Contract checks                       (only if --baseline given; one row per contract metric)
4. Render frame deadline misses          (table; empty if zero misses)
5. Per-counter statistics                (one subsection per counter; table + ASCII histogram + sparkline)
6. Per-span duration statistics          (one subsection per span name)
7. Instant frequency                     (single table)
8. Regime comparison                     (only if regions detected; side-by-side stats table)
9. Conditional analyses                  (one subsection per --conditional flag)
10. Baseline comparison                  (only if --baseline given; ±% delta table per metric)
11. Parse warnings                       (only if non-empty)
12. Footer: tool version, input path
```

Stat tables use exactly these columns in this order: `count | min | max | mean | p50 | p90 | p95 | p99 | p999`. All values rendered as integers when `dur`/`ts`/counter values are integer-typed; otherwise rounded to 2 decimal places.

ASCII histogram format (16 buckets, fixed-width):

```
[  1400 ..  1437 µs] ████████████████████████████████  3120  (86.7%)
[  1437 ..  1474 µs] ███▎                                 410  (11.4%)
[  1474 ..  1511 µs] ▎                                     45   (1.3%)
...
                     0%                              100%
```

Bar characters: full block `█` plus eighths (`▏▎▍▌▋▊▉`) for sub-cell precision. Fixed 32-character maximum bar width. Bucket labels use the same unit as the source counter (µs for durations; raw for unitless counters).

Sparkline format (single line, 60 chars wide):

```
trend: ▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁
```

Computed by binning the time-series into 60 buckets, taking each bucket's mean, normalising to [0..7], and mapping to the eight unicode block characters `▁▂▃▄▅▆▇█`.

### HTML report (--html flag)

Same content and section order as markdown. Differences:
- Single self-contained `.html` file with inline `<style>` and inline `<script>` (no external CDN, no bundler).
- Each section wrapped in `<details><summary>` for collapse/expand.
- Stat tables become `<table>` with sticky header.
- ASCII histograms preserved as `<pre>` blocks (not rendered as charts — leave that to v2).
- Verdict line rendered as a coloured banner (green for ✓, red for ✗, grey for EMPTY).

### JSON summary (--json flag)

Schema (deterministic key order, sorted; arrays preserve insertion order):

```json
{
  "schema_version": "1.0",
  "tool_version": "analyse_trace v1.0",
  "input": {
    "path": "/abs/path/to/trace.json",
    "n_events": 14823,
    "duration_us": 30012345,
    "displayTimeUnit": "ms"
  },
  "verdict": {
    "status": "PASS" | "FAIL" | "EMPTY" | "NO_BASELINE",
    "summary": "render_frame p99 = 1957 µs (contract: < 2000 µs) ✓"
  },
  "counters": {
    "<counter_name>": {
      "count": 3600,
      "min": 1402,
      "max": 1989,
      "mean": 1612.4,
      "p50": 1605,
      "p90": 1740,
      "p95": 1812,
      "p99": 1957,
      "p999": 1981,
      "histogram": {
        "buckets": [{"lo": 1402, "hi": 1439, "count": 312}, ...],
        "n_buckets": 16
      }
    }
  },
  "spans": {
    "<span_name>": { /* same shape as counters */ }
  },
  "instants": {
    "<instant_name>": {
      "count": 3,
      "first_ts_us": 12340,
      "last_ts_us": 28912000,
      "mean_interarrival_us": 9637000
    }
  },
  "regions": [
    {
      "label": "warmup",
      "start_ts_us": 0,
      "end_ts_us": 5000000,
      "counters": { /* same shape */ },
      "spans": { /* same shape */ }
    }
  ],
  "deadline_misses": {
    "metric": "render_frame",
    "threshold_us": 2000,
    "count": 2,
    "events": [
      {"ts_us": 12340000, "dur_us": 2143, "effect_id": 8448}
    ]
  },
  "conditional": [
    {
      "metric": "render_frame_work_us",
      "condition": "ws_msg_dispatch in last 16ms",
      "with": { /* stats */ },
      "without": { /* stats */ },
      "ks_d": 0.342
    }
  ],
  "baseline_comparison": {
    "baseline_path": "/abs/path/to/baseline.json",
    "deltas": [
      {
        "metric": "render_frame_work_us",
        "stat": "p99",
        "current": 1957,
        "baseline": 1820,
        "delta_pct": 7.53,
        "regression": false,
        "contract_us": 2000
      }
    ]
  },
  "parse_warnings": []
}
```

`null` is used for absent sections (`"baseline_comparison": null`) rather than omitting the key, so consumers can rely on schema stability.

### CSV exports (--csv-dir flag)

One CSV per counter and per span. File naming:

- `counter_<sanitised_name>.csv` — columns `ts_us,value`
- `span_<sanitised_name>.csv` — columns `ts_us,dur_us`
- `instant_<sanitised_name>.csv` — columns `ts_us`
- `regions.csv` — columns `index,label,start_ts_us,end_ts_us`
- `summary.csv` — columns `metric,kind,count,min,max,mean,p50,p90,p95,p99,p999`

Sanitisation rule: replace any character outside `[A-Za-z0-9_.-]` with `_`, collapse runs.

## Core analyses

### A. Per-counter histograms

For every distinct `name` among `ph == "C"` events:

```
samples = sorted([e.args.value for e in events if e.ph == "C" and e.name == name])
n = len(samples)
stats.count = n
stats.min, stats.max = samples[0], samples[-1]
stats.mean = sum(samples) / n
stats.p50  = percentile(samples, 0.50)
stats.p90  = percentile(samples, 0.90)
stats.p95  = percentile(samples, 0.95)
stats.p99  = percentile(samples, 0.99)
stats.p999 = percentile(samples, 0.999)
```

Percentile uses **nearest-rank** (`samples[ceil(p*n)-1]`) for determinism — `statistics.quantiles` is **not** used (its method varies across Python versions).

16-bucket linear histogram from `[stats.min, stats.max]`. Equal-width buckets; if `min == max`, single bucket of count `n`. Edge value belongs to the lower bucket (right-open) except the final bucket which is closed on both ends.

Sparkline computed from the time-ordered (not value-sorted) sample list. 60 buckets across `[first_ts, last_ts]`.

### B. Per-span duration histograms

Identical to counters but operate on `e.dur` for `ph == "X"` events. For `B`/`E` pairs, durations are reconstructed by matching `name`+`tid` LIFO; unmatched markers go to `parse_warnings`.

### C. Instant frequency

For every distinct `name` among `ph == "i"` events:

```
ts_list = sorted([e.ts for e in events if e.ph == "i" and e.name == name])
count = len(ts_list)
first_ts_us = ts_list[0]
last_ts_us = ts_list[-1]
if count >= 2:
    deltas = [ts_list[i+1] - ts_list[i] for i in range(count-1)]
    mean_interarrival_us = sum(deltas) / len(deltas)
else:
    mean_interarrival_us = None
```

### D. Render frame deadline-miss table

Specific extraction targeting `render_frame` span (or any span name supplied via `--deadline-span <name>`, default `render_frame`) with `dur > deadline_threshold` (default `2000` µs, override via `--deadline-us`).

For each miss, attach the active effect ID by looking at the most recent `effect_id_active` counter sample with `ts ≤ miss.ts`. If the counter is absent, `effect_id` is `null`.

Output table columns: `# | ts_us | dur_us | over_budget_us | effect_id | effect_hex`.

### E. Regime segmentation

Algorithm — see "Algorithm pseudocode > Regime segmentation" below.

Output: side-by-side comparison table. For each region, compute the same stats as A/B/C scoped to events with `start_ts ≤ e.ts < end_ts`. Render one column per region; rows are `metric.stat` (e.g. `render_frame_work_us.p99`). Regions are columns (left-to-right in chronological order), making horizontal scrolling unnecessary for ≤ 6 regions.

Custom segmentation: `--regime-by <event_prefix>` partitions on instant events whose name starts with `<event_prefix>` (e.g. `--regime-by phase_` would split on `phase_warmup`, `phase_main`, `phase_cooldown`).

### F. Conditional stats — the killer feature

Syntax: `--conditional <metric> BY <event> in last <Nms>` (repeatable).

Algorithm — see "Algorithm pseudocode > Conditional stats" below.

Output: a subsection per conditional invocation showing:
- The condition string verbatim
- Two stat tables (`with` and `without`) side-by-side
- Sample counts and the **Kolmogorov-Smirnov D-statistic** (max absolute difference between the two empirical CDFs)
- A textual interpretation: `D = 0.342 — distributions differ substantially` (D > 0.2), `D = 0.04 — distributions effectively identical` (D < 0.05), or middle ground.

The KS statistic is computed without `scipy`:

```
def ks_d(a_sorted, b_sorted):
    if not a_sorted or not b_sorted: return None
    pooled = sorted(set(a_sorted) | set(b_sorted))
    max_d = 0.0
    for x in pooled:
        cdf_a = bisect_right(a_sorted, x) / len(a_sorted)
        cdf_b = bisect_right(b_sorted, x) / len(b_sorted)
        if abs(cdf_a - cdf_b) > max_d:
            max_d = abs(cdf_a - cdf_b)
    return max_d
```

### G. Baseline comparison

Algorithm — see "Algorithm pseudocode > Baseline comparison" below.

Output: a single table with columns `metric.stat | current | baseline | Δ | Δ% | regression?`. A row is flagged `regression` when:
- the metric has an explicit `contract` threshold and `current > contract`, OR
- the metric has a `regression_pct` threshold (default `+10%`) and `current > baseline * (1 + regression_pct/100)`.

Improvements (current < baseline by more than `improvement_pct`, default 5%) are highlighted but not flagged as regression.

## Baseline file format

Same overall shape as `--json` output, with these additions:

```json
{
  "schema_version": "1.0",
  "tool_version": "analyse_trace v1.0",
  "captured_on": "2026-04-27T18:00:00Z",
  "captured_from": "esp32dev_audio_esv11_k1v2_32khz_trace",
  "git_sha": "4d12edc5",
  "notes": "Effect 0x2102 — Light Mode Bloom — 30s soak — K1 V2 b4:3a:45:a5:87:f8",
  "contracts": {
    "render_frame.p99": {"max_us": 2000, "regression_pct": 5.0},
    "render_frame_work_us.p99": {"max_us": 2000, "regression_pct": 5.0},
    "audio_snapshot_age_us.p99": {"max_us": 5000, "regression_pct": 10.0},
    "render_frame_deadline_miss.count": {"max": 5, "regression_pct": 100.0}
  },
  "counters": { /* identical schema to --json output */ },
  "spans": { /* same */ },
  "instants": { /* same */ }
}
```

A baseline file is consumed by `--baseline <path>`. Surface 1 (render budget) is responsible for **producing** the canonical baseline (typically by running the analyser with `--json` on a known-good trace and adding the `contracts` block by hand or via `--init-baseline-from <metrics>` helper, out of scope for v1).

The `contracts` map keys use dotted form `<metric>.<stat>` where stat ∈ `{p50, p90, p95, p99, p999, count, min, max, mean}`. Values are objects with at least one of `max_us` (or `max` for unitless), `min_us`/`min`, `regression_pct`. Missing contracts are not enforced but are still compared and shown.

## CLI specification

```
analyse_trace.py <trace.json> [options]

Positional:
  trace                Input Chrome Trace Format JSON.

Output flags (any combination):
  --output PATH        Markdown report path. Default: <trace>.report.md
  --html PATH          Also write HTML report. PATH optional; default <trace>.report.html
  --json PATH          Also write JSON summary. PATH optional; default <trace>.report.json
  --csv-dir DIR        Also write per-counter/span CSVs into DIR (created if absent).

Analysis flags:
  --baseline PATH                    Baseline JSON file for comparison + contract checks.
  --conditional 'M BY E in last Nms' Conditional analysis (repeatable). M is a counter or
                                     span name; E is an instant name; Nms is the lookback
                                     window in milliseconds. Quote the whole expression.
  --regime-by PREFIX                 Custom regime segmentation on instants matching prefix.
                                     Default: bench_begin / bench_split / bench_end.
  --deadline-span NAME               Span name for deadline-miss extraction. Default: render_frame.
  --deadline-us N                    Deadline threshold µs. Default: 2000.
  --top N                            Show top N counters/spans by event count. Default: all.
  --strict                           Exit code 7 if verdict is FAIL.

Misc:
  --quiet                Suppress progress output to stderr.
  --version              Print tool version and exit.
  -h, --help             Show help and exit.
```

Argparse details: every flag uses long form only (no short flags except `-h`); `--conditional` uses `action="append"` with `default=[]`. `--html`, `--json`, `--csv-dir` are `nargs="?"` with sentinel `None` meaning "use default path derived from input".

## Algorithm pseudocode

### Regime segmentation

```
def segment_regions(events, regime_by=None):
    instants = sorted([e for e in events if e["ph"] == "i"], key=lambda e: e["ts"])

    if regime_by:
        markers = [e for e in instants if e["name"].startswith(regime_by)]
    else:
        markers = [e for e in instants
                   if e["name"] in ("bench_begin", "bench_split", "bench_end")]

    if not markers:
        # Whole trace is one anonymous region
        all_ts = [e["ts"] for e in events if "ts" in e]
        if not all_ts:
            return []
        return [Region(label="whole_trace",
                       start_ts_us=min(all_ts),
                       end_ts_us=max(all_ts) + 1)]

    regions = []
    cursor_label = "pre_begin"
    cursor_start = markers[0]["ts"]   # first marker opens the first region

    for i, m in enumerate(markers):
        if i == 0:
            # opens region; do not emit yet
            cursor_label = label_for(m, default="region_0")
            cursor_start = m["ts"]
            continue

        # m closes the previous region and (if not bench_end) opens the next
        regions.append(Region(label=cursor_label,
                              start_ts_us=cursor_start,
                              end_ts_us=m["ts"]))

        if m["name"] == "bench_end" or (regime_by and i == len(markers)-1):
            cursor_label = None
            cursor_start = None
        else:
            cursor_label = label_for(m, default=f"region_{i}")
            cursor_start = m["ts"]

    # If we never saw bench_end, close the trailing region at last event ts
    if cursor_start is not None:
        last_ts = max(e["ts"] for e in events if "ts" in e)
        regions.append(Region(label=cursor_label,
                              start_ts_us=cursor_start,
                              end_ts_us=last_ts + 1))

    return regions

def label_for(marker, default):
    args = marker.get("args") or {}
    return args.get("label") or default
```

For each region, scope events:

```
def scope_events_to_region(events, region):
    return [e for e in events
            if "ts" in e
            and region.start_ts_us <= e["ts"] < region.end_ts_us]
```

### Conditional stats

```
def conditional_split(events, metric, event_name, window_ms):
    window_us = window_ms * 1000
    metric_samples = []  # list of (ts, value, span_or_counter)
    for e in events:
        if e["name"] != metric:
            continue
        if e["ph"] == "C":
            metric_samples.append((e["ts"], e["args"]["value"]))
        elif e["ph"] == "X":
            metric_samples.append((e["ts"], e["dur"]))

    trigger_ts = sorted(e["ts"] for e in events
                        if e["ph"] == "i" and e["name"] == event_name)

    with_samples, without_samples = [], []
    j = 0
    for ts, value in metric_samples:
        # Find the most recent trigger ≤ ts via two-pointer walk (events sorted by ts)
        while j < len(trigger_ts) and trigger_ts[j] <= ts:
            j += 1
        # j now points one-past the last trigger ≤ ts; previous trigger is trigger_ts[j-1]
        in_window = j > 0 and (ts - trigger_ts[j-1]) <= window_us
        (with_samples if in_window else without_samples).append(value)

    if not metric_samples:
        raise SystemExit(f"--conditional: metric '{metric}' not found in trace [exit 4]")
    if not trigger_ts:
        # Trigger absent: everything is "without"
        return ConditionalResult(
            with_stats=None,
            without_stats=compute_stats(without_samples),
            ks_d=None,
            note=f"trigger event '{event_name}' never fired"
        )

    return ConditionalResult(
        with_stats=compute_stats(sorted(with_samples)),
        without_stats=compute_stats(sorted(without_samples)),
        ks_d=ks_d(sorted(with_samples), sorted(without_samples)),
        note=None
    )
```

Caller pre-sorts `events` by `ts` once before calling `conditional_split`. The two-pointer walk requires events to be in ts-order.

### Baseline comparison

```
def compare_to_baseline(current, baseline):
    deltas = []
    for metric_kind in ("counters", "spans"):
        for name, cur_stats in current[metric_kind].items():
            base_stats = baseline.get(metric_kind, {}).get(name)
            if not base_stats:
                continue  # new metric, not in baseline; skip silently
            for stat in ("p50", "p90", "p95", "p99", "p999", "mean", "count"):
                cur_v = cur_stats.get(stat)
                base_v = base_stats.get(stat)
                if cur_v is None or base_v is None:
                    continue
                contract_key = f"{name}.{stat}"
                contract = baseline.get("contracts", {}).get(contract_key, {})
                regression_pct = contract.get("regression_pct", 10.0)
                contract_max = contract.get("max_us") or contract.get("max")

                delta_pct = ((cur_v - base_v) / base_v * 100) if base_v else None
                regression = False
                if contract_max is not None and cur_v > contract_max:
                    regression = True
                if delta_pct is not None and delta_pct > regression_pct:
                    regression = True

                deltas.append({
                    "metric": name, "stat": stat,
                    "current": cur_v, "baseline": base_v,
                    "delta_pct": delta_pct,
                    "regression": regression,
                    "contract": contract_max,
                })
    return deltas
```

Verdict line construction:

```
def build_verdict(current, baseline, deltas):
    if not current["counters"] and not current["spans"]:
        return Verdict("EMPTY", "trace contains no measurable events")
    if baseline is None:
        # No baseline — verdict is descriptive only
        rfw = current["counters"].get("render_frame_work_us")
        if rfw:
            return Verdict("NO_BASELINE",
                f"render_frame_work_us p99 = {rfw['p99']} µs (no baseline supplied)")
        return Verdict("NO_BASELINE", f"{len(current['counters'])} counters captured, no baseline")

    contract_failures = [d for d in deltas
                         if d["regression"] and d["contract"] is not None]
    if contract_failures:
        first = contract_failures[0]
        return Verdict("FAIL",
            f"{first['metric']}.{first['stat']} = {first['current']} µs (contract: < {first['contract']} µs)")
    return Verdict("PASS",
        "all contracts met; " + summary_of_top_metrics(current))
```

The verdict prefix `✓` / `✗` / `…` is rendered by the markdown writer based on `status`.

## Implementation file: firmware-v3/tools/analyse_trace.py

### Module layout

| Function | Signature | Responsibility |
|---|---|---|
| `parse_trace` | `(path: Path) -> Trace` | Loads JSON, validates phases, fills `parse_warnings`, returns `Trace`. |
| `compute_stats` | `(samples: list[float\|int]) -> Stats` | Stats + 16-bucket histogram from a sorted sample list. |
| `compute_sparkline` | `(time_value_pairs: list[tuple[int, float]]) -> str` | 60-char sparkline from time-ordered samples. |
| `regime_segment` | `(events, regime_by: str\|None) -> list[Region]` | Bench/custom-prefix segmentation. |
| `scope_events_to_region` | `(events, region) -> list[event]` | Filter events into region time bounds. |
| `extract_deadline_misses` | `(events, span_name, threshold_us) -> list[Miss]` | Deadline-miss table with effect-ID join. |
| `conditional_split` | `(events, metric, event_name, window_ms) -> ConditionalResult` | The killer-feature analysis. |
| `ks_d` | `(a_sorted, b_sorted) -> float\|None` | Kolmogorov-Smirnov D-statistic, stdlib only. |
| `compare_to_baseline` | `(current, baseline) -> list[Delta]` | Pairwise comparison + contract enforcement. |
| `build_verdict` | `(current, baseline, deltas) -> Verdict` | Status + first-line summary. |
| `render_markdown` | `(report: Report) -> str` | Markdown writer (no I/O). |
| `render_html` | `(report: Report) -> str` | HTML writer (no I/O). |
| `render_json` | `(report: Report) -> dict` | Stable-key JSON dict for `json.dumps(..., sort_keys=True)`. |
| `write_csvs` | `(report, dir: Path) -> None` | CSV exporter (only function permitted to write multiple files). |
| `main` | `(argv) -> int` | Argparse, orchestration, exit code mapping. |

### Data structures (dataclasses)

```python
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
    histogram: list[Bucket]

@dataclass(frozen=True)
class Bucket:
    lo: float
    hi: float
    count: int

@dataclass(frozen=True)
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
    status: str   # "PASS" | "FAIL" | "EMPTY" | "NO_BASELINE"
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
```

### Dependencies

- Python 3.10+ (uses `X | None` union syntax, `dataclasses` with `field`, `match` available but not required).
- Stdlib only: `json`, `statistics`, `argparse`, `pathlib`, `dataclasses`, `math`, `re`, `sys`, `csv`, `html`, `bisect`, `collections`, `typing`, `datetime` (verdict text only — not in JSON output).
- Optional v2: `matplotlib` for `--png` rendering. **Out of scope for v1.**

## Validation tests

The implementing agent must add `firmware-v3/test/test_native/test_analyse_trace.py` covering at minimum:

| # | Test | Expected |
|---|---|---|
| 1 | Round-trip on `/tmp/k1_trace_0x2100.json` (or any captured fixture) | Exit 0; markdown contains "Per-counter statistics" and at least one `render_frame_work_us` row. |
| 2 | Determinism: run twice, diff JSON outputs | Byte-identical. |
| 3 | Empty trace `{"traceEvents": [], "displayTimeUnit": "ms"}` | Exit 0; verdict status `EMPTY`; markdown ≤ 30 lines. |
| 4 | `--conditional missing_metric BY any in last 16ms` | Exit 4; stderr contains `'missing_metric' not found in trace`. |
| 5 | `--baseline /no/such/file.json` | Exit 5; stderr contains `cannot read baseline`. |
| 6 | Synthetic trace with three `bench_split` events | Three regions detected; per-region stats produced. |
| 7 | Synthetic trace with five `render_frame` durations 1500/1600/1700/1800/2143 µs | Deadline-miss table shows exactly 1 row at 2143 µs. |
| 8 | `effect_id_active` counter present, deadline-miss at ts T | Miss row's `effect_id` matches the most recent counter sample with `ts ≤ T`. |
| 9 | Conditional with KS D-statistic on identical distributions | KS D = 0.0 (within 1e-9). |
| 10 | Baseline regression: current p99 = 2200, baseline p99 = 1900, contract 2000 | Verdict status `FAIL`; with `--strict`, exit code 7. |
| 11 | `--csv-dir` produces `summary.csv` with one row per metric | Row count matches `len(counters) + len(spans)`. |
| 12 | Span B/E pair without matching E | Recorded in `parse_warnings`; not counted in span stats. |

Fixtures live in `firmware-v3/test/test_native/fixtures/trace/`. Captured fixtures from real hardware (e.g. `/tmp/k1_trace_0x2102.json`) are checked in trimmed to ≤ 100 KB to keep the repo lean.

## Acceptance criteria

1. Running `analyse_trace.py /tmp/k1_trace_0x2102.json --output /tmp/report.md` produces a markdown report whose first non-frontmatter line begins with `✓` or `✗`.
2. Report's first line is `✓ render_frame p99 = NNNN µs (contract: < 2000 µs)` when a baseline with that contract is supplied; or `… render_frame_work_us p99 = NNNN µs (no baseline supplied)` when none is.
3. Running with `--baseline baseline.json` shows a `Baseline comparison` section with one row per (metric, stat) pair common to both files, including `Δ%` and a `regression?` flag.
4. Running `analyse_trace.py trace.json --conditional 'render_frame_us BY ws_msg_dispatch in last 16ms'` correctly partitions samples and reports a non-null KS D-statistic when both buckets are non-empty.
5. Running twice on the same input with the same flags produces byte-identical `--json` output.
6. `python3 -c "import analyse_trace"` succeeds with stdlib only — no `pip install` required.
7. Empty trace produces a valid report with verdict `EMPTY` and exit 0.
8. Deadline-miss table joins effect IDs from `effect_id_active` counter samples (Surface 1, Addition 4) when present.

## Open questions

1. **Should `--strict` default to on?** Captain decision. Recommended OFF for interactive use, ON for CI invocations. Tooling agent should document the CI invocation in `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md`.
2. **HTML charting in v1 or v2?** Spec scopes v1 as ASCII histograms only. If Captain wants inline SVG/Canvas charts, that is a v2 expansion. The JSON summary already contains buckets, so external dashboards can render charts independently.
3. **Region overlap policy.** If a future surface adds nested bench regions (e.g. `bench_begin > sub_begin > sub_end > bench_end`), should the analyser report nested regions, flatten, or warn? v1 flattens (last marker wins) and emits a `parse_warnings` entry.
4. **Sparkline character set on Windows.** Eight-block characters render in modern terminals; older Windows consoles may not. Spec assumes UTF-8 stdout. If portability matters, a `--ascii-only` flag could downgrade to `.,-=+*#@`. Not in v1.
5. **Baseline producer.** This spec defines the baseline *consumer*. Surface 1 (render budget) should specify how the canonical `baseline.json` is produced and committed (likely under `firmware-v3/tools/baselines/<env>_<effect>.json`).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-27 | agent:claude-opus-4-7 | Created. Spec for analyse_trace.py post-process companion to capture_trace.py. Defines 7 core analyses (A-G), CLI, JSON/markdown/HTML/CSV outputs, baseline file format, regime segmentation + conditional stats pseudocode, 12-test validation matrix. Stdlib-only constraint. |
