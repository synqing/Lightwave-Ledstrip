---
abstract: "Complete inventory of every audio accessor on ctx.audio (EffectContext::AudioContext) and the underlying ControlBusFrame. Distinguishes raw vs smoothed signals, catalogues beat/tempo/percussion surfaces, and identifies the best smoothed motion-driver candidates to replace the spazzy `(getHeavyBand(1) + getHeavyBand(2)) / 2.0f` pattern. Read when redesigning audio-driven motion or auditing the `ctx.audio` API surface."
---

# SSA-9 — `ctx.audio` Audio API Inventory and Motion-Driver Candidates

**Files inspected:**
- `firmware-v3/src/plugins/api/EffectContext.h` (1,154 lines, both `FEATURE_AUDIO_SYNC=1` and stub branches)
- `firmware-v3/src/audio/contracts/ControlBus.h` (650 lines)

**Scope:** Read-only enumeration. Cross-references ControlBusFrame fields with the AudioContext accessors that surface them to effects.

---

## 1. Raw signals available

These are unsmoothed (or only minimally smoothed) per-hop signals. They are appropriate for **discrete events** (kick/snare/hat triggers) but NOT for driving continuous motion (phase rate, position) without further filtering — they jitter at the audio hop cadence (~125 Hz frame rate on ESV11 32 kHz).

| Accessor | Returns | Source field | Description |
|---|---|---|---|
| `rms()` | `float` | `controlBus.rms` | Slow-smoothed RMS energy [0,1]. Has internal one-pole at `m_alpha_slow` (~0.12 default). Borderline raw/smoothed — see §2. |
| `fastRms()` | `float` | `controlBus.fast_rms` | Fast-smoothed RMS [0,1]. Internal alpha `m_alpha_fast` (~0.35). Reactive. |
| `flux()` | `float` | `controlBus.flux` | Slow-smoothed spectral flux (novelty proxy) [0,1]. |
| `fastFlux()` | `float` | `controlBus.fast_flux` | Fast-smoothed flux. |
| `getBand(i)` | `float` | `controlBus.bands[0..7]` | Per-octave band energy [0,1]. Smoothed with asymmetric attack/release (`m_band_attack=0.15`, `m_band_release=0.03` at 50 Hz reference). Fast attack — visible jitter on transient material. |
| `bands()` | `const float*` | `controlBus.bands[]` | Pointer to all 8 bands. |
| `bass()` | `float` | mean of `bands[0..1]` | Bass mean. Spazzy because `bands[]` itself is fast-attack. |
| `mid()` | `float` | mean of `bands[2..4]` | Mid mean. |
| `treble()` | `float` | mean of `bands[5..7]` | Treble mean. |
| `getChroma(i)` | `float` | `controlBus.chroma[0..11]` | Per-pitch-class energy [0,1]. Same smoothing tier as bands. |
| `chroma()` | `const float*` | `controlBus.chroma[]` | Pointer to all 12 chroma. |
| `bin(i)` | `float` | `controlBus.bins64[0..63]` | 64-bin Goertzel spectrum [0,1]. Raw (no per-frame smoothing). |
| `bins64()` | `const float*` | `controlBus.bins64[]` | Pointer. |
| `binAdaptive(i)` | `float` | `controlBus.bins64Adaptive[0..63]` | 64-bin with Sensory Bridge max follower normalisation [0,1]. |
| `bins64Adaptive()` / `musicalBin(i)` | `const float*` / `float` | `controlBus.bins64Adaptive[]` | Adaptive bins; `musicalBin()` is the canonical effect-facing alias. |
| `musicalRange(lo, hi)` / `musicalRange(MusicalRange)` | `float` | mean of `bins64Adaptive[lo..hi)` | Mean energy in a named or numeric 64-bin window. Inherits adaptive normalisation but no extra temporal smoothing. |
| `bins256()` (PipelineCore) | `const float*` | `controlBus.bins256[]` | 256-bin FFT magnitudes [0,1]. Raw spectrum. |
| `binHz()` (PipelineCore) | `float` | `controlBus.binHz` | Hz/bin resolution. |
| `subBass()` / `kick()` / `lowMid()` / `midPresence()` / `shimmer()` / `air()` (PipelineCore) | `float` | `energyInRange(...)` over `bins256` | Mean energy in named bands. **Raw** — no temporal smoothing. Falls back to `bass()`/`mid()`/`treble()` when PipelineCore disabled. |
| `energyInRange(loHz, hiHz)` (PipelineCore) | `float` | mean of `bins256[lo..hi)` | Generic frequency-range mean. Raw. |
| `getWaveformSample(i)` / `waveformSize()` / `waveform()` | `int16_t` / `uint8_t` / `const int16_t*` | `controlBus.waveform[0..127]` | Time-domain samples. Raw. |
| `getWaveformAmplitude(i)` / `getWaveformNormalized(i)` | `float` | derived from `waveform[i]` | Normalised wave samples [0,1] / [-1,1]. Raw. |
| `sbWaveform()` / `sbWaveformPeakScaled()` / `hasSbWaveform()` / `preferredWaveform()` | `const int16_t*` / `float` / `bool` / `const int16_t*` | `controlBus.sb_waveform[]`, `controlBus.sb_waveform_peak_scaled` | Sensory Bridge parity waveform side-car. Raw. |
| `onsetFlux()` | `float` | `onset.raw.flux` ?? `controlBus.onsetFlux` | Raw spectral flux from OnsetDetector. Detector-scale sensitive. |
| `onsetEnv()` | `float` | `onset.raw.env` ?? `controlBus.onsetEnv` | Thresholded flux envelope. Less raw than `onsetFlux()` but not heavily smoothed. |
| `kickFlux()` / `snareFlux()` / `hihatFlux()` | `float` | `onset.raw.{bass,mid,high}Flux` ?? `controlBus.onset{Bass,Mid,High}Flux` | Per-band flux (kick/snare/hat detection inputs). Raw. |
| `hopSequence()` | `uint32_t` | `controlBus.hop_seq` | Monotonic hop counter. |
| `audioConfidence()` | `float` | `controlBus.audioConfidence` | "Music present" envelope [0,1]. ~200-500ms response — moderately smooth, sits between raw and heavy. |
| `silentScale()` / `isSilent()` | `float` / `bool` | `controlBus.silentScale` / `controlBus.isSilent` | Silence fade scalar [0,1] (10s hysteresis); silent flag. |
| `liveliness()` | `float` | `controlBus.liveliness` | Smoothed audio-driven liveliness scalar [0,1]. |
| `timing_jitter` (via no public accessor) | `float` (struct member) | `controlBus.timing_jitter` | CV of inter-onset intervals [0,1]. |
| `syncopation_level` | `float` | `controlBus.syncopation_level` | Onset-vs-grid deviation [0,1]. |
| `pitch_contour_dir` | `float` | `controlBus.pitch_contour_dir` | Spectral centroid direction [-1,+1]. Smoothed with tau~200ms. |

