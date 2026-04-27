#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 SpectraSynq
"""
test_analyse_trace.py — 12-test validation matrix for analyse_trace.py.

Tests are importable-module level (no subprocess). analyse_trace.main() returns
an exit code. All fixtures are synthesised in-memory; real hardware traces from
/tmp/k1_trace_0x*.json are used when present.

Run with:
    python3 firmware-v3/test/test_native/test_analyse_trace.py -v
or:
    python3 -m pytest firmware-v3/test/test_native/test_analyse_trace.py -v
"""
from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

# Add tools/ to path for importing analyse_trace
_TOOLS_DIR = Path(__file__).parent.parent.parent / "tools"
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

import analyse_trace as at


# ---------------------------------------------------------------------------
# Fixture helpers
# ---------------------------------------------------------------------------


def _write_trace(path: Path, events: list[dict], **extra) -> None:
    """Write a minimal Chrome Trace Format JSON file."""
    data = {
        "traceEvents": events,
        "displayTimeUnit": "ms",
    }
    data.update(extra)
    path.write_text(json.dumps(data), encoding="utf-8")


def _counter(name: str, ts: int, value: int | float, pid: int = 1, tid: str = "Test") -> dict:
    return {"ph": "C", "name": name, "ts": ts, "pid": pid, "tid": tid, "args": {"value": value}}


def _span(name: str, ts: int, dur: int, pid: int = 1, tid: str = "Test") -> dict:
    return {"ph": "X", "name": name, "ts": ts, "dur": dur, "pid": pid, "tid": tid}


def _instant(name: str, ts: int, pid: int = 1, tid: str = "Test", label: str | None = None) -> dict:
    e: dict = {"ph": "i", "name": name, "ts": ts, "pid": pid, "tid": tid}
    if label is not None:
        e["args"] = {"label": label}
    return e


def _make_run_args(trace_path: Path, **kwargs) -> list[str]:
    """Build argv list for main()."""
    args = [str(trace_path)]
    for k, v in kwargs.items():
        flag = "--" + k.replace("_", "-")
        if v is True:
            args.append(flag)
        elif v is not False and v is not None:
            args.extend([flag, str(v)])
    return args


def _run(argv: list[str]) -> int:
    """Run analyse_trace.main() catching SystemExit."""
    try:
        return at.main(argv)
    except SystemExit as exc:
        return int(exc.code)


# ---------------------------------------------------------------------------
# Synthetic 30-second fixture (used when real traces absent)
# ---------------------------------------------------------------------------


def _make_synthetic_30s_trace() -> list[dict]:
    """Generate a realistic 30-second synthetic trace (no hardware required)."""
    events: list[dict] = []
    base_ts = 1_000_000  # 1 second offset to simulate real firmware timestamps
    # 30 seconds at 120 FPS ≈ 3600 render frames
    frame_us = 8333  # ~120 FPS
    for i in range(3600):
        ts = base_ts + i * frame_us
        # render_frame span (mostly under 2000 µs, occasional miss)
        dur = 1600 + (i % 7) * 50  # varies 1600..1900
        if i == 1800:
            dur = 2143  # one intentional miss
        events.append(_span("render_frame", ts, dur))
        # render_frame_work_us counter
        events.append(_counter("render_frame_work_us", ts + 10, dur - 100))
        # effect_id_active counter (changes at frame 1200)
        eid = 0x2100 if i < 1200 else 0x2101
        events.append(_counter("effect_id_active", ts + 5, eid))
    # audio counters at 125 Hz
    audio_frame_us = 8000
    for i in range(3750):
        ts = base_ts + i * audio_frame_us
        events.append(_counter("audio_snapshot_age_us", ts, 800 + (i % 5) * 100))
    return events


# ---------------------------------------------------------------------------
# Test 1: Round-trip — counter histogram counts match input
# ---------------------------------------------------------------------------


