"""
EdgeMixer evaluation report generation and safety gate checks.

Produces plain-text reports suitable for terminal output and JSON export
for downstream analysis tooling. No external formatting dependencies
(no rich, colorama, etc.).
"""

from __future__ import annotations

import json
import math
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

import numpy as np


# ---------------------------------------------------------------------------
# Safety gate evaluation
# ---------------------------------------------------------------------------

def check_safety_gates(run_metrics: dict, profile_name: str) -> dict:
    """Check safety gates for a single profile run.

    Parameters
    ----------
    run_metrics : dict
        Output from ``metrics.compute_run_metrics``.
    profile_name : str
        Name of the profile being evaluated (case-insensitive check
        for MIRROR identity gate).

    Returns
    -------
    dict
        ``profile``: profile name.
        ``gates``: dict of gate results, each with ``value``, ``threshold``,
        and ``pass`` keys.
        ``overall_pass``: True if all gates passed.
    """
    gates: Dict[str, dict] = {}

    from .metrics import SAFETY_GATES

    # Gate 1: Near-black leaks below threshold (not zero — baseline strips
    # are already differentiated by the effect renderer before EdgeMixer).
    nb_leaks = run_metrics.get("near_black_leaks_mean", float("nan"))
    nb_threshold = SAFETY_GATES["near_black_leaks"]
    gates["near_black_leaks"] = {
        "value": nb_leaks,
        "threshold": nb_threshold,
        "pass": (not math.isnan(nb_leaks)) and nb_leaks <= nb_threshold,
    }

    # Gate 2: Brightness preservation — ratio must be within bounds.
    br = run_metrics.get("brightness_ratio_mean", float("nan"))
    br_min = SAFETY_GATES["brightness_ratio_min"]
    br_max = SAFETY_GATES["brightness_ratio_max"]
    br_pass = (not math.isnan(br)) and br_min <= br <= br_max
    gates["brightness_preservation"] = {
        "value": br,
        "threshold": f"{br_min} - {br_max}",
        "pass": br_pass,
    }

    # Gate 3: Mirror identity — only for MIRROR profile (mode 0).
    if profile_name.lower() == "mirror":
        div = run_metrics.get("strip_divergence_l2_mean", float("nan"))
        mirror_threshold = SAFETY_GATES["max_frame_divergence_mirror"]
        gates["mirror_identity"] = {
            "value": div,
            "threshold": mirror_threshold,
            "pass": (not math.isnan(div)) and div < mirror_threshold,
        }

    overall = all(g["pass"] for g in gates.values())

    return {
        "profile": profile_name,
        "gates": gates,
        "overall_pass": overall,
    }


# ---------------------------------------------------------------------------
# Text formatting helpers
# ---------------------------------------------------------------------------

def _fmt(val: Any, decimals: int = 4) -> str:
    """Format a numeric value for display, handling NaN gracefully."""
    if val is None:
        return "N/A"
    if isinstance(val, float) and math.isnan(val):
        return "NaN"
    if isinstance(val, float):
        return f"{val:.{decimals}f}"
    return str(val)


def _sign(val: float) -> str:
    """Return a value with explicit +/- sign."""
    if math.isnan(val):
        return "NaN"
    if val >= 0:
        return f"+{val:.4f}"
    return f"{val:.4f}"


def _gate_line(name: str, gate: dict) -> str:
    """Format a single gate result line."""
    status = "PASS" if gate["pass"] else "FAIL"
    return f"    {name:30s}  {_fmt(gate['value']):>10s}  threshold={gate['threshold']}  [{status}]"


# ---------------------------------------------------------------------------
# Run summary
# ---------------------------------------------------------------------------

