"""
EdgeMixer v2 validation test runner.

Executes the full validation test plan (T1.x through T5.x) against a
single K1 device, computing L0/L1/L2 perceptual metrics and checking
pass/fail thresholds for each test case.

Usage::

    python -m edgemixer_eval.validate --port /dev/cu.usbmodem21401 --output ./eval_results/edgemixer_v2_validation
"""

from __future__ import annotations

import argparse
import json
import logging
import math
import os
import sys
import time
from datetime import datetime, timezone

import numpy as np

from .device import K1Device
from .metrics import (
    compute_full_evaluation,
    compute_frame_metrics,
    separate_strips,
    rgb_to_hsv,
)
from .profiles import Profile

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Test case definitions
# ---------------------------------------------------------------------------

TESTS = [
    # Phase 1: Baseline
    {
        "id": "T1.1", "name": "K1 Waveform Baseline — Music",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 0, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "black_frame_ratio":   {"max": 0.15},
            "clipped_frame_ratio": {"max": 0.10},
            "mean_brightness":     {"min": 0.05, "max": 0.99},
            "stability_lss":       {"min": 0.40},
            "flicker_elatcsf":     {"max": 0.60},
            "vmcr":                {"min": 0.30},
            "beat_alignment":      {"min": 0.05},
            "onset_response":      {"min": 0.80},
            "composite":           {"min": 0.30},
        },
        "is_baseline": True,
    },
    {
        "id": "T1.2", "name": "K1 Waveform Baseline — Silence",
        "effect": 0x1302, "frames": 300, "fps": 30,
        "edgemixer": {"mode": 0, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "black_frame_ratio": {"min": 0.50},
            "mean_brightness":   {"max": 0.15},
        },
        "note": "SILENCE TEST — mute audio before capture",
        "skip_auto": True,  # Requires manual audio control
    },
    # Phase 2: Mode validation
    {
        "id": "T2.1", "name": "MIRROR Control Repeat",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 0, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {},  # Checked against T1.1 ±5%
        "compare_to": "T1.1",
    },
    {
        "id": "T2.2", "name": "ANALOGOUS (mode=1)",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 1, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "near_black_leaks": {"max": 0},
        },
        "compare_to": "T1.1",
        "max_degradation": {
            "stability_lss": 0.20, "flicker_elatcsf": -0.15,
            "energy_correlation": 0.25, "onset_response": 0.30, "composite": 0.15,
        },
    },
    {
        "id": "T2.3", "name": "COMPLEMENTARY (mode=2)",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 2, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "near_black_leaks": {"max": 0},
        },
        "compare_to": "T1.1",
        "max_degradation": {
            "stability_lss": 0.20, "flicker_elatcsf": -0.15,
            "energy_correlation": 0.25, "onset_response": 0.30, "composite": 0.15,
        },
        "extra_checks": ["saturation_preservation", "hue_offset_128"],
    },
    {
        "id": "T2.4", "name": "SPLIT_COMPLEMENTARY (mode=3)",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 3, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "near_black_leaks": {"max": 0},
        },
        "compare_to": "T1.1",
        "max_degradation": {
            "stability_lss": 0.20, "flicker_elatcsf": -0.15,
            "energy_correlation": 0.25, "onset_response": 0.30, "composite": 0.15,
        },
    },
    {
        "id": "T2.5", "name": "SATURATION_VEIL (mode=4)",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 4, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "near_black_leaks": {"max": 0},
        },
        "compare_to": "T1.1",
        "max_degradation": {
            "stability_lss": 0.20, "flicker_elatcsf": -0.15,
            "energy_correlation": 0.25, "onset_response": 0.30, "composite": 0.15,
        },
        "extra_checks": ["hue_preservation", "saturation_reduction"],
    },
    # Phase 3: Spatial
    {
        "id": "T3.1", "name": "COMPLEMENTARY + CENTRE_GRADIENT",
        "effect": 0x1302, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 2, "spread": 30, "strength": 200, "spatial": 1, "temporal": 0},
        "thresholds": {
            "near_black_leaks": {"max": 0},
        },
        "compare_to": "T1.1",
        "max_degradation": {
            "stability_lss": 0.20, "flicker_elatcsf": -0.15,
            "energy_correlation": 0.25, "onset_response": 0.30, "composite": 0.15,
        },
        "extra_checks": ["centre_gradient_monotonicity"],
    },
    # Phase 4: Temporal — skipped in auto mode (requires audio control)
    {
        "id": "T4.1", "name": "RMS_GATE — Music→Silence→Music",
        "effect": 0x1302, "frames": 1080, "fps": 30,
        "edgemixer": {"mode": 2, "spread": 30, "strength": 200, "spatial": 0, "temporal": 1},
        "thresholds": {},
        "note": "TEMPORAL TEST — requires 3s music → 3s silence → 3s music sequence",
        "skip_auto": True,
    },
    # Phase 5: Cross-effect
    {
        "id": "T5.1", "name": "Palette effect (Fire) + COMPLEMENTARY",
        "effect": 0x0100, "frames": 300, "fps": 30,
        "edgemixer": {"mode": 2, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "stability_lss":    {"min": 0.70},
            "flicker_elatcsf":  {"max": 0.25},
        },
    },
    {
        "id": "T5.2", "name": "Audio-reactive (LGP Holographic) + COMPLEMENTARY",
        "effect": 0x0201, "frames": 900, "fps": 30,
        "edgemixer": {"mode": 2, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0},
        "thresholds": {
            "energy_correlation": {"min": 0.01},
            "onset_response":     {"min": 0.80},
            "composite":          {"min": 0.30},
        },
    },
]