class TestRoundTrip(unittest.TestCase):
    def test_round_trip_counter_counts(self):
        """Counter event count in report matches input event count."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            n = 50
            events = [_counter("my_counter", 1000 + i * 100, i * 10) for i in range(n)]
            _write_trace(tp, events)

            argv = [str(tp), "--json", str(Path(td) / "out.json"), "--quiet"]
            rc = _run(argv)
            self.assertEqual(rc, 0)

            out = json.loads((Path(td) / "out.json").read_text())
            self.assertIn("my_counter", out["counters"])
            self.assertEqual(out["counters"]["my_counter"]["count"], n)
            # Total bucket counts must equal n
            total_bucket = sum(
                b["count"] for b in out["counters"]["my_counter"]["histogram"]["buckets"]
            )
            self.assertEqual(total_bucket, n)

    def test_round_trip_real_trace(self):
        """Round-trip on /tmp/k1_trace_0x2100.json if present."""
        real = Path("/tmp/k1_trace_0x2100.json")
        if not real.exists():
            self.skipTest("Real trace not available at /tmp/k1_trace_0x2100.json")
        with tempfile.TemporaryDirectory() as td:
            md_path = Path(td) / (real.stem + ".report.md")
            argv = [
                str(real),
                "--output", str(md_path),
                "--json", str(Path(td) / "out.json"),
                "--quiet",
            ]
            rc = _run(argv)
            self.assertEqual(rc, 0)
            out = json.loads((Path(td) / "out.json").read_text())
            self.assertIn("render_frame", out["spans"])
            # Markdown report should contain per-counter section
            md = md_path.read_text()
            self.assertIn("Per-span duration statistics", md)


# ---------------------------------------------------------------------------
# Test 2: Determinism — byte-identical JSON across runs
# ---------------------------------------------------------------------------


class TestDeterminism(unittest.TestCase):
    def test_json_output_is_deterministic(self):
        """Two runs on the same trace produce byte-identical JSON."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_counter("foo", 1000 + i * 100, i) for i in range(100)]
            events += [_span("bar", 1000 + i * 80, 500 + i) for i in range(80)]
            _write_trace(tp, events)

            out1 = Path(td) / "out1.json"
            out2 = Path(td) / "out2.json"
            _run([str(tp), "--json", str(out1), "--quiet"])
            _run([str(tp), "--json", str(out2), "--quiet"])

            self.assertEqual(
                out1.read_bytes(), out2.read_bytes(),
                "JSON outputs differ between identical runs"
            )


# ---------------------------------------------------------------------------
# Test 3: Empty trace → EMPTY, exit 0
# ---------------------------------------------------------------------------


class TestEmptyTrace(unittest.TestCase):
    def test_empty_trace_exit_0(self):
        """Empty traceEvents array: exit 0, verdict EMPTY, markdown ≤ 30 lines."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "empty.json"
            _write_trace(tp, [])

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            self.assertEqual(out["verdict"]["status"], "EMPTY")

            md_path = Path(td) / "empty.report.md"
            if md_path.exists():
                lines = md_path.read_text().splitlines()
                self.assertLessEqual(len(lines), 30, f"Empty trace report too long: {len(lines)} lines")


# ---------------------------------------------------------------------------
# Test 4: Missing required field → exit 3
# ---------------------------------------------------------------------------


class TestMissingRequiredField(unittest.TestCase):
    def test_counter_without_args_value_exits_3(self):
        """Counter event without args.value: exit 3."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "bad.json"
            # Counter event missing args.value
            events = [{"ph": "C", "name": "broken", "ts": 1000, "pid": 1, "tid": "T"}]
            _write_trace(tp, events)
            rc = _run([str(tp), "--quiet"])
            self.assertEqual(rc, 3)


# ---------------------------------------------------------------------------
# Test 5: Missing baseline file → exit 5
# ---------------------------------------------------------------------------


