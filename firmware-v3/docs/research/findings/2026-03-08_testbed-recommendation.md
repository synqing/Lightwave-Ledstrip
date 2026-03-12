# Production vs Standalone Testbed — Decision Recommendation

**Date:** 2026-03-08 UTC
**Context:** Synthesised from 5 parallel agent sweeps of LightwaveOS firmware-v3
**Decision Required:** Use production firmware builds for the DIFNO training/test pipeline, or create a standalone testbed by extracting the PDE core?

---

## 1. Core Problem

We need to generate large volumes of `(parameters, audio_features) → LED_RGB[320]` training pairs for the DIFNO surrogate. The firmware is 157K LOC with 168 effect implementations. Only ~8K LOC of PDE maths (in `BeatPulseTransportCore`, plus ~45 distinct formulations across 12 key effect families) actually produce the physics we need to model. The question is whether to instrument the production build or extract the maths into an isolated harness.

---

## 2. Evidence Summary (from Agent Sweeps)

### 2.1 Bloat Ratio
| Metric | Value |
|--------|-------|
| Total C/C++ LOC | 157,000 |
| Effect files LOC | 57,305 |
| PDE-relevant physics LOC | ~8,000 |
| Effect .cpp files | 168 |
| **Bloat ratio (total:physics)** | **~19:1** |
| **Bloat ratio (total:effects)** | **~2.7:1** |

[FACT] The physics we want to surrogate is embedded in ~5% of the codebase. The remaining 95% is hardware drivers, networking, OTA, web UI, RTOS task management, plugin system, and peripheral effects.

### 2.2 Isolation Quality
[FACT] Effects implement `IEffect` interface with three methods: `init()`, `render(EffectContext&)`, `cleanup()`.
[FACT] `EffectContext` is a dependency-injection struct containing: LED buffer, palette, 8 scalar UI params (brightness/speed/complexity/mood/variation/intensity/saturation/fadeAmount, all uint8_t 0-255), timing (deltaTimeMs/Seconds), and `AudioContext` (by-value copy, ~550 floats).
[FACT] `BeatPulseTransportCore` has its own `advectOutward()`, `injectAtCentre()`, and `renderToLeds()` methods — self-contained PDE engine with no hardware dependencies beyond the CRGB type.
[INFERENCE] The `EffectContext` + `IEffect` boundary is a clean seam. Effects don't reach into hardware; they receive pre-processed inputs and write to an LED buffer.

### 2.3 Existing Native Test Infrastructure
[FACT] `platformio.ini` defines a `native_test` environment compiling for host (x86/ARM).
[FACT] Mock stack exists: `fastled_mock.h` (full CRGB + palette), `freertos_mock.cpp`, `arduino_mock.cpp`.
[FACT] 200+ `NATIVE_BUILD` guards throughout the codebase already separate hardware from logic.
[INFERENCE] The native test path is battle-tested — effects already compile and run without ESP32-S3 hardware.

### 2.4 Fixed-Point / Integer Arithmetic Risk Surface
| Call Pattern | Occurrences | Sim-Sim Gap Risk |
|-------------|-------------|------------------|
| `scale8()` (8-bit multiply, ±2 LSB) | 307 | Systematic rounding bias, compound error in chains |
| `sin8()`/`cos8()` (8-bit LUT, ±1 step) | 95 | Phase drift in oscillating effects |
| `inoise8()`/`inoise16()` (discrete Perlin) | 34 | Different noise texture from continuous float Perlin |
| `random8()`/`random16()` | 84 | Non-deterministic, cannot reproduce |

**Critical structural risk:** `addScaled()` in BeatPulseTransportCore truncates `float × uint16 → uint32 → uint16`. This accumulates ~0.2% energy drift per diffusion pass. Over 100 frames, the Kuramoto transport drifts measurably from a float32 equivalent. This is not cosmetic — it affects the energy conservation of the PDE.