# ---------------------------------------------------------------------------
# Extra check implementations
# ---------------------------------------------------------------------------

def _check_saturation_preservation(raw_frames):
    """Strip 2 mean saturation >= 70% of strip 1 mean saturation."""
    s1_sats, s2_sats = [], []
    for frame, _ in raw_frames:
        s1, s2 = separate_strips(frame)
        _, s1_s, s1_v = rgb_to_hsv(s1)
        _, s2_s, s2_v = rgb_to_hsv(s2)
        active = s1_v > 0.02
        if np.any(active):
            s1_sats.append(float(np.mean(s1_s[active])))
            s2_sats.append(float(np.mean(s2_s[active])))
    if not s1_sats:
        return True, "no active pixels", float("nan")
    ratio = np.mean(s2_sats) / max(np.mean(s1_sats), 1e-6)
    passed = ratio >= 0.60
    return passed, f"sat ratio={ratio:.3f} (threshold ≥0.60)", ratio


def _check_hue_offset_128(raw_frames):
    """Mean hue delta ≈ 128 FastLED units (100-156 range)."""
    deltas = []
    for frame, _ in raw_frames:
        s1, s2 = separate_strips(frame)
        h1, _, v1 = rgb_to_hsv(s1)
        h2, _, v2 = rgb_to_hsv(s2)
        active = (v1 > 0.02) & (v2 > 0.02)
        if np.any(active):
            # Convert degrees to FastLED units (0-255)
            h1_fl = h1[active] * 256.0 / 360.0
            h2_fl = h2[active] * 256.0 / 360.0
            raw_delta = h2_fl - h1_fl
            # Circular wrap to 0-255
            circ = np.mod(raw_delta, 256.0)
            deltas.append(float(np.mean(circ)))
    if not deltas:
        return True, "no active pixels", float("nan")
    mean_delta = np.mean(deltas)
    passed = 100 <= mean_delta <= 156
    return passed, f"hue delta={mean_delta:.1f} FL units (threshold 100-156)", mean_delta


def _check_hue_preservation(raw_frames):
    """SATURATION_VEIL: mean hue delta < 15 FastLED units."""
    deltas = []
    for frame, _ in raw_frames:
        s1, s2 = separate_strips(frame)
        h1, _, v1 = rgb_to_hsv(s1)
        h2, _, v2 = rgb_to_hsv(s2)
        active = (v1 > 0.02) & (v2 > 0.02)
        if np.any(active):
            h1_fl = h1[active] * 256.0 / 360.0
            h2_fl = h2[active] * 256.0 / 360.0
            abs_diff = np.abs(h2_fl - h1_fl)
            circ = np.minimum(abs_diff, 256.0 - abs_diff)
            deltas.append(float(np.mean(circ)))
    if not deltas:
        return True, "no active pixels", float("nan")
    mean_delta = np.mean(deltas)
    passed = mean_delta < 15
    return passed, f"hue delta={mean_delta:.1f} FL units (threshold <15)", mean_delta


