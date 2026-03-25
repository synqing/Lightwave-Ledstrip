---
abstract: "Comparison of silence detection approaches: Emotiscope (spectral novelty contrast) vs. amplitude-based squelch vs. LightwaveOS current architecture. Recommendations for K1 implementation."
---

# Silence Detection Approaches: Comparative Analysis

## Current LightwaveOS Architecture

### Absence of Dedicated Silence Detection

The current firmware (ESV11 backend) provides:
- **ControlBus fields:** `rms`, `bands[0..7]`, `chroma[0..11]`, `rms_ungated` (raw before any squelch)
- **Per-effect responsibility:** Effects decide when to fade based on `rms` or `bands[]` values
- **Example (AudioReactivityPillar):** Falls back to baseColor when `rms < rmsFloor`

**Gaps:**
1. No global silence/activity metric shared across all effects
2. Each effect implements its own fade-to-darkness logic (inconsistent behavior)
3. No automatic screensaver or idle-state management
4. Amplitude-based (rms) means quiet passages appear "silent" to effects

### Current Fade Behavior

Effects currently fade via:
```c
// Typical pattern in effects
if (vu_level < rmsFloor) {
    brightness *= 0.95f;  // Manual exponential decay per effect
}
```

**Problems:**
- Inconsistent between effects
- Quiet musical passages incorrectly trigger fade
- No screensaver or automatic darkness transition

---

## Emotiscope Approach: Spectral Novelty Contrast

### Advantages Over Amplitude-Based Squelch

| Scenario | Amplitude Squelch | Emotiscope Novelty |
|----------|-------------------|-------------------|
| Quiet piano passage | Fades (wrong!) | Stays bright ✓ |
| Sustained bass note | Responsive (correct) | Stays bright ✓ |
| Room silence | Fades (correct) | Fades ✓ |
| Ambient noise | False trigger (wrong) | Ignored ✓ |
| Fast tempo breaks | No indication | Detects novelty drop ✓ |

### Why Spectral Contrast Works

Emotiscope's approach exploits a key property:

> **Music has spectral variation. Silence does not.**

- **Quiet music** (soft piano): Frequencies change even at low amplitude → novelty varies → detected as "active"
- **True silence**: All frequencies near-zero → novelty flat → detected as "silent"
- **Static ambient noise** (AC hum, fan): Frequencies constant → novelty flat → ignored as "silent"

### Implementation Cost

Emotiscope pays for this with:
- Circular buffer of 128 novelty samples (~512 bytes)
- Per-frame spectral flux computation (already done for beat tracking)
- Simple contrast calculation (max-min over buffer)

**For K1:** LightwaveOS already computes per-frequency magnitudes (ESV11 backend), so novelty computation is nearly free.

---

## Recommendation for LightwaveOS K1

### Phase 1: Add Novelty-Based Silence Detection (Minimal Risk)

**Add to ControlBus:**
```c
struct ControlBus {
    // ... existing fields ...
    float silence_level;  // 0.0 (active) to 1.0 (silent)
    float novelty;        // 0.0 to 1.0 (spectral change rate)
};
```

**In PipelineAdapter (where ControlBus is populated):**
```c
// 1. Compute novelty from magnitude changes
float novelty = compute_spectral_flux(current_magnitudes, prev_magnitudes);

// 2. Update circular history
novelty_history[history_index] = novelty;
history_index = (history_index + 1) % NOVELTY_HISTORY_LEN;

// 3. Compute silence
float novelty_contrast = max - min;
float silence_level_raw = 1.0f - novelty_contrast;
float silence_level = fmaxf(0.0f, (silence_level_raw - 0.5f) * 2.0f);

// 4. Update ControlBus
controlBus.silence_level = silence_level;
controlBus.novelty = novelty;
```

**Benefits:**
- Non-breaking change (effects ignore new fields)
- Shared across all effects (consistency)
- No amplitude-based squelch (quiet passages unaffected)
- Foundation for screensaver/idle management

### Phase 2: Global Screensaver (Opt-In)

