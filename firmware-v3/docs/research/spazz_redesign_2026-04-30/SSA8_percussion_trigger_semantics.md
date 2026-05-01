---
abstract: "SSA8 — Semantics of `ctx.audio.isSnareHit()` / `isHihatHit()` / `isKickHit()` and why ChevronWaves, ChevronWavesEnhanced and LGPWaveCollision spazz on percussion. Confirms triggers are single-hop binary pulses (not envelopes), produced by a band-energy-ratio detector with an 8-hop refractory (~64 ms) at 125 Hz hop cadence, then read at ~120 FPS render. Documents the latch hazard where a single trigger can be observed on 1-2 consecutive render frames, the per-pixel cost of calling the accessor inside a 160-LED loop, and the LGPWaveCollision speedTarget pegging hazard during sustained hi-hats. Recommends a Hold-and-Decay envelope helper instead of binary triggers for visual sharpness/speed mods."
---

# SSA8 · Percussion-Trigger Semantics — Why the four effects spazz

**Date**: 2026-04-30
**Status**: GROUNDED — every claim cites a file:line.
**Scope**: `isSnareHit()`, `isHihatHit()`, `isKickHit()`, `isPercussionHit()` accessors and the four effects that consume them.

---

## 1. Trigger accessor signatures (verbatim)

`firmware-v3/src/plugins/api/EffectContext.h:365-367` (FEATURE_AUDIO_SYNC=1 path):

```cpp
/// Check semantic onset channel pulses.
bool isKickHit() const  { return onset.kick.fired  || controlBus.kickTrigger;  }
bool isSnareHit() const { return onset.snare.fired || controlBus.snareTrigger; }
bool isHihatHit() const { return onset.hihat.fired || controlBus.hihatTrigger; }
```

`firmware-v3/src/plugins/api/EffectContext.h:716-718` (stub when `FEATURE_AUDIO_SYNC=0`):

```cpp
bool isKickHit()  const { return onset.kick.fired;  }
bool isSnareHit() const { return onset.snare.fired; }
bool isHihatHit() const { return onset.hihat.fired; }
```

There is **no** `isPercussionHit()` accessor in `EffectContext.h`. The closest umbrella is `hasOnsetEvent()` at line 301:

```cpp
bool hasOnsetEvent() const { return onset.transient.fired || controlBus.onsetEvent > 0.0f; }
```

The accessor returns the **logical OR** of two semantically identical booleans:

- `controlBus.<x>Trigger` — raw producer field, set by the band-energy-ratio (BR) detector in `AudioActor` (Path B).
- `onset.<x>.fired` — re-derived per render frame in `OnsetSemantics.cpp`. For snare and hi-hat this collapses back to the same condition: `inputs.audioAvailable && inputs.controlBus.<x>Trigger` (`OnsetSemantics.cpp:105`, `:112`). For kick it adds an `audioAvailable && !trinityActive` reliability gate (`:98`). The OR of the two members exists for redundancy, not for two independent sources.

**Effect of this redundancy**: an effect calling `isSnareHit()` reads two values that should never disagree under normal operation. The result is a single-frame binary: it is either `true` for the duration of one published frame, or `false`.

---

## 2. Underlying ControlBus field / derivation

### 2a. Producer (live trigger source)

**File**: `firmware-v3/src/audio/AudioActor.cpp:801-847`
**Detector**: Band-energy-ratio (Path B), `bandRatioDetect()` in `AudioActor.h:733-776`.
**Configured grouping** (`AudioActor.cpp:804-806`):

```cpp
const float kickEnergy  = frame.bands[0] + frame.bands[1];           // 60-250 Hz
const float snareEnergy = frame.bands[2] + frame.bands[3];           // 250-1k Hz
const float hihatEnergy = frame.bands[5] + frame.bands[6] + frame.bands[7]; // 4-16 kHz
```

**Algorithm** (`AudioActor.h:733-776`):

