---
abstract: "Final port plan for the spazz/jerk bug afflicting ChevronWavesEffect, ChevronWavesEffectEnhanced, SnapwaveLinearEffect, LGPWaveCollisionEffect. Synthesised from 16 SSA investigations (SSA1–10 internal + SSA-A/C/D/E/F/H against upstream SB 3.1.0/4.1.0/4.1.1, Emotiscope 1.2, and LightwaveOS internal Snapwave docs). Establishes that K1's broken 4 are NOT canonical SB descendants. Recommends two-tier action: (1) IMMEDIATE port to canonical SB 4.1.0 patterns by mechanism, ship-fast. (2) FUTURE infrastructure: BeatDetectionAPI / tempi[] bank port, architectural alignment. Captain decision points listed at end."
---

# Spazz Redesign — Port Plan

**Date**: 2026-04-30
**Branch**: `feature/synergy-topology-phase-0-1`
**Outstanding work**: SSA-B (SB 4.1.1 diff vs 4.1.0) and SSA-G (per-effect port pseudocode) hit Anthropic usage cap and did NOT complete. Resume after cap reset (11:50am AEST). They are NOT blockers — sufficient information exists in SSA-A (4.1.0 baseline) + SSA-D (Emotiscope 1.2 patterns) + SSA-H (LightwaveOS internal docs) to write the port plan.

---

## §1 Verdict: K1's broken 4 are imposters

Cross-source convergent finding (SSA-A, SSA-C, SSA-3, SSA-2):

| Effect | Claimed lineage | Actual upstream cousin |
|---|---|---|
| `ChevronWavesEffect` | "Sensory Bridge reference" (per its own comments) | **None** — closest mechanism is SB 4.1.0 `kaleidoscope` Pattern C |
| `ChevronWavesEffectEnhanced` | "Sensory Bridge reference" | **None** — closest mechanism is SB 4.1.0 `bloom` Pattern B |
| `SnapwaveLinearEffect` | Cited as "ORIGINAL SNAPWAVE ALGORITHM - Restored from SensoryBridge `light_mode_snapwave()`" | **`light_mode_snapwave` does not exist in any SB version read** (3.1.0, 4.1.0, 4.1.1) |
| `LGPWaveCollisionEffect` | LGP-specific K1 design | No SB cousin; mechanism is two `bloom` Pattern B scrollers blended additively |

The K1 internal `firmware-v3/src/effects/ieffect/sensorybridge_reference/` directory is a K1-team mimic that drifted from upstream and self-cites as authority. The K1 ratification doctrine (`EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md`) endorses this drifted pattern as canonical via circular reference.

The `m_phase += speedNorm × 240 × smoothedSpeed × dt` pattern that K1 docs name as "the canonical reference" exists nowhere in upstream SB or Emotiscope. It is an inherited K1 invention.

---

## §2 The canonical motion vocabulary (verified upstream)

From SSA-A (SB 4.1.0) + SSA-D (Emotiscope 1.2) + SSA-C (SB 3.1.0) + SSA-F (audio infrastructure):

### SB 4.1.0 motion patterns (3 only)

- **Pattern A — Amplitude→Position direct + motion-blur trail.** `vu_dot`, `chromagram_dots`. Audio amplitude drives a position. `draw_dot()` with linear-interp line-fill provides implicit smoothing across the previous-frame buffer. Smoothing is asymmetric attack/decay EMA at fixed alpha (e.g. 0.1 attack / 0.9999 decay).
- **Pattern B — Sub-pixel scroll of previous-frame buffer.** `bloom`. `draw_sprite(leds_16_prev, alpha=0.99, position=...)` shifts buffer outward and multiplies by 0.99. New content additively blended in centre. Edge-fade `prog²` on last 32 LEDs. NO phase accumulator.
- **Pattern C — Phase accumulator on band energy → noise sample index.** `kaleidoscope`. Per-channel `phase += band_energy × frame_step`, then `inoise16(phase)` gives the geometry. Phase walks based on integrated band energy, not raw `dt`.