[FACT] The firmware uses RGB16 (uint16_t per channel) as its HDR intermediate, then tone-maps `out = in/(in+knee) × (1+knee)` to 8-bit CRGB for LED output.
[HYPOTHESIS] A float32 PyTorch port that tone-maps only at display time will produce systematically different dynamics from the uint16 firmware, especially in long-running transport effects (BeatPulse family). This gap must be measured and bounded.

### 2.5 Parameter Space Dimensionality
| Category | Count | Key Items |
|----------|-------|-----------|
| UI scalar params | 8 | brightness, speed, complexity, mood, variation, intensity, saturation, fadeAmount |
| Core PDE params (BeatPulseTransportCore) | 6 | offsetPerFrame60Hz, persistencePerFrame60Hz, diffusion01, amount01, spread01, knee |
| Static PDE constants | 4 | Du=1.0, Dv=0.5, F=0.038, K=0.063 (Gray-Scott) |
| Audio features (scalar) | ~13 | RMS, fastRMS, flux, BPM, tempoConfidence, beatPhase, etc. |
| Audio features (vector) | ~520+ | 8-band + 64-bin Goertzel, 12-bin chroma, 128-sample waveform |
| Smoothing/framework params | ~15 | Spring stiffness, tau values, mood scaling curves |
| **Estimated DIFNO input dim** | **~40D** | 25-30 scalar + 12-dim chromagram (after dimensionality reduction of audio) |

### 2.6 PDE Families Requiring Porting (Priority Order)

| Priority | Effect Family | PDE Type | Why First |
|----------|--------------|----------|-----------|
| **P0** | BeatPulseTransportCore | Semi-Lagrangian advection + diffusion + edge sink + injection + tone map | Core engine shared by 6+ BeatPulse variants. Highest audio-reactivity. Most complex dynamics. |
| **P1** | LGPReactionDiffusion | Gray-Scott (Du/Dv/F/K, Euler, 1-2 iter/frame) | Self-organising patterns. 4 static + 2 dynamic params. Clean port. |
| **P2** | LGPInterferenceScanner | Wave superposition (2 sinusoids + bass/treble modulation + tanh clip) | Analytically differentiable. Good validation target. |
| **P3** | LGPStandingWaves / Collision | Standing wave + collision dynamics | Phase accumulation tests sin8→sin float gap. |
| **P4** | LGPFresnelZones | √r zone plate (heuristic) | Simple geometry, useful for bilateral symmetry validation. |
| **Deferred** | Perlin-based effects (Shocklines, Veil, Interference Weave, Caustics) | Multi-octave noise | Requires continuous Perlin equivalent. Non-trivial to match inoise8 exactly. |

---

## 3. Decision Matrix

| Criterion | Production Instrumentation | Standalone PyTorch Testbed |
|-----------|--------------------------|---------------------------|
| **Setup effort** | Low (add sweep harness to existing native_test) | Medium (port ~1500 LOC of PDE core to PyTorch) |
| **Throughput** | ~1,000 sweeps/hr on ESP32; ~10,000/hr native_test | ~100,000+ sweeps/hr on GPU |
| **Autodiff Jacobians** | ❌ Not available (must use finite differences) | ✅ Native PyTorch autograd |
| **Ground truth fidelity** | ✅ Exact firmware behaviour (uint16, scale8, etc.) | ⚠️ Float32 approximation — sim-sim gap |
| **Training data volume** | Bottleneck: days for 1M pairs on ESP32 | Minutes for 1M pairs on laptop GPU |
| **Maintenance coupling** | High — testbed breaks when firmware changes | Low — testbed is a snapshot of PDE core |
| **Test reproducibility** | ⚠️ random8/random16 non-deterministic | ✅ Seeded torch.manual_seed |
| **Bilateral symmetry enforcement** | Must verify per-effect | Can enforce architecturally (group averaging) |
| **DIFNO training compatibility** | Requires offline data pipeline | ✅ Native in-loop training |
| **Dependency count** | Full ESP-IDF + Arduino + FastLED + 30 libs | PyTorch only |

---

## 4. Recommendation: **Standalone PyTorch Testbed** (with calibration bridge)