**Note on `bands[]` smoothing:** `bands[]` is NOT raw — it has asymmetric attack/release internally. But the attack rate (`0.15`) is fast enough that for transient-heavy material the effective response IS jittery. This is the trap the spazzy effects fell into.

---

## 2. Smoothed signals available — what's already pre-smoothed?

These are signals that have explicit slow smoothing applied beyond the default `bands[]`/`chroma[]` smoothing. They are the **right tool for driving continuous motion** (phase rates, brightness envelopes, position drift).

| Accessor | Returns | Source field | Smoothing | Description |
|---|---|---|---|---|
| `getHeavyBand(i)` | `float` | `controlBus.heavy_bands[0..7]` | `m_heavy_band_attack=0.08` / `m_heavy_band_release=0.015` (50 Hz reference) | **Extra-slow** asymmetric smoothing for LGP viewing. ~2-3× slower than `bands[]`. Attack is still relatively quick — release is the slow side. |
| `heavyBands()` | `const float*` | `controlBus.heavy_bands[]` | Same | Pointer for iteration. |
| `heavyBass()` | `float` | mean of `heavy_bands[0..1]` | Same | Pre-smoothed bass mean. |
| `heavyMid()` | `float` | mean of `heavy_bands[2..4]` | Same | Pre-smoothed mid mean. |
| `heavyTreble()` | `float` | mean of `heavy_bands[5..7]` | Same | Pre-smoothed treble mean. |
| `getHeavyChroma(i)` / `heavyChroma()` | `float` / `const float*` | `controlBus.heavy_chroma[0..11]` | Same | Pre-smoothed chroma. |
| `cymbalSustain()` (HF semantics) | `float` | `controlBus.cymbalSustain` | Smoothed internally (`m_cymbal_sustain_s`) | Sustained cymbal/noisy HF envelope [0,1]. |
| `airEnergy()` (HF semantics) | `float` | `controlBus.airEnergy` | Smoothed internally (`m_air_energy_s`) | Smooth upper-air shimmer [0,1]. |
| `hfEnergy()` (HF semantics) | `float` | `controlBus.hfEnergy` | Smoothed internally (`m_hf_energy_s`) | Smooth high-frequency content [0,1]. |
| `spectralBrightness()` / `brightness()` (HF semantics) | `float` | `controlBus.spectralBrightness` | Smoothed internally (`m_prev_spectral_brightness`) | Spectral centroid / upper-balance proxy [0,1]. |
| `liveliness()` | `float` | `controlBus.liveliness` | Internal `m_liveliness_s` | Audio-driven liveliness scalar [0,1]. |
| `audioConfidence()` | `float` | `controlBus.audioConfidence` | ~200-500ms tau | "Music present" envelope. |
| `silentScale()` | `float` | `controlBus.silentScale` | 10s hysteresis | Silent fade scalar [0,1]. |
| Sensory Bridge parity smoothed | various | `sb_spectrogram_smooth[]`, `sb_chromagram_smooth[]` | SB internal smoothing | Available via `controlBus.*` direct access; no convenience accessors. |
| `stmTemporal[]` / `stmSpectral[]` / `stmTemporalEnergy` / `stmSpectralEnergy` | `float` arrays / scalars | `controlBus.stm*` | Internal STM smoothing | STM dual-edge decomposition (smoothed). No effect-side accessor — direct controlBus access. |
| Translation scene fields | `SceneParameters` | `controlBus.scene` | Translation engine smoothing | `sceneParameters()`, `motionType()`, `phraseProgress()`, `tension()`, `beatPulse()`, `timingReliable()`. **`tension()` and `phraseProgress()` are perceptually smoothed** by the translation layer. |
| Saliency (smooth versions) | `float` | `saliency.{harmonic,rhythmic,timbral,dynamic}NoveltySmooth` | Internal smoothing | `harmonicSaliency()`, `rhythmicSaliency()`, `timbralSaliency()`, `dynamicSaliency()`, `overallSaliency()` — all return the *Smooth versions. |
| Motion-semantic frame | `float` (axes) | `motionFrame.{weight,time_quality,space,flow,fluidity}` | Layer 2 inference smoothing | `motionWeight()`, `motionTime()`, `motionSpace()`, `motionFlow()`, `motionFluidity()`. **Strongly smoothed perceptual axes [0,1]**. |
| Layer 3 shaping | `float` / `bool` | `motionShaping.{intensity,decayMs,accentScale,active}` | Envelope shaping | `shapedIntensity()`, `shapedDecayMs()`, `shapedAccent()`, `shapingActive()`. Envelope-shaped onset intensity. |