Enable ShowDirector to manage idle state:
```c
// In ShowDirectorActor
if (controlBus.silence_level > 0.5f && now - last_activity > 5000ms) {
    enter_screensaver();
}
```

### Phase 3: Unified Fade Mechanism (Optional)

Migrate effect fade logic to ControlBus fade factor:
```c
// In RendererActor
float global_fade = 1.0f - (silence_level * 0.10f);  // 0% to 10% per frame
render_context.fade_scale *= global_fade;
```

---

## Implementation Path: Minimal vs. Full

### Minimal (Phase 1 Only)
- **Effort:** ~200 LOC in PipelineAdapter
- **Risk:** None (backward compatible)
- **Benefit:** Better quiet passage handling, foundation for future work
- **Result:** Effects can opt-in to `controlBus.silence_level` if desired

### Full (Phases 1–3)
- **Effort:** ~500 LOC (silence detection + screensaver + fade)
- **Risk:** Low (if implemented carefully, effects can override)
- **Benefit:** Unified behavior, automatic screensaver, consistent darkness transitions
- **Result:** K1 has production-grade silence handling like Emotiscope

---

## Decision Factors

### Use Emotiscope Novelty Approach If:
- Quiet passages in songs should NOT trigger fade (✓ K1 use case)
- Want to distinguish music from silence automatically (✓ Desired)
- Can afford ~512 bytes for novelty history (✓ Abundant on ESP32-S3)
- Want screensaver/idle management (✓ Desirable for user experience)

### Stick with Current Amplitude-Based If:
- Effects should fade on quiet music (✗ Undesirable)
- No screensaver/idle management needed (✗ Desirable)
- Minimal firmware changes required (partial concern)

---

## Emotiscope Parameters Specific to K1

### Recommended Tuning

| Parameter | Emotiscope | K1 Recommendation | Rationale |
|-----------|-----------|-------------------|-----------|
| Novelty history window | 128 samples | 128–256 | 2.67–5.3ms window @ 48kHz |
| Silence threshold | `raw > 0.5` | `raw > 0.5` | 50% contrast activates silence |
| Fade rate on silence | 10% per frame | 5–10% per frame | Adjust for desired transition speed |
| Screensaver timeout | 5 seconds | 5–30 seconds | User preference via REST API |
| Noise floor window | 20 samples, 250ms | 20 samples, 250ms | Proven values, no change needed |

### K1-Specific Additions

```c
// REST API endpoint for user control
PUT /api/config/silence_fade_ms
// Allow user to set darkness transition time (100–2000ms)

PUT /api/config/screensaver_timeout_s
// Allow user to disable/customize screensaver (0 = disabled)
```

---

## Validation Plan

### Test Cases (Phase 1)

1. **Quiet passage detection:**
   - Play soft piano piece
   - Verify LEDs remain bright (silence_level stays low)
   - Compare to current behavior (should fade with amplitude squelch)

2. **Silence detection:**
   - 30 seconds of silence
   - Verify fade completes over ~5–10 seconds
   - Compare to current behavior (immediate darkness)

3. **Ambient noise rejection:**
   - Room AC/fan running
   - Verify novelty_curve stays flat
   - Verify silence_level activates despite ~30dB ambient noise

4. **Music transition:**
   - Music stops abruptly
   - Verify fade starts within 1 frame
   - Verify fade takes ~5 seconds to black

### Metrics (Phase 2–3)

- **Screensaver activation:** Log time from silence to screensaver
- **User satisfaction:** (subjective) Does darkness feel natural?
- **False positives:** Does screensaver trigger during quiet music?

---

## References

- **Emotiscope algorithm analysis:** `/firmware-v3/docs/research/emotiscope-silence-detection-research.md`
- **Quick implementation reference:** `/firmware-v3/docs/reference/emotiscope-algorithms.md`
- **Current control bus:** `src/audio/contracts/ControlBus.h`
- **PipelineAdapter:** `src/audio/backends/esv11/EsV11Backend.cpp`

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created comparative analysis of silence detection approaches with implementation recommendations for K1, phased rollout plan, and validation strategy. |
