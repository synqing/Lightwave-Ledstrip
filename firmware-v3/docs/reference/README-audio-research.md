---
abstract: "Index and quick-reference guide to WLED audio processing research. Three documents with different detail levels: architecture overview, parameter extract, and comparison to K1."
---

# Audio Processing Research — WLED Sound Reactive Reference

This directory contains three research documents analyzing WLED's audio processing implementation on ESP32, extracted for K1 architectural comparison and porting reference.

## Documents

### 1. WLED Audio Reactive Analysis
**File:** `wled-audio-reactive-analysis.md`

**Best for:** Understanding WLED's complete audio pipeline.

**Contents:**
- Full FFT processing pipeline (sampling, windowing, bin mapping)
- Pink noise equalization table and rationale
- AGC algorithm with all three presets documented
- Noise floor gating and squelch parameters
- Beat & peak detection algorithm (non-invasive single-bin design)
- Dynamics limiting (attack/decay envelope control)
- Post-processing scaling modes

**Key takeaway:** WLED is simple but battle-tested. FFT + threshold-based beat detection works adequately for 1000s of deployments.

---

### 2. K1 vs WLED — Architectural Comparison
**File:** `k1-vs-wled-audio-comparison.md`

**Best for:** Understanding why K1 took a different design path.

**Contents:**
- Side-by-side comparison of design philosophy
- Frequency analysis: FFT vs Goertzel bin resolution
- AGC parameters for both systems
- Beat & onset detection comparison
- Silence gating: threshold vs Schmitt trigger
- Noise floor handling and equalization
- Parameter density and user tuning surface
- Computational cost breakdown (both systems)
- Summary table of feature comparison
- Design decision rationale

**Key takeaway:** K1 chose semantic richness (octaves + pitch + drums + tempo) over simplicity (raw FFT bins). More output complexity but zero tuning required.

---

### 3. WLED Parameter Extract
**File:** `wled-parameter-extract.md`

**Best for:** Exact constant values for porting or replication.

**Contents:**
- Every constant from WLED source code (sampling, FFT, AGC, thresholds)
- Pink noise correction table (16 channels, exact multipliers)
- AGC preset tuning (all 8 parameters × 3 presets = 24 constants)
- Peak detection debounce timing (100ms minimum)
- Bin mapping constants for 22 kHz mode
- Dynamics limiting time constants (80ms attack, 1400ms decay)
- Hardware constraints (I2S buffer sizes, core affinity)
- UDP network format for AudioSync packets
- Porting checklist (how to replicate in C++)

**Key takeaway:** If you want to implement WLED's algorithm exactly, this document has every number.

---

## Quick Reference: Algorithm Comparison

| Aspect | WLED | K1 ESV11 |
|--------|------|---------|
| **Core algorithm** | FFT (256 bins) | Goertzel (64 harmonic filters) |
| **Output channels** | 16 GEQ + 1 beat | 8 octaves + 12 chroma + 4 triggers + grid |
| **Beat detection** | Single-bin threshold pulse | Multi-band onset + Kalman tempo grid |
| **Drum separation** | None | Kick/snare/hihat onset flags |
| **Silence gating** | Squelch (threshold) | Schmitt trigger (hysteresis) |
| **AGC** | 3 global presets (Normal/Vivid/Lazy) | Per-band adaptive |
| **Noise handling** | Pink noise correction (1.7–11.9× per channel) | A-weighting + mic calibration per band |
| **User tuning** | 7 parameters | 0 (semantic output, effects are the UI) |
| **Computational cost** | ~5 ms per 45ms cycle | ~12–15 ms per 23ms cycle (overlapped) |
| **Maturity** | Battle-tested (1000s) | Newer (50+ effects) |

---

## When to Read Each Document

### Reading `wled-audio-reactive-analysis.md`

**Start here if you want to:**
- Understand WLED's complete architecture
- Learn about FFT processing on ESP32
- Study AGC, gating, and dynamics algorithms
- Understand pink noise correction

**Time investment:** 20–30 minutes

---

### Reading `k1-vs-wled-audio-comparison.md`

**Start here if you want to:**
- Compare K1 and WLED side by side
- Understand K1's design rationale
- Learn what K1 gains (and loses) vs WLED
- Study computational trade-offs

**Time investment:** 15–25 minutes

---

### Reading `wled-parameter-extract.md`

**Start here if you want to:**
- Port WLED's algorithm to K1
- Find exact constant values
- Implement a C++ version of WLED
- Understand hardware constraints

