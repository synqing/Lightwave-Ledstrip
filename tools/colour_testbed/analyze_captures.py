#!/usr/bin/env python3
"""
Comprehensive Capture Data Analysis

Indexes baseline vs candidate datasets, runs LGP-first metrics, generates visualizations,
and produces a ranked regression report with next-stage recommendations.
"""

import os
import sys
import struct
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from frame_parser import Frame, parse_serial_dump, get_frame_statistics
from analyse import (
    compare_baseline_candidate, AnalysisResults,
    calculate_edge_amplitude_metrics, calculate_temporal_metrics,
    calculate_luma, calculate_per_pixel_metrics, calculate_transfer_curve
)
try:
    from visualize import (
        plot_side_by_side, plot_difference_heatmap,
        plot_time_series, plot_transfer_curves
    )
    VISUALIZATION_AVAILABLE = True
except ImportError:
    VISUALIZATION_AVAILABLE = False
    print("Warning: Visualization module not available. Skipping visual artifact generation.")

# ============================================================================
# Data Structures
# ============================================================================

@dataclass
class DatasetIndex:
    """Index of capture files by effect and tap"""
    baseline_dir: str
    candidate_dir: str
    file_map: Dict[Tuple[int, int], Tuple[str, str]]  # (effect_id, tap_id) -> (baseline_file, candidate_file)
    missing_pairs: List[Tuple[int, int]]
    invalid_files: List[str]

@dataclass
class LGPRegressionScore:
    """LGP-first regression score for an effect/tap combination"""
    effect_id: int
    tap_id: int
    tap_name: str
    
    # LGP-critical metrics
    edge_ratio_drift: float  # |R_candidate - R_baseline| / R_baseline
    low_value_crush_score: float  # Fraction of low values (1-50) crushed to near-zero
    symmetry_deviation: float  # Change in left/right correlation
    
    # Secondary metrics
    l1_delta: float
    psnr: float
    
    # Overall LGP risk score (weighted combination)
    lgp_risk_score: float
    
    # Root cause attribution
    tap_a_match: bool  # Does Tap A match baseline?
    tap_b_divergence: float  # How much does Tap B diverge?
    tap_c_divergence: float  # How much does Tap C diverge?

# ============================================================================
# Phase 1: Data Indexing & Validation
# ============================================================================

def validate_frame_file(filepath: str) -> Tuple[bool, Optional[Dict]]:
    """
    Validate a capture file: size, header format, magic byte.
    
    Returns:
        (is_valid, header_dict) or (False, None)
    """
    if not os.path.exists(filepath):
        return False, None
    
    size = os.path.getsize(filepath)
    if size != 977:
        return False, {"error": f"Wrong size: {size} bytes (expected 977)"}
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    if len(data) < 17:
        return False, {"error": "File too short for header"}
    
    # Check magic byte
    if data[0] != 0xFD:
        return False, {"error": f"Wrong magic byte: 0x{data[0]:02X} (expected 0xFD)"}
    
    # Parse header
    try:
        header = struct.unpack('<BBBBBBBIIH', data[0:17])
        magic, version, tap_id, effect_id, palette_id, brightness, speed = header[0:7]
        frame_index = header[7]
        timestamp_us = header[8]
        frame_len = header[9]
        
        if frame_len != 960:
            return False, {"error": f"Wrong frame_len in header: {frame_len} (expected 960)"}
        
        return True, {
            "magic": magic,
            "version": version,
            "tap_id": tap_id,
            "effect_id": effect_id,
            "palette_id": palette_id,
            "brightness": brightness,
            "speed": speed,
            "frame_index": frame_index,
            "timestamp_us": timestamp_us,
            "frame_len": frame_len
        }
    except struct.error as e:
        return False, {"error": f"Header parse error: {e}"}

def find_baseline_directory() -> Optional[str]:
    """Try to locate baseline capture directory."""
    possible_paths = [
        "captures/baseline",
        "captures/baseline_gamut",
        "captures/baseline_9734cca",
        "captures/baseline_commit",
    ]
    for path in possible_paths:
        if os.path.exists(path) and os.path.isdir(path):
            return path
    return None