def format_run_summary(
    run_metrics: dict,
    profile: "Profile",  # noqa: F821 — forward ref avoids circular import
    safety_result: dict,
) -> str:
    """Format a single evaluation run into a readable text block.

    Parameters
    ----------
    run_metrics : dict
        Output from ``metrics.compute_run_metrics``.
    profile : Profile
        The profile used for this run.
    safety_result : dict
        Output from ``check_safety_gates``.

    Returns
    -------
    str
        Multi-line plain-text report block.
    """
    lines: list[str] = []
    sep = "=" * 72

    lines.append(sep)
    lines.append(f"  Profile: {profile.name}")
    lines.append(f"  Description: {profile.description}")
    lines.append(f"  Settings: mode={profile.mode}  spread={profile.spread}  "
                 f"strength={profile.strength}  spatial={profile.spatial}  "
                 f"temporal={profile.temporal}")
    lines.append(sep)

    # Key metrics
    lines.append("")
    lines.append("  Metrics")
    lines.append("  " + "-" * 40)
    lines.append(f"    Strip divergence (L2 mean):  {_fmt(run_metrics.get('strip_divergence_l2_mean'))}")
    lines.append(f"    Strip divergence (L2 p95):   {_fmt(run_metrics.get('strip_divergence_l2_p95'))}")
    lines.append(f"    Hue shift mean:              {_fmt(run_metrics.get('hue_shift_mean_mean'))}")
    lines.append(f"    Hue shift std:               {_fmt(run_metrics.get('hue_shift_std_mean'))}")
    lines.append(f"    Saturation delta mean:       {_fmt(run_metrics.get('saturation_delta_mean_mean'))}")
    lines.append(f"    Brightness ratio mean:       {_fmt(run_metrics.get('brightness_ratio_mean'))}")
    lines.append(f"    Active pixels mean:          {_fmt(run_metrics.get('active_pixel_count_mean'), 1)}")
    lines.append(f"    Near-black leaks (total):    {run_metrics.get('near_black_total_leaks', 'N/A')}")

    # Spatial gradient
    sge = run_metrics.get("spatial_gradient_effectiveness_mean", float("nan"))
    lines.append(f"    Spatial gradient effect.:     {_fmt(sge)}")

    # Temporal responsiveness (if applicable)
    tr = run_metrics.get("temporal_responsiveness", float("nan"))
    if not math.isnan(tr):
        lines.append(f"    Temporal responsiveness:      {_fmt(tr)}")

    # Frame count
    lines.append("")
    lines.append(f"    Frames captured:             {run_metrics.get('n_frames', 0)}")
    lines.append(f"    Beat events:                 {run_metrics.get('n_beats', 0)}")

    # Safety gates
    lines.append("")
    overall = "PASS" if safety_result["overall_pass"] else "FAIL"
    lines.append(f"  Safety Gates [{overall}]")
    lines.append("  " + "-" * 40)
    for gate_name, gate_data in safety_result["gates"].items():
        lines.append(_gate_line(gate_name, gate_data))

    lines.append(sep)
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Baseline vs candidate comparison
# ---------------------------------------------------------------------------