### Rationale

[INFERENCE] The standalone testbed is strictly higher leverage on every dimension that matters for DIFNO training: throughput (100× faster), autodiff (eliminates finite differences entirely), reproducibility (deterministic seeds), and training integration (PyTorch-native).

The single argument for production instrumentation — ground truth fidelity — is addressed by building a **calibration bridge**:

1. Generate N reference pairs from the production firmware (native_test build, deterministic inputs, random seed pinned)
2. Generate the same N pairs from the PyTorch port
3. Measure the sim-sim gap: per-pixel L2, frame-level cosine similarity, energy conservation divergence over T frames
4. If gap exceeds threshold → fix the PyTorch port until it converges
5. Ship a regression test that runs both and fails if gap drifts

This gives us the best of both worlds: the speed and differentiability of PyTorch with a bounded, measured fidelity guarantee against the real firmware.

### What to Port First

**BeatPulseTransportCore** — this is the decision. Reasons:

1. It is the most complex PDE (advection + diffusion + edge sink + injection + tone mapping)
2. It is shared by 6+ BeatPulse effect variants (highest coverage per port effort)
3. It has the most interesting audio-reactive dynamics (energy injection scales with beat strength)
4. Its `addScaled()` truncation is the worst sim-sim gap risk — porting it first surfaces the hardest calibration problem immediately
5. It is self-contained: 417 LOC in one header file with no dependencies beyond CRGB and clamp/floatToByte helpers

### Architecture

```
┌──────────────────────────────────────────────────────────┐
│                   PyTorch Testbed                         │
│                                                          │
│  ┌─────────────┐    ┌──────────────────┐                │
│  │ AudioContext │───▶│ BeatPulseTransport│                │
│  │ (synthetic   │    │ Core (float32)   │                │
│  │  or replay)  │    │                  │                │
│  └─────────────┘    │  advect()        │                │
│                     │  diffuse()       │───▶ RGB[320]   │
│  ┌─────────────┐    │  edgeSink()      │     float32    │
│  │ UI Params   │───▶│  inject()        │                │
│  │ (grid sweep │    │  toneMap()       │                │
│  │  or Sobol)  │    └──────────────────┘                │
│  └─────────────┘              │                         │
│                               │ autograd                 │
│                               ▼                         │
│                     ┌──────────────────┐                │
│                     │ Jacobian          │                │
│                     │ ∂RGB/∂params      │                │
│                     └──────────────────┘                │
│                                                          │
│  ┌──────────────────────────────────────────────┐       │
│  │ Calibration Bridge                            │       │
│  │  • Native_test reference pairs (ground truth) │       │
│  │  • Per-pixel L2 regression test               │       │
│  │  • Energy conservation divergence test        │       │
│  │  • Tone-map output comparison (8-bit final)   │       │
│  └──────────────────────────────────────────────┘       │
└──────────────────────────────────────────────────────────┘
```

---

## 5. Implementation Sequence

### Phase A: Port + Calibrate (Weeks 1-2)

| Step | Task | Pass/Fail Criterion |
|------|------|-------------------|
| A1 | Port `BeatPulseTransportCore` to PyTorch (float32, no quantisation) | All 5 methods compile. Output shape matches [batch, 320, 3]. |
| A2 | Port `BeatPulseRenderUtils` helpers (clamp01, floatToByte, knee tone map) | Unit tests match firmware output for 100 known inputs. |
| A3 | Build `AudioContext` mock/replay system in Python | Can replay recorded audio feature streams frame-by-frame. |
| A4 | Generate 10,000 reference pairs from `native_test` build (deterministic seed, grid sweep over UI params) | Pairs written to .npz with full parameter metadata. |
| A5 | Run same 10,000 inputs through PyTorch port | Per-pixel L2 < 0.01 (normalised to [0,1] range) on tone-mapped 8-bit output. |
| A6 | Measure energy conservation: run 1,000-frame trajectories, compare cumulative energy in PyTorch vs native_test | Energy divergence < 1% after 1,000 frames. If `addScaled()` truncation matters, add matching quantisation. |
| A7 | **HARD GATE:** If A5 or A6 fail, debug and fix before proceeding. The sim-sim gap must be bounded. | |