def index_datasets(baseline_dir: str, candidate_dir: str) -> DatasetIndex:
    """
    Index baseline and candidate capture datasets.
    
    Args:
        baseline_dir: Path to baseline captures directory (or None to auto-detect)
        candidate_dir: Path to candidate captures directory
        
    Returns:
        DatasetIndex with file map and validation results
    """
    # Auto-detect baseline if not provided
    if not baseline_dir or not os.path.exists(baseline_dir):
        baseline_dir = find_baseline_directory()
        if baseline_dir:
            print(f"Auto-detected baseline directory: {baseline_dir}")
        else:
            print("Warning: No baseline directory found. Will only analyze candidate data.")
    
    file_map = {}
    missing_pairs = []
    invalid_files = []
    
    # Expected effect IDs from our capture run
    expected_effects = [3, 0, 1, 2, 4, 6, 8, 10, 13, 16, 26, 32, 39, 40, 58, 65, 66, 67]
    tap_names = {
        0: "pre_correction",
        1: "post_correction",
        2: "pre_ws2812"
    }
    
    for effect_id in expected_effects:
        for tap_id, tap_name in tap_names.items():
            # Construct expected filenames
            if baseline_dir and os.path.exists(baseline_dir):
                baseline_file = os.path.join(baseline_dir, f"effect_{effect_id}", 
                                            f"effect_{effect_id}_tap_{tap_name}.bin")
            else:
                baseline_file = None
            candidate_file = os.path.join(candidate_dir, f"effect_{effect_id}",
                                         f"effect_{effect_id}_tap_{tap_name}.bin")
            
            # Validate candidate file (required)
            candidate_valid, candidate_info = validate_frame_file(candidate_file)
            if not candidate_valid:
                missing_pairs.append((effect_id, tap_id))
                if candidate_info and "error" in candidate_info:
                    invalid_files.append(f"{candidate_file}: {candidate_info['error']}")
                continue
            
            # Validate baseline file (optional if baseline_dir not found)
            if baseline_file and os.path.exists(baseline_file):
                baseline_valid, baseline_info = validate_frame_file(baseline_file)
                if not baseline_valid:
                    missing_pairs.append((effect_id, tap_id))
                    if baseline_info and "error" in baseline_info:
                        invalid_files.append(f"{baseline_file}: {baseline_info['error']}")
                    continue
                
                # Verify tap_id matches (effect_id may differ if capture was done with wrong effect)
                if baseline_info["tap_id"] != tap_id:
                    invalid_files.append(f"{baseline_file}: tap_id mismatch (expected {tap_id}, got {baseline_info['tap_id']})")
                    continue
                
                # Warn if effect_id doesn't match filename (but don't fail - header is authoritative)
                if baseline_info["effect_id"] != effect_id:
                    print(f"  Warning: {baseline_file} has effect_id={baseline_info['effect_id']} in header, but filename suggests {effect_id}")
            else:
                baseline_file = None
                baseline_info = None
            
            # Verify candidate tap_id matches
            if candidate_info["tap_id"] != tap_id:
                invalid_files.append(f"{candidate_file}: tap_id mismatch (expected {tap_id}, got {candidate_info['tap_id']})")
                continue
            
            # Warn if effect_id doesn't match filename (but don't fail - header is authoritative)
            if candidate_info["effect_id"] != effect_id:
                print(f"  Warning: {candidate_file} has effect_id={candidate_info['effect_id']} in header, but filename suggests {effect_id}")
            
            # Use header effect_id as authoritative
            actual_effect_id = candidate_info["effect_id"]
            
            file_map[(actual_effect_id, tap_id)] = (baseline_file, candidate_file)
    
    return DatasetIndex(
        baseline_dir=baseline_dir,
        candidate_dir=candidate_dir,
        file_map=file_map,
        missing_pairs=missing_pairs,
        invalid_files=invalid_files
    )

# ============================================================================
# Phase 2: LGP-First Metrics
# ============================================================================