def format_comparison(
    baseline_metrics: dict,
    candidate_metrics: dict,
    delta: dict,
    baseline_profile: "Profile",  # noqa: F821
    candidate_profile: "Profile",  # noqa: F821
    baseline_safety: dict,
    candidate_safety: dict,
) -> str:
    """Format a baseline vs candidate comparison report.

    Parameters
    ----------
    baseline_metrics, candidate_metrics : dict
        Output from ``metrics.compute_run_metrics``.
    delta : dict
        Output from ``metrics.compute_delta``.
    baseline_profile, candidate_profile : Profile
        Profile objects for each side.
    baseline_safety, candidate_safety : dict
        Output from ``check_safety_gates``.

    Returns
    -------
    str
        Multi-line plain-text comparison report.
    """
    lines: list[str] = []
    sep = "=" * 80

    lines.append(sep)
    lines.append(f"  COMPARISON: {baseline_profile.name} (baseline)  vs  {candidate_profile.name} (candidate)")
    lines.append(sep)

    # Table header
    hdr = f"  {'Metric':36s} {'Baseline':>10s} {'Candidate':>10s} {'Delta':>10s}"
    lines.append("")
    lines.append(hdr)
    lines.append("  " + "-" * 70)

    # Metric rows
    rows = [
        ("Strip divergence L2 mean",
         baseline_metrics.get("strip_divergence_l2_mean"),
         candidate_metrics.get("strip_divergence_l2_mean"),
         delta.get("delta_divergence")),
        ("Hue shift mean",
         baseline_metrics.get("hue_shift_mean_mean"),
         candidate_metrics.get("hue_shift_mean_mean"),
         delta.get("delta_hue_shift")),
        ("Saturation delta mean",
         baseline_metrics.get("saturation_delta_mean_mean"),
         candidate_metrics.get("saturation_delta_mean_mean"),
         delta.get("delta_saturation")),
        ("Brightness ratio mean",
         baseline_metrics.get("brightness_ratio_mean"),
         candidate_metrics.get("brightness_ratio_mean"),
         delta.get("delta_brightness_ratio")),
        ("Near-black leaks (total)",
         baseline_metrics.get("near_black_total_leaks"),
         candidate_metrics.get("near_black_total_leaks"),
         delta.get("delta_near_black_leaks")),
        ("Temporal responsiveness",
         baseline_metrics.get("temporal_responsiveness"),
         candidate_metrics.get("temporal_responsiveness"),
         delta.get("delta_temporal_responsiveness")),
    ]

    for label, bval, cval, dval in rows:
        b_str = _fmt(bval) if bval is not None else "N/A"
        c_str = _fmt(cval) if cval is not None else "N/A"
        d_str = _sign(float(dval)) if dval is not None else "N/A"
        lines.append(f"  {label:36s} {b_str:>10s} {c_str:>10s} {d_str:>10s}")

    # Frame counts
    lines.append("")
    lines.append(f"  Frames:  baseline={delta.get('baseline_n_frames', '?')}  "
                 f"candidate={delta.get('candidate_n_frames', '?')}")

    # Safety results
    lines.append("")
    b_overall = "PASS" if baseline_safety["overall_pass"] else "FAIL"
    c_overall = "PASS" if candidate_safety["overall_pass"] else "FAIL"
    lines.append(f"  Safety:  baseline=[{b_overall}]  candidate=[{c_overall}]")

    for side_label, safety in [("Baseline", baseline_safety), ("Candidate", candidate_safety)]:
        for gname, gdata in safety["gates"].items():
            if not gdata["pass"]:
                lines.append(f"    {side_label} FAIL: {gname}  "
                             f"value={_fmt(gdata['value'])}  "
                             f"threshold={gdata['threshold']}")

    # Interpretation hints
    lines.append("")
    lines.append("  Interpretation:")
    lines.append("    - Higher divergence = more visible EdgeMixer effect")
    lines.append("    - Brightness ratio near 1.0 = good energy preservation")
    lines.append("    - Temporal responsiveness > 0.5 = strong audio correlation")
    lines.append("    - Near-black leaks must be 0 (no colour injected into dark pixels)")

    lines.append(sep)
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Multi-profile shootout
# ---------------------------------------------------------------------------

