---
abstract: "Index of Emotiscope silence detection research documents. Contains comprehensive algorithm analysis, quick implementation reference, and comparison with current K1 architecture."
---

# Emotiscope Silence Detection Research — Index

This directory contains three documents analyzing how Emotiscope (Lixie Labs) handles silence detection and LED fade transitions for audio-reactive visualization.

## Documents

### 1. **emotiscope-silence-detection-research.md** (Comprehensive Analysis)
- **Length:** 12 KB | **Read time:** 15–20 minutes
- **Best for:** Deep understanding of algorithms and design rationale
- **Contains:**
  - Detailed silence detection algorithm (novelty contrast-based)
  - Noise floor tracking implementation
  - LED fade and darkness transition logic
  - Novelty curve mechanics
  - Screensaver activation logic
  - User-adjustable parameters
  - Key differences from traditional amplitude-based squelch

**Key findings:**
- Emotiscope detects silence via spectral novelty contrast, not amplitude thresholding
- Distinguishes quiet musical passages (high novelty) from true silence (flat novelty)
- Uses adaptive noise floor tracking (20-sample, 5-second rolling average)
- LED fade via exponential decay (95% brightness retention per frame) plus novelty-driven suppression
- Sixth-power compression on novelty during silence creates very responsive darkening

### 2. **emotiscope-algorithms.md** (Quick Reference)
- **Length:** 3.5 KB | **Read time:** 5–7 minutes
- **Best for:** Implementation — copy-paste ready code snippets
- **Contains:**
  - Silence detection formula (exact math)
  - Noise floor tracking with constants
  - VU level processing pipeline
  - Phosphor decay implementation
  - Novelty normalization formulas
  - Screensaver activation code
  - Configuration parameters

**Use this when:** Writing code or tuning parameters.

### 3. **silence-detection-comparison.md** (Architecture Analysis)
- **Length:** 7.6 KB | **Read time:** 10–12 minutes
- **Best for:** Decision-making on K1 implementation approach
- **Contains:**
  - Current LightwaveOS architecture (gap analysis)
  - Emotiscope vs. amplitude-based squelch comparison
  - Recommendations for K1 implementation (phases)
  - Implementation path: minimal vs. full
  - K1-specific tuning parameters
  - Validation plan

**Key recommendation:** Implement novelty-based silence detection in Phase 1 (minimal effort, high value), add screensaver in Phase 2.

---

## Quick Start

### If you have 5 minutes:
Read the **Silence Detection Formula** section in `emotiscope-algorithms.md` (line ~8).

### If you have 15 minutes:
Read the **Silence Detection Algorithm** section in `emotiscope-silence-detection-research.md` (sections 1.1–1.3).

### If you're implementing:
1. Read `silence-detection-comparison.md` — understand why Emotiscope's approach is better
2. Reference `emotiscope-algorithms.md` — use code snippets
3. Implement Phase 1 (novelty-based `silence_level` in ControlBus)
4. Validate with test cases from `silence-detection-comparison.md`

### If you're auditing effects:
1. Read `emotiscope-silence-detection-research.md` sections 3.1–3.3
2. Check current effect fade logic in `src/effects/` for amplitude-based squelch
3. Recommend migration to `controlBus.silence_level` for consistency

---

## Key Algorithms at a Glance

### Silence Detection (Novelty Contrast)
```c
float novelty_contrast = max - min;  // Over recent history
float silence_level = max(0.0, (1.0 - novelty_contrast - 0.5) * 2.0);
// Range: 0.0 (active) to 1.0 (silent)
```

### Why It Works
- **Quiet passages:** Notes change → novelty high → stays active ✓
- **True silence:** No change → novelty flat → fades ✓
- **Ambient noise:** Static → novelty flat → ignored ✓

### Fade Mechanism
```c
// Phosphor decay
brightness *= (1.0 - 0.05);  // Retain 95% per frame (exponential)

// Silence-driven reduction
brightness *= (1.0 - silence_level * 0.10);  // 0–10% reduction per frame
```

### Noise Floor (Squelch)
```c
float noise_floor = average(last_20_amplitudes) * 0.90;
float signal = max(raw_amplitude - noise_floor, 0.0);  // Subtract floor
```
- Tracks over 5 seconds
- Auto-calibrates to microphone and room noise

---

## Emotiscope vs. Current K1

| Feature | Emotiscope | Current K1 | Improvement |
|---------|-----------|-----------|-------------|
| Silence detection | ✓ Spectral contrast | ✗ None | Add phase 1 |
| Quiet passages | ✓ Responsive | ✗ Fade (wrong) | ✓ Phase 1 |
| Screensaver | ✓ Auto-triggered | ✗ Manual only | Add phase 2 |
| Fade consistency | ✓ Global | ✗ Per-effect | Add phase 3 |
| Noise floor tracking | ✓ Automatic | ✗ None | Add phase 1 |

**Effort estimate:**
- Phase 1: ~200 LOC, 4–6 hours
- Phase 2: ~150 LOC, 2–3 hours
- Phase 3: ~250 LOC, 4–6 hours

---

## For LightwaveOS Integration

### Recommended Implementation

**Phase 1 (Minimal):**
1. Add `silence_level` and `novelty` to `ControlBus`
2. Compute novelty in `PipelineAdapter` from magnitude changes
3. Calculate silence via contrast formula
4. Effects can use `controlBus.silence_level` if desired

**Phase 2 (Screensaver):**
1. Monitor `silence_level` in ShowDirector
2. Trigger screensaver after 5+ seconds of silence
3. Auto-reset on touch or audio resumption

**Phase 3 (Unified Fade):**
1. Apply global fade factor in `RendererActor`
2. Multiply per-effect brightness by silence-driven fade
3. Achieve consistent darkness transitions across all effects

### File Locations to Modify
- `src/audio/contracts/ControlBus.h` — add fields
- `src/audio/backends/esv11/EsV11Backend.cpp` — compute novelty
- `src/actors/ShowDirectorActor.cpp` — screensaver logic (phase 2)
- `src/actors/RendererActor.cpp` — global fade (phase 3)

---

## Testing Approach

### Validation Matrix

| Test | Method | Expected |
|------|--------|----------|
| **Quiet passage** | Play soft piano | Stays bright (silence_level low) |
| **True silence** | 30s quiet room | Fades over ~5–10s |
| **Ambient noise** | AC/fan running | Novelty stays flat (ignored) |
| **Music stop** | Pause playback | Fade begins within 1 frame |
| **Screensaver** | Wait 5s silent | Animated dots appear |

---

## References

### Source Material
- **Repository:** https://github.com/connornishijima/Emotiscope
- **Project site:** https://emotiscope.rocks/
- **License:** Check repository (research for reference only)

### Internal References
- `src/audio/contracts/ControlBus.h` — current audio state
- `src/audio/backends/esv11/EsV11Backend.cpp` — audio processing pipeline
- `src/effects/` — current effect fade patterns

---

## Document Maintenance

This research is current as of **2026-03-25**. If Emotiscope updates silence detection algorithms or K1 architecture changes significantly, revisit and update.

**Last verified:** Emotiscope main branch @ 2026-03-25, LightwaveOS main branch @ 39fbb6f4

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created research index and quick-start guide for Emotiscope silence detection analysis. Links three complementary documents (detailed research, implementation reference, architecture comparison). |