### Emotiscope 1.2 primitives (3 in `leds.h`)

- `draw_dot(slot, hue, value, position)` — per-slot position memory + motion-blur smear
- `draw_sprite(buffer, alpha, position, scale)` — additive sub-pixel scroll with multiplicative trail
- Direct fill `leds[i] = hsv(hue, sat, magnitude)`

### Three audio→motion laws (Emotiscope, ONE per mode)

1. `sin(tempi[i].phase)` for rhythmic motion (Metronome, Beat Tunnel, Tempiscope)
2. `vu_level_smooth → position` for amplitude-driven motion (Analog)
3. Scroll feedback buffer N pixels/frame (Bloom, Beat Tunnel)

### Common to ALL canonical motion (SB + Emotiscope)

- **NO `dt` in phase update.** SB uses fixed-step integration at LED_FPS; Emotiscope uses `phase_radians_per_reference_frame × delta` where delta is integer frames not seconds.
- **NO Spring / second-order dynamics anywhere.** All smoothing is one-pole IIR with fixed alpha, sometimes asymmetric attack/decay.
- **NO binary `is*Hit()` triggers driving continuous parameters.** Beat impacts are sinusoidal phase or scroll-rate modulation — never step changes.
- **MOOD scales smoothing alpha, NOT motion velocity.**
- **Audio rate-coupling on motion is forbidden** by the architecture: rate is either time-driven (clock or `tempi[].phase`) or scroll-by-fixed-pixels-per-frame; amplitude is what audio modulates.

### LightwaveOS canonical Snapwave (SSA-H)

```cpp
// Canonical Snapwave per SNAPWAVE_BEAT_DETECTION_API.md + INTEGRATION_EXAMPLES.md
float oscillation = BeatDetectionAPI::getBeatOscillation();   // [-1, +1] upstream
float position    = tanhf(oscillation * 2.0f) * 0.7f;
position *= (1.0f + beat_strength * 0.5f);
// Render dot at halfStrip + position * (halfStrip - 1)
```

`BeatDetectionAPI::getBeatOscillation()` is computed UPSTREAM from spectrogram + chromagram + tempo + onset, smoothed via `1 - exp(-dt × 10Hz)`, with **3s latched silence mode** (halves amplitude, lengthens trails — never zeroes motion).

K1 has no `BeatDetectionAPI`. The fields exist on ControlBus piecewise (`tempoConfidence`, `beatPhase`, etc.) but no consolidated `getBeatOscillation()` accessor.

---

## §3 What's actually broken (root cause, ranked)

| # | Mechanism | Affected | Evidence |
|---|---|---|---|
| 1 | **Audio-driven phase rate via triple-multiplication** `speedNorm × dt × smoothedSpeed`, all carrying audio modulation | All 4 | SSA-10, SSA-4 |
| 2 | **Snapwave's `Σ chroma_i × sin(t × multi_freq_i)` is mathematically discontinuous on chord transitions** | Snapwave only | SSA-H, SSA-10 |
| 3 | **`fadeToBlackByDt(K<255)` trail-smear used as motion** instead of redrawing-from-smoothed-state | All 4 | SSA-C |
| 4 | **`isSnareHit()` / `isHihatHit()` 1-frame binary triggers driving continuous params** | ChevronWaves, ChevronEnh, LGPWaveCollision | SSA-8 |
| 5 | **Frame-rate-dependent decay** `m_speedTarget *= 0.95 + 1.0·0.05` not dt-corrected | LGPWaveCollision | SSA-8 |
| 6 | **Audio-modulated `tanh` slope** `tanh(v × (k0 + k1·audio))` ≡ whole-strip flicker | ChevronWaves, ChevronEnh | SSA-4 |
| 7 | **`sin(kx-φ) + sin(kx+φ)` is by trig identity a STANDING WAVE** — phase modulates whole-strip amplitude not motion | LGPWaveCollision | SSA-4 |
| 8 | **`heavy_bands` deeper smoothing (`UpdateFromHop` envelope) is dead code on K1** — only ~80ms shallow tau active | All 4 | SSA-5 |