def format_shootout(results: List[dict]) -> str:
    """Format a multi-profile shootout comparison table.

    Parameters
    ----------
    results : list of dict
        Each dict must have keys: ``profile`` (Profile object), ``run_metrics``
        (dict), ``safety`` (dict from ``check_safety_gates``).

    Returns
    -------
    str
        Multi-line plain-text ranked comparison table.
    """
    if not results:
        return "(No results to display.)"

    lines: list[str] = []
    sep = "=" * 100

    lines.append(sep)
    lines.append("  SHOOTOUT — EdgeMixer Profile Comparison")
    lines.append(sep)

    # Sort by strip divergence L2 mean (descending — most visible effect first).
    sorted_results = sorted(
        results,
        key=lambda r: r["run_metrics"].get("strip_divergence_l2_mean", 0.0),
        reverse=True,
    )

    # Table header
    col_hdr = (
        f"  {'#':>2s}  {'Profile':20s}  {'Divergence':>10s}  {'Hue Shift':>10s}  "
        f"{'Sat Delta':>10s}  {'Bright.R':>10s}  {'Active Px':>10s}  {'Safety':>6s}"
    )
    lines.append("")
    lines.append(col_hdr)
    lines.append("  " + "-" * 94)

    for rank, entry in enumerate(sorted_results, 1):
        rm = entry["run_metrics"]
        prof = entry["profile"]
        safety = entry["safety"]
        status = "PASS" if safety["overall_pass"] else "FAIL"

        lines.append(
            f"  {rank:>2d}  {prof.name:20s}  "
            f"{_fmt(rm.get('strip_divergence_l2_mean')):>10s}  "
            f"{_fmt(rm.get('hue_shift_mean_mean')):>10s}  "
            f"{_fmt(rm.get('saturation_delta_mean_mean')):>10s}  "
            f"{_fmt(rm.get('brightness_ratio_mean')):>10s}  "
            f"{_fmt(rm.get('active_pixel_count_mean'), 1):>10s}  "
            f"{status:>6s}"
        )

    # Summary footer
    n_pass = sum(1 for r in sorted_results if r["safety"]["overall_pass"])
    n_total = len(sorted_results)
    lines.append("")
    lines.append(f"  Safety: {n_pass}/{n_total} profiles passed all gates.")

    # Flag any failures
    failures = [r for r in sorted_results if not r["safety"]["overall_pass"]]
    if failures:
        lines.append("")
        lines.append("  Gate failures:")
        for entry in failures:
            prof = entry["profile"]
            for gname, gdata in entry["safety"]["gates"].items():
                if not gdata["pass"]:
                    lines.append(f"    {prof.name}: {gname}  "
                                 f"value={_fmt(gdata['value'])}  "
                                 f"threshold={gdata['threshold']}")

    lines.append(sep)
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# JSON export
# ---------------------------------------------------------------------------

def _numpy_serialiser(obj: Any) -> Any:
    """Convert numpy types to native Python for JSON serialisation."""
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.floating,)):
        return float(obj)
    if isinstance(obj, np.ndarray):
        return obj.tolist()
    if isinstance(obj, (np.bool_,)):
        return bool(obj)
    raise TypeError(f"Object of type {type(obj).__name__} is not JSON serialisable")


def export_json(results: List[dict], output_path: str) -> None:
    """Export evaluation results to a JSON file.

    Handles numpy types automatically (converts to Python float/int).

    Parameters
    ----------
    results : list of dict
        Each dict should contain ``profile``, ``run_metrics``, ``safety``,
        and optionally ``metadata``.
    output_path : str
        File path for the JSON output.
    """
    # Build a serialisable copy.
    export_data: list[dict] = []
    for entry in results:
        record: dict = {}

        # Profile — convert Profile dataclass to plain dict.
        prof = entry.get("profile")
        if prof is not None:
            record["profile"] = {
                "name": prof.name,
                "description": prof.description,
                **prof.as_dict(),
            }

        # Run metrics — strip numpy arrays, keep summary stats.
        rm = entry.get("run_metrics", {})
        clean_metrics: dict = {}
        for key, val in rm.items():
            if isinstance(val, np.ndarray):
                # Skip per-frame arrays in export; keep summary stats only.
                continue
            elif isinstance(val, (np.integer,)):
                clean_metrics[key] = int(val)
            elif isinstance(val, (np.floating,)):
                clean_metrics[key] = float(val)
            elif isinstance(val, (np.bool_,)):
                clean_metrics[key] = bool(val)
            else:
                clean_metrics[key] = val
        record["run_metrics"] = clean_metrics

        # Safety gates
        safety = entry.get("safety")
        if safety is not None:
            record["safety"] = safety

        # Delta (if present, e.g. from comparison runs)
        delta = entry.get("delta")
        if delta is not None:
            record["delta"] = delta

        # Metadata (if present)
        meta = entry.get("metadata")
        if meta is not None:
            record["metadata"] = meta

        export_data.append(record)

    payload = {
        "tool": "edgemixer_eval",
        "version": 1,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "results": export_data,
    }

    with open(output_path, "w") as f:
        json.dump(payload, f, indent=2, default=_numpy_serialiser)