def calculate_lgp_metrics(baseline_frame: Frame, candidate_frame: Frame) -> Dict:
    """
    Calculate LGP-first metrics for a single frame pair.
    
    Returns:
        Dictionary with LGP-critical metrics
    """
    from analyse import calculate_luma
    
    # Edge amplitude metrics
    baseline_luma1 = calculate_luma(baseline_frame.strip1)
    baseline_luma2 = calculate_luma(baseline_frame.strip2)
    candidate_luma1 = calculate_luma(candidate_frame.strip1)
    candidate_luma2 = calculate_luma(candidate_frame.strip2)
    
    baseline_i1 = float(baseline_luma1.mean())
    baseline_i2 = float(baseline_luma2.mean())
    candidate_i1 = float(candidate_luma1.mean())
    candidate_i2 = float(candidate_luma2.mean())
    
    # Edge ratio
    baseline_ratio = baseline_i1 / baseline_i2 if baseline_i2 > 0 else 1.0
    candidate_ratio = candidate_i1 / candidate_i2 if candidate_i2 > 0 else 1.0
    ratio_drift = abs(candidate_ratio - baseline_ratio) / baseline_ratio if baseline_ratio > 0 else 0.0
    
    # Low-value preservation
    baseline_low = baseline_frame.rgb_data[(baseline_frame.rgb_data >= 1) & (baseline_frame.rgb_data <= 50)]
    candidate_low = candidate_frame.rgb_data[(baseline_frame.rgb_data >= 1) & (baseline_frame.rgb_data <= 50)]
    
    crush_score = 0.0
    if len(baseline_low) > 0:
        # Count how many baseline low values became near-zero (< 5) in candidate
        baseline_low_mask = (baseline_frame.rgb_data >= 1) & (baseline_frame.rgb_data <= 50)
        candidate_crushed = (candidate_frame.rgb_data[baseline_low_mask] < 5).sum()
        crush_score = float(candidate_crushed) / len(baseline_low)
    
    # Symmetry (left/right correlation on each strip)
    def strip_symmetry(strip_rgb):
        left_half = strip_rgb[0:80]
        right_half = strip_rgb[80:160]
        left_luma = calculate_luma(left_half)
        right_luma = calculate_luma(right_half)
        if left_luma.std() > 0 and right_luma.std() > 0:
            import numpy as np
            return float(np.corrcoef(left_luma, right_luma)[0, 1])
        return 0.0
    
    baseline_sym1 = strip_symmetry(baseline_frame.strip1)
    baseline_sym2 = strip_symmetry(baseline_frame.strip2)
    candidate_sym1 = strip_symmetry(candidate_frame.strip1)
    candidate_sym2 = strip_symmetry(candidate_frame.strip2)
    
    baseline_sym = (baseline_sym1 + baseline_sym2) / 2.0
    candidate_sym = (candidate_sym1 + candidate_sym2) / 2.0
    symmetry_deviation = abs(candidate_sym - baseline_sym)
    
    # Centre vs edge structure
    centre_indices = list(range(70, 90))  # Around LEDs 79/80
    edge_indices = list(range(0, 40)) + list(range(120, 160))  # Outer quartiles
    
    baseline_centre_luma = calculate_luma(baseline_frame.rgb_data[centre_indices])
    baseline_edge_luma = calculate_luma(baseline_frame.rgb_data[edge_indices])
    candidate_centre_luma = calculate_luma(candidate_frame.rgb_data[centre_indices])
    candidate_edge_luma = calculate_luma(candidate_frame.rgb_data[edge_indices])
    
    centre_ratio_baseline = float(baseline_centre_luma.mean()) / baseline_edge_luma.mean() if baseline_edge_luma.mean() > 0 else 1.0
    centre_ratio_candidate = float(candidate_centre_luma.mean()) / candidate_edge_luma.mean() if candidate_edge_luma.mean() > 0 else 1.0
    centre_structure_drift = abs(centre_ratio_candidate - centre_ratio_baseline) / centre_ratio_baseline if centre_ratio_baseline > 0 else 0.0
    
    return {
        "edge_i1_baseline": baseline_i1,
        "edge_i2_baseline": baseline_i2,
        "edge_ratio_baseline": baseline_ratio,
        "edge_i1_candidate": candidate_i1,
        "edge_i2_candidate": candidate_i2,
        "edge_ratio_candidate": candidate_ratio,
        "edge_ratio_drift": ratio_drift,
        "low_value_crush_score": crush_score,
        "symmetry_baseline": baseline_sym,
        "symmetry_candidate": candidate_sym,
        "symmetry_deviation": symmetry_deviation,
        "centre_structure_drift": centre_structure_drift
    }