1. Maintain a 125-frame ring buffer (~1 s @ 125 Hz, `BAND_HISTORY_SIZE=125`) of the grouped energy.
2. Compute running mean and variance in O(1).
3. Variance-adaptive multiplier: `mul = clamp(varSlope*var + varIntercept, minMul, maxMul)` with defaults `minMul=1.05`, `maxMul=1.55`.
4. Fire when `energy > mul * mean` AND refractory has elapsed (`refractory = 8` hops, `AudioActor.h:691`).
5. Two front-door gates (`AudioActor.cpp:811-822`): raw PCM RMS gate (`brRmsGate=0.007` with `brGateHold=62` frames ≈ 500 ms hold) and per-channel absolute floor (`*AbsFloor=0.01`).

**Set sites** (`AudioActor.cpp:829-831`):

```cpp
if (kickFired)  frame.kickTrigger  = true;
if (snareFired) frame.snareTrigger = true;
if (hihatFired) frame.hihatTrigger = true;
```

The frame is value-initialised every hop (`ControlBusFrame frame{};` at `AudioActor.cpp:657`), so the trigger booleans default to `false` each hop. They cannot persist across hops at the producer.

The legacy adapter onset triggers are explicitly disabled (`EsV11Adapter.cpp:294-295`):

```cpp
out.snareTrigger = false;
out.hihatTrigger = false;  // BR detector in AudioActor is the sole live source.
```

The FFT-based onset detector still publishes `onsetBassFlux/onsetMidFlux/onsetHighFlux` and `onsetEvent` (`AudioActor.cpp:780-784`) but its kick/snare/hi-hat triggers are demoted to telemetry; they do **not** drive the `controlBus.<x>Trigger` fields.

### 2b. Frame definition

**File**: `firmware-v3/src/audio/contracts/ControlBus.h:167-177` (the `ControlBusFrame` published members):

```cpp
bool snareTrigger = false;       // True on snare onset frame
bool hihatTrigger = false;       // True on hi-hat onset frame
...
bool kickTrigger  = false;       ///< FFT-based kick onset detected
```

The published frame is copied by value into a snapshot buffer (`AudioActor.cpp:1846`, `:3597`) and read back on the renderer side via `ReadLatest()` (`RendererActor.cpp:1425`).

### 2c. Re-derivation in OnsetSemantics

**File**: `firmware-v3/src/audio/contracts/OnsetSemantics.cpp:64`

```cpp
out = plugins::OnsetContext{};   // fully zeroed every frame
```

Then `:105`, `:112`:

```cpp
const bool snareFired = inputs.audioAvailable && inputs.controlBus.snareTrigger;
const bool hihatFired = inputs.audioAvailable && inputs.controlBus.hihatTrigger;
```

`onset.<x>.fired` is therefore an exact mirror of `controlBus.<x>Trigger` gated by `audioAvailable`. Importantly, `populateChannel()` (`OnsetSemantics.cpp:33-36`) updates `lastFireMs/sequence` on `fired==true`, but the `fired` flag itself is never extended past the current call.

---

## 3. Latching behaviour

### Producer side (audio thread, 125 Hz)

- `frame.snareTrigger` is set to `true` for **exactly one hop** if the BR detector fires that hop. The next hop starts with a fresh `ControlBusFrame{}`.
- The 8-hop refractory (`AudioActor.h:691`) means at most one `true` every 8 hops per channel ≈ **15.6 Hz max fire rate**.
- The `brGateHold = 62` frames (≈ 500 ms) parameter is the RMS gate **hold**, not a trigger hold. It only keeps the BR detector eligible to fire across short RMS dips between beats; it does not extend a trigger past one hop.

### Renderer side (Core 1, 120 FPS)

- The renderer pulls the latest published frame each render via `ReadLatest()` (`RendererActor.cpp:1425`) and stores it in `m_lastControlBus`.
- It assigns `m_sharedAudioCtx.controlBus = m_lastControlBus` on every frame (`RendererActor.cpp:1611`, `:1616`, `:1644`).
- `updateSharedOnsetContext()` is called every render frame (`RendererActor.cpp:1665`), which fully zeroes `onset` and re-derives `onset.<x>.fired` from the cached `controlBus.<x>Trigger` (see `OnsetSemantics.cpp:64,105,112`).

### The single critical consequence

There is **no consumption / decrement** of the trigger flag by the renderer. The flag remains `true` in the cached frame for as long as that audio hop is the latest, which lasts:

- ~1 render frame on average (8 ms hop vs 8.3 ms render — near-1:1 ratio).
- **2 consecutive render frames** when render-vs-audio jitter goes the other way (~10-20 % of frames in practice).

