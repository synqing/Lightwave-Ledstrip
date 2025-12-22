#!/usr/bin/env python3
"""
Report Generator for Colour Pipeline Testbed

Generates comprehensive markdown/HTML reports from analysis results.
"""

import os
import json
from typing import Dict, List, Optional
from dataclasses import dataclass, asdict
from frame_parser import Frame, get_frame_statistics
from analyse import AnalysisResults, compare_baseline_candidate

# ============================================================================
# Report Generation
# ============================================================================

def generate_markdown_report(
    results: Dict[str, AnalysisResults],
    output_path: str,
    baseline_commit: str = "baseline",
    candidate_commit: str = "candidate"
) -> None:
    """
    Generate markdown report from analysis results.
    
    Args:
        results: Dictionary mapping effect_id -> AnalysisResults
        output_path: Path to save markdown file
        baseline_commit: Baseline commit hash/name
        candidate_commit: Candidate commit hash/name
    """
    with open(output_path, 'w') as f:
        f.write(f"# Colour Pipeline Testbed Report\n\n")
        f.write(f"**Baseline:** {baseline_commit}\n")
        f.write(f"**Candidate:** {candidate_commit}\n")
        f.write(f"**Date:** {__import__('datetime').datetime.now().isoformat()}\n\n")
        
        f.write("## Summary\n\n")
        
        # Overall statistics
        total_effects = len(results)
        passed_effects = sum(1 for r in results.values() if r.passed)
        failed_effects = total_effects - passed_effects
        
        f.write(f"- **Total Effects Tested:** {total_effects}\n")
        f.write(f"- **Passed:** {passed_effects}\n")
        f.write(f"- **Failed:** {failed_effects}\n")
        f.write(f"- **Pass Rate:** {passed_effects/total_effects*100:.1f}%\n\n")
        
        f.write("## Per-Effect Results\n\n")
        f.write("| Effect ID | Tap | Status | L1 Delta | PSNR (dB) | I₁/I₂ Ratio | Ratio Drift | Failures |\n")
        f.write("|-----------|-----|--------|---------|-----------|-------------|-------------|----------|\n")
        
        for effect_id in sorted(results.keys()):
            r = results[effect_id]
            status = "✅ PASS" if r.passed else "❌ FAIL"
            failures_str = "; ".join(r.failures) if r.failures else "-"
            
            f.write(f"| {effect_id} | {r.per_pixel} | {status} | {r.per_pixel.l1_delta:.1f} | "
                   f"{r.per_pixel.psnr:.1f} | {r.edge_amplitude.i1_i2_ratio:.3f} | "
                   f"{r.edge_amplitude.ratio_stability*100:.2f}% | {failures_str} |\n")
        
        f.write("\n## Detailed Results\n\n")
        
        for effect_id in sorted(results.keys()):
            r = results[effect_id]
            f.write(f"### Effect {effect_id}\n\n")
            f.write(f"**Status:** {'✅ PASS' if r.passed else '❌ FAIL'}\n\n")
            
            f.write("#### Per-Pixel Metrics\n")
            f.write(f"- L1 Delta: {r.per_pixel.l1_delta:.1f}\n")
            f.write(f"- L2 Delta: {r.per_pixel.l2_delta:.1f}\n")
            f.write(f"- Max Error: {r.per_pixel.max_error}\n")
            f.write(f"- PSNR: {r.per_pixel.psnr:.2f} dB\n")
            f.write(f"- Mean Error: {r.per_pixel.mean_error:.2f}\n")
            f.write(f"- Std Error: {r.per_pixel.std_error:.2f}\n\n")
            
            f.write("#### Edge Amplitude Metrics\n")
            f.write(f"- I₁ Mean: {r.edge_amplitude.i1_mean:.1f}\n")
            f.write(f"- I₂ Mean: {r.edge_amplitude.i2_mean:.1f}\n")
            f.write(f"- I₁/I₂ Ratio: {r.edge_amplitude.i1_i2_ratio:.3f}\n")
            f.write(f"- Ratio Drift: {r.edge_amplitude.ratio_stability*100:.2f}%\n")
            f.write(f"- Spatial Symmetry: {r.edge_amplitude.spatial_symmetry:.3f}\n\n")
            
            f.write("#### Temporal Metrics\n")
            f.write(f"- Frame-to-Frame Delta: {r.temporal.frame_to_frame_delta:.1f}\n")
            f.write(f"- Trail Persistence (Q25): {r.temporal.trail_persistence_q25:.1f} frames\n")
            f.write(f"- Trail Persistence (Q50): {r.temporal.trail_persistence_q50:.1f} frames\n")
            f.write(f"- Trail Persistence (Q75): {r.temporal.trail_persistence_q75:.1f} frames\n\n")
            
            f.write("#### Transfer Curves\n")
            for channel, curve in r.transfer_curves.items():
                f.write(f"- {channel.upper()} Channel: γ = {curve.gamma_estimate:.2f}\n")
            f.write("\n")
            
            if r.failures:
                f.write("#### Failures\n")
                for failure in r.failures:
                    f.write(f"- {failure}\n")
                f.write("\n")
            
            f.write("---\n\n")
        
        print(f"Markdown report saved: {output_path}")

