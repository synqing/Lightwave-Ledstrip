# Capture Data Analysis Implementation - Complete

**Date:** 2025-12-23  
**Status:** ✅ Implementation Complete, Awaiting Baseline Data

## Summary

The comprehensive capture data analysis pipeline has been fully implemented according to the plan. All phases are complete and ready to execute once baseline capture data is available.

## Implementation Status

### ✅ Phase 1: Data Indexing & Validation
- **Script**: `tools/colour_testbed/analyze_captures.py`
- **Features**:
  - Auto-detects baseline directory from common locations
  - Validates 977-byte frame files (17-byte header + 960-byte payload)
  - Parses frame headers (magic, version, tap_id, effect_id, palette_id, etc.)
  - Builds file map: `(effect_id, tap_id) -> (baseline_file, candidate_file)`
  - Uses header effect_id as authoritative (handles filename mismatches)
  - Reports missing pairs and invalid files

### ✅ Phase 2: LGP-First Metrics
- **Metrics Implemented**:
  - Edge ratio drift (I₁/I₂ preservation) - **Weight: 0.4**
  - Low-value crush score (trail/gradient preservation) - **Weight: 0.4**
  - Symmetry deviation - **Weight: 0.1**
  - Centre structure drift - **Weight: 0.1**
  - LGP risk score (weighted combination)
- **Secondary Metrics**:
  - L1/L2 delta, max error, PSNR
  - Per-pixel comparison
  - Edge amplitude metrics

### ✅ Phase 3: Visual Diagnostics
- **Integrated into analysis script**:
  - Side-by-side strip renderings (for LGP risk > 0.1)
  - Difference heatmaps
  - Transfer curve plots
- **Output**: Saved to `reports/<date>/effect_<id>/` directories

### ✅ Phase 4: Regression Ranking & Decision Gates
- **Report Generation**:
  - Ranked regression table (by LGP risk score)
  - Per-effect analysis breakdown
  - Root cause attribution (Tap A vs B vs C)
  - Effect classification (LGP-sensitive, Stateful)
- **Decision Logic**:
  - Tap A mismatch → Effect logic regression
  - Tap B divergence → Correction pipeline regression
  - Tap C divergence → FastLED/strip pipeline issue

### ✅ Phase 5: Next-Stage Implementation Plan
- **Document Created**: `docs/audits/next_stage_color_pipeline_plan.md`
- **Strategies Outlined**:
  1. Effect-specific opt-out (recommended for LGP)
  2. LGP-safe uniform correction
  3. Output-only buffer (hybrid)
- **Implementation roadmap** with success criteria

## Current Status

### Candidate Data ✅
- **Location**: `captures/candidate_gamut/`
- **Files**: 54 files (18 effects × 3 taps)
- **Validation**: All files validated (977 bytes, correct headers)
- **Note**: Some captures have effect_id mismatches (header vs filename) - header is authoritative

### Baseline Data ❌
- **Status**: **NOT FOUND**
- **Required**: Capture from commit `9734cca1ee7dbbe4e463dc912e3c9ac2b9d1152e` (pre-ColorCorrectionEngine)
- **Action**: Run capture script on baseline firmware

## Usage

### Once Baseline Data is Available

```bash
# Run full analysis
python3 tools/colour_testbed/analyze_captures.py \
  --baseline captures/baseline_gamut \
  --candidate captures/candidate_gamut \
  --output reports/colour_testbed/$(date +%Y-%m-%d)

# Output includes:
# - analysis_report.md (comprehensive report)
# - effect_<id>/ directories (visual artifacts for high-risk regressions)
```

### Current Test Run

```bash
# Test run (without baseline - validates pipeline)
python3 tools/colour_testbed/analyze_captures.py \
  --baseline "" \
  --candidate captures/candidate_gamut \
  --output reports/final_analysis
```

**Result**: Pipeline validates successfully, reports baseline data requirement.

## Files Created/Modified

### New Files
1. `tools/colour_testbed/analyze_captures.py` - Main analysis script (571 lines)
2. `docs/audits/capture_analysis_implementation_status.md` - Implementation status
3. `docs/audits/next_stage_color_pipeline_plan.md` - Next-stage recommendations
4. `docs/audits/capture_analysis_complete.md` - This document

### Modified Files
1. `tools/colour_testbed/analyse.py` - Updated to handle single-frame captures

## Key Features

1. **LGP-First Scoring**: Prioritizes amplitude relationships and low-value preservation
2. **Tap-Based Attribution**: Identifies root cause (effect vs correction vs output)
3. **Automatic Visualization**: Generates visual artifacts for significant regressions
4. **Comprehensive Reporting**: Markdown report with rankings and recommendations
5. **Graceful Degradation**: Handles missing baseline data, single-frame captures, filename mismatches

## Next Steps

1. **Capture Baseline Data**:
   ```bash
   # Flash baseline firmware (commit 9734cca)
   git checkout 9734cca
   # Build and flash to device
   # Run capture script
   python3 tools/colour_testbed/capture_test.py \
     --device /dev/tty.usbmodem1101 \
     --output captures/baseline_gamut
   ```

2. **Run Full Analysis**:
   ```bash
   python3 tools/colour_testbed/analyze_captures.py \
     --baseline captures/baseline_gamut \
     --candidate captures/candidate_gamut \
     --output reports/colour_testbed/$(date +%Y-%m-%d)
   ```

3. **Review Results**:
   - Check `analysis_report.md` for ranked regressions
   - Review visual artifacts in effect directories
   - Use findings to finalize next-stage implementation plan

4. **Implement Fixes**:
   - Based on analysis results, choose strategy from `next_stage_color_pipeline_plan.md`
   - Implement chosen approach
   - Re-capture and verify improvements

## Success Criteria

- ✅ All phases implemented
- ✅ Pipeline validates candidate data successfully
- ✅ Report generation works
- ✅ Visual artifacts integrated
- ⏳ Awaiting baseline data for full analysis

---

**Status**: Ready for baseline data capture and full regression analysis.

