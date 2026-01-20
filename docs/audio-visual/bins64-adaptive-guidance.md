# bins64 vs bins64Adaptive (Sensory Bridge parity)

**Scope:** LightwaveOS v2 firmware (`firmware/v2/`).  
**Audience:** Effect authors (IEffect) and audio pipeline maintainers.  
**Goal:** Make it unambiguous when an effect should use `bins64` vs `bins64Adaptive`, and why.

---

## Executive summary

- `bins64` is an **absolute-ish energy** signal (still gated by `activity` and silence logic). It is appropriate for **thresholds, triggers, “punch”, and loudness-driven behaviours**.
- `bins64Adaptive` is a **spectral-shape** signal. It intentionally **cancels absolute loudness** via a max-follower normaliser (Sensory Bridge style). It is appropriate for **wavelength/texture modulation, relative band weighting, shimmer overlays, and “what part of the spectrum is dominant?”**.
- Switching trigger logic (kick/bass thresholds) from `bins64` → `bins64Adaptive` will usually make effects feel “always-on” and less volume-sensitive unless you re-introduce amplitude gating explicitly.

---

## What these signals actually are

### `bins64` (raw 64-bin Goertzel magnitudes)

- Produced by `GoertzelAnalyzer::analyze64()` (`firmware/v2/src/audio/GoertzelAnalyzer.*`).
- 64 semitone-spaced bins from **A1 (55 Hz)** to **C7 (2093 Hz)**.
- Analyzer output is documented as magnitudes **`[0,1]`**, then LightwaveOS applies `activity` gating before publishing to the ControlBus.
- **Cadence:** `analyze64()` completes roughly every ~94ms (needs 1500 samples @ 12.8kHz). Rendering is typically 120 FPS, so effects need a stable value between analysis updates.

**Implementation detail (important):**
- LightwaveOS now *persists* the last completed 64-bin spectrum between `analyze64()` completions to avoid “picket fence” frames of zeros.
- See `firmware/v2/src/audio/AudioActor.cpp` (search: `Persist 64-bin spectrum between analysis triggers`).

### `bins64Adaptive` (max-follower normalised spectrum)

This is a Sensory Bridge-style adaptive normalisation layer computed from `bins64`, and published alongside it.

- Computed per hop in `firmware/v2/src/audio/AudioActor.cpp` (search: `Sensory Bridge adaptive normalisation`).
- Uses a max-follower (`m_bins64AdaptiveMax`) with independent rise/fall and a floor clamp.
- Uses these tunables (persisted + runtime adjustable):
  - `bins64AdaptiveScale`
  - `bins64AdaptiveFloor`
  - `bins64AdaptiveRise`
  - `bins64AdaptiveFall`
  - `bins64AdaptiveDecay`
  - Defined in `firmware/v2/src/audio/AudioTuning.h`.

**Key behavioural property:**
- The spectrum is normalised by “recent maximum energy” (after scaling), so the dominant bin tends towards **~1.0-ish** regardless of overall volume, especially once the max follower has stabilised.
- When the normaliser is held at its **floor**, the output can be noticeably larger than 1.0 even for modest raw bins. This is by design: it preserves “shape” sensitivity at low volumes.

---

## How effects access these signals

In `firmware/v2/src/plugins/api/EffectContext.h`:

- Raw:
  - `ctx.audio.bin(i)` and `ctx.audio.bins64()`
- Adaptive:
  - `ctx.audio.binAdaptive(i)` and `ctx.audio.bins64Adaptive()`

---

## Decision rule: when to use which

### Use `bins64` (raw) when…

You are treating the spectrum as **energy**:

- Kick/sub-bass thresholds (spawn / burst / “punch”)
- Volume-sensitive events
- “Only do X when bass is actually loud”

If you switch these to adaptive, you will usually lose the notion of “loud vs quiet”.

### Use `bins64Adaptive` when…

You are treating the spectrum as **shape / distribution**:

- Wavelength modulation (“wider fringes when bass dominates relative to the rest”)
- Overlay intensity based on “how much treble is present” in a *relative* sense
- Spectrogram / spectrum visualisers
- Any modulation where you want consistency across different speaker volumes / placements

### Hybrid pattern (often best)

Use adaptive for *shape* and raw (or RMS/heavy bands) for *amplitude*:

- `shape = avg(binAdaptive(range))`
- `amplitude = heavyBass()` or `rms()` or `avg(bin(range))`
- `result = shape * amplitude` (or use amplitude as a gate/threshold)

This keeps “what frequencies are prominent?” while still respecting loudness.

---

## Audit: which IEffect patterns actually use 64-bin data

From `rg "ctx\\.audio\\.bin\\(" firmware/v2/src/effects/ieffect` at the time of writing, only these effects use 64-bin bins directly:

### Recommended to stay on raw `bins64` (energy/trigger semantics)

- `firmware/v2/src/effects/ieffect/AudioBloomEffect.cpp`  
  Sub-bass centre pulse uses bins 0–5 as a **punchy brightness boost**.
- `firmware/v2/src/effects/ieffect/BPMEnhancedEffect.cpp`  
  Sub-bass energy influences ring intensity; intended to remain volume-sensitive.