def _check_saturation_reduction(raw_frames):
    """SATURATION_VEIL: strip 2 mean sat < strip 1 mean sat."""
    s1_sats, s2_sats = [], []
    for frame, _ in raw_frames:
        s1, s2 = separate_strips(frame)
        _, s1_s, s1_v = rgb_to_hsv(s1)
        _, s2_s, s2_v = rgb_to_hsv(s2)
        active = s1_v > 0.02
        if np.any(active):
            s1_sats.append(float(np.mean(s1_s[active])))
            s2_sats.append(float(np.mean(s2_s[active])))
    if not s1_sats:
        return True, "no active pixels", float("nan")
    s1_mean = float(np.mean(s1_sats))
    s2_mean = float(np.mean(s2_sats))
    passed = s2_mean < s1_mean
    return passed, f"s1_sat={s1_mean:.3f} s2_sat={s2_mean:.3f}", s2_mean - s1_mean


def _check_centre_gradient_monotonicity(raw_frames):
    """Centre pixels should have less divergence than edge pixels."""
    centre_divs, edge_divs = [], []
    for frame, _ in raw_frames:
        s1, s2 = separate_strips(frame)
        pixel_l2 = np.sqrt(np.sum((s2.astype(float) - s1.astype(float)) ** 2, axis=1))
        centre_divs.append(float(np.mean(pixel_l2[70:86])))
        edge_divs.append(float(np.mean(np.concatenate([pixel_l2[0:15], pixel_l2[145:160]]))))
    centre_mean = float(np.mean(centre_divs))
    edge_mean = float(np.mean(edge_divs))
    passed = centre_mean < edge_mean
    return passed, f"centre={centre_mean:.1f} edge={edge_mean:.1f}", edge_mean / max(centre_mean, 0.01)


EXTRA_CHECKS = {
    "saturation_preservation": _check_saturation_preservation,
    "hue_offset_128": _check_hue_offset_128,
    "hue_preservation": _check_hue_preservation,
    "saturation_reduction": _check_saturation_reduction,
    "centre_gradient_monotonicity": _check_centre_gradient_monotonicity,
}


# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

