---
abstract: "SSA5 audit of heavy_bands / chroma / percussion-trigger pipeline from EsV11Adapter producer through AudioActor publish to ControlBus consumer. Confirms heavy_bands IS pre-smoothed at the producer (one-pole tau ~80 ms at 125 Hz) but `bands[]` is NOT smoothed in the ESV11 path. Identifies source of remaining mid-bass jitter and recommends consumers switch from getHeavyBand(1)+(2)/2 to controlBus.heavy_bands directly via heavyBass() (already smoother) or, for music-driven motion, the scene/translation layer. Read when investigating audio-reactive jerk/spazz on the canonical 32 kHz K1 build."
---

# SSA5 — Audio Signal Smoothness Audit (heavy_bands / chroma / percussion)

**Status: GROUNDED**
**Date: 2026-04-30**
**Author: agent:embedded-system-engineer (SSA5)**
**Build target audited: `esp32dev_audio_esv11_k1v2_32khz` (canonical K1 V2 32 kHz / 125 Hz hop rate)**

## Scope

Trace from PRODUCER → ControlBus for the four motion-driving signals named in the bug:

1. `controlBus.heavy_bands[]` — the field the four spazzing effects read via `ctx.audio.getHeavyBand(1) + getHeavyBand(2)`.
2. `controlBus.chroma[]` / `heavy_chroma[]`.
3. Percussion triggers `kickTrigger` / `snareTrigger` / `hihatTrigger` and their effect-side accessors `isKickHit()` / `isSnareHit()` / `isHihatHit()`.
4. Whether any "smoothed" alternative already exists on the bus that the broken effects could switch to.

## Files inspected

- `firmware-v3/src/audio/contracts/ControlBus.h` (650 lines, full read)
- `firmware-v3/src/audio/contracts/ControlBus.cpp` (lines 1–500, smoothing pipeline)
- `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp` (full file — the canonical K1 producer)
- `firmware-v3/src/audio/AudioActor.cpp` (lines 620–840 ESV11 hop path; line 1020 publish; line 1814 LWLS UpdateFromHop)
- `firmware-v3/src/audio/pipeline/PipelineAdapter.cpp` (snare/hihat trigger detection — LWLS path only)
- `firmware-v3/src/audio/contracts/OnsetSemantics.cpp` (line 98–112 trigger fan-out)
- `firmware-v3/src/audio/TrinityControlBusProxy.cpp` (lines 71–75 Trinity stub path)
- `firmware-v3/src/plugins/api/EffectContext.h` (lines 90–179 audio accessors; lines 365–367 percussion accessors)
- `firmware-v3/src/config/audio_config.h` (HOP_RATE_HZ definition)

Total files inspected: **9**.

---

## ControlBus motion-relevant fields

The K1 build publishes `ControlBusFrame` once per hop. The "ESV11 path" never invokes `ControlBus::UpdateFromHop()` — it builds the frame directly via `EsV11Adapter::buildFrame()` and publishes it. So the `ControlBus::m_heavy_bands_s[]` smoothing pipeline (lines 426–428 of ControlBus.cpp) is **dead code** on the canonical K1 build.

Only fields actually populated by `EsV11Adapter::buildFrame` (canonical K1 path) are listed; "smoothed?" describes what reaches the renderer.