**Heaviest-smoothed continuous signals (in approximate order of "smoothness"):**

1. `motionWeight()` / `motionFlow()` / `motionFluidity()` — Laban motion axes, perceptual smoothing.
2. `audioConfidence()` — ~200-500ms tau music-present envelope.
3. `liveliness()` — global liveliness.
4. `harmonicSaliency()` / `dynamicSaliency()` — *Smooth saliency versions.
5. `cymbalSustain()` / `airEnergy()` — sustained HF envelopes.
6. `tension()` / `phraseProgress()` — translation-layer perceptual signals.
7. `getHeavyBand(i)` / `heavyBass()` / `heavyMid()` / `heavyTreble()` — extra-slow asymmetric band smoothing (slower than `bands[]` but still attack-fast).
8. `rms()` — slow-smoothed RMS.

---

## 3. Beat / tempo signals available

| Accessor | Returns | Description |
|---|---|---|
| `beatPhase()` | `float` | `onset.phase01` — beat phase in current beat [0,1). **Continuous, beat-coherent, monotonic between beats.** Ideal for driving rotation/position phase that locks to musical time. |
| `bpm()` | `float` | BPM estimate (prefers `onset.bpm` if confident, else `musicalGrid.bpm_smoothed`). |
| `tempoConfidence()` | `float` | Tempo tracking confidence [0,1]. |
| `tempoBeatConfidence()` | `float` | Strongest of `tempoConfidence()`, `controlBus.tempoConfidence`, `controlBus.es_tempo_confidence`. |
| `tempoBeatTick()` | `bool` | Single-frame tempo beat tick (locked source). |
| `isOnBeat()` | `bool` | Single-frame beat pulse (`onset.beat.fired ?? musicalGrid.beat_tick`). |
| `isOnDownbeat()` | `bool` | Downbeat pulse. |
| `beatInBar()` | `uint8_t` | 0-based beat position in bar (typically 0..3 for 4/4). |
| `beatStrength()` | `float` | `onset.beat.level01` (decaying envelope from beat) ?? `musicalGrid.beat_strength`. **Continuous decaying envelope [0,1] — peaks on beat, decays to 0.** |
| `beatPulse()` | `float` | `controlBus.scene.beat_pulse` — translation-layer beat envelope. |
| `timingReliable()` | `bool` | `controlBus.scene.timing_reliable`. |