def calculate_lgp_risk_score(lgp_metrics: Dict) -> float:
    """
    Calculate overall LGP risk score from metrics.
    
    Weights:
    - Edge ratio drift: 0.4 (critical for interference)
    - Low-value crush: 0.4 (critical for trails/gradients)
    - Symmetry deviation: 0.1
    - Centre structure drift: 0.1
    """
    ratio_drift = lgp_metrics.get("edge_ratio_drift", 0.0)
    crush_score = lgp_metrics.get("low_value_crush_score", 0.0)
    sym_dev = lgp_metrics.get("symmetry_deviation", 0.0)
    centre_drift = lgp_metrics.get("centre_structure_drift", 0.0)
    
    # Normalize to 0-1 range (clamp at 1.0)
    ratio_drift_norm = min(ratio_drift, 1.0)
    crush_norm = min(crush_score, 1.0)
    sym_norm = min(sym_dev, 1.0)
    centre_norm = min(centre_drift, 1.0)
    
    risk = (0.4 * ratio_drift_norm + 
            0.4 * crush_norm + 
            0.1 * sym_norm + 
            0.1 * centre_norm)
    
    return risk

# ============================================================================
# Phase 3: Analysis Runner
# ============================================================================

def analyze_all_pairs(index: DatasetIndex, output_dir: str) -> List[LGPRegressionScore]:
    """
    Analyze all baseline/candidate pairs and compute LGP regression scores.
    """
    os.makedirs(output_dir, exist_ok=True)
    
    scores = []
    
    # Get Tap A results for comparison
    tap_a_results = {}
    for (effect_id, tap_id), (baseline_file, candidate_file) in index.file_map.items():
        if tap_id == 0:  # Tap A
            if baseline_file:
                baseline_frames = parse_serial_dump(baseline_file)
            else:
                baseline_frames = None
            candidate_frames = parse_serial_dump(candidate_file)
            if baseline_frames and candidate_frames:
                try:
                    tap_a_results[effect_id] = compare_baseline_candidate(
                        baseline_frames, candidate_frames, tap_id
                    )
                except Exception as e:
                    print(f"  Warning: Could not compare Tap A for effect {effect_id}: {e}")
    
    # Analyze each pair
    for (effect_id, tap_id), (baseline_file, candidate_file) in sorted(index.file_map.items()):
        tap_names = {0: "pre_correction", 1: "post_correction", 2: "pre_ws2812"}
        tap_name = tap_names.get(tap_id, f"tap_{tap_id}")
        
        print(f"Analyzing effect {effect_id}, {tap_name}...")
        
        # Parse frames
        if baseline_file:
            baseline_frames = parse_serial_dump(baseline_file)
        else:
            baseline_frames = None
        candidate_frames = parse_serial_dump(candidate_file)
        
        if not candidate_frames:
            print(f"  Warning: Could not parse candidate frames for effect {effect_id}, tap {tap_id}")
            continue
        
        if not baseline_frames:
            print(f"  Warning: No baseline data for effect {effect_id}, tap {tap_id} - skipping comparison")
            continue
        
        # Standard analysis (handle single-frame case)
        try:
            analysis = compare_baseline_candidate(baseline_frames, candidate_frames, tap_id)
        except ValueError as e:
            if "temporal" in str(e).lower() or "Need at least 2 frames" in str(e):
                # Single frame - create minimal analysis with dummy temporal
                from analyse import PerPixelMetrics, EdgeAmplitudeMetrics, TemporalMetrics, ChannelTransferCurve, AnalysisResults
                per_pixel = calculate_per_pixel_metrics(baseline_frames[0], candidate_frames[0])
                baseline_edge = calculate_edge_amplitude_metrics(baseline_frames)
                candidate_edge = calculate_edge_amplitude_metrics(candidate_frames)
                ratio_drift = abs(candidate_edge.i1_i2_ratio - baseline_edge.i1_i2_ratio) / baseline_edge.i1_i2_ratio if baseline_edge.i1_i2_ratio > 0 else 0.0
                edge_amplitude = EdgeAmplitudeMetrics(
                    i1_mean=candidate_edge.i1_mean,
                    i2_mean=candidate_edge.i2_mean,
                    i1_i2_ratio=candidate_edge.i1_i2_ratio,
                    ratio_stability=ratio_drift,
                    spatial_symmetry=candidate_edge.spatial_symmetry
                )
                temporal = TemporalMetrics(0.0, 0.0, 0.0, 0.0)
                transfer_curves = {}
                for channel in ['r', 'g', 'b']:
                    transfer_curves[channel] = calculate_transfer_curve(baseline_frames, candidate_frames, channel)
                analysis = AnalysisResults(
                    per_pixel=per_pixel,
                    edge_amplitude=edge_amplitude,
                    temporal=temporal,
                    transfer_curves=transfer_curves,
                    passed=True,
                    failures=[]
                )
            else:
                # For normal path, ensure transfer_curves exists
                if not hasattr(analysis, 'transfer_curves') or not analysis.transfer_curves:
                    analysis.transfer_curves = {}
                    for channel in ['r', 'g', 'b']:
                        analysis.transfer_curves[channel] = calculate_transfer_curve(baseline_frames, candidate_frames, channel)
            else:
                raise
        
        # LGP-specific metrics
        lgp_metrics = calculate_lgp_metrics(baseline_frames[0], candidate_frames[0])
        lgp_risk = calculate_lgp_risk_score(lgp_metrics)
        
        # Tap A comparison (for root cause attribution)
        tap_a_match = True
        if effect_id in tap_a_results:
            tap_a_analysis = tap_a_results[effect_id]
            tap_a_match = tap_a_analysis.per_pixel.l1_delta < 100  # Threshold for "match"
        
        # Build score
        score = LGPRegressionScore(
            effect_id=effect_id,
            tap_id=tap_id,
            tap_name=tap_name,
            edge_ratio_drift=lgp_metrics["edge_ratio_drift"],
            low_value_crush_score=lgp_metrics["low_value_crush_score"],
            symmetry_deviation=lgp_metrics["symmetry_deviation"],
            l1_delta=analysis.per_pixel.l1_delta,
            psnr=analysis.per_pixel.psnr,
            lgp_risk_score=lgp_risk,
            tap_a_match=tap_a_match,
            tap_b_divergence=lgp_risk if tap_id == 1 else 0.0,
            tap_c_divergence=lgp_risk if tap_id == 2 else 0.0
        )
        
        scores.append(score)
        
        # Generate visual artifacts for top regressions (if visualization available)
        if VISUALIZATION_AVAILABLE and lgp_risk > 0.1:  # Threshold for "significant" regression
            effect_output_dir = os.path.join(output_dir, f"effect_{effect_id}")
            os.makedirs(effect_output_dir, exist_ok=True)
            
            try:
                # Side-by-side comparison
                plot_side_by_side(
                    baseline_frames[0],
                    candidate_frames[0],
                    output_path=os.path.join(effect_output_dir, f"{tap_name}_side_by_side.png"),
                    title=f"Effect {effect_id} - {tap_name}"
                )
                
                # Difference heatmap
                plot_difference_heatmap(
                    baseline_frames[0],
                    candidate_frames[0],
                    output_path=os.path.join(effect_output_dir, f"{tap_name}_diff_heatmap.png"),
                    title=f"Effect {effect_id} - {tap_name} - Difference"
                )
                
                # Transfer curves (if analysis has them)
                if hasattr(analysis, 'transfer_curves') and analysis.transfer_curves:
                    plot_transfer_curves(
                        analysis.transfer_curves,
                        output_path=os.path.join(effect_output_dir, f"{tap_name}_transfer_curves.png"),
                        title=f"Effect {effect_id} - {tap_name} - Transfer Curves"
                    )
            except Exception as e:
                print(f"  Warning: Could not generate visualizations for effect {effect_id}, {tap_name}: {e}")
    
    return scores