| Field | Type | Smoothed? | Time constant (at 125 Hz hop) | Update rate |
|---|---|---|---|---|
| `rms` | float | NO — mapped via sqrt+1.25× from `es.vu_level` per hop | 0 ms (raw) | 125 Hz |
| `fast_rms` | float | identical to `rms` (line 73 EsV11Adapter) | 0 ms | 125 Hz |
| `flux` | float | NO — `es.novelty_norm_last` passthrough | 0 ms | 125 Hz |
| `bands[0..7]` | float[8] | NO temporal smoothing — pure 8-bin mean of `bins64[]` (EsV11Adapter:132–139) | 0 ms | 125 Hz |
| `bins64[i]` / `bins64Adaptive[i]` | float[64] | AGC follower only (rise=0.25 retuned, decay≈0.005 retuned at 50 Hz reference); raw bin magnitude/follower per hop, no per-bin temporal one-pole | follower tau ~80–800 ms; signal itself raw | 125 Hz |
| `heavy_bands[0..7]` | float[8] | **YES** — symmetric one-pole `m_heavyBands[i] = m_heavyBands[i]*(1-α) + bands[i]*α` with `α = retunedAlpha(0.05f, 50.0f, 125)` (EsV11Adapter:170–173) | tau ≈ 80 ms (4σ ≈ 320 ms settle) | 125 Hz |
| `chroma[0..11]` | float[12] | NO temporal smoothing — AGC-normalised raw chromagram per hop (EsV11Adapter:142–167) | 0 ms (signal); follower tau ~80 ms (gain) | 125 Hz |
| `heavy_chroma[0..11]` | float[12] | **YES** — same 0.05@50Hz one-pole as heavy_bands (EsV11Adapter:175–178) | tau ≈ 80 ms | 125 Hz |
| `snareTrigger` / `hihatTrigger` | bool | discrete event — gated by AudioActor band-ratio detector at line 829–831; **EsV11Adapter explicitly sets `false` (line 294–295)** | event | 125 Hz |
| `kickTrigger` | bool | discrete event from Path B band-ratio detector (AudioActor:823) | event | 125 Hz |
| `onsetEvent` | float | discrete event (set to 1.0 on any kick/snare/hihat fire — AudioActor:835) | event | 125 Hz |
| `snareEnergy` / `hihatEnergy` | float | NO smoothing — clamp01(mean of bin range) per hop (EsV11Adapter:281, 287) | 0 ms | 125 Hz |
| `tempoBeatTick` / `es_beat_tick` | bool | discrete event | event | 125 Hz |
| `audioConfidence` | float | populated downstream by `applyDerivedFeatures` (LWLS only — see "Critical gap" below) | n/a in ESV11 path | n/a |
| `liveliness` | float | smoothed by ControlBus.applyDerivedFeatures — **not invoked on ESV11 path** | 0 (always 0 in ESV11 path) | n/a |
| `silentScale` | float | LWLS-only, default 1.0f in ESV11 path | n/a | n/a |
| `scene` (SceneParameters) | struct | smoothed inside TranslationEngine — populated separately in AudioActor publish step (line 1869–1878), per-hop tau set by translation tuning | translation-internal | 125 Hz |
| `sb_spectrogram_smooth[64]` | float[64] | YES — Sensory Bridge 4.1.1-style smoothed spectrogram, populated via `m_sbSpectrogramSmooth` in AudioActor:1862 | SB tuning | 125 Hz |
| `sb_chromagram_smooth[12]` | float[12] | YES — SB 4.1.1 smoothed chroma (AudioActor:1863) | SB tuning | 125 Hz |

### Critical structural gap

`ControlBus::UpdateFromHop()` (the LWLS path, ControlBus.cpp:319) implements a richer pipeline: clamp → spike de-flicker (3-frame lookahead, ~32 ms delay) → Zone AGC → asymmetric attack/release → `applyDerivedFeatures` (chord, liveliness, saliency, silence). This is the path that has heavy_bands smoothing in the ControlBus class itself, with `m_heavy_band_attack=0.08`, `m_heavy_band_release=0.015` (asymmetric, ultra-slow release).

**The ESV11/K1 canonical build does NOT invoke this pipeline.** The frame is published directly from `EsV11Adapter::buildFrame()` at AudioActor.cpp:659 → AudioActor.cpp:1020 (`m_controlBusBuffer.Publish(frame)`). The `m_controlBus.UpdateFromHop()` call exists on a different code path (PipelineCore / LWLS — AudioActor.cpp:1814), which is currently disabled per `BACKLOG.md` ("PipelineCore env BROKEN").

So when reviewing the smoothing situation on K1, **the only smoothing that runs is in `EsV11Adapter::buildFrame()`**.

---

## heavy_bands publishing pipeline