def generate_html_report(
    results: Dict[str, AnalysisResults],
    output_path: str,
    baseline_commit: str = "baseline",
    candidate_commit: str = "candidate",
    image_dir: Optional[str] = None
) -> None:
    """
    Generate HTML report from analysis results.
    
    Args:
        results: Dictionary mapping effect_id -> AnalysisResults
        output_path: Path to save HTML file
        baseline_commit: Baseline commit hash/name
        candidate_commit: Candidate commit hash/name
        image_dir: Optional directory containing visualization images
    """
    html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Colour Pipeline Testbed Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        table {{ border-collapse: collapse; width: 100%; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
        .pass {{ color: green; }}
        .fail {{ color: red; }}
        .summary {{ background-color: #f9f9f9; padding: 15px; margin: 20px 0; }}
    </style>
</head>
<body>
    <h1>Colour Pipeline Testbed Report</h1>
    <div class="summary">
        <p><strong>Baseline:</strong> {baseline_commit}</p>
        <p><strong>Candidate:</strong> {candidate_commit}</p>
        <p><strong>Date:</strong> {__import__('datetime').datetime.now().isoformat()}</p>
    </div>
    
    <h2>Summary</h2>
    <ul>
"""
    
    total_effects = len(results)
    passed_effects = sum(1 for r in results.values() if r.passed)
    failed_effects = total_effects - passed_effects
    
    html += f"        <li><strong>Total Effects Tested:</strong> {total_effects}</li>\n"
    html += f"        <li><strong>Passed:</strong> {passed_effects}</li>\n"
    html += f"        <li><strong>Failed:</strong> {failed_effects}</li>\n"
    html += f"        <li><strong>Pass Rate:</strong> {passed_effects/total_effects*100:.1f}%</li>\n"
    
    html += """    </ul>
    
    <h2>Per-Effect Results</h2>
    <table>
        <tr>
            <th>Effect ID</th>
            <th>Status</th>
            <th>L1 Delta</th>
            <th>PSNR (dB)</th>
            <th>I₁/I₂ Ratio</th>
            <th>Ratio Drift</th>
            <th>Failures</th>
        </tr>
"""
    
    for effect_id in sorted(results.keys()):
        r = results[effect_id]
        status_class = "pass" if r.passed else "fail"
        status_text = "✅ PASS" if r.passed else "❌ FAIL"
        failures_str = "; ".join(r.failures) if r.failures else "-"
        
        html += f"""        <tr>
            <td>{effect_id}</td>
            <td class="{status_class}">{status_text}</td>
            <td>{r.per_pixel.l1_delta:.1f}</td>
            <td>{r.per_pixel.psnr:.1f}</td>
            <td>{r.edge_amplitude.i1_i2_ratio:.3f}</td>
            <td>{r.edge_amplitude.ratio_stability*100:.2f}%</td>
            <td>{failures_str}</td>
        </tr>
"""
    
    html += """    </table>
</body>
</html>
"""
    
    with open(output_path, 'w') as f:
        f.write(html)
    
    print(f"HTML report saved: {output_path}")

# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    import sys
    import argparse
    
    parser = argparse.ArgumentParser(description="Generate testbed report")
    parser.add_argument("--baseline", required=True, help="Baseline capture directory")
    parser.add_argument("--candidate", required=True, help="Candidate capture directory")
    parser.add_argument("--output", required=True, help="Output directory for reports")
    parser.add_argument("--baseline-commit", default="baseline", help="Baseline commit hash/name")
    parser.add_argument("--candidate-commit", default="candidate", help="Candidate commit hash/name")
    parser.add_argument("--format", choices=["markdown", "html", "both"], default="both")
    
    args = parser.parse_args()
    
    # Create output directory
    os.makedirs(args.output, exist_ok=True)
    
    # Find all capture files
    from frame_parser import parse_serial_dump
    
    baseline_files = {}
    candidate_files = {}
    
    # Scan directories for capture files
    for filename in os.listdir(args.baseline):
        if filename.endswith('.bin'):
            # Parse filename: effect_<id>_tap_<tap>.bin
            parts = filename.replace('.bin', '').split('_')
            if len(parts) >= 4 and parts[0] == 'effect' and parts[2] == 'tap':
                effect_id = int(parts[1])
                tap_id = int(parts[3])
                key = (effect_id, tap_id)
                baseline_files[key] = os.path.join(args.baseline, filename)
    
    for filename in os.listdir(args.candidate):
        if filename.endswith('.bin'):
            parts = filename.replace('.bin', '').split('_')
            if len(parts) >= 4 and parts[0] == 'effect' and parts[2] == 'tap':
                effect_id = int(parts[1])
                tap_id = int(parts[3])
                key = (effect_id, tap_id)
                candidate_files[key] = os.path.join(args.candidate, filename)
    
    # Process each effect/tap combination
    results = {}
    
    for key in set(list(baseline_files.keys()) + list(candidate_files.keys())):
        if key not in baseline_files or key not in candidate_files:
            print(f"Warning: Missing capture for effect {key[0]}, tap {key[1]}")
            continue
        
        effect_id, tap_id = key
        
        print(f"Processing effect {effect_id}, tap {tap_id}...")
        
        # Parse frames
        baseline_frames = parse_serial_dump(baseline_files[key])
        candidate_frames = parse_serial_dump(candidate_files[key])
        
        if not baseline_frames or not candidate_frames:
            print(f"Warning: Could not parse frames for effect {effect_id}, tap {tap_id}")
            continue
        
        # Compare
        result = compare_baseline_candidate(baseline_frames, candidate_frames, tap_id)
        results[f"effect_{effect_id}_tap_{tap_id}"] = result
    
    # Generate reports
    if args.format in ["markdown", "both"]:
        generate_markdown_report(
            results,
            os.path.join(args.output, "report.md"),
            args.baseline_commit,
            args.candidate_commit
        )
    
    if args.format in ["html", "both"]:
        generate_html_report(
            results,
            os.path.join(args.output, "report.html"),
            args.baseline_commit,
            args.candidate_commit
        )
    
    print(f"\nReport generation complete. Results: {len(results)} effect/tap combinations")

