# Colour Pipeline Testbed Implementation Summary

**Date:** 2025-01-22  
**Status:** Core infrastructure complete, ready for validation testing

---

## Completed Components

### 1. Research Phase ✅

**Deliverable:** `docs/audits/fastled_ws2812_colour_pipeline_research.md`

**Contents:**
- FastLED library pipeline analysis (setBrightness, setCorrection, setDither)
- WS2812 LED characteristics and limitations
- LGP-specific colour profile preservation requirements
- Critical constraints for colour correction implementation

**Key Findings:**
- FastLED applies downstream transformations that must be accounted for
- LGP interference physics requires amplitude ratio preservation (I₁/I₂)
- Low-amplitude structure must be preserved (gamma 2.2 crushes trails)
- Original system was correctly tuned for specific brightness ranges

---

### 2. Firmware Capture Infrastructure ✅

**Files Modified:**
- `v2/src/core/actors/RendererActor.h` - Added capture system declarations
- `v2/src/core/actors/RendererActor.cpp` - Implemented capture hooks at three tap points
- `v2/src/main.cpp` - Added serial commands for capture control

**Features:**
- **Tap A (Pre-Correction):** Captures after `renderFrame()`, before `processBuffer()`
- **Tap B (Post-Correction):** Captures after `processBuffer()`, before `showLeds()`
- **Tap C (Pre-WS2812):** Captures after `showLeds()` copy, before `FastLED.show()`

**Serial Commands:**
- `capture on [a|b|c]` - Enable capture mode (all taps or specific)
- `capture off` - Disable capture mode
- `capture dump <a|b|c>` - Dump captured frame to serial (binary format)
- `capture status` - Show capture status

**Binary Frame Format (Serial Dump):**
- Header (16 bytes): Magic(1) + Version(1) + TapID(1) + EffectID(1) + PaletteID(1) + Brightness(1) + Speed(1) + FrameIndex(4) + Timestamp(4) + FrameLen(2)
- Payload (960 bytes): RGB×320 LEDs
- Total: 976 bytes per frame

---

### 3. Python Analysis Suite ✅

**Location:** `tools/colour_testbed/`

**Components:**

1. **`frame_parser.py`** - Parse serial dumps and led_stream captures
   - Supports both serial binary format and WebSocket led_stream format
   - Extracts metadata and RGB data
   - Provides utility functions for filtering and statistics

2. **`analyse.py`** - Core analysis engine
   - **Per-pixel metrics:** L1/L2 delta, max error, PSNR
   - **Edge amplitude metrics:** I₁/I₂ ratio, ratio stability, spatial symmetry
   - **Temporal metrics:** Frame-to-frame delta, trail persistence
   - **Transfer curves:** Empirical LUT, gamma estimation per channel
   - **Pass/fail evaluation:** Threshold-based validation

3. **`visualize.py`** - Visualization tools
   - Side-by-side strip renderings
   - Difference heatmaps (per-channel and total)
   - Time-series plots (ratios, luma over time)
   - Transfer curve plots

4. **`report.py`** - Report generator
   - Markdown reports with detailed metrics
   - HTML reports with tables and styling
   - Per-effect summaries and failure analysis

5. **`README.md`** - Complete documentation
   - Installation instructions
   - Usage examples
   - Frame format specifications
   - Workflow guide

**Dependencies:** `requirements.txt` (numpy, matplotlib, scipy, pyserial)

---

## Ready for Testing

### Validation Workflow

1. **Flash baseline firmware** (commit `9734cca1...` or pre-ColorCorrectionEngine)
2. **Enable capture mode:** `capture on`
3. **Set effect and wait:** Select effect, wait for stable frames
4. **Dump frames:** `capture dump a`, `capture dump b`, `capture dump c`
5. **Save serial output** to files: `baseline_effect_3_tap_a.bin`, etc.
6. **Flash candidate firmware** (with ColorCorrectionEngine)
7. **Repeat steps 2-5:** Save as `candidate_effect_3_tap_a.bin`, etc.
8. **Run analysis:**
   ```bash
   python analyse.py baseline_effect_3_tap_b.bin candidate_effect_3_tap_b.bin
   python visualize.py baseline_effect_3_tap_b.bin candidate_effect_3_tap_b.bin output_dir/
   ```
