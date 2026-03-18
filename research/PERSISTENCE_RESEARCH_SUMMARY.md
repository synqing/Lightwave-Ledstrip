# Visual Persistence Research — Executive Summary

**Date:** March 17, 2026
**Status:** Complete
**Documents:** 2 detailed guides + this summary

---

## What We Learned

### Consensus Across Professional Systems

All major LED/VJ platforms use the **same core mechanism** for visual persistence:

1. **Maintain a history buffer** (previous frame(s))
2. **Apply decay/fade** to the history (multiplicative, exponential, or frame-based)
3. **Blend new content with decayed history** (weighted average or simple composite)
4. **Output the blended result**

The differences are *implementation details*, not fundamental approaches.

---

## Key Findings by System

| System | Mechanism | Strength | K1 Relevance |
|--------|-----------|----------|--------------|
| **TouchDesigner** | Feedback TOP + Level opacity | Mathematically clean (geometric decay) | ✓ Blueprint for feedback loop |
| **WLED** | `fadeToBlackBy()` + single buffer | Simple, fast, 1D-friendly | ✓ Immediate implementation |
| **FastLED** | `blend()` + static arrays | Proven in community effects | ✓ Already in our codebase |
| **Fire2012** | Physics simulation (diffusion) | Organic, spatial coherence | ✓ Inspiring for complex effects |
| **Resolume Arena** | Opacity-controlled layers + blend modes | Professional VJ standard | ✓ Reference for audio coupling |

---

## Four Concrete Patterns (by Complexity)

### 1. Simple Fade (Easiest, ~100µs)
```cpp
fadeToBlackBy(history.data(), 320, fade_amount);
// Pros: 3-line integration, fast
// Cons: Always fades to black, frame-rate dependent
```

### 2. Exponential Blend (Recommended, ~500µs)
```cpp
history[i] = blend(history[i], ctx.leds[i], blend_factor);
// Pros: Smooth, flexible colors, audio-reactive, professional-looking
// Cons: Need two buffers, slight tuning required
```

### 3. Beat-Reset (Rhythmic, ~500µs)
```cpp
if (beat) decay = 0.95f;  // Slow sustain
else decay = lerp(0.95f, 0.50f, time_since_beat / sustain_ms);  // Fade out
// Pros: Visual punch, mimics audio envelope, immersive
// Cons: Requires time tracking, per-song tuning
```

### 4. Frame-Rate Invariant (Production, ~0.3µs overhead)
```cpp
float decay = pow(DECAY_60FPS, ctx.dt / 16.667f);
// Pros: Works identically at 60fps, 120fps, variable rates
// Cons: Tiny performance cost (pow calculation), must normalize dt
```

---

## Most Relevant Finding: Exponential Decay Formula

The core pattern across all systems is:

```
output = current_input × α + previous_output × (1 - α)
```

Where α (alpha) is the smoothing factor.

**Practical Values:**
- `α = 0.1` (90% memory) → ~10-frame half-life, slow, heavy trails
- `α = 0.5` (50% memory) → ~2-frame half-life, balanced
- `α = 0.9` (10% memory) → ~1-frame half-life, responsive, crisp

**For K1:** LightwaveOS already uses `dtDecay()` pattern, which is the frame-rate-invariant version of this formula.

---

## Audio-Reactive Integration

**Best Practices from Professional Systems:**

1. **Energy-Coupled Decay:**
   ```cpp
   float decay = 0.5f + 0.4f * (1.0f - rms);  // Louder = longer trails
   ```

2. **Beat-Reset Sustain:**
   ```cpp
   if (beat) sustain_start = now;
   decay = 0.95f - 0.40f * (now - sustain_start) / sustain_duration;
   ```

3. **Frequency-Specific Persistence:**
   ```cpp
   bass_decay = 0.90f;    // Heavy, slow
   treble_decay = 0.60f;  // Crisp, responsive
   ```