**Beat-coherent continuous signals (most useful for motion):**
- `beatPhase()` — monotonically advancing 0→1 within each beat. Drives rotation/position naturally.
- `beatStrength()` — decaying envelope from beat. Drives amplitude.
- `beatPulse()` — translation-layer alternative to `beatStrength()`.

---

## 4. Percussion signals available — link to SSA-8

These are the discrete-impulse channels for percussive elements. Use for **single-event punctuation** (flashes, spawns), not for continuous motion.

| Accessor | Returns | Source | Tier |
|---|---|---|---|
| `kickLevel()` | `float` | `onset.kick.level01` ?? `kickFlux()` | Semantic kick channel level [0,1] — has decay envelope. |
| `snare()` | `float` | `onset.snare.level01` ?? `controlBus.snareEnergy` | Semantic snare channel level [0,1]. |
| `hihat()` | `float` | `onset.hihat.level01` ?? `controlBus.hihatEnergy` | Semantic hi-hat channel level [0,1]. |
| `isKickHit()` / `isSnareHit()` / `isHihatHit()` | `bool` | `onset.{kick,snare,hihat}.fired` ?? `controlBus.{kick,snare,hihat}Trigger` | Single-frame fire pulse. |
| `kickFlux()` / `snareFlux()` / `hihatFlux()` | `float` | `onset.raw.{bass,mid,high}Flux` ?? `controlBus.onset{Bass,Mid,High}Flux` | Raw flux per band. Detector-scale. |
| `hatEvent()` (HF semantics) | `float` | `controlBus.hatEvent.strength / 65535.0f` | Compact Q15 hat event strength [0,1]. |
| `hatEventInfo()` (HF semantics) | `const AudioEventQ15&` | `controlBus.hatEvent` | Full event: strength, confidence, ageMs, flags. |
| `onsetEnv()` / `onsetEvent()` / `hasOnsetEvent()` / `onsetFlux()` | `float` / `float` / `bool` / `float` | `onset.{transient,raw}.{...}` ?? `controlBus.onset{Env,Event,Flux}` | Broadband onset surface. |

Cross-reference with **SSA-8** (percussion API audit) for the canonical recommendation tiers. The level01 channels (`kickLevel`, `snare`, `hihat`) are the first-class smoothed-decay surface; the Trigger booleans are the discrete-event surface.

---

## 5. Motion-driver candidates — given the spazz bug

The broken pattern is `(getHeavyBand(1) + getHeavyBand(2)) / 2.0f` driving **continuous motion** (phase rate, position). Even with the `heavy_bands[]` extra smoothing, two issues remain:

1. **Heavy bands have fast-ish attack** (0.08 vs 0.015 release) — they spike on transients, then slowly fall. Used as a *rate* driver, the attack edges produce visible jerks.
2. **Bass-band sums (bands 1+2)** are inherently transient-loaded — kick drums hit bands 0-2 hard, producing exactly the spike pattern that drives spazz.
3. **No music-present gating** — silence still produces noise floor in heavy bands; effects continue to twitch in silence.

### Top 3 smoothed signals best suited to drive phase/position

**Ranking criterion:** continuous, [0,1]-bounded, perceptually smoothed, music-correlated, and not transient-loaded.

#### 1. `motionFlow()` (or `motionFluidity()`) — *strongest motion-suitable smoothing*
- **Returns:** `float` [0,1]. `motionFrame.flow` (Bound ↔ Free) or `motionFrame.fluidity` (Jerky ↔ Fluid).
- **Smoothing:** Layer 2 motion-semantic inference, perceptual smoothing across multiple Layer-1 inputs.
- **Why it fits:** Designed exactly for "how should motion feel right now". Maps onto continuous motion drivers without spikes.
- **Drives:** Phase rate, drift speed, smoothness of position update.
- **Caveat:** Defaults to 0.5 in stub mode (FEATURE_AUDIO_SYNC=0), so always provides safe neutral motion.

#### 2. `liveliness()` × `audioConfidence()` — *correlated music presence*
- **Returns:** `float` [0,1] each.
- **Smoothing:** Liveliness is internally smoothed; audioConfidence has ~200-500ms tau.
- **Why it fits:** "How energetic is the music right now" without per-frame spikes. Multiplied by `audioConfidence()` it gates motion to actual music presence — silence stops the motion gracefully.
- **Drives:** Global motion speed multiplier, amplitude scalar.