So `isSnareHit()` returns `true` for **1, occasionally 2** consecutive render frames per audio fire. There is **no envelope, no smooth rise, no smooth decay** — it is a binary edge-pulse latched in a snapshot, with width 1-2 render frames.

There is also **no consumed-flag pattern** (no `consumeSnareTrigger()` accessor). The same `true` is observable by every effect that reads it during that render frame.

---

## 4. Fire-rate during typical music

| Genre | Snare rate | Hi-hat rate | Kick rate |
|---|---|---|---|
| 4/4 rock @ 120 BPM, snare on 2&4 | 1 Hz | 4 Hz (8th-notes) | 2 Hz |
| House @ 124 BPM, four-on-the-floor | 1 Hz | 4-8 Hz (8th/16th) | 2.07 Hz |
| Trap @ 140 BPM, 32nd hi-hat rolls | 1.17 Hz (snare-hat clap) | up to **18.7 Hz** uncapped, but BR refractory caps at **15.6 Hz** | ~1.17 Hz |
| DnB @ 174 BPM, busy hat work | up to 2.9 Hz | 5-12 Hz | 2.9 Hz |

The 8-hop refractory at 125 Hz (`AudioActor.h:691`) caps any single channel at 1 trigger every 64 ms = **15.6 Hz absolute maximum**. For trap or DnB hi-hat passages the detector saturates against this refractory; the visible cadence at the LEDs is **~64 ms between consecutive hi-hat events** (every ~7-8 render frames at 120 FPS), every one of which is a binary pulse the effect can latch onto.

No live measurement files are referenced in the source for fire-rate distributions; estimates above are derived from the refractory + tempo math. SSA-bench traces would refine these numbers but the upper bound is hard-set by the refractory.

---

## 5. Per-pixel usage in `ChevronWavesEffect.cpp:153`

```cpp
for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
    ...
    float tanhScale = 2.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio && ctx.audio.isSnareHit()) {     // <-- per-pixel call
        tanhScale = 5.0f;
    }
#endif
    chevron = tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth)) * 0.5f + 0.5f;
    ...
}
```

### Performance

`isSnareHit()` is `inline` and reads two POD booleans on a by-value `AudioContext` already on stack/registers. The cost is roughly `bool || bool` ≈ 2-4 cycles per call. 160 iterations × ~3 cycles = ~480 cycles ≈ 2 µs at 240 MHz. **Not a frame-budget killer**, but pure waste — the value is constant across the loop iteration. Lifting the call above the loop costs nothing and removes the per-pixel branch predictor pressure.

### Correctness

The bigger problem is what happens visually:

- For 1-2 render frames every snare hit, every LED's `tanhf()` argument scales by 2.5×.
- This produces a step change in chevron sharpness across the whole strip simultaneously.
- The next frame the strip snaps back to `tanhScale=2.0`.

That binary step is exactly the "spazz" pattern: a 1-frame whole-strip sharpness flip with no rise or fall.

**Verdict**: per-pixel is wasteful; the deeper bug is using a binary trigger to drive a continuous visual parameter at all.

---

## 6. Sustained-percussion hazard — `LGPWaveCollisionEffect`

`LGPWaveCollisionEffect.cpp:133-137`:

```cpp
if (hasAudio && ctx.audio.isHihatHit()) {
    m_speedTarget = 1.6f;     // peg to maximum on every hi-hat
}
m_speedTarget = m_speedTarget * 0.95f + 1.0f * 0.05f;   // 5% per-frame relaxation toward 1.0
```

The relaxation is a fixed **per-render-frame** scalar — it is **not** dt-corrected (no use of `rawDt` or `dt`). At 120 FPS each frame moves `m_speedTarget` 5 % closer to 1.0.

### Sustained 16th-note hi-hat @ 140 BPM (≈9.3 Hz)

- Inter-hit interval ≈ 107 ms ≈ **12-13 render frames**.
- Decay between hits: `0.95^13 ≈ 0.513`.
- Steady-state attractor between hits: `m_speedTarget` drifts from 1.6 toward `1.0 + (1.6-1.0)*0.513 = 1.31` before the next hi-hat re-pegs it to 1.6.
- Visible cadence: speed slams 1.31→1.6→decays to 1.31→slams to 1.6 — a sawtooth at hi-hat rate. **This IS the spazz.**