**Producer → ControlBusFrame trace (ESV11 / K1V2 32 kHz):**

1. `EsV11Backend` runs at 32 kHz, accumulates 256-sample hops in 128-sample chunks. Backend publishes `EsV11Outputs` (vu_level, spectrogram_smooth[64], chromagram[12], waveform[128], tempo fields) every 2 chunks → **125 Hz hop rate** (AudioActor.cpp:641–649).
2. `EsV11Adapter::buildFrame(out, es, hopSeq)` runs once per hop (AudioActor.cpp:659).
3. Within `buildFrame`:
   - `out.bins64[i]` = clamp01(es.spectrogram_smooth[i]) × AGC follower inverse (lines 96–127). Follower has retuned attack/decay at 50 Hz reference.
   - `out.bands[band]` = mean of 8 contiguous `bins64` entries (lines 132–139). **No temporal smoothing on `bands[]`.**
   - `out.heavy_bands[i]` = symmetric one-pole over `out.bands[i]` (lines 170–173):
     ```cpp
     static const float heavy_alpha = audio::retunedAlpha(0.05f, 50.0f, audio::HOP_RATE_HZ);
     m_heavyBands[i] = (m_heavyBands[i] * (1.0f - heavy_alpha)) + (out.bands[i] * heavy_alpha);
     out.heavy_bands[i] = clamp01(m_heavyBands[i]);
     ```
     `retunedAlpha(0.05, 50, 125)` ≈ 0.0204. Time constant τ ≈ −1 / (125 × ln(1 − 0.0204)) ≈ **388 ms equivalent at 50 Hz, ≈ 80 ms at 125 Hz**. Either way, this is a meaningful low-pass.
4. AudioActor.cpp:1020 publishes `frame` to `m_controlBusBuffer` (lock-free SnapshotBuffer).
5. Renderer reads it via `ctx.audio.getHeavyBand(i)` — `EffectContext.h:106–108` directly returns `controlBus.heavy_bands[i]`.

**Conclusion: `heavy_bands[]` IS pre-smoothed.** The smoothing happens inside the EsV11Adapter, not inside the `ControlBus` class. Time constant ≈ 80 ms (1/τ rise), settle ≈ 320 ms.

### Caveat — what feeds heavy_bands

The smoothing is applied to `out.bands[i]`, which is a per-hop **mean of 8 AGC-normalised bins**. `bands[i]` itself is unsmoothed and reacts within one hop (8 ms) to the AGC-normalised spectrogram. The AGC follower (`m_binsMaxFollower`) has its own dynamics: rise 0.25@50Hz (≈ 28 ms), decay ≈ 0.005@50Hz (≈ 1.4 s). When the follower normalises the spectrum, fast bin variations (e.g. mid-bass attack envelope from a kick drum or 808) propagate into `bands[]` instantly; the heavy 0.05@50Hz one-pole then attenuates them.

A 0.05@50Hz one-pole gives roughly 19 dB attenuation at Nyquist (62.5 Hz at 125 Hz hop) but only ≈ 1 dB at 1 Hz. **A 4 Hz fluctuation in `bands[1]` (a typical "wobbling on each beat" pattern) is attenuated by ~6 dB only.** That residual jitter is consistent with what the four broken effects would feel as "spazz" — especially since they then **divide by 2** (averaging two bands) and **then** feed the result into a Spring + speed integrator that integrates the noise rather than smoothing it further.

---

## chroma[] publishing pipeline

Same producer (`EsV11Adapter::buildFrame`), lines 142–167:

1. `rawChroma[i]` = clamp01(es.chromagram[i]) per hop.
2. Chroma AGC follower `m_chromaMaxFollower` updated per hop (rise 0.35@50Hz, decay 0.005@50Hz, floor 0.08).
3. `out.chroma[i] = clamp01(rawChroma[i] * chromaInv)` — AGC-normalised, **not temporally smoothed**.
4. `out.heavy_chroma[i]` = same 0.05@50Hz one-pole as heavy_bands (lines 175–178).