Mechanisms 1, 2, 4, 5, 6, 7 are all music-coupled and explain why silence is fine. Mechanism 3 is a compounding factor that makes any per-frame spike persist in trail.

---

## §4 Two-tier port plan

### Tier 1 — IMMEDIATE (fix the spazz now, no infrastructure changes)

Per-effect rewrite using SB 4.1.0 patterns mapped by mechanism. K1 audio API used as-is. No new ControlBus fields. No new EffectContext accessors. No Spring class changes. Effect-layer-only blast radius.

#### LGPWaveCollisionEffect — **Port first** (cleanest single-cause diagnosis)

Adopt SB 4.1.0 `bloom` Pattern B. Two-pass scroll (centre→outward) with `alpha=0.96` multiplicative decay. Inject snare-driven brightness pulses centred on LED 79/80, additive, no rate modulation. `m_speedTarget` removed entirely (it was the sustained-hihat sawtooth). Standing-wave `sin(kx-φ)+sin(kx+φ)` formula deleted.

```cpp
// LGPWaveCollisionEffect::render() — port skeleton (Tier 1)
const float rawDt = enhancement::getSafeDeltaSeconds(ctx.rawDeltaTimeSeconds);
const float speedNorm = ctx.speed / 50.0f;     // user knob ONLY

// Multiplicative decay across whole strip (replaces fadeToBlackByDt + sin(kx±φ))
//   tau ≈ 200ms → alpha = 1 - exp(-dt/0.20)
const float scrollAlpha = 1.0f - expf(-rawDt / 0.20f);
for (uint16_t i = 0; i < ctx.ledCount; ++i) {
    ctx.leds[i].nscale8((uint8_t)(255.0f * (1.0f - scrollAlpha)));
}

// Outward sub-pixel scroll (1 LED per 30ms baseline, modulated by user speed)
const float scrollPxPerSec = 33.0f * speedNorm;
m_scrollAccum += scrollPxPerSec * rawDt;
while (m_scrollAccum >= 1.0f) {
    // Shift outward from centre (79/80)
    for (uint16_t i = ctx.ledCount - 1; i > centre; --i) ctx.leds[i] = ctx.leds[i-1];
    for (uint16_t i = 0;             i < centre; ++i) ctx.leds[i] = ctx.leds[i+1];
    m_scrollAccum -= 1.0f;
}

// Snare → centred pulse (audio drives BRIGHTNESS, never motion rate)
if (ctx.audio.isSnareHit()) {
    m_pulse = 1.0f;  // hold-and-decay envelope, dt-corrected
}
m_pulse *= expf(-rawDt / 0.18f);  // 180ms decay

uint8_t pulseV = (uint8_t)(255.0f * m_pulse * intensityNorm);
ctx.leds[centre]   += ctx.palette.getColor(chromaHue, pulseV);
ctx.leds[centre-1] += ctx.palette.getColor(chromaHue, pulseV);
```

#### ChevronWavesEffect — Adopt SB 4.1.0 `kaleidoscope` Pattern C

Replace `m_phase += 240 × smoothedSpeed × dt` with **fixed-rate phase from time only**: `m_phase += BASE_RATE × speedNorm × rawDt`. `BASE_RATE` becomes a constant (180.0f rad/s — tuned from `kaleidoscope` 100+500·MOOD = ~350 rad/s scaled to K1 strip width). `freqBase` 0.25 → 0.18. `tanh` slope FIXED (no audio modulation). Audio drives `audioGain` LINEARLY post-tanh. `isSnareHit()` removed from per-pixel loop — replaced with envelope updated once-per-frame, drives BRIGHTNESS not slope.

#### ChevronWavesEffectEnhanced — Same as basic + use `rawDt` (not scaled `dt`) on Spring

Plus: PLL clamp on lock-acquire delta to prevent phase snap on confidence-Schmitt crossing.

#### SnapwaveLinearEffect — **Most structural rewrite**