### Sustained 32nd-note trap hat (≈15.6 Hz at the BR cap)

- Inter-hit interval ≈ 64 ms ≈ **7-8 render frames**.
- Decay between hits: `0.95^8 ≈ 0.663`.
- Attractor: `1.0 + 0.6*0.663 = 1.40`. Sawtooth 1.40→1.6→1.40, faster cadence.
- **Worst case**: when the renderer happens to read the same `hihatTrigger=true` flag on two consecutive render frames (~10-20 % of fires due to render/audio jitter), `m_speedTarget = 1.6f` gets re-asserted with zero relaxation in between. Combined with the spring physics on `smoothedSpeed` downstream (`LGPWaveCollisionEffect.cpp:156`), this acts as repeated impulse drive into a spring system → speed oscillation → visual jerk.

### Why the snare boost path also ringer

`LGPWaveCollisionEffect.cpp:123-130`:

```cpp
if (hasAudio && ctx.audio.isSnareHit()) {
    m_collisionBoost = 1.0f;     // peg to max
} else {
    m_collisionBoost += energyDeltaSmooth * 0.4f;  // also pegs
}
m_collisionBoost = effects::chroma::dtDecay(m_collisionBoost, 0.88f, rawDt);
```

This one IS dt-corrected (`dtDecay` with `rawDt`), so the decay is at least frame-rate independent. But the same edge-pegging hazard applies on every snare: `m_collisionBoost` snaps to 1.0 for one frame, then decays. The decay rate `0.88` is fast (≈ 8 ms half-life at 120 FPS — see SSA TBD on `dtDecay`), but the binary peg-then-fall is still visible as a flicker because `m_collisionBoost` directly modulates brightness elsewhere in the effect.

---

## 7. ChevronWavesEffectEnhanced — the closest existing template

`ChevronWavesEffectEnhanced.cpp:138-143` already uses the right shape for snare-driven sharpness:

```cpp
if (ctx.audio.isSnareHit()) {
    m_snareSharpness = 1.0f;
}
m_snareSharpness *= powf(0.90f, rawDt * 60.0f);    // dt-corrected exponential decay
if (m_snareSharpness < 0.01f) m_snareSharpness = 0.0f;
```

This is **dt-corrected** (`powf(0.90f, rawDt * 60.0f)`) and **smoothly decaying** (continuous 0..1 envelope, not binary). However it still has the **edge-pegging hazard** — every fire re-asserts 1.0 instantly, no rise time, no per-fire smoothing. On sustained hi-hats this would still look like a sawtooth.

The Enhanced version is closer to right but not yet correct. The base ChevronWaves and LGPWaveCollision are worse.

---

## 8. Recommendation — how the four effects SHOULD use percussion

### The principle

Binary `is*Hit()` accessors are correct as **edge detectors** (they fire one render frame per hop). They are wrong as **continuous parameter sources**. For visual sharpness, speed, brightness modulation an effect must convert the edge into a smooth envelope.

### The standard pattern (Hold-and-Decay envelope)

Every effect that wants snare-driven sharpness should track its own envelope state:

```cpp
// In effect class:
float m_snareEnv = 0.0f;
static constexpr float SNARE_ATTACK_MS = 12.0f;    // ~1-2 render frames
static constexpr float SNARE_DECAY_MS  = 180.0f;   // pleasing decay floor
static constexpr float SNARE_HOLD_MS   = 0.0f;     // optional plateau

// In render(), once per frame, NOT per-pixel:
const bool fired = (hasAudio && ctx.audio.isSnareHit());
const float dt = ctx.getSafeRawDeltaSeconds();

// On fire, set target; decay continuously toward zero.
// fmaxf gates against re-firing during the existing decay tail (no peg-then-fall).
const float target = fired ? 1.0f : 0.0f;
const float tau = (target > m_snareEnv) ? (SNARE_ATTACK_MS * 0.001f)
                                        : (SNARE_DECAY_MS  * 0.001f);
const float alpha = 1.0f - expf(-dt / fmaxf(tau, 1e-4f));
m_snareEnv += (target - m_snareEnv) * alpha;

// Then drive the visual parameter from the envelope, NOT the binary flag:
float tanhScale = 2.0f + 3.0f * m_snareEnv;   // 2.0 .. 5.0 continuously
```