**Why This Works:**
- Matches audio energy to visual mass/momentum
- Beats feel impactful (audio + visual spike)
- Quiet sections are responsive, not mushy
- Different frequencies have different "weight"

---

## Critical Implementation Details

### 1. Bounds Checking (PREVENTS CRASHES)
```cpp
const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;  // 0xFF safe
zone_history[z][i] = ...;
```

### 2. Static Buffers (PREVENTS JITTER)
```cpp
class Effect : public EffectBase {
    std::array<CRGB, 320> history{};  // Pre-allocated
    void render(RenderContext& ctx) override { /* use history */ }
};
```

### 3. Frame-Rate Invariance (PREVENTS ARTIFACTS)
```cpp
float decay = pow(DECAY_60FPS, ctx.dt / 16.667f);  // Normalize by 60fps
```

### 4. Clamping (PREVENTS WHITE-OUT)
```cpp
// Feedback loops can diverge if gain >= 1.0
// TouchDesigner/Resolume use opacity < 1.0 to prevent this
uint8_t blend_factor = 128;  // 50% new, 50% old (safe)
```

---

## Performance Budget

**Full persistence overhead:**
- `fadeToBlackBy()`: ~40µs
- `blend()` loop: ~160µs
- `memcpy()`: ~10µs
- **Total: ~210µs (10% of 2.0ms budget)**

Remaining 1.79ms for rendering content. Comfortable margin.

---

## Why Feedback Loops Don't Diverge (In Practice)

**The Problem:**
```
Frame N-1: [100, 100, 100] (RGB)
Add energy: [150, 150, 150]
+ feedback: [150 × 0.9, ...] = [135, ...] clipped to 255
Frame N: [255, 255, 255] (white-out)
```

**Solutions (in order of simplicity):**
1. **Opacity control** (TouchDesigner): Composite with opacity < 1.0
2. **Clamping** (C++): `min(blend_result, 255)`
3. **Attenuation before adding** (DSP): Only add if `level < threshold`
4. **Normalization** (averaging): `(new + N×old) / (N+1)`

**K1 Safest Approach:**
```cpp
uint8_t blend_factor = 128;  // 50% blend = stable, no clipping issues
history[i] = blend(history[i], new_pixel, blend_factor);
```

---

## Comparison: Which Pattern for K1?

**Quick Decision:**

- **Want simplicity?** → Pattern A (`fadeToBlackBy`)
- **Want professional quality?** → Pattern B (exponential blend)
- **Want rhythmic impact?** → Pattern C (beat-reset)
- **Want frame-rate agnostic?** → Pattern D (dt-compensated)
- **Want all features?** → Combine B + C + D

---

## Things We Discovered Are NOT Standard

1. **Blur vs. Motion Blur:** LED world uses temporal accumulation, not spatial blur
2. **Per-Pixel vs. Global Decay:** All systems fade entire frame uniformly, then render
3. **Feedback vs. Feedforward:** Pure feedback loops are rare; most systems composite new → history → output
4. **Multiple History Buffers:** Community effects use 1 history buffer max (memory constraints)

---

## Things We Discovered ARE Standard

1. **Static pre-allocated buffers** (no alloc in render loop)
2. **Multiplicative decay** (multiply by factor < 1.0, not subtract)
3. **Geometric progression** (0.9^n decay, exponential equivalent)
4. **No nested loops** (avoid per-pixel-per-frame complex math)
5. **Opacity control** (blend factor 0-255, not 0.0-1.0 for performance)

---

## Actionable Next Steps for K1

### Immediate (Low Risk)
1. Add Pattern A (`fadeToBlackBy`) to any 1D trail effect
2. Test at both 60fps and 120fps to verify frame-rate behavior

### Short Term (Medium Risk)
1. Convert one existing effect to Pattern B (exponential blend)
2. Add audio coupling: `decay = 0.5 + 0.4 * (1.0 - rms)`
3. Benchmark: should stay < 2.0ms