class TestMissingBaseline(unittest.TestCase):
    def test_missing_baseline_exits_5(self):
        """--baseline pointing to nonexistent file: exit 5, stderr mentions 'cannot read baseline'."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            _write_trace(tp, [_counter("x", 1000, 5)])

            import io
            old_stderr = sys.stderr
            sys.stderr = io.StringIO()
            try:
                rc = _run([str(tp), "--baseline", "/no/such/baseline_file.json", "--quiet"])
            finally:
                err_output = sys.stderr.getvalue()
                sys.stderr = old_stderr

            self.assertEqual(rc, 5)
            self.assertIn("baseline", err_output.lower())


# ---------------------------------------------------------------------------
# Test 6: Three-region segmentation
# ---------------------------------------------------------------------------


class TestThreeRegionSegmentation(unittest.TestCase):
    def test_three_regions_from_bench_markers(self):
        """bench_begin + 2x bench_split + bench_end produces exactly 3 regions."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = []
            # Region 0: warmup (0..100 000 µs)
            events.append(_instant("bench_begin", 0, label="warmup"))
            for i in range(20):
                events.append(_counter("my_metric", i * 5000, 100 + i))
            # Region 1: main (100 000..200 000 µs)
            events.append(_instant("bench_split", 100_000, label="main"))
            for i in range(20):
                events.append(_counter("my_metric", 100_000 + i * 5000, 200 + i))
            # Region 2: cooldown (200 000..300 000 µs)
            events.append(_instant("bench_split", 200_000, label="cooldown"))
            for i in range(20):
                events.append(_counter("my_metric", 200_000 + i * 5000, 50 + i))
            events.append(_instant("bench_end", 300_000))

            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            regions = out["regions"]
            self.assertEqual(len(regions), 3, f"Expected 3 regions, got {len(regions)}: {[r['label'] for r in regions]}")
            labels = [r["label"] for r in regions]
            self.assertIn("warmup", labels)
            self.assertIn("main", labels)
            self.assertIn("cooldown", labels)

            # Each region should have per-region counter stats
            for r in regions:
                self.assertIn("my_metric", r["counters"])


# ---------------------------------------------------------------------------
# Test 7: Deadline-miss table shows exactly 1 row
# ---------------------------------------------------------------------------


class TestDeadlineMissTable(unittest.TestCase):
    def test_one_deadline_miss_detected(self):
        """Five render_frame spans: one over 2000 µs → exactly 1 miss row."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            durations = [1500, 1600, 1700, 1800, 2143]
            events = [_span("render_frame", 10_000 + i * 10_000, d) for i, d in enumerate(durations)]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            dm = out["deadline_misses"]
            self.assertIsNotNone(dm)
            self.assertEqual(dm["count"], 1)
            self.assertEqual(len(dm["events"]), 1)
            self.assertEqual(dm["events"][0]["dur_us"], 2143)
            self.assertEqual(dm["events"][0]["ts_us"], 10_000 + 4 * 10_000)


# ---------------------------------------------------------------------------
# Test 8: effect_id join edge cases
# ---------------------------------------------------------------------------


class TestEffectIdJoin(unittest.TestCase):
    def test_effect_id_join_at_before_after_miss(self):
        """effect_id_active join: sample before miss, sample after miss, sample at miss ts."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            # effect_id_active: 0x2100 at ts=5000, 0x2102 at ts=100000
            # Miss at ts=50000 → should join to 0x2100 (most recent ≤ 50000)
            events = [
                _counter("effect_id_active", 5_000, 0x2100),
                _counter("effect_id_active", 100_000, 0x2102),
                _span("render_frame", 50_000, 2500),   # miss at ts=50000
            ]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            dm = out["deadline_misses"]
            self.assertEqual(dm["count"], 1)
            miss = dm["events"][0]
            self.assertEqual(miss["effect_id"], 0x2100)
            self.assertEqual(miss["effect_hex"], "0x2100")

    def test_effect_id_join_sample_at_miss_ts(self):
        """effect_id_active sample at exact miss ts → should join."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [
                _counter("effect_id_active", 50_000, 0x2101),   # exactly at miss ts
                _span("render_frame", 50_000, 2200),
            ]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            miss = out["deadline_misses"]["events"][0]
            self.assertEqual(miss["effect_id"], 0x2101)

    def test_effect_id_null_when_no_counter(self):
        """No effect_id_active counter present → miss effect_id is null."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_span("render_frame", 10_000, 2300)]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            miss = out["deadline_misses"]["events"][0]
            self.assertIsNone(miss["effect_id"])