### Phase B: Training Data + DIFNO (Weeks 3-4)

| Step | Task | Pass/Fail Criterion |
|------|------|-------------------|
| B1 | Generate 1M training pairs (Sobol quasi-random over ~40D input space) | GPU throughput > 50K pairs/hour. |
| B2 | Train DIFNO on forward map: `(params, audio) → RGB[320]` | Validation L2 < 0.02 on held-out test set. |
| B3 | Train DIFNO derivative branch: joint loss on output + Jacobian samples | Learned Jacobian passes 5-gate validation protocol (from locked research doc). |
| B4 | Apply bilateral group averaging to enforce centre-origin symmetry | Equivariance error < 10⁻⁶ on symmetric inputs. |

### Phase C: Extend (Week 5+)

| Step | Task |
|------|------|
| C1 | Port Gray-Scott (LGPReactionDiffusion) — simple, clean validation target |
| C2 | Port wave superposition (LGPInterferenceScanner) — analytically differentiable |
| C3 | Port standing wave effects — surfaces sin8→float phase drift |
| C4 | Assess whether Perlin-based effects need porting or can be approximated by spectral noise |

---

## 6. Sim-Sim Gap Mitigation Strategy

The fixed-point arithmetic creates three categories of divergence:

| Category | Source | Severity | Mitigation |
|----------|--------|----------|------------|
| **Truncation drift** | `addScaled()`: float×uint16→uint32→uint16 | HIGH (~0.2%/pass, compounds) | Option 1: Add matching quantisation to PyTorch. Option 2: Accept float32 as "ideal" and train DIFNO on that — the surrogate targets the idealised physics, not the firmware artifacts. |
| **Rounding bias** | `scale8()`: 8-bit multiply, ±2 LSB | MEDIUM (systematic but small per call) | For BeatPulseTransportCore: not directly used in PDE core (it uses float internally). Only matters in output stage. |
| **Stochastic divergence** | `random8()`/`random16()` | LOW for BeatPulseTransportCore (no randomness in core PDE) | Pin random seeds in native_test. For effects using random: generate multiple trajectories and compare statistics, not point values. |

**Recommended approach for truncation drift:** Accept float32 as the target. The DIFNO surrogate will learn the *idealised* PDE dynamics. The firmware's uint16 truncation is an implementation artifact, not intentional physics. If the firmware is ever improved (e.g., moved to float32 internally), the surrogate remains valid.

[HYPOTHESIS] The tone-mapped 8-bit output will mask most of the intermediate drift. The perceptually relevant quantity is the final CRGB[320], not the intermediate RGB16 state. Need to verify: if float32 and uint16 paths produce identical 8-bit output for >99% of frames, the sim-sim gap is cosmetic.

---

## 7. Open Questions (Require User Decision)

1. **Training data audio source:** Synthetic audio features (random/structured) or replayed real audio captures? Synthetic is faster; real captures the actual distribution of audio features the firmware encounters in practice.

2. **Effect scope for Phase B:** Train one DIFNO per effect family, or one DIFNO for the full effect catalogue? Per-family is simpler but doesn't capture cross-effect parameter interactions.

3. **Tone-map gate threshold:** What's the acceptable 8-bit output divergence between PyTorch and native_test? Proposed: ≤1 LSB per channel for 99% of pixels, ≤2 LSB for 100%.

---

## 8. Decision Record

**Decision:** Build standalone PyTorch testbed, starting with BeatPulseTransportCore port.
**Rejected alternative:** Production build instrumentation (too slow, no autodiff, high coupling).
**Calibration requirement:** Bounded sim-sim gap verified against native_test reference pairs.
**Escalation trigger:** If A5/A6 hard gate fails after two debug cycles → reconsider production harness for that specific effect.
**Owner:** Captain
**Status:** PROPOSED — awaiting approval