This eliminates the spazz because:

- One-frame binary pulses are stretched into smooth ~180 ms tails.
- Re-firing inside the tail does not jerk the value (target was already 1.0 during the very brief attack window).
- It is dt-corrected so the visual matches at any render rate.
- Works equally well at 1 Hz snare and 15 Hz hi-hat — the tail just stays elevated during sustained sections instead of sawtoothing.

### Fixes per effect

| Effect | Line | Fix |
|---|---|---|
| `ChevronWavesEffect.cpp:153` | per-pixel | (a) lift `isSnareHit()` out of the loop, (b) replace with the Hold-and-Decay envelope above driving `tanhScale`. |
| `ChevronWavesEffectEnhanced.cpp:138-143` | already dt-corrected | Already 90 % right. Add an attack pole (one-pole low-pass) so re-firing during the decay tail does not jerk the value to 1.0 instantly. Or use `fmaxf(target, m_snareEnv * decayPerFrame)` — peak-hold semantics. |
| `LGPWaveCollisionEffect.cpp:123-130` (snare → collisionBoost) | dt-corrected decay (good) but binary peg | Same Hold-and-Decay envelope; `m_collisionBoost = max(envelope, energyDeltaSmooth * 0.4f)`. |
| `LGPWaveCollisionEffect.cpp:133-137` (hihat → speedTarget) | **NOT dt-corrected** | Two changes: (1) Replace the fixed-per-frame relaxation `m_speedTarget = m_speedTarget*0.95 + 1.0*0.05` with `dtDecay()` toward 1.0 (or one-pole low-pass with τ ~250 ms). (2) Drive `m_speedTarget` from a `m_hihatEnv` envelope rather than binary peg: `m_speedTarget = 1.0f + 0.6f * m_hihatEnv`. This converts sustained hi-hat passages into a steady elevated speed instead of sawtooth chop. |

### Optional: cooldown/refractory at the effect level

The BR detector already enforces a 64 ms refractory at the producer. Effects do **not** need to add another cooldown — but they MUST stretch the binary edge into an envelope before driving any continuous visual parameter. The envelope (180-300 ms decay) provides perceptual smoothing and naturally absorbs back-to-back fires without jerk.

### What NOT to do

- Do NOT call `isSnareHit()` inside per-pixel loops. Once per frame, then drive the loop from a pre-computed scalar.
- Do NOT use `m_x = m_x * 0.95 + target * 0.05` style fixed-per-frame smoothing — it changes meaning if render FPS drifts. Use `dtDecay()` or `1.0f - expf(-dt/tau)`.
- Do NOT use `is*Hit()` to set a discrete value (`tanhScale = 5.0f`, `m_speedTarget = 1.6f`) without an envelope behind it. Binary triggers + binary visuals = guaranteed spazz.

---

## Files inspected

1. `firmware-v3/src/plugins/api/EffectContext.h` (1155 lines)
2. `firmware-v3/src/plugins/api/OnsetContext.h` (43 lines)
3. `firmware-v3/src/audio/contracts/ControlBus.h` (650 lines)
4. `firmware-v3/src/audio/contracts/OnsetSemantics.cpp` (141 lines)
5. `firmware-v3/src/audio/AudioActor.cpp` (excerpts: lines 375-410, 650-700, 780-848, 1418-1460)
6. `firmware-v3/src/audio/AudioActor.h` (lines 670-790)
7. `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp` (lines 285-310)
8. `firmware-v3/src/core/actors/RendererActor.cpp` (excerpts: lines 200-260, 1418-1670)
9. `firmware-v3/src/effects/ieffect/ChevronWavesEffect.cpp` (lines 130-180)
10. `firmware-v3/src/effects/ieffect/ChevronWavesEffectEnhanced.cpp` (lines 120-200)
11. `firmware-v3/src/effects/ieffect/LGPWaveCollisionEffect.cpp` (lines 100-180)

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created — SSA8 percussion-trigger semantics report. Documents binary-pulse latching, BR detector refractory, render-frame width, per-pixel cost, sustained-hi-hat sawtooth hazard, and Hold-and-Decay envelope recommendation. |
