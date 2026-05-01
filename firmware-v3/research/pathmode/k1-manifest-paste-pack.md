# K1 Product Manifest — Paste Pack

**Purpose.** The validated Pathmode intents propose specific replacement wording for the K1 product manifest fields. Pathmode's MCP API doesn't expose a `update_product` tool, so these changes must be made in the Pathmode UI. This document collects every replacement text in copy-paste-ready form so a single Pathmode UI session can apply them all without back-referencing intents.

**Source workspace:** SpectraSynq → K1 (productId `efc1a976-9527-4578-8401-634a7f86096d`).
**Generated:** 2026-04-26 from validated intents.

---

## 1. Product Vision

**Replacement target field:** `productVision`
**Driving intent:** Slot 2 — `cb928ddd` Local-only control surface clarity.

```
K1 is a dedicated hi-fi instrument for music visualisation. The device itself strips away cloud and third-party app dependencies — eliminating the latency, vendor lock-in, and privacy compromises of smart-home accessories. Control is local-only: tactile encoders on the device, plus optional companion peripherals (Tab5 desktop controller, iOS remote) connected through K1's own Wi-Fi access point. K1 never reaches the public internet and exposes no cloud APIs.
```

**Why:** removes the false implication that K1 forbids any companion software. Distinguishes the *device* (cloudless, appless) from local-AP *peripherals* (Tab5, iOS) which are first-class.

---

## 2. North Star

**Replacement target field:** `northStar`
**Driving intent:** Slot 3 — `e3102ffc` Audio-to-photon latency programme.

```
End-to-end latency (microphone sample → first photon emitted) targets ≤ 8 ms at the 99th percentile under steady-state operation. Decomposition: I2S capture ≤ 1 ms, ESV11 DSP hop ≤ 1 ms (50 Hz cadence), cross-core publish ≤ 100 µs, effect render ≤ 2 ms, FastLED/RMT WS2812 transmit ≤ 3.2 ms (parallel RMT channels per strip — required to meet the budget).
```

**Why:** replaces the un-budgeted aspirational claim with a decomposed, measurable target. The ≤ 3.2 ms wire transfer is conditional on the parallel-RMT engineering work tracked in Slot 3.

---

## 3. Target Audience

**Replacement target field:** `targetAudience`
**Driving intent:** Slot 2 — `cb928ddd`.

```
High-fidelity audiophiles and dedicated music room owners who prioritise tactile hardware and zero-latency visual feedback over remote-controlled or cloud-bound visualisers.
```

**Why:** replaces "app-controlled smart home gimmicks" (which reads as forbidding *all* apps, including the local-AP iOS/Tab5 peripherals) with "remote-controlled or cloud-bound visualisers" (which preserves the positioning while permitting local-AP control).

---

## 4. Principles (replace the REACTIVE Pattern Contract bullet)

**Replacement target field:** `manifest.principles[2]` (the REACTIVE Pattern Contract entry)
**Driving intent:** Slot 4 — `bf67678d` silentScale framework enforcement.

**Replace this bullet:**
> ~~REACTIVE Pattern Contract: Patterns must respect the silence contract (fade to black via silentScale).~~

**With:**
```
REACTIVE Pattern Contract: Audio-reactive patterns honour a silence contract enforced by the rendering framework. After each effect's render() returns, RendererActor multiplies the output buffer by controlBus.silentScale unless the effect's static EffectDescriptor declares SilenceBehaviour::InternalFade or SilenceBehaviour::IntentionallyPersistent. Default is FrameworkFade. Lint and a backend-side runtime sampler are layered insurance, not the primary mechanism.
```

**Other principles unchanged:**
- Spatial Consistency: Users must be able to operate Unit A by feel. Never break the 1-to-1 mapping of global parameters.
- Aesthetic: Geometric/Abstract priority. Use premium organic smoothness. Avoid 'party-light' chaos.
- Frequency-Spatial Logic: Bass/Kick = Centre (dist 0-20); Midrange = Inner-middle; Treble/Hats = Outer edge (dist 60-79).

---

## 5. Key Decisions (revise + add)

**Replacement target field:** `manifest.keyDecisions[]`
**Driving intents:** Slot 5 — `fbc867b6` (audio backend); Slot 2 — `cb928ddd` (encoder routing); Slot 6 — `5a3abcab` (FrequencyMap canonical).

### 5a. Replace the Unit B Modes / Encoder Map bullets with this consolidated entry

**Replace:**
> ~~Controller: 2x M5ROTATE8 units (Unit A = Global, Unit B = Contextual).~~
> ~~Unit A Map: 0:Effect, 1:Palette, 2:Brightness, 3:Sensitivity, 4:Speed, 5:Intensity, 6:Complexity, 7:Variation.~~
> ~~Unit B Modes: FX Params, Edge/Color, Zones, Presets (Switched via touchscreen tabs).~~

**With:**
```
Controller (Tab5-enforced): Unit A (M5ROTATE8 @ 0x42, encoders 0–7) maps to global performance parameters (effect, palette, brightness, sensitivity, speed, intensity, complexity, variation). Unit B (M5ROTATE8 @ 0x41, encoders 8–15) maps to contextual parameters determined by the active Tab5 UI tab (FX Params, Edge/Color, Zones, Presets). The mapping is enforced in Tab5's parameter handler (ParameterHandler.cpp / ParameterMap.h), not in K1 firmware; Tab5 must validate parameter routing to prevent cross-tab interference. A change to either side of the contract requires a Pathmode amendment cycle.
```

### 5b. Replace the Silence Gate Logic bullet