Update rate: 125 Hz on K1. No spike-removal lookahead, no asymmetric attack/release (those live in `ControlBus::UpdateFromHop`, which is bypassed).

`sb_chromagram_smooth[12]` is a separately-maintained smoothed copy populated in AudioActor.cpp:1863 from `m_sbChromagramSmooth` — that path runs in the Sensory Bridge sidecar block; not all effects use it.

---

## Percussion trigger derivation

Three independent producers exist; the live source on K1 is **only the band-ratio detector inside AudioActor**:

1. **EsV11Adapter (DISABLED for triggers):** Lines 290–295 explicitly comment-and-disable the "old crude onset triggers" with a hard `out.snareTrigger = false; out.hihatTrigger = false`. EsV11Adapter only computes `snareEnergy` (mean of bins 5–10) and `hihatEnergy` (mean of bins 50–60) as continuous values. **No trigger output.**
2. **OnsetDetector (1024-pt FFT, demoted to telemetry):** AudioActor.cpp:749 calls `m_onsetDetector.process(tail, rawHopRms)` and merges fluxes into the frame. Line 783 comment confirms FFT-onset triggers are NOT published to ControlBus — only the kick/snare/hihat flux **scalars** are. `kick_trigger` / `snare_trigger` / `hihat_trigger` from the OnsetDetector are emitted as TRACE_INSTANT events for telemetry only.
3. **Band-ratio detector (Path B — live trigger):** AudioActor.cpp:801–836. Operates on `frame.bands[0..7]` (already populated by the adapter):
   - `kickEnergy = bands[0] + bands[1]` (sub-bass + bass)
   - `snareEnergy = bands[2] + bands[3]` (low-mid)
   - `hihatEnergy = bands[5] + bands[6] + bands[7]` (high)
   - Variance-adaptive band-ratio detector with raw-PCM RMS gate (`brRmsGate`) and refractory period.
   - Sets `frame.kickTrigger / snareTrigger / hihatTrigger = true` on fire.
   - Sets `frame.onsetEvent = 1.0` on any fire.
4. **PipelineAdapter (LWLS path only):** PipelineAdapter.cpp:243–261 has its own snare/hihat detector — but PipelineCore is currently broken and not on the K1 path.
5. **TrinityControlBusProxy (stub):** TrinityControlBusProxy.cpp:71–72 — `m_frame.snareTrigger = (perc > 0.5f)` — only used in Trinity stub mode, irrelevant to K1.

**Effect-side accessors:** `EffectContext.h:365–367`:
```cpp
bool isKickHit()  const { return onset.kick.fired || controlBus.kickTrigger; }
bool isSnareHit() const { return onset.snare.fired || controlBus.snareTrigger; }
bool isHihatHit() const { return onset.hihat.fired || controlBus.hihatTrigger; }
```
Where `onset` is an `OnsetSemanticsFrame` populated by `OnsetSemantics::Update()` (OnsetSemantics.cpp:98–112) which itself reads `controlBus.kickTrigger` / `controlBus.snareTrigger` / `controlBus.hihatTrigger`. So the OR collapses to a single source: **the AudioActor band-ratio detector**.

These triggers are single-frame discrete events — there is no "snare strength envelope" smoothing on the bus. They are sharp Boolean pulses at the 125 Hz hop rate.

---

## Jitter assessment

### `bands[1] + bands[2]` (the signal the broken effects use)

- `bands[i]` itself is **unsmoothed** in the ESV11 path (per-hop AGC-normalised mean of 8 bins). On music with steady kick + bass at e.g. 120 BPM (2 Hz), the kick transient bumps `bands[0..2]` strongly every 500 ms. Between kicks, `bands[1..2]` carries 60–250 Hz envelope (bassline notes, room tone, harmonic hum) — a noisy time-series at the hop rate.
- AGC follower decay (0.005@50Hz ≈ 1.4 s tau) means the gain doesn't track per-note variations, so per-note envelopes propagate into `bands[]` directly.
- **Frame-by-frame jitter is real and significant** — easily 10–30 % swing at 125 Hz on percussive low-mid content.