- `firmware/v2/src/effects/ieffect/BreathingEnhancedEffect.cpp`  
  Sub-bass boosts pulse intensity; intended to remain volume-sensitive.
- `firmware/v2/src/effects/ieffect/LGPPhotonicCrystalEffectEnhanced.cpp`  
  Sub-bass affects speed/response; intended to remain volume-sensitive.
- `firmware/v2/src/effects/ieffect/LGPStarBurstEffectEnhanced.cpp`  
  Sub-bass energy affects speed and burst boost; intended to remain volume-sensitive.
- `firmware/v2/src/effects/ieffect/LGPWaveCollisionEffectEnhanced.cpp`  
  Sub-bass blended into wave intensity; intended to remain volume-sensitive.
- `firmware/v2/src/effects/ieffect/RippleEffect.cpp`  
  Kick bins 0–5 are a **hard trigger** (`m_kickPulse > 0.5f`). Adaptive would likely cause always-on spawns without retuning.
- `firmware/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp`  
  Kick burst uses bins 0–5 to bypass the story conductor (must remain “actual bass energy”).

### Recommended to use `bins64Adaptive` (shape/overlay semantics)

- `firmware/v2/src/effects/ieffect/LGPSpectrumDetailEffect.cpp`  
  Uses full spectrum as a visualiser (adaptive-first, raw fallback).
- `firmware/v2/src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.cpp`  
  Same: adaptive-first, raw fallback.
- `firmware/v2/src/effects/ieffect/LGPInterferenceScannerEffect.cpp`  
  Sub-bass controls **wavelength** and treble controls **sparkle overlay** → better as shape signals.
- `firmware/v2/src/effects/ieffect/LGPInterferenceScannerEffectEnhanced.cpp`  
  Same as above.
- `firmware/v2/src/effects/ieffect/LGPStarBurstNarrativeEffect.cpp` (treble shimmer only)  
  Treble shimmer is an *overlay multiplier*; adaptive is a better “relative treble presence” measure.

**Rationale:** overlays and wavelength/texture modulation should not collapse when the room volume changes. Triggers and punch should.

---

## Known gotcha that made spectrum-based effects feel “deaf”

Prior behaviour: `bins64` was being published only when `analyze64()` completed (~10 Hz). Most hops contained zeros, so render loops saw “mostly nothing”.

Fix: persist the last spectrum each hop (stale data is better than zeros).  
See `firmware/v2/src/audio/AudioActor.cpp` (search: `picket fence`).

This change is why Spectrum-style effects can now respond smoothly at render rate.

---

## Runtime tuning (API + persistence)

LightwaveOS now exposes and persists additional DSP knobs so different audio input sources (different speakers/rooms) can be tuned without reflashing.

Key additions (all in `firmware/v2/src/audio/AudioTuning.h`):

- Novelty controls:
  - `noveltyUseSpectralFlux`
  - `noveltySpectralFluxScale`
- Adaptive bins controls:
  - `bins64AdaptiveScale`
  - `bins64AdaptiveFloor`
  - `bins64AdaptiveRise`
  - `bins64AdaptiveFall`
  - `bins64AdaptiveDecay`

WS + REST handlers were updated to match so the Tab5 and dashboard can safely drive the same parameter set.

### REST shape (v1)

Endpoints are registered in `firmware/v2/src/network/webserver/V1ApiRoutes.cpp`:

- `GET /api/v1/audio/parameters` returns `pipeline.bins64Adaptive` and `pipeline.novelty` alongside the other tuning fields.
- `POST /api/v1/audio/parameters` / `PATCH /api/v1/audio/parameters` accept either:
  - a nested object under `pipeline`, or
  - top-level `bins64Adaptive` / `novelty` objects for backwards compatibility.

Minimal example payload:

```json
{
  "pipeline": {
    "bins64Adaptive": {
      "scale": 200.0,
      "floor": 4.0,
      "rise": 0.005,
      "fall": 0.0025,
      "decay": 0.995
    },
    "novelty": {
      "useSpectralFlux": true,
      "spectralFluxScale": 1.0
    }
  }
}
```

---

## Practical guidance for effect authors

- **If you need a trigger:** prefer `ctx.audio.isOnBeat()`, `ctx.audio.isSnareHit()`, `ctx.audio.isHihatHit()`, or raw `bins64` thresholds.
- **If you need a spectrum “shape”:** prefer `bins64Adaptive` or `binAdaptive(i)` and keep your maths bounded (avoid unbounded multiplication; adaptive bins can exceed 1.0).
- **Do not assume update cadence:** bin values are stable per hop, but visuals render per frame; hop-gate expensive analysis, frame-smooth the results.
- **Keep render loops allocation-free:** never allocate when iterating bins (no `std::vector` growth, no `String` concatenations, etc.).

---

## Future option (if we want to reduce confusion)

If effect authors keep mixing semantics, consider a naming convention:

- `binEnergy(i)` → raw `bins64` (energy semantics)
- `binShape(i)` → adaptive `bins64Adaptive` (shape semantics)

This is purely a readability upgrade; the underlying signals already exist.