**Replace:**
> ~~Silence Gate Logic: Single RMS test (clamp01(rmsUngated) < threshold) + sustain timer + EMA fade-to-black.~~

**With:**
```
Silence Gate Logic: framework-enforced. RendererActor applies controlBus.silentScale post-render to every effect tagged SilenceBehaviour::FrameworkFade (the default). silentScale is computed in the audio backend from rmsUngated with hysteresis. Per-effect customisation is via constexpr SilenceBehaviour descriptor field (FrameworkFade / InternalFade / IntentionallyPersistent). Algorithm changes at the framework layer require Captain re-approval and bump kSilenceFadeFrameworkVersion.
```

### 5c. Add new key decision — Audio backend

**Add this bullet:**
```
Audio analysis backend: ESV11 (64-bin Goertzel, 32 kHz sampling, 50 Hz hop). PipelineCore is deprecated for production builds and retained only for offline regression harnesses. New audio-feature work targets ESV11 exclusively.
```

### 5d. Add new key decision — Frequency-spatial map

**Add this bullet:**
```
Frequency-to-space mapping uses canonical named bands (KICK → centre, SNARE → inner, HIHAT → outer). Effects obtain energy via FrequencyMap named-band queries, not hard-coded bin indices. FrequencyMap.h boundaries are published in firmware-v3/docs/audio-visual/.
```

### 5e. Add new key decision — Tempo tracker

**Add this bullet:**
```
Tempo tracker: comb-tooth (OSS ring → CBSS peak detection → BPM density histogram σ=2 BPM Gaussian → argmax with bidirectional subharmonic 0.5/0.33 + log-Gaussian prior at tempoPriorBpm). Captain-approved 2026-03-01, shipped commit fab1802d (2026-03-20). Algorithm is locked behind kBeatTrackerAlgoVersion + the test_spine16k_acceptance regression gate (≥ 9/12 coherent before any release tag). Algorithm change requires Captain re-approval and HANDOVER_BeatTracker.md update. ACF is rejected per HANDOVER §2. Goertzel TempoTracker (firmware/v2/.../TempoTracker.h, 48–143 BPM) retained as documented fallback only.
```

**Other keyDecisions unchanged:**
- Hardware: Dual 160-LED linear strips (320 total).

---

## 6. Workspace strategy (currently `null` — recommended additions)

The SpectraSynq workspace `strategy` field is null. These cross-cutting commitments belong above the K1 product layer:

```yaml
vision: |
  SpectraSynq builds physical instruments for hi-fi music visualisation.
  Light is treated with the same fidelity standard as audio:
  zero cloud, zero apps, zero compromise on responsiveness.

nonNegotiables:
  - Local-only control surfaces (Wi-Fi AP, no STA, no cloud).
  - Audio is mic-only at the consumer device boundary; line-in is lab/factory only.
  - Centre-origin frequency-spatial geography (bass = centre, treble = edges)
    is canonical for all SpectraSynq products that render audio-reactive content.
  - British English in all product copy, code comments, and UI strings.

architecturePrinciples:
  - Constraint > Standard > Pattern (lifted from workspace constitution).
  - Framework-enforce contracts that fail on convention alone (silentScale,
    no-heap-in-render, paired-marker Inversion Bypass).
  - Separate device from peripheral. Devices are appless / cloudless.
    Peripherals (Tab5, iOS) live on the device's local AP and may carry
    UI logic, but cannot relax device-level constraints.
```

---

## 7. Application order in the Pathmode UI

1. Apply Section 1 (productVision) — replace verbatim.
2. Apply Section 2 (northStar) — replace verbatim.
3. Apply Section 3 (targetAudience) — replace verbatim.
4. Apply Section 4 (principles[2]) — edit the REACTIVE Pattern Contract bullet only; leave the other three.
5. Apply Section 5a–5e (keyDecisions) — apply 5a/5b as edits, 5c/5d/5e as additions.
6. Apply Section 6 (workspace strategy) — populate the currently-null fields.
7. Once all of the above are saved, the K1 product manifest will be consistent with the validated intents. The intents themselves can then be progressed validated → approved → shipped → verified as their implementations land.

---

## 8. Audit trail

Every change above derives from a specific validated intent. If you want to verify a change before applying:

| Section | Intent | Location |
|---------|--------|----------|
| 1, 3 | Slot 2 `cb928ddd` | <https://pathmode.io/spectrasynq/k1/intents/cb928ddd-e877-4d7b-96fd-5c280898ee5e> |
| 2 | Slot 3 `e3102ffc` | <https://pathmode.io/spectrasynq/k1/intents/e3102ffc-d733-4ab2-a160-019fd32f51c0> |
| 4, 5b | Slot 4 `bf67678d` | <https://pathmode.io/spectrasynq/k1/intents/bf67678d-700a-4d9d-b4d7-c2c9da3893c5> |
| 5c | Slot 5 `fbc867b6` | <https://pathmode.io/spectrasynq/k1/intents/fbc867b6-299a-42a8-ae75-81d6dd938cea> |
| 5a | Slot 2 `cb928ddd` | (as above) |
| 5d | Slot 6 `5a3abcab` | <https://pathmode.io/spectrasynq/k1/intents/5a3abcab-3101-43b9-8c63-3a954b9b1629> |
| 5e | Slot 7 `4df0963f` | <https://pathmode.io/spectrasynq/k1/intents/4df0963f-fe02-42dc-9d3d-b5ee565d502b> |

Every quoted source artefact (HANDOVER_BeatTracker.md, CLAUDE.md, CONSTRAINTS.md, the 2026-04-26 spec recommendations report, commit fab1802d) is now linked as evidence on the relevant intent.