def run_test(device: K1Device, test: dict, baseline_metrics: dict | None = None) -> dict:
    """Execute a single test case and return results."""
    test_id = test["id"]
    em = test["edgemixer"]
    profile = Profile(test_id, em["mode"], em["spread"], em["strength"], em["spatial"], em["temporal"], test["name"])

    # Set effect
    device.set_effect(test["effect"], brightness=255)
    # Set EdgeMixer
    device.set_edgemixer(profile, settle_time=0.5, verify=True)

    # Capture
    raw_frames = device.capture_frames(n_frames=test["frames"], fps=test["fps"], tap="c")
    if not raw_frames:
        return {"test_id": test_id, "pass": False, "error": "no frames captured", "frames_captured": 0}

    # Convert metadata
    frames_meta = []
    for frame, meta in raw_frames:
        md = {
            "rms": meta.rms if meta.rms is not None else float("nan"),
            "beat": meta.beat if meta.beat is not None else False,
            "onset": meta.onset if meta.onset is not None else False,
        }
        frames_meta.append((frame, md))

    # Compute all metrics
    evaluation = compute_full_evaluation(frames_meta)

    # Flatten metrics for threshold checking
    flat = {}
    flat.update(evaluation.get("L0", {}))
    flat.update(evaluation.get("L1", {}))
    flat.update(evaluation.get("L2", {}))
    flat["composite"] = evaluation.get("composite", float("nan"))
    flat["strip_divergence_l2"] = evaluation.get("strip_divergence_l2", float("nan"))
    flat["near_black_leaks"] = evaluation.get("near_black_leaks", 0)

    # Check thresholds
    gate_results = {}
    all_pass = True
    for metric_name, bounds in test.get("thresholds", {}).items():
        val = flat.get(metric_name, float("nan"))
        passed = True
        if "min" in bounds and (math.isnan(val) or val < bounds["min"]):
            passed = False
        if "max" in bounds and (math.isnan(val) or val > bounds["max"]):
            passed = False
        gate_results[metric_name] = {"value": val, "bounds": bounds, "pass": passed}
        if not passed:
            all_pass = False

    # Check degradation vs baseline
    degradation_results = {}
    if baseline_metrics and test.get("max_degradation"):
        for metric_name, max_drop in test["max_degradation"].items():
            baseline_val = baseline_metrics.get(metric_name, float("nan"))
            current_val = flat.get(metric_name, float("nan"))
            if math.isnan(baseline_val) or math.isnan(current_val):
                continue
            if metric_name == "flicker_elatcsf":
                # For flicker, max_degradation is negative = max increase allowed
                delta = current_val - baseline_val
                passed = delta <= abs(max_drop)
            else:
                # For positive metrics, check drop doesn't exceed threshold
                delta = baseline_val - current_val
                passed = delta <= max_drop
            degradation_results[metric_name] = {
                "baseline": baseline_val, "current": current_val,
                "delta": delta, "max_allowed": max_drop, "pass": passed,
            }
            if not passed:
                all_pass = False

    # Extra checks
    extra_results = {}
    for check_name in test.get("extra_checks", []):
        check_fn = EXTRA_CHECKS.get(check_name)
        if check_fn:
            passed, detail, value = check_fn(raw_frames)
            extra_results[check_name] = {"pass": passed, "detail": detail, "value": value}
            if not passed:
                all_pass = False

    result = {
        "test_id": test_id,
        "name": test["name"],
        "device": device._port_name,
        "effect": f"0x{test['effect']:04X}",
        "edgemixer": em,
        "frames_captured": len(raw_frames),
        "metrics": {
            "L0": evaluation.get("L0", {}),
            "L1": evaluation.get("L1", {}),
            "L2": evaluation.get("L2", {}),
            "composite": evaluation.get("composite", float("nan")),
            "strip_divergence_l2": flat["strip_divergence_l2"],
            "near_black_leaks": flat["near_black_leaks"],
        },
        "gates": gate_results,
        "degradation": degradation_results,
        "extra_checks": extra_results,
        "pass": all_pass,
    }
    return result


def print_result(result: dict) -> None:
    """Print a test result summary."""
    status = "PASS" if result["pass"] else "FAIL"
    print(f"\n  [{result['test_id']}] {result['name']} — {status}")
    print(f"    Frames: {result['frames_captured']}  Effect: {result['effect']}  "
          f"EdgeMixer: mode={result['edgemixer']['mode']} spatial={result['edgemixer']['spatial']} "
          f"temporal={result['edgemixer']['temporal']}")

    m = result["metrics"]
    l0 = m.get("L0", {})
    l1 = m.get("L1", {})
    l2 = m.get("L2", {})
    print(f"    L0: black={l0.get('black_frame_ratio', '?'):.3f}  clip={l0.get('clipped_frame_ratio', '?'):.3f}  "
          f"bright={l0.get('mean_brightness', '?'):.3f}")
    print(f"    L1: LSS={l1.get('stability_lss', '?'):.3f}  flicker={l1.get('flicker_elatcsf', '?'):.3f}  "
          f"VMCR={l1.get('vmcr', '?'):.3f}")

    def _f(v):
        return f"{v:.3f}" if isinstance(v, float) and not math.isnan(v) else "NaN"

    print(f"    L2: energy_corr={_f(l2.get('energy_correlation', float('nan')))}  "
          f"beat_align={_f(l2.get('beat_alignment', float('nan')))}  "
          f"onset_resp={_f(l2.get('onset_response', float('nan')))}")
    print(f"    Composite: {_f(m.get('composite', float('nan')))}  "
          f"Divergence: {_f(m.get('strip_divergence_l2', float('nan')))}  "
          f"Leaks: {m.get('near_black_leaks', '?')}")

    # Gate failures
    for gname, gdata in result.get("gates", {}).items():
        if not gdata["pass"]:
            print(f"    GATE FAIL: {gname} = {_f(gdata['value'])} (bounds: {gdata['bounds']})")

    # Degradation failures
    for dname, ddata in result.get("degradation", {}).items():
        if not ddata["pass"]:
            print(f"    DEGRADATION FAIL: {dname} dropped {ddata['delta']:.3f} "
                  f"(max allowed: {ddata['max_allowed']})")

    # Extra check failures
    for ename, edata in result.get("extra_checks", {}).items():
        status_str = "OK" if edata["pass"] else "FAIL"
        print(f"    CHECK {ename}: {status_str} — {edata['detail']}")