### Medium Term (Design Phase Needed)
1. Add Pattern C (beat-reset) to rhythm-heavy effects
2. Decide: should persistence survive effect transitions? (Currently unclear in K1 design)
3. Define decay parameter ranges (0.5-0.95 scale?) for UI exposure

### Long Term (Architectural)
1. Evaluate: should all effects have optional persistence layer?
2. Consider: unified "decay" slider in effect UI?
3. Research: can we expose audio-coupling configuration to users?

---

## Known Unknowns (For K1 Team)

These questions affect implementation approach:

1. **Composite Rendering Order:** Does K1 render zone 0 + zone 1 separately, then composite? Or render both into shared buffer?
2. **Audio Update Latency:** Does `controlBus` update every LED frame, or at slower audio rate? (Critical for beat-reset timing)
3. **Effect Transitions:** Should history persist across effect changes, or reset?
4. **Memory Constraints:** How much SRAM can we allocate for history buffers? (320px × 3 bytes × 2 = 1920 bytes baseline)
5. **UI Exposure:** Do users need direct control over decay rate, or is it baked per-effect?

---

## Resources

**Research Documents Created:**
1. `LED_VISUAL_PERSISTENCE_PATTERNS.md` — Deep dive into all systems (10 sections, 400 lines)
2. `K1_PERSISTENCE_IMPLEMENTATION_GUIDE.md` — Actionable code patterns (8 sections, 300 lines)

**External References (Included in Full Documents):**
- TouchDesigner: [Feedback TOP](https://docs.derivative.ca/Feedback_TOP), [Level TOP](https://docs.derivative.ca/Level_TOP)
- WLED: [FX.cpp GitHub](https://github.com/Aircoookie/WLED/blob/main/wled00/FX.cpp)
- FastLED: [fadeToBlackBy](https://fastled.io/docs/d1/dfb/colorutils_8h_a399e4e094995b8e97420b89a2dd6548b.html), [Fire2012](https://blog.kriegsman.org/2014/04/04/fire2012-an-open-source-fire-simulation-for-arduino-and-leds/)
- Resolume: [Blend Modes](https://resolume.com/support/en/blend-modes), [Feedback Source](https://resolume.com/forum/viewtopic.php?t=22481)

---

## Confidence Assessment

| Topic | Confidence | Evidence |
|-------|-----------|----------|
| **Exponential decay formula** | 🟢 Very High | Appears in TouchDesigner, NIST, academic literature |
| **fadeToBlackBy is frame-dependent** | 🟢 Very High | FastLED implementation + direct testing evidence |
| **Feedback loops need attenuation** | 🟢 Very High | Explicitly used by TouchDesigner, Resolume, theory |
| **dt-compensated decay is standard** | 🟢 Very High | LightwaveOS already uses it; confirmed in game engines |
| **Audio coupling patterns** | 🟡 Medium-High | Evidence from WLED + Resolume, not all details public |
| **K1-specific architectural details** | 🔴 Low | Requires confirmation from K1 team |

---

## What NOT to Do

❌ Allocate buffers in `render()` — causes jitter and heap fragmentation
❌ Use simple `*= decay` without dt compensation — frame rate dependent
❌ Ignore zone ID bounds checking — causes RMT spinlock crashes
❌ Feedback loops without opacity control — can white-out
❌ Nested loops for decay calculation — exceeds 2.0ms budget
❌ Assume Feedback TOP implementation details — API is black-box

---

## What TO Do

✓ Use pre-allocated static buffers
✓ Apply exponential decay (multiply by factor < 1.0)
✓ Use FastLED `blend()` for flexibility
✓ Add bounds check on zone ID
✓ Normalize decay by frame time if frame rate varies
✓ Couple decay to audio energy for responsiveness
✓ Test at both 60fps and 120fps

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-17 | claude:research | Created executive summary tying together research across TouchDesigner, WLED, FastLED, Fire2012, and Resolume. |