**Time investment:** 10–15 minutes (reference reading, not sequential)

---

## Key Findings Summary

### WLED Strengths
1. **Proven in the field:** 1000s of user deployments, thousands of effect combinations
2. **Simple to understand:** Single FFT + threshold beat detection
3. **Low CPU cost:** ~5 ms per 45ms cycle (only 11% of budget)
4. **No user tuning needed:** 3 presets handle 80% of cases
5. **Great documentation:** Public source + active community

### WLED Limitations
1. **No kick/snare/hihat separation:** Only single-bin beat detection
2. **No tempo tracking:** No beat grid alignment
3. **No chroma/pitch info:** Only magnitude FFT, no phase
4. **Single threshold gating:** Prone to chatter during quiet passages
5. **Threshold-based beat:** No velocity or attack envelope

### K1 Advantages (Why It's Better for Professional Effects)
1. **Multi-band onset detection:** Kick = sub-bass onset, snare = upper-mid, hihat = high
2. **Tempo grid synchronisation:** Effects can snap to bar lines, quantise animations
3. **Pitch/chroma data:** Musical effects can react to specific note pitches
4. **Schmitt trigger gating:** No chatter, hysteresis prevents false triggers
5. **Semantic richness:** 40+ output fields, zero tuning required
6. **Velocity tracking:** "How hard" the onset (for dynamic amplitude control)

### K1 Trade-offs
1. **More complex:** More algorithm, more parameters (internal, not user-facing)
2. **Higher CPU cost:** ~12–15 ms per cycle (but overlapped with sampling on Core 0)
3. **Newer:** Less battle-tested than WLED (but 50+ effects, 80+ hours development)
4. **More code:** Goertzel + onset detection + Kalman tracking vs simple FFT + threshold

---

## Decision Framework: Which to Use?

### Use WLED's approach if:
- You're building a consumer product for general users
- You need minimal configuration (3 presets = done)
- You want proven, time-tested algorithms
- You have 1000s of users giving feedback

### Use K1's approach if:
- You're building effects for artists/musicians
- You need kick/snare/hihat separation
- You want tempo-grid-aware animations
- You need pitch information for musical effects
- You can afford more development/tuning time

---

## File Locations in K1 Codebase

**If you want to see K1's implementation:**

| Component | File | Lines |
|-----------|------|-------|
| Goertzel analyzer | `src/audio/GoertzelAnalyzer.cpp` | 500+ |
| Beat tracking | `src/audio/pipeline/BeatTracker.cpp` | 300+ |
| Audio adapter | `src/audio/pipeline/PipelineAdapter.cpp` | 400+ |
| Onset detection | `src/audio/contracts/OnsetSemantics.cpp` | 200+ |
| Control bus | `src/audio/contracts/ControlBus.cpp` | 150+ |
| Audio actor | `src/audio/AudioActor.cpp` | 400+ |

**Total K1 audio pipeline:** ~2,000 LOC (vs WLED ~800 LOC for audio_reactive.cpp)

---

## Sources & Credits

All research sourced from:
- **WLED Official Repository:** https://github.com/WLED/WLED
- **WLED Audio Reactive Usermod:** https://github.com/WLED/WLED/tree/main/usermods/audioreactive
- **WLED Documentation:** https://kno.wled.ge/advanced/audio-reactive/
- **WLED MoonModules (Sound Reactive fork):** https://github.com/MoonModules/WLED-MM
- **ArduinoFFT Library:** https://github.com/kosme/ArduinoFFT
- **ESP32 Audio Forums:** https://forum.arduino.cc/

All code examples and constants extracted directly from public GitHub repositories.

---

## Next Steps

**If you want to integrate WLED-style beat detection into K1:**
1. Read `wled-parameter-extract.md` for exact constants
2. Implement single-bin peak detection in parallel with K1's multi-band onset
3. Use K1's snapshot system to feed both WLED style + K1 style to effects
4. Effects can choose which one to use via `ctx.beat` (K1) or custom field

**If you want to port WLED to a new platform:**
1. Read `wled-audio-reactive-analysis.md` for architecture
2. Use `wled-parameter-extract.md` for all constants
3. Replace ArduinoFFT with platform-equivalent (DSP library)
4. Estimated time: 4–6 hours implementation + testing

**If you want to understand K1's design decisions:**
1. Read `k1-vs-wled-audio-comparison.md` (architecture decisions explained)
2. Compare side-by-side to understand trade-offs
3. Reference the source code for implementation details

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created index and quick-reference guide to WLED audio research |
