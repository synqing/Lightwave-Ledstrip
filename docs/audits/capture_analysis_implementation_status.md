# Capture Data Analysis Implementation Status

**Date:** 2025-12-23  
**Status:** Phase 1 Complete, Baseline Data Required for Phase 2

## Summary

The comprehensive capture analysis pipeline has been implemented and is ready to process baseline vs candidate comparisons. However, **baseline capture data is required** before meaningful regression analysis can be performed.

## Implementation Complete

### Phase 1: Data Indexing & Validation ✅

- **Script**: `tools/colour_testbed/analyze_captures.py`
- **Functionality**:
  - Auto-detects baseline directory from common locations
  - Validates 977-byte frame files (17-byte header + 960-byte payload)
  - Parses frame headers (magic, version, tap_id, effect_id, palette_id, etc.)
  - Builds file map: `(effect_id, tap_id) -> (baseline_file, candidate_file)`
  - Uses header effect_id as authoritative (handles filename mismatches gracefully)
  - Reports missing pairs and invalid files

### Current Status

- **Candidate data**: ✅ Available at `captures/candidate_gamut/`
  - 18 effects captured (3 taps each = 54 files)
  - All files validated (977 bytes, correct headers)
  - Note: Some captures have effect_id mismatches (header vs filename) - header is authoritative

- **Baseline data**: ❌ **NOT FOUND**
  - Expected locations checked:
    - `captures/baseline`
    - `captures/baseline_gamut`
    - `captures/baseline_9734cca`
    - `captures/baseline_commit`
  - **Action Required**: Capture baseline data from commit `9734cca` (pre-ColorCorrectionEngine)

## Next Steps

### Immediate: Capture Baseline Data

1. **Flash baseline firmware** (commit `9734cca1ee7dbbe4e463dc912e3c9ac2b9d1152e`)
2. **Run capture script** for the same effect list:
   ```bash
   python3 tools/colour_testbed/capture_test.py --device /dev/tty.usbmodem1101 --output captures/baseline_gamut
   ```
3. **Verify captures**: All 54 files (18 effects × 3 taps) should be 977 bytes each

### Phase 2: Run Analysis (Pending Baseline)

Once baseline data is available:

```bash
python3 tools/colour_testbed/analyze_captures.py \
  --baseline captures/baseline_gamut \
  --candidate captures/candidate_gamut \
  --output reports/colour_testbed/$(date +%Y-%m-%d)
```

This will:
- Compare all baseline/candidate pairs
- Calculate LGP-first metrics (edge ratio drift, low-value crush, symmetry)
- Generate regression scores
- Produce ranked regression report

### Phase 3: Visual Diagnostics (Pending Analysis Results)

For top-N regressions, generate:
- Side-by-side strip renderings
- Diff heatmaps
- Transfer curve plots
- Time-series (if multi-frame captures available)

### Phase 4: Next-Stage Plan (Pending Analysis Results)

Based on regression findings, propose:
- ColorCorrectionEngine simplification strategy
- LGP-safe correction modes
- Effect-specific opt-out lists
- Verification capture requirements

## Known Issues

1. **Effect ID Mismatches**: Some captures have effect_id in header that doesn't match filename
   - **Resolution**: Header is authoritative, warnings are logged
   - **Root Cause**: Capture script may have switched effects between command and capture
   - **Impact**: None - analysis uses header effect_id

2. **Single-Frame Captures**: Current captures are single frames (no temporal analysis)
   - **Impact**: Temporal metrics (trail persistence, frame-to-frame delta) are zero
   - **Future**: Multi-frame captures would enable temporal regression detection

## Files Created

- `tools/colour_testbed/analyze_captures.py` - Main analysis script
- `docs/audits/capture_analysis_implementation_status.md` - This document

## Dependencies

- `tools/colour_testbed/frame_parser.py` - Frame parsing (existing)
- `tools/colour_testbed/analyse.py` - Metric calculations (existing, updated for single-frame)
- `tools/colour_testbed/visualize.py` - Visualization (existing)
- `tools/colour_testbed/report.py` - Report generation (existing)

## Test Run Results

- **Indexed pairs**: 30 (using header effect_ids, some duplicates from filename mismatches)
- **Missing pairs**: 0 (all candidate files found)
- **Invalid files**: 0 (all files validated successfully)
- **Analysis**: Skipped (no baseline data)

---

**Status**: Ready for baseline data capture and full analysis.