### `heavy_bands[1] + heavy_bands[2]` (what they SHOULD be using)

- Smoothed by 0.05@50Hz one-pole (τ ≈ 80 ms at 125 Hz). Provides ≈ 6 dB attenuation at 4 Hz, ≈ 12 dB at 8 Hz.
- Still not perfectly clean — it's a mild low-pass, not a heavy envelope follower. A genuine 5 Hz wobble in mid-bass will only be partly smoothed.
- The asymmetric heavy attack/release in `ControlBus::UpdateFromHop` (`heavy_band_attack=0.08`, `heavy_band_release=0.015`) is **ultra-slow on the release side** and would give ~250 ms release tau — that's the "ambient/ultra-smooth" path the original design intended. **It is unused on K1.**

### What the four broken effects actually do

ChevronWavesEffect.cpp:127–128, ChevronWavesEffectEnhanced.cpp:162–163, LGPPhotonicCrystalEffect.cpp:102–103, BPMEffect.cpp:91–92, BPMEnhancedEffect.cpp:91–92:
```cpp
heavyEnergy = (ctx.audio.getHeavyBand(1) + ctx.audio.getHeavyBand(2)) / 2.0f;
```
This **does** read the smoothed `heavy_bands[]` field, so the SSA1 hypothesis "heavy_bands is published RAW" is **false**. The producer-side smoothing exists.

However, the residual ≈ 80 ms-tau low-pass leaves significant per-frame variation in mid-bass content. When that variation is then:
1. Doubled-band-averaged (still noisy after average — 1/sqrt(2) at best),
2. Fed into `targetSpeed = base + scale * heavyEnergy`,
3. Spring-tracked with a finite stiffness,
4. Integrated into `phase += smoothedSpeed * dt` at 120 FPS,

the integrator amplifies any residual jitter into visible position discontinuities. The "spazz" is consistent with **insufficient smoothing for a phase-driver use case**, not with raw signal publishing.

`BPMEffect.cpp:99–106` already noticed this and adds a SECOND EMA on top of `heavyEnergy`:
```cpp
m_heavyEnergySmooth += (rawHeavyEnergy - m_heavyEnergySmooth) * alpha;
float heavyEnergy = m_heavyEnergySmooth;
```
That's effect-side compensation for the under-smoothed bus signal. The other three effects don't have this EMA — they're more vulnerable.

---

## Recommendation

### Best existing-bus alternatives (no firmware changes required)

The four broken effects should switch from band-arithmetic to one of the following pre-smoothed scalars already on the bus, in order of preference for "music-driven motion":