# ---------------------------------------------------------------------------
# Test 9: KS D = 0 on identical samples
# ---------------------------------------------------------------------------


class TestKSIdentical(unittest.TestCase):
    def test_ks_zero_on_identical_distributions(self):
        """ks_d([x, x, x...], [x, x, x...]) = 0.0."""
        samples = [float(i) for i in range(100)]
        d = at.ks_d(sorted(samples), sorted(samples))
        self.assertIsNotNone(d)
        self.assertAlmostEqual(d, 0.0, places=9)

    def test_ks_conditional_identical(self):
        """Conditional with KS on identical distributions → D ≈ 0."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            # Build events: counter samples and trigger instants interleaved equally
            events = []
            for i in range(200):
                ts = 1000 + i * 1000
                events.append(_counter("latency", ts, 500))  # all same value
                if i % 2 == 0:
                    events.append(_instant("trigger", ts - 100))
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([
                str(tp),
                "--conditional", "latency BY trigger in last 2ms",
                "--json", str(out_json),
                "--quiet",
            ])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            cond = out["conditional"]
            self.assertTrue(len(cond) > 0)
            d = cond[0]["ks_d"]
            if d is not None:
                self.assertAlmostEqual(d, 0.0, places=6)


# ---------------------------------------------------------------------------
# Test 10: KS D > 0.5 on disjoint samples
# ---------------------------------------------------------------------------


class TestKSDisjoint(unittest.TestCase):
    def test_ks_large_on_disjoint_distributions(self):
        """ks_d([0..100], [200..300]) returns D > 0.5."""
        a = [float(i) for i in range(0, 101)]
        b = [float(i) for i in range(200, 301)]
        d = at.ks_d(sorted(a), sorted(b))
        self.assertIsNotNone(d)
        self.assertGreater(d, 0.5)

    def test_ks_conditional_disjoint(self):
        """Conditional with disjoint distributions reports KS D > 0.5."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = []
            # Without trigger: values 0..100
            for i in range(100):
                ts = 1000 + i * 1000
                events.append(_counter("metric", ts, i))
            # Trigger fires at ts=200000
            events.append(_instant("trig", 200_000))
            # With trigger (within 50ms window): values 200..300
            for i in range(100):
                ts = 200_001 + i * 500
                events.append(_counter("metric", ts, 200 + i))
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([
                str(tp),
                "--conditional", "metric BY trig in last 50ms",
                "--json", str(out_json),
                "--quiet",
            ])
            self.assertEqual(rc, 0)
            out = json.loads(out_json.read_text())
            cond = out["conditional"]
            self.assertTrue(len(cond) > 0)
            d = cond[0]["ks_d"]
            if d is not None:
                self.assertGreater(d, 0.5)


# ---------------------------------------------------------------------------
# Test 11: Regression detection — --strict → exit 7
# ---------------------------------------------------------------------------