#### 3. `beatPhase()` (for rate) + `beatStrength()` (for amplitude) — *beat-coherent driver*
- **Returns:** `float` [0,1) phase, `float` [0,1] strength.
- **Smoothing:** Phase is continuous and monotonic between beats — no spikes by construction. Strength is a decaying envelope.
- **Why it fits:** When tempo is reliable (`timingReliable()` / `tempoBeatConfidence()` high), phase IS the natural motion driver — it advances at musical time, not audio-hop time. Effects locked to `beatPhase()` won't jerk on individual transients because phase advances between beats according to tempo, not flux.
- **Drives:** Rotation, orbit, anything periodic that should lock to the beat grid.
- **Gate:** Always check `timingReliable()` or `tempoBeatConfidence() > threshold` before trusting phase as primary driver; fall back to time-based phase otherwise.

### Honourable mentions

- **`tension()`** — translation-layer perceptual tension scalar, smoothed. Good for "build-up" motion.
- **`phraseProgress()`** — long-time-scale [0,1] musical phrase position. Drives slow drift / global hue.
- **`dynamicSaliency()`** / **`harmonicSaliency()`** — change-detection signals for moments when motion should *speed up* or *change character*, not for the baseline rate itself.
- **`heavyBass()` / `heavyMid()` for *amplitude* only** — acceptable for amplitude scalars but NOT for rate. Pair with a smoothed rate driver above.

---

## 6. Verdict

**YES — the broken 4 are using sub-optimal raw signals when smoothed alternatives exist.**

`(getHeavyBand(1) + getHeavyBand(2)) / 2.0f` is the wrong choice for driving continuous motion because:

1. **`heavy_bands[]` is asymmetric-smoothed, not slow-smoothed.** Attack at `0.08` (50 Hz reference) is still fast enough to register kick transients as visible spikes when the result is integrated into a position/rate.
2. **Bands 1-2 are bass-loaded** — the most transient-heavy region of the spectrum. Even with heavy smoothing, this is the worst-case input for a motion driver.
3. **No silence/confidence gating** — the signal has no notion of "music absent → don't move".
4. **Better-suited signals exist and are already populated each frame:** `motionFlow()`, `motionFluidity()`, `liveliness() × audioConfidence()`, and `beatPhase() × timingReliable()` are explicitly designed for continuous motion and are stable across transients.

### Recommended refactor pattern

Replace:
```cpp
float drive = (ctx.audio.getHeavyBand(1) + ctx.audio.getHeavyBand(2)) * 0.5f;
phase += drive * dt * RATE;
```

With either (perceptual / motion-semantic):
```cpp
float drive = ctx.audio.motionFlow() * ctx.audio.audioConfidence();
phase += drive * dt * RATE;
```

Or (beat-coherent, falling back to time-based):
```cpp
if (ctx.audio.timingReliable() && ctx.audio.tempoBeatConfidence() > 0.5f) {
    phase = ctx.audio.beatPhase();   // Locks to musical time
} else {
    phase += dt * (RATE * ctx.audio.liveliness());
}
```

Or (energy-driven amplitude only, motion rate from time):
```cpp
float amp = ctx.audio.heavyBass() * ctx.audio.audioConfidence();
phase += dt * RATE;            // Time-based rate, not audio-band rate
brightness = amp;              // Bass drives amplitude, not motion
```

The mistake is conflating *amplitude* (where bass-band heavy smoothing is acceptable) with *rate* (where it isn't).

---

**Confidence:** High — both source files read end-to-end; every accessor traced to its underlying ControlBusFrame field; smoothing tiers verified against ControlBus class member declarations and comments in ControlBus.h.

**Open questions:**
1. Are any of the 4 broken effects already gated by `audioConfidence()` or `silentScale()`? (Out of scope for this SSA — covered by SSA-1/SSA-2 effect-source audit.)
2. What is the exact tau (in ms at the actual hop cadence) for `motionFlow()` vs `liveliness()` vs `audioConfidence()`? The values quoted here are inferred from comments and EMA alphas — direct measurement against runtime telemetry would tighten the recommendations.
3. Does `beatPhase()` extrapolate smoothly between hops at 120 FPS render rate, or does it step at hop cadence? RendererActor populates AudioContext per frame with extrapolated timing per the file header — confirm by reading RendererActor render path (out of scope).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created. Full inventory of `ctx.audio` API surface from EffectContext.h + ControlBus.h. Identified `motionFlow()`, `liveliness()×audioConfidence()`, and `beatPhase()×timingReliable()` as the three best smoothed motion-driver candidates for the spazz redesign. |