Drop `Σ chroma × sin(t × multi-freq)` entirely. Replace with **chroma-centroid position follower** (SSA-10's recommendation):

```cpp
// SnapwaveLinearEffect::computeOscillation() — Tier 1 replacement
// Position is the smoothed angle of the chroma vector — moves continuously with
// chord changes instead of jumping between sin(...) basis functions.
float ax = 0.0f, ay = 0.0f;
for (uint8_t i = 0; i < 12; ++i) {
    float c = ctx.audio.getChroma(i);
    float th = (float)i * (PI / 6.0f);  // 12 chroma → angle on circle
    ax += c * cosf(th);
    ay += c * sinf(th);
}
float chromaAngle = atan2f(ay, ax);  // [-PI, PI]
// Smooth via wrapped EMA (atan2 of smoothed vector — handles wrap correctly)
m_chromaVecX += (ax - m_chromaVecX) * (1.0f - expf(-dt / 0.12f));  // 120ms tau
m_chromaVecY += (ay - m_chromaVecY) * (1.0f - expf(-dt / 0.12f));
float smoothAngle = atan2f(m_chromaVecY, m_chromaVecX);
float oscillation = sinf(smoothAngle);  // [-1, +1] continuous in chord space

// Apply tanh "snap" + RMS amplitude modulation (canonical Snapwave shape)
oscillation = tanhf(oscillation * TANH_SCALE);
return oscillation;
```

Plus: replace hard `rms < threshold` gate with smooth `silentScale` ramp (SSA-2 recommendation). Linear (not quadratic) trail-fade falloff.

### Tier 2 — FUTURE (architectural alignment, post-Tier-1 hardware validation)

Only if Captain decides to commit to canonical alignment beyond the 4 effects:

1. **Add `tempi[]` bank** to ControlBus per SSA-F (16 bins, Goertzel on novelty curve, integrated phase). Then expose `ctx.audio.tempoBankPhase(i) / Magnitude(i) / Beat(i)`.
2. **Wire `ctx.mood` into spectrogram low-pass** per SB 4.1.1 pattern — MOOD knob as smoothing-α controller, not motion-velocity controller.
3. **Implement `BeatDetectionAPI::getBeatOscillation()`** consolidating spectrogram + chromagram + tempo + onset into the canonical scalar. Then SnapwaveLinear becomes the 4-line consumer per LightwaveOS internal docs.
4. **Update K1 effect framework doctrine** — strip the self-citing canonical claims, replace with attribution to upstream SB/Emotiscope patterns.

---

## §5 Implementation order

1. **LGPWaveCollisionEffect** first. Cleanest mechanism, single-cause-dominant (sustained hi-hat sawtooth + standing-wave formula). Confirms the kinematic-scroll-with-fade pattern works before rolling to 3 more effects.
2. **ChevronWavesEffect** second. Phase-accumulator rewrite + tanh slope fix.
3. **ChevronWavesEffectEnhanced** third. Diff against basic ChevronWaves only — `dt` → `rawDt` on Spring + PLL clamp.
4. **SnapwaveLinearEffect** last. Most structural change (chroma-centroid replacement).

After EACH step: build K1v1, flash, hardware A/B with bass-heavy + sustained-hihat + vocal-driven music. Captain sign-off before commit. Per `feedback_test_them_all.md` and `feedback_hardware_test_before_commit.md`.

---

## §6 Captain decision points (must answer before any code touches tree)

1. **Tier 1 vs Tier 1+2**: Tier 1 alone fixes the spazz with effect-layer-only changes. Tier 2 adds infrastructure for canonical alignment but is a separate multi-week effort. **Recommendation: ship Tier 1 now, schedule Tier 2 separately.**

2. **Snapwave intent**: chroma-centroid replacement (SSA-10 recommendation, kept above) vs port the LightwaveOS internal canonical 4-liner using only existing K1 ControlBus fields (no `BeatDetectionAPI` write). The latter would use `ctx.audio.beatPhase()` + `ctx.audio.beatStrength()` if available. **Need verification of K1 accessor names.** Dispatch SSA-I to confirm what's available?

3. **`fadeToBlackByDt` policy**: SB 3.1.0 doesn't use it at all. Tier 1 plan above keeps it as the trail mechanism (multiplicative decay across whole strip). Captain — is full removal of `fadeToBlackByDt` from these 4 effects in favour of explicit shift-buffer style something to consider, or out of scope for this fix?

4. **Resume SSA-B and SSA-G after cap reset**? They would have:
   - SSA-B: SB 4.1.1 diff vs 4.1.0 — would confirm whether 4.1.1 changed any canonical motion patterns
   - SSA-G: per-effect port-ready C++ pseudocode (closer to drop-in than the sketch in §4)
   
   **Recommendation: skip SSA-B (4.1.0 baseline is sufficient), resume SSA-G after cap reset before final implementation.**

5. **Doctrine cleanup**: SSA-2 found K1 written doctrine endorses the broken pattern as canonical. After Tier 1 lands, should we rip the false-canonical claims out of `EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` and `EFFECT_FRAMEWORK_STANDARD.md`? Separate task either way.

---

## §7 What stays as-is (out of scope)

- I-1 contrast migration (4 effects already converted to `applyContrast(bin, kSbK1SquareIter)` — Captain hardware-validated, commit `04bee2e4`)
- The Spring class itself (Tier 1 doesn't replace it; effects use it differently or not at all)
- AUTO_SPEED behaviour on K1 (SSA-5 vs SSA-4 conflict on whether `liveliness` is alive on K1; out of scope for this fix)
- All other 195+ effects in the tree
- ControlBus / AudioActor / EsV11Adapter (Tier 1 is effect-layer only)
- K1 doctrine documents (Tier 2)

---

## §8 SSA index

| SSA | Topic | Status | File |
|---|---|---|---|
| 1 | K1 doctrine extract | ✓ | SSA1_canonical_motion_doctrine.md |
| 2 | K1 audio→visual mapping | ✓ | SSA2_audio_visual_motion_mapping.md |
| 3 | K1 internal SB-mimic motion model | ✓ | SSA3_sb_reference_motion_model.md |
| 4 | Working K1 LGP effects | ✓ | SSA4_working_lgp_motion_audit.md |
| 5 | ControlBus signal smoothness | ✓ | SSA5_audio_signal_smoothness.md |
| 6 | Spring numerical analysis | ✓ | SSA6_spring_numerical_analysis.md |
| 7 | dt path + frame timing | ✓ | SSA7_dt_path_and_frame_timing.md |
| 8 | Percussion trigger semantics | ✓ | SSA8_percussion_trigger_semantics.md |
| 9 | EffectContext audio API | ✓ | SSA9_effect_context_audio_api.md |
| 10 | Cross-effect differential + redesign | ✓ | SSA10_differential_diagnosis_and_redesign.md |
| A | Upstream SB 4.1.0 lightshow_modes | ✓ | canonical_SB_4_1_0.md |
| B | Upstream SB 4.1.1 lightshow_modes | **CAP HIT — re-run after 11:50am AEST** | (not written) |
| C | Upstream SB 3.1.0 lightshow_modes | ✓ | canonical_SB_3_1_0.md |
| D | Emotiscope 1.2 active modes | ✓ | canonical_emotiscope_active.md |
| E | Emotiscope 1.2 beta + waveform | ✓ | canonical_emotiscope_beta.md |
| F | SB+Emotiscope audio infrastructure | ✓ | canonical_audio_infrastructure.md |
| G | Per-effect port pseudocode | **CAP HIT — re-run after 11:50am AEST** | (not written) |
| H | LightwaveOS Snapwave + beat docs | ✓ | canonical_snapwave_beat_api.md |

**Synthesis**: SYNTHESIS.md (this directory) — written before SSA-A/B/C/D/E/F/G/H landed, superseded by this PORT_PLAN.md.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | Claude (claude-sonnet-4-6) | Created — synthesised 16 SSAs (10 internal + 6 upstream of 8 launched; 2 hit Anthropic cap). Final port plan with Tier 1/Tier 2 split + Captain decision points. |