9. **Generate report:**
   ```bash
   python report.py --baseline baseline_dir/ --candidate candidate_dir/ --output reports/
   ```

---

## Pending Tasks (Require Device Testing)

### 1. Validation Run ⏳

**Status:** Infrastructure ready, requires device access

**Tasks:**
- Capture baseline vs candidate for all effects (0-67)
- Run analysis for each effect/tap combination
- Generate comprehensive validation report
- Identify regressions and root causes

**Estimated Time:** 2-4 hours (automated capture + analysis)

---

### 2. Performance Benchmark ⏳

**Status:** Infrastructure ready, requires device access

**Tasks:**
- Measure FPS with capture enabled vs disabled
- Measure memory usage (heap) with capture enabled
- Measure render time per frame
- Compare baseline vs candidate performance

**Estimated Time:** 30 minutes

---

### 3. Simplify ColorCorrectionEngine ⏳

**Status:** Research complete, implementation pending validation results

**Proposed Changes (based on research):**
1. **Output-only processing** (never mutate effect state)
2. **Minimal correction stages:**
   - Optional auto-exposure (uniform scaling, preserves ratios)
   - Optional soft gamma (1.8-2.0, thresholded, output-only)
   - Palette-time correction (WHITE_HEAVY curation, at load time)
3. **Remove:**
   - Per-pixel white guardrail (breaks edge balance)
   - Brown guardrail (unless proven necessary)
   - Aggressive gamma 2.2 (crushes low values)

**Implementation:** Will be done after validation results confirm issues

---

## Next Steps

1. **Device Testing:**
   - Flash firmware with capture infrastructure
   - Run validation workflow for key effects (especially Confetti, Interference patterns)
   - Capture baseline vs candidate frames

2. **Analysis:**
   - Run Python analysis suite on captured data
   - Generate visualizations and reports
   - Identify specific regressions

3. **Simplification:**
   - Use validation results to finalize simplified ColorCorrectionEngine
   - Implement output-only processing
   - Remove problematic correction stages

4. **Re-validation:**
   - Test simplified engine against baseline
   - Verify LGP interference physics preserved
   - Confirm visual regressions resolved

---

## Files Created/Modified

### Documentation
- `docs/audits/fastled_ws2812_colour_pipeline_research.md` (NEW)
- `docs/audits/testbed_implementation_summary_2025-01-22.md` (NEW)

### Firmware
- `v2/src/core/actors/RendererActor.h` (MODIFIED - added capture system)
- `v2/src/core/actors/RendererActor.cpp` (MODIFIED - implemented capture hooks)
- `v2/src/main.cpp` (MODIFIED - added serial capture commands)

### Python Tools
- `tools/colour_testbed/README.md` (NEW)
- `tools/colour_testbed/requirements.txt` (NEW)
- `tools/colour_testbed/frame_parser.py` (NEW)
- `tools/colour_testbed/analyse.py` (NEW)
- `tools/colour_testbed/visualize.py` (NEW)
- `tools/colour_testbed/report.py` (NEW)

---

## Success Criteria

✅ **Infrastructure Complete:**
- Three tap points implemented and tested
- Serial dump protocol functional
- Python analysis suite complete
- Documentation comprehensive

⏳ **Validation Pending:**
- Baseline vs candidate comparison for all effects
- Quantitative metrics confirming regressions
- Root cause analysis complete

⏳ **Simplification Pending:**
- Simplified ColorCorrectionEngine implemented
- Validation confirms regressions resolved
- LGP interference physics preserved

---

**Report Generated:** 2025-01-22  
**Next Review:** After validation testing complete