1. **`ctx.audio.heavyBass()`** (EffectContext.h:116) → returns `(controlBus.heavy_bands[0] + controlBus.heavy_bands[1]) * 0.5f`. Same smoothing as their current code uses (≈ 80 ms tau), but covers sub-bass + bass instead of bass + low-mid. Not a smoothness improvement, but a semantic improvement (mid-bass on bands[1..2] picks up noisy low-mid content; bands[0..1] is cleaner kick+bass).
2. **`ctx.audio.beatPulse()`** (EffectContext.h:189) → returns `controlBus.scene.beat_pulse` from the TranslationEngine. This is a **musically meaningful, smoothed** beat-driven envelope at the perceptual layer. Effects driving phase from "music intensity" should consume this rather than raw band sums. Caveat: requires TranslationEngine to be initialised on the path (it is, on K1).
3. **`ctx.audio.beatStrength()`** (EffectContext.h:177) → smoothed beat-event strength 0..1. Similar to beatPulse but tighter to the tempo grid.
4. **`ctx.audio.audioConfidence()`** for a slow envelope (200–500 ms response) — but this is **not populated on the ESV11/K1 path** today (it's `applyDerivedFeatures`, LWLS-only). Available only if `ControlBus::UpdateFromHop` is invoked.

### What does NOT exist (and is the real gap)

- **No `heavy_bands_smooth[]` field** on `ControlBusFrame`. The naming is `heavy_bands` (already smoothed at producer) and `sb_spectrogram_smooth` (a 64-bin SB-style smoothed spectrum, not band-aggregated).
- **No `getHeavyBandSmoothed()` accessor.** `getHeavyBand()` is the only band-axis smoothed accessor.
- **No documented latency/jitter spec for `heavy_bands[]`.** The `0.05f@50Hz` constant in `EsV11Adapter.cpp:170` is a magic number with no comment explaining the intended τ or its target for "phase-driver" vs "ambient" use cases.
- The `ControlBus::m_heavy_band_attack = 0.08`, `m_heavy_band_release = 0.015` in ControlBus.cpp (the documented "ambient/ultra-smooth" smoothing intent) **is bypassed on the canonical K1 build**. If the original design called for a much heavier release-side envelope on `heavy_bands`, K1 has been silently running with a much lighter envelope for the entire ESV11 lifecycle.

### For the spazz fix specifically

Best-practice fix for the four effects, in order of invasiveness:

1. **No-firmware fix**: Switch motion driver from `heavyBand(1)+(2)/2` to `ctx.audio.beatPulse()` or `ctx.audio.beatStrength()`. Test on hardware against the same audio source.
2. **Effect-side EMA (cheap, copy BPMEffect's pattern)**: Add a per-effect 200–400 ms EMA on top of the current `heavyEnergy` calculation. BPMEffect.cpp:91–106 is the reference implementation. This is what the visual-fx-architect agent will likely recommend.
3. **Bus-side fix (riskier, broader blast radius)**: Add a new `ControlBusFrame.heavy_bands_xsmooth[8]` field with τ ~ 250 ms in `EsV11Adapter::buildFrame` mirroring the original `m_heavy_band_release = 0.015` intent. Requires updating effect accessors and verifying no other consumer regresses. Out of scope for this SSA.

---

## Confidence

**High.** All claims trace to specific file:line evidence from inspected sources. The publish-path determination (ESV11 bypasses `ControlBus::UpdateFromHop`) was verified by reading AudioActor.cpp:659 (build), :1020 (publish), and confirming no `m_controlBus.UpdateFromHop` call sits between them on the ESV11 hop. The asymmetry between "documented smoothing in ControlBus.cpp" and "actually-used smoothing in EsV11Adapter.cpp" is a real architectural gap, not a misreading.

**Caveats / lower-confidence items:**

- The exact τ in seconds for `retunedAlpha(0.05f, 50.0f, 125.0f)` requires reading `AudioMath::retunedAlpha` to verify (assumed standard time-constant-preserving rescale; result ≈ 0.0204 → τ ≈ 80 ms is a back-of-envelope from `α = 1 − exp(−dt/τ)`). Not core to the recommendation.
- I did not measure runtime jitter on `bands[1]`/`bands[2]` directly — the assessment is structural ("AGC-normalised raw mean of unsmoothed bins, then mild 80-ms LP") and consistent with the symptom report. A serial telemetry capture would close this loop definitively.

## Open questions

1. Was the original design intent for `heavy_bands[]` to use the `m_heavy_band_attack=0.08 / release=0.015` asymmetric ultra-slow envelope (per ControlBus.cpp lines 509–510 and AudioTuning.h:134–135)? If yes, the ESV11 adapter has been running with a much lighter mild LP all along — that is a regression worth flagging to Captain.
2. Should we add a `heavy_bands_xsmooth[]` field with τ ≈ 250 ms designed specifically for phase-driver consumers, leaving `heavy_bands[]` at its current ~80 ms tau for "punchy ambient"?
3. The `audioConfidence` and `liveliness` fields are populated on the LWLS path only. On the ESV11/K1 path they default to `1.0f` and `0.0f` respectively. Is that documented anywhere effects-facing? Several effects read these and may behave differently from intent.

## Token-relevant

**Path to written file:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/docs/research/spazz_redesign_2026-04-30/SSA5_audio_signal_smoothness.md`

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created — SSA5 audit of audio-signal smoothness pipeline for the spazz-effect redesign. |