class TestRegressionDetection(unittest.TestCase):
    def test_regression_strict_exits_7(self):
        """
        Baseline: render_frame_work_us.p99 contract < 2000 µs.
        Trace p99 = 2500 µs → verdict FAIL → --strict → exit 7.
        """
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            # 100 samples of render_frame_work_us — mostly normal but p99 will be ~2500
            events = []
            for i in range(98):
                events.append(_counter("render_frame_work_us", 1000 + i * 1000, 1800))
            events.append(_counter("render_frame_work_us", 99_000, 2500))
            events.append(_counter("render_frame_work_us", 100_000, 2600))
            _write_trace(tp, events)

            # Build baseline JSON with contract
            baseline_data = {
                "schema_version": "1.0",
                "tool_version": "analyse_trace v1.0",
                "contracts": {
                    "render_frame_work_us.p99": {
                        "max_us": 2000,
                        "regression_pct": 5.0,
                    }
                },
                "counters": {
                    "render_frame_work_us": {
                        "count": 100,
                        "min": 1800.0,
                        "max": 1900.0,
                        "mean": 1820.0,
                        "p50": 1800.0,
                        "p90": 1800.0,
                        "p95": 1800.0,
                        "p99": 1900.0,
                        "p999": 1900.0,
                    }
                },
                "spans": {},
            }
            bp = Path(td) / "baseline.json"
            bp.write_text(json.dumps(baseline_data), encoding="utf-8")

            out_json = Path(td) / "out.json"
            rc = _run([
                str(tp),
                "--baseline", str(bp),
                "--json", str(out_json),
                "--strict",
                "--quiet",
            ])
            self.assertEqual(rc, 7, f"Expected exit 7 (contract FAIL), got {rc}")

            out = json.loads(out_json.read_text())
            self.assertEqual(out["verdict"]["status"], "FAIL")

    def test_regression_without_strict_exits_0(self):
        """Same regression without --strict → exit 0 (still reports FAIL but no strict exit)."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_counter("render_frame_work_us", 1000 + i * 1000, 2500) for i in range(10)]
            _write_trace(tp, events)

            baseline_data = {
                "schema_version": "1.0",
                "tool_version": "analyse_trace v1.0",
                "contracts": {
                    "render_frame_work_us.p99": {"max_us": 2000, "regression_pct": 5.0}
                },
                "counters": {
                    "render_frame_work_us": {
                        "count": 10, "min": 1800.0, "max": 1900.0, "mean": 1850.0,
                        "p50": 1850.0, "p90": 1850.0, "p95": 1850.0,
                        "p99": 1900.0, "p999": 1900.0,
                    }
                },
                "spans": {},
            }
            bp = Path(td) / "baseline.json"
            bp.write_text(json.dumps(baseline_data), encoding="utf-8")

            rc = _run([str(tp), "--baseline", str(bp), "--quiet"])
            self.assertEqual(rc, 0)


# ---------------------------------------------------------------------------
# Test 12: CSV row count = N (counter + span count)
# ---------------------------------------------------------------------------


class TestCSVRowCount(unittest.TestCase):
    def test_summary_csv_row_count(self):
        """summary.csv has one row per distinct counter + span metric (excluding header)."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = []
            # 3 distinct counters
            for i in range(20):
                events.append(_counter("metric_a", 1000 + i * 100, i))
                events.append(_counter("metric_b", 1000 + i * 100, i * 2))
                events.append(_counter("metric_c", 1000 + i * 100, i * 3))
            # 2 distinct spans
            for i in range(15):
                events.append(_span("span_x", 1000 + i * 200, 100 + i))
                events.append(_span("span_y", 1000 + i * 200, 200 + i))
            _write_trace(tp, events)

            csv_dir = Path(td) / "csvs"
            rc = _run([str(tp), "--csv-dir", str(csv_dir), "--quiet"])
            self.assertEqual(rc, 0)

            summary_csv = csv_dir / "summary.csv"
            self.assertTrue(summary_csv.exists(), "summary.csv not created")

            with summary_csv.open(newline="", encoding="utf-8") as f:
                rows = list(csv.reader(f))

            # rows[0] = header, remaining = data
            data_rows = rows[1:]
            # 3 counters + 2 spans = 5 rows
            self.assertEqual(
                len(data_rows), 5,
                f"Expected 5 data rows (3 counters + 2 spans), got {len(data_rows)}: {data_rows}"
            )

    def test_per_metric_csvs_created(self):
        """Individual counter/span CSV files created in csv-dir."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [
                *[_counter("rms", 1000 + i * 100, i) for i in range(10)],
                *[_span("render_frame", 1000 + i * 100, 500 + i) for i in range(10)],
                *[_instant("kick", 1000 + i * 500) for i in range(5)],
            ]
            _write_trace(tp, events)

            csv_dir = Path(td) / "csvs"
            rc = _run([str(tp), "--csv-dir", str(csv_dir), "--quiet"])
            self.assertEqual(rc, 0)

            self.assertTrue((csv_dir / "counter_rms.csv").exists())
            self.assertTrue((csv_dir / "span_render_frame.csv").exists())
            self.assertTrue((csv_dir / "instant_kick.csv").exists())
            self.assertTrue((csv_dir / "regions.csv").exists())


# ---------------------------------------------------------------------------
# Additional edge-case tests (spec items not in numbered matrix)
# ---------------------------------------------------------------------------


class TestBEPairUnmatched(unittest.TestCase):
    """Spec item: Unmatched B/E pair → parse_warnings, not in span stats."""
    def test_unmatched_b_event_in_parse_warnings(self):
        """B event without matching E: recorded in parse_warnings, not counted in spans."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [
                # Normal complete span (X)
                _span("complete_span", 1000, 500),
                # Unmatched B
                {"ph": "B", "name": "orphan_span", "ts": 2000, "pid": 1, "tid": "T"},
                # Unmatched E (different name, also unmatched)
                {"ph": "E", "name": "ghost_span", "ts": 3000, "pid": 1, "tid": "T"},
            ]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            # orphan_span should NOT appear in spans stats
            self.assertNotIn("orphan_span", out["spans"])
            # parse_warnings should mention the unmatched events
            warnings = out["parse_warnings"]
            warning_text = " ".join(warnings)
            self.assertIn("orphan_span", warning_text)
            self.assertIn("ghost_span", warning_text)

    def test_matched_be_pair_counts_as_span(self):
        """Matched B/E pair appears in spans stats with correct duration."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [
                {"ph": "B", "name": "be_span", "ts": 10_000, "pid": 1, "tid": "T"},
                {"ph": "E", "name": "be_span", "ts": 10_500, "pid": 1, "tid": "T"},
            ]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            self.assertIn("be_span", out["spans"])
            self.assertEqual(out["spans"]["be_span"]["count"], 1)
            self.assertEqual(out["spans"]["be_span"]["min"], 500.0)


class TestMissingConditionalMetric(unittest.TestCase):
    """--conditional references metric not in trace → exit 4."""
    def test_missing_conditional_metric_exits_4(self):
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_counter("real_metric", 1000, 5)]
            _write_trace(tp, events)

            import io
            old_stderr = sys.stderr
            sys.stderr = io.StringIO()
            try:
                rc = _run([
                    str(tp),
                    "--conditional", "missing_metric BY any_event in last 16ms",
                    "--quiet",
                ])
            finally:
                err_text = sys.stderr.getvalue()
                sys.stderr = old_stderr

            self.assertEqual(rc, 4)
            self.assertIn("missing_metric", err_text)


class TestRegimeByPrefix(unittest.TestCase):
    """--regime-by custom prefix segmentation."""
    def test_regime_by_custom_prefix(self):
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [
                _instant("phase_init", 0),
                *[_counter("cpu_us", i * 1000, 100 + i) for i in range(10)],
                _instant("phase_main", 10_000),
                *[_counter("cpu_us", 10_000 + i * 1000, 200 + i) for i in range(10)],
                _instant("phase_done", 20_000),
            ]
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([
                str(tp),
                "--regime-by", "phase_",
                "--json", str(out_json),
                "--quiet",
            ])
            self.assertEqual(rc, 0)
            out = json.loads(out_json.read_text())
            labels = [r["label"] for r in out["regions"]]
            # Should have regions labelled by the instant names
            self.assertTrue(len(labels) >= 2)

    def test_regime_by_no_match_exits_6(self):
        """--regime-by prefix matching zero events → exit 6."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_counter("x", 1000, 5)]
            _write_trace(tp, events)

            import io
            old_stderr = sys.stderr
            sys.stderr = io.StringIO()
            try:
                rc = _run([str(tp), "--regime-by", "nonexistent_prefix_", "--quiet"])
            finally:
                sys.stderr = old_stderr

            self.assertEqual(rc, 6)