# ============================================================================
# Phase 4: Report Generation
# ============================================================================

def generate_analysis_report(
    index: DatasetIndex,
    scores: List[LGPRegressionScore],
    output_dir: str
) -> None:
    """
    Generate comprehensive analysis report.
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # Sort by LGP risk score (highest first)
    scores_sorted = sorted(scores, key=lambda s: s.lgp_risk_score, reverse=True)
    
    # Group by effect
    by_effect = {}
    for score in scores:
        if score.effect_id not in by_effect:
            by_effect[score.effect_id] = []
        by_effect[score.effect_id].append(score)
    
    # Effect classifications
    lgp_sensitive = [10, 13, 16, 26, 32, 65, 66, 67]  # Interference/chromatic family
    stateful_sensitive = [3, 8]  # Confetti, Ripple
    
    # Generate markdown report
    report_path = os.path.join(output_dir, "analysis_report.md")
    with open(report_path, 'w') as f:
        f.write("# Capture Data Analysis Report (LGP-First)\n\n")
        f.write(f"**Date:** {__import__('datetime').datetime.now().isoformat()}\n\n")
        
        f.write("## Dataset Summary\n\n")
        f.write(f"- **Baseline directory:** {index.baseline_dir}\n")
        f.write(f"- **Candidate directory:** {index.candidate_dir}\n")
        f.write(f"- **Total pairs indexed:** {len(index.file_map)}\n")
        f.write(f"- **Missing pairs:** {len(index.missing_pairs)}\n")
        if index.invalid_files:
            f.write(f"- **Invalid files:** {len(index.invalid_files)}\n")
            for invalid in index.invalid_files[:10]:
                f.write(f"  - {invalid}\n")
        f.write("\n")
        
        f.write("## Top Regressions (by LGP Risk Score)\n\n")
        f.write("| Rank | Effect | Tap | LGP Risk | Ratio Drift | Crush Score | Symmetry Dev | PSNR | Root Cause |\n")
        f.write("|------|--------|-----|----------|-------------|-------------|---------------|------|------------|\n")
        
        for rank, score in enumerate(scores_sorted[:20], 1):
            root_cause = "Tap A mismatch" if not score.tap_a_match else (
                "Tap B (correction)" if score.tap_id == 1 else "Tap C (output)"
            )
            f.write(f"| {rank} | {score.effect_id} | {score.tap_name} | "
                   f"{score.lgp_risk_score:.3f} | {score.edge_ratio_drift*100:.1f}% | "
                   f"{score.low_value_crush_score*100:.1f}% | {score.symmetry_deviation:.3f} | "
                   f"{score.psnr:.1f} dB | {root_cause} |\n")
        
        f.write("\n## Per-Effect Analysis\n\n")
        
        for effect_id in sorted(by_effect.keys()):
            effect_scores = by_effect[effect_id]
            max_risk = max(s.lgp_risk_score for s in effect_scores)
            
            classification = []
            if effect_id in lgp_sensitive:
                classification.append("LGP-sensitive")
            if effect_id in stateful_sensitive:
                classification.append("Stateful")
            
            f.write(f"### Effect {effect_id}")
            if classification:
                f.write(f" ({', '.join(classification)})")
            f.write(f"\n\n")
            f.write(f"**Max LGP Risk:** {max_risk:.3f}\n\n")
            
            for score in sorted(effect_scores, key=lambda s: s.tap_id):
                f.write(f"#### {score.tap_name}\n")
                f.write(f"- Edge ratio drift: {score.edge_ratio_drift*100:.2f}%\n")
                f.write(f"- Low-value crush: {score.low_value_crush_score*100:.1f}%\n")
                f.write(f"- Symmetry deviation: {score.symmetry_deviation:.3f}\n")
                f.write(f"- PSNR: {score.psnr:.1f} dB\n")
                f.write(f"- L1 Delta: {score.l1_delta:.0f}\n\n")
        
        # Add visual artifacts section if any were generated
        if scores_sorted and VISUALIZATION_AVAILABLE:
            f.write("## Visual Artifacts\n\n")
            f.write("Visual comparisons have been generated for effects with LGP risk score > 0.1.\n")
            f.write("See individual effect directories in the output folder for:\n")
            f.write("- Side-by-side strip renderings\n")
            f.write("- Difference heatmaps\n")
            f.write("- Transfer curve plots\n\n")
        
        # Add next-stage recommendations section
        f.write("## Next-Stage Recommendations\n\n")
        
        if not index.baseline_dir or not os.path.exists(index.baseline_dir):
            f.write("### ⚠️ Baseline Data Required\n\n")
            f.write("**Status**: Analysis cannot proceed without baseline capture data.\n\n")
            f.write("**Action Required**:\n")
            f.write("1. Flash baseline firmware (commit `9734cca1ee7dbbe4e463dc912e3c9ac2b9d1152e`)\n")
            f.write("2. Run capture script for the same effect list:\n")
            f.write("   ```bash\n")
            f.write("   python3 tools/colour_testbed/capture_test.py \\\n")
            f.write("     --device /dev/tty.usbmodem1101 \\\n")
            f.write("     --output captures/baseline_gamut\n")
            f.write("   ```\n")
            f.write("3. Re-run this analysis with baseline data\n\n")
        else:
            # Analyze results and provide recommendations
            high_risk_effects = [s for s in scores_sorted if s.lgp_risk_score > 0.3]
            tap_b_regressions = [s for s in scores_sorted if s.tap_id == 1 and s.lgp_risk_score > 0.2]
            
            if tap_b_regressions:
                f.write("### ColorCorrectionEngine Impact\n\n")
                f.write(f"**{len(tap_b_regressions)} effects show significant regression at Tap B (post-correction)**\n\n")
                f.write("**Recommended Actions**:\n")
                f.write("1. **Disable buffer correction for LGP-sensitive effects**\n")
                f.write("   - Add opt-out list in `RendererActor::onTick()`\n")
                f.write("   - Key by PatternRegistry family/tags\n\n")
                f.write("2. **Make correction LGP-safe**\n")
                f.write("   - Remove per-pixel guardrails (white/brown)\n")
                f.write("   - Use only uniform scaling (auto-exposure style)\n")
                f.write("   - Apply very soft gamma (or disable for low values)\n\n")
                f.write("3. **Move correction to output-only buffer**\n")
                f.write("   - Do not mutate `m_leds` in-place\n")
                f.write("   - Copy to separate buffer before correction\n\n")
            
            if high_risk_effects:
                f.write("### High-Risk Effects\n\n")
                f.write(f"**{len(high_risk_effects)} effects have LGP risk score > 0.3**\n\n")
                f.write("| Effect | Max Risk | Primary Issue |\n")
                f.write("|--------|----------|---------------|\n")
                for score in high_risk_effects[:10]:
                    primary_issue = "Ratio drift" if score.edge_ratio_drift > 0.2 else (
                        "Low-value crush" if score.low_value_crush_score > 0.3 else "Symmetry"
                    )
                    f.write(f"| {score.effect_id} | {score.lgp_risk_score:.3f} | {primary_issue} |\n")
                f.write("\n")
    
    print(f"\nReport saved: {report_path}")

# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Analyze baseline vs candidate captures")
    parser.add_argument("--baseline", default="", help="Baseline captures directory (empty to auto-detect or skip)")
    parser.add_argument("--candidate", required=True, help="Candidate captures directory")
    parser.add_argument("--output", default="reports/colour_testbed", help="Output directory for reports")
    
    args = parser.parse_args()
    
    print("=== Phase 1: Indexing Datasets ===\n")
    index = index_datasets(args.baseline, args.candidate)
    
    print(f"Indexed {len(index.file_map)} pairs")
    if index.missing_pairs:
        print(f"Warning: {len(index.missing_pairs)} missing pairs")
    if index.invalid_files:
        print(f"Warning: {len(index.invalid_files)} invalid files")
    
    print("\n=== Phase 2: Running LGP-First Metrics ===\n")
    scores = analyze_all_pairs(index, args.output)
    
    print("\n=== Phase 3: Generating Report ===\n")
    generate_analysis_report(index, scores, args.output)
    
    print(f"\n=== Analysis Complete ===\n")
    print(f"Results saved to: {args.output}")