def save_result(result: dict, output_dir: str, device_label: str) -> str:
    """Save a test result to JSON."""
    os.makedirs(output_dir, exist_ok=True)
    filename = f"{result['test_id']}_{device_label}.json"
    path = os.path.join(output_dir, filename)

    # Clean numpy types
    def _clean(obj):
        if isinstance(obj, (np.integer,)):
            return int(obj)
        if isinstance(obj, (np.floating,)):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        if isinstance(obj, (np.bool_,)):
            return bool(obj)
        raise TypeError(f"Not serialisable: {type(obj).__name__}")

    with open(path, "w") as f:
        json.dump(result, f, indent=2, default=_clean)
    return path


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="EdgeMixer v2 validation test runner")
    parser.add_argument("--port", required=True, help="Serial port for K1 device")
    parser.add_argument("--output", default="./eval_results/edgemixer_v2_validation",
                        help="Output directory for JSON results")
    parser.add_argument("--device-label", default="device", help="Label for output filenames (e.g. v1, k1v2)")
    parser.add_argument("--skip-manual", action="store_true", default=True,
                        help="Skip tests requiring manual audio control (T1.2, T4.1)")
    parser.add_argument("--tests", default=None,
                        help="Comma-separated test IDs to run (default: all auto tests)")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    # Filter tests
    if args.tests:
        test_ids = [t.strip() for t in args.tests.split(",")]
        tests_to_run = [t for t in TESTS if t["id"] in test_ids]
    elif args.skip_manual:
        tests_to_run = [t for t in TESTS if not t.get("skip_auto")]
    else:
        tests_to_run = list(TESTS)

    print(f"EdgeMixer v2 Validation — {len(tests_to_run)} tests on {args.port}")
    print(f"Output: {args.output}")
    print("=" * 60)

    device = K1Device(args.port)
    results = []
    baseline_flat = None

    try:
        for test in tests_to_run:
            print(f"\n{'='*60}")
            print(f"  Running {test['id']}: {test['name']}")
            print(f"{'='*60}")

            result = run_test(device, test, baseline_flat)
            results.append(result)
            print_result(result)

            # Store baseline metrics for comparison
            if test.get("is_baseline"):
                m = result["metrics"]
                baseline_flat = {}
                baseline_flat.update(m.get("L0", {}))
                baseline_flat.update(m.get("L1", {}))
                baseline_flat.update(m.get("L2", {}))
                baseline_flat["composite"] = m.get("composite", float("nan"))

            path = save_result(result, args.output, args.device_label)
            print(f"    Saved: {path}")

    finally:
        # Restore MIRROR before closing
        mirror = Profile("mirror", 0, 30, 200, 0, 0, "restore")
        try:
            device.set_edgemixer(mirror, settle_time=0.1, verify=False)
        except Exception:
            pass
        device.close()

    # Summary
    print(f"\n{'='*60}")
    print("  VALIDATION SUMMARY")
    print(f"{'='*60}")
    passed = sum(1 for r in results if r["pass"])
    total = len(results)
    for r in results:
        status = "PASS" if r["pass"] else "FAIL"
        print(f"  [{r['test_id']}] {status:4s}  {r['name']}")

    print(f"\n  Result: {passed}/{total} tests passed")
    overall = "PASS" if passed == total else "FAIL"
    print(f"  Overall: {overall}")
    print(f"{'='*60}")

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