class TestVerdictFirstLine(unittest.TestCase):
    """Verdict-first output: first line begins with ✓ / ✗ / …"""
    def test_verdict_first_line_no_baseline(self):
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_counter("render_frame_work_us", 1000 + i * 100, 1800) for i in range(10)]
            _write_trace(tp, events)

            rc = _run([str(tp), "--quiet"])
            self.assertEqual(rc, 0)

            md_path = Path(td) / "trace.report.md"
            if md_path.exists():
                first_line = md_path.read_text().splitlines()[0]
                self.assertTrue(
                    first_line.startswith(("✓", "✗", "…")),
                    f"First line does not start with verdict symbol: {first_line!r}"
                )

    def test_verdict_first_line_pass(self):
        """With passing baseline, first line is ✓"""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "trace.json"
            events = [_counter("render_frame_work_us", 1000 + i * 100, 1800) for i in range(100)]
            _write_trace(tp, events)

            baseline_data = {
                "schema_version": "1.0",
                "tool_version": "analyse_trace v1.0",
                "contracts": {
                    "render_frame_work_us.p99": {"max_us": 2000, "regression_pct": 5.0}
                },
                "counters": {
                    "render_frame_work_us": {
                        "count": 100, "min": 1800.0, "max": 1900.0, "mean": 1850.0,
                        "p50": 1800.0, "p90": 1800.0, "p95": 1800.0,
                        "p99": 1900.0, "p999": 1900.0,
                    }
                },
                "spans": {},
            }
            bp = Path(td) / "baseline.json"
            bp.write_text(json.dumps(baseline_data), encoding="utf-8")

            md_path = Path(td) / "trace.report.md"
            rc = _run([str(tp), "--baseline", str(bp), "--quiet"])
            self.assertEqual(rc, 0)

            if md_path.exists():
                first_line = md_path.read_text().splitlines()[0]
                self.assertTrue(
                    first_line.startswith("✓"),
                    f"Expected ✓ on passing verdict, got: {first_line!r}"
                )


class TestSyntheticFixture(unittest.TestCase):
    """Validate against the synthetic 30-second fixture."""
    def test_synthetic_30s_fixture(self):
        """Synthetic 30s trace runs cleanly with exit 0 and expected metrics."""
        with tempfile.TemporaryDirectory() as td:
            tp = Path(td) / "synthetic_30s.json"
            events = _make_synthetic_30s_trace()
            _write_trace(tp, events)

            out_json = Path(td) / "out.json"
            rc = _run([str(tp), "--json", str(out_json), "--quiet"])
            self.assertEqual(rc, 0)

            out = json.loads(out_json.read_text())
            # Should detect the one deadline miss at frame 1800
            dm = out["deadline_misses"]
            self.assertEqual(dm["count"], 1)
            self.assertEqual(dm["events"][0]["dur_us"], 2143)

            # render_frame span should be present
            self.assertIn("render_frame", out["spans"])

            # effect_id should be joined on the miss (0x2100 since miss is at frame 1800 < 1200 threshold? No — 1800 > 1200)
            miss_eff = dm["events"][0]["effect_id"]
            self.assertIsNotNone(miss_eff)
            # Frame 1800 >= 1200 → eff_id should be 0x2101
            self.assertEqual(miss_eff, 0x2101)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


if __name__ == "__main__":
    unittest.main(verbosity=2)
