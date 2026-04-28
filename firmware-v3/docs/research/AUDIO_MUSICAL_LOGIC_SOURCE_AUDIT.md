---
abstract: "Read-only source audit of the firmware-v3 ESV11_32kHz audio pipeline's musical-logic surface. Establishes that the canonical Goertzel detector bank starts at bin 0 = D#2 (77.78 Hz), the chromagram fold applies `i % 12` with no pitch-class offset, and the resulting chroma[0] therefore carries D# energy — directly contradicting the ControlBus.h:42 and EffectContext.h:248,276 contract that documents chroma[0] = C. Also documents that hihatEnergy/airEnergy/cymbalSustain/hfEnergy/spectralBrightness fields are sourced from bins 50–63 (≈1.40–2.96 kHz, upper-midrange) despite naming and comments implying 6–12 kHz air/HF semantics; bins256 (FFT, 0–16 kHz) is declared but not populated on the canonical path, so no true HF-air energy reaches ControlBus. Read this when evaluating any chroma-, rootNote-, chord-, or HF-semantic dependent change. No architecture decision is made and no code is modified."
---

# Audio Musical Logic Source Audit

**Mode:** READ-ONLY investigation. No source modified, renamed, or patched. No architecture proposed.
**Author:** orchestrator-claude (synthesised from 6 parallel read-only SSAs)
**Date:** 2026-04-28
**Scope:** firmware-v3 audio pipeline, canonical env `esp32dev_audio_esv11_k1v2_32khz` (ESV11 + 32 kHz shim).
**RBDO label:** GROUNDED — every premise traced to file:line evidence and arithmetic verification.

---

## 0. Executive Summary

Verified facts only — no architecture decisions:

- **Verified build path.** Canonical env `esp32dev_audio_esv11_k1v2_32khz` compiles ESV11+32kHz exclusively. PipelineCore is mutually excluded at compile time. Sole ControlBusFrame producer chain is `EsV11Adapter::buildFrame()` → `ControlBus::applyDerivedFeatures()`. (platformio.ini:209–214; src/config/features.h:84–98, 120–122; src/audio/AudioActor.cpp:659, 940.)

- **Verified bin lattice.** NUM_FREQS=64, BOTTOM_NOTE=12, NOTE_STEP=2 over a quarter-tone-spaced `notes[]` table (A1=55 Hz baseline). Bin 0 detector target = `notes[12]` = **77.78 Hz = D#2**. Bin 63 = `notes[138]` = **2959.96 Hz = F#7**. Coverage ≈ 5.25 octaves, semitone-spaced. (vendor/goertzel.h:28–29, 35–53, 90–117; vendor/global_defines.h:6.)

- **Verified chroma fold.** Canonical fold is the unmodified vendor function `get_chromagram()` at `vendor/goertzel.h:282–297`: 60 of the 64 bins are summed into `chromagram[i % 12] += spectrogram_smooth[i] / 5.0f` with no offset, no pitch-class lookup, no compensation. Bins 60–63 are excluded from the fold. SB sidecar (`EsV11Adapter.cpp:245–268`) folds the same offset bin array via `(12*octave + note)` indexing into `bins64Adaptive[]`, inheriting the same offset.

- **Verified pitch-class origin status.** **Source contradicts documented contract.** ControlBus.h:42 and EffectContext.h:248,276 document `chroma[0] = C, chroma[1] = C#, …, chroma[11] = B`. The implementation places D# in chroma[0], E in chroma[1], …, C in chroma[9], …, D in chroma[11]. No compensation found in any chroma writer on the canonical path.

- **Verified HF-semantic naming risk.** All HF-semantic field writers (`hfEnergy`, `hfFlux`, `hatEvent`, `cymbalSustain`, `airEnergy`, `spectralBrightness`) source from bins 50–63, which map to **1396.91–2959.96 Hz (F6 to F#7)** — upper-midrange / presence range. Comments and field names imply 6–12 kHz / "air shimmer" / "cymbal" semantics. **No 6–12 kHz energy enters ControlBus on the canonical path** because `bins256[]` (declared, 0–16 kHz capable) is not populated by any writer in the canonical chain.

- **Verified test coverage.** No test fixture validates `pure C tone → chroma[0]`, no chromatic-walk test, no chord-dominance test, no hihat/cymbal semantic test. Existing audio tests (`test_goertzel_basic.cpp`, `test_esv11_parity.cpp`, `test_esv11_real_music.cpp`) cover legacy 8-band Goertzel target frequencies, ESV11 pipeline parity, and tempo tracking — none assert pitch-class semantics.

- **Verified risk level.** Pitch-class-origin defect is **HIGH risk** for the 4 effects with hardcoded C-origin assumptions and for the chord detector consumer chain. **LOW risk** for the ~11 effects using the rotation-invariant `circularChromaHueSmoothed()` weighted-mean pattern. **HIGH risk** of stale comments on HF semantic fields.

---

## 1. Evidence Table

| Claim | Status | Evidence | Source path:line | Notes |
|---|---|---|---|---|
| Canonical env compiles ESV11+32kHz exclusively | VERIFIED | `-D FEATURE_AUDIO_BACKEND_ESV11_32KHZ=1`, mutual exclusion guard | platformio.ini:209–214; features.h:120–122 | Auto-enables ESV11 base flag |
| PipelineCore inactive on canonical env | VERIFIED | No `-D FEATURE_AUDIO_BACKEND_PIPELINECORE=1` in env block; default 0 | features.h:103–104; AudioActor.h:49–72 | Compile-time mutual exclusion |
| MabuTrace disabled on canonical env | VERIFIED | No `-D FEATURE_MABUTRACE=1`; default 0 | platformio.ini:209–214; features.h:282–283 | Trace envs are separate targets |
| Sole ControlBusFrame spectral writer | VERIFIED | `EsV11Adapter::buildFrame()` | EsV11Adapter.cpp:58–392; AudioActor.cpp:659 | applyDerivedFeatures fills derived only |
| NUM_FREQS = 64 | VERIFIED | `#define NUM_FREQS (64)`; no shim override | vendor/global_defines.h:6; EsV11_32kHz_Shim.h:1–37 | Shim overrides SAMPLE_RATE/CHUNK only |
| BOTTOM_NOTE = 12 | VERIFIED | `#define BOTTOM_NOTE 12` | vendor/goertzel.h:28 | Comment claims "Quarter-step index" |
| NOTE_STEP = 2 | VERIFIED | `#define NOTE_STEP 2` | vendor/goertzel.h:29 | Step of 2 in quarter-tone table = one semitone |
| Notes table is quarter-tone spaced | VERIFIED | notes[1] = 56.635 ≈ 55 × 2^(1/24) | vendor/goertzel.h:35–53 (198-entry table) | NOT semitone-spaced |
| Bin 0 frequency = 77.78 Hz | VERIFIED | `notes[12]` value, computed | vendor/goertzel.h:35; vendor/goertzel.h:90–117 | Equals 55 × 2^(12/24) = 55 × √2 |
| Bin 0 pitch class = D# | VERIFIED | 77.78 Hz under 12-TET A4=440 Hz | (computed) | 6 semitones above A1 = D#2 |
| Bin 63 frequency = 2960 Hz | VERIFIED | `notes[138]` | vendor/goertzel.h:35–53 | F#7 |
| Canonical fold span = 60 bins | VERIFIED | `for (i = 0; i < 60; i++)` | vendor/goertzel.h:287 | Bins 60–63 excluded |
| Canonical fold uses `i % 12` | VERIFIED | `chromagram[ i % 12 ] += spectrogram_smooth[i] / 5.0f` | vendor/goertzel.h:288 | No offset expression |
| No pitch-class offset in canonical fold | VERIFIED (absence) | Grep for OFFSET / `(i + ` returned no fold-modifying matches | vendor/goertzel.h:282–297; EsV11Adapter.cpp:141–167 | No compensation |
| chroma[0] receives D# energy | VERIFIED | bin 0 (D#2) → `chromagram[0]` directly; same for bins 12, 24, 36, 48 (all D# octaves) | vendor/goertzel.h:288 + bin lattice | Captain-intended C-origin not honoured |
| ControlBus contract documents chroma[0] = C | VERIFIED | "0-11 (C=0, C#=1, …)" | ControlBus.h:42; EffectContext.h:248, 276 | Contract contradicts implementation |
| SB sidecar inherits the same offset | VERIFIED | `noteIndex = 12 * octave + note`; `m_sbNoteChroma[note] += bins64Adaptive[noteIndex]` | EsV11Adapter.cpp:245–268 | Same D#-origin defect |
| heavy_chroma is a smoothed envelope of chroma | VERIFIED | Slow exponential smoothing applied in EsV11Adapter | EsV11Adapter.cpp:171–178 | Same origin as chroma — no independent fold |
| sb_chromagram_smooth has no visible writer in EsV11 path | PARTIAL | Field declared; no writer found in EsV11Adapter | ControlBus.h:152 | Possibly dead code or external writer |
| HF source bins = 50..63 (hfEnergy, hfFlux) | VERIFIED | `hfSum = average(bins64Adaptive[50..63]) / 14.0` | EsV11Adapter.cpp:358–359 | |
| HF bin actual frequency range = 1397–2960 Hz | VERIFIED (computed) | bin 50 → notes[112] = 1396.91 Hz (F6); bin 63 → notes[138] = 2960 Hz (F#7) | vendor/goertzel.h:35–53 | NOT 6–12 kHz |
| Comment claims hihat = 6–12 kHz | VERIFIED | "0..1 hi-hat band energy (6-12 kHz)"; "Hihat: bins 50-60 (~6-12 kHz)" | ControlBus.h:74, 166; EsV11Adapter.cpp:273 | Off by ≈3–4× from actual |
| bins256 is declared but unwritten on canonical path | VERIFIED | Field present in struct; no writer found in EsV11Adapter or ControlBus.cpp | ControlBus.h:93–94, 196–197; EsV11Adapter.cpp:303 ("no bins256 dependency") | True 6–16 kHz energy never reaches ControlBus on canonical |
| 60 in source = chroma fold span only | VERIFIED | Single occurrence in fold loop | vendor/goertzel.h:287 | 60 / 12 = 5 (clean) |
| 64 in source = NUM_FREQS / BINS_64_COUNT | VERIFIED | 3 compiled definitions | ControlBus.h:88; global_defines.h:6; TempoTracker.h:48 | Canonical bin count |
| 72-bin spectrum | UNRESOLVED | No source occurrence; documented in research/AFSv2.md as deferred | research/AFSv2.md:553, 649, 1280, 1376, 1667, 1698 | Documented-only; not compiled |
| 96 in source = NUM_TEMPI (tempo, not spectral) | VERIFIED | `#define NUM_TEMPI (96)` | vendor/global_defines.h:28 | Tempo resonator count |
| 128 in source = WAVEFORM_N / mel filterbank | VERIFIED | Waveform sample count and mel band count | ControlBus.h:14; STMExtractor.cpp:264 | Not Goertzel spectrum |
| Pure-tone → chroma dominance fixture | NOT EXIST | No matching test | — | Tier-1 musical correctness has zero coverage |
| Chromatic-walk fixture | NOT EXIST | No matching test | — | |
| Chord-dominance fixture | NOT EXIST | Sets chroma values for velocity tests, not validation | test/test_native/test_attack_only_pitch_velocity.cpp:230–231 | |
| 8-band Goertzel target test | EXISTS (legacy) | `test_target_frequencies()` for 60–7800 Hz 8-band | test/test_audio/test_goertzel_basic.cpp:89–113 | Does not cover 64-bin or chroma |
| 4 effects assume chroma[0] = C | VERIFIED | Hardcoded spatial / palette / hue mappings | SbK1WaveformHarmonicEffect.cpp:10–14, 177–182; LGPExperimentalAudioPack.cpp NOTE_HUES; BeatPulseBloomEffect.cpp; RippleEffect.cpp | Defect carriers if origin is non-C |
| ≥11 effects use rotation-invariant chroma weighting | VERIFIED | `circularChromaHueSmoothed()` via atan2(Σ sin, Σ cos) | ChromaUtils.h:94–110; consumers in 11+ effects | Origin-agnostic |
| chordState.rootNote derivation traced to chroma | UNRESOLVED | Writer cited at ControlBus.cpp:670–672 but chord-detector internals not audited | ControlBus.cpp:670–672 | Open question — see §10 |
| Zero HF-semantic field consumers in effects | VERIFIED | grep across src/effects/ for hfEnergy/airEnergy/cymbalSustain/hatEvent | (grep result) | Mislabelling has no current visual impact |

---

## 2. Canonical Build Path

**Env:** `esp32dev_audio_esv11_k1v2_32khz` (firmware-v3/platformio.ini:209–214)

**Compile flags driving audio path (extracted from extends chain + env block):**
- `-D FEATURE_AUDIO_BACKEND_ESV11_32KHZ=1` — explicit env flag
- `-include src/audio/backends/esv11/EsV11_32kHz_Shim.h` — applies sample-rate / hop-rate calibration
- (auto-implied) `FEATURE_AUDIO_BACKEND_ESV11=1` (features.h:95–98 forces this when 32KHZ flag is set)
- (default) `FEATURE_AUDIO_BACKEND_PIPELINECORE=0` (features.h:103–104) — mutual-exclusion guard at features.h:120–122 enforces only one backend at compile time
- (default) `FEATURE_MABUTRACE=0` (features.h:282–283) — trace macros are no-ops; trace envs are separate targets
- (default) `FEATURE_SB_PARITY_SIDECAR=1` (features.h:143–147; defaults to FEATURE_AUDIO_SYNC) — SB-style sidecar updates internal AudioActor state but does NOT write `chroma[]` directly; it writes `sb_note_chromagram[]` and waveform fields

**ControlBusFrame producer chain (canonical, single-threaded):**
1. AudioActor::run loop (Core 0) → I2S DMA → ESV11 vendor DSP
2. `EsV11Adapter::buildFrame(frame, es, m_esHopSeq)` — sole writer of raw spectral fields (chroma[], heavy_chroma[], bands[], bins64[], bins64Adaptive[], waveform[128], sb_*, hf*, etc.) — EsV11Adapter.cpp:58–392, called at AudioActor.cpp:659
3. `ControlBus::applyDerivedFeatures(frame, ES_HOP_DT, rmsUngated)` — fills derived fields (chordState, liveliness, saliency, motion semantics, silentScale) — ControlBus.cpp:585–783, called at AudioActor.cpp:940

**Confirmed inactive on canonical env:** PipelineAdapter::adapt() (guarded by FEATURE_AUDIO_BACKEND_PIPELINECORE at AudioActor.cpp:1438, 1663, 1731). No alternative chroma producer detected.

---

## 3. Goertzel Detector Bank

**Constants (effective compiled values after shim overrides):**

| Constant | Value | Source |
|---|---|---|
| `NUM_FREQS` | **64** | vendor/global_defines.h:6 |
| `BOTTOM_NOTE` | **12** | vendor/goertzel.h:28 |
| `NOTE_STEP` | **2** | vendor/goertzel.h:29 |
| `SAMPLE_RATE` | **32000 Hz** | EsV11_32kHz_Shim.h:19 |
| `CHUNK_SIZE` | **128** | EsV11_32kHz_Shim.h:20 |
| `SAMPLE_HISTORY_LENGTH` | **10240** | EsV11_32kHz_Shim.h:21 |
| `NOVELTY_LOG_HZ` | **50** | EsV11_32kHz_Shim.h:26 |
| `NOVELTY_HISTORY_LENGTH` | **1024** | EsV11_32kHz_Shim.h:27 |
| Window lookup size | 4096 | vendor/goertzel.h:120 |
| Gaussian σ | 0.8 | vendor/goertzel.h:121 |

**Frequency table (`vendor/goertzel.h:35–53`):** `static const float notes[]` with **198 entries**, **quarter-tone spaced** (24 entries per octave, baseline `notes[0] = 55.0 Hz = A1`). Spot check: `notes[1] = 56.635` ≈ 55 × 2^(1/24). Each step of `NOTE_STEP = 2` in this table therefore advances **one semitone** (50 cents × 2 = 100 cents).

**Detector init formula (`vendor/goertzel.h:90–117`):**

```cpp
for (uint16_t i = 0; i < NUM_FREQS; i++) {
    uint16_t note = BOTTOM_NOTE + (i * NOTE_STEP);
    frequencies_musical[i].target_freq = notes[note];
    init_goertzel(i, frequencies_musical[i].target_freq, neighbor_distance_hz * 4.0f);
}
```

**Computed bin lattice:**

- Bin 0 → `notes[12]` = **77.78 Hz** = **D#2** (pitch class 3 in C-origin convention; 6 semitones above A1)
- Bin 11 → `notes[34]` = 146.83 Hz = D3 (pitch class 2)
- Bin 12 → `notes[36]` = 155.56 Hz = D#3 (one octave above bin 0; pitch class 3)
- Bin 30 → `notes[72]` = 440.00 Hz = A4
- Bin 50 → `notes[112]` = **1396.91 Hz** = F6
- Bin 60 → `notes[132]` = 2489.02 Hz = D#7
- Bin 63 → `notes[138]` = **2959.96 Hz** = F#7

**Octave coverage:** 5.25 octaves, D#2 → F#7. The lattice does NOT begin or end on a C; it is anchored on D#/Eb.

**Sensory Bridge lineage (verified):** `EsV11Adapter.h:49–54`, `EsV11Adapter.cpp:40, 197, 210` explicitly cite "Sensory Bridge 3.1.0" parity. The notes[] table inherits SB's A1-baseline, 12-TET, quarter-tone-spaced design. `BOTTOM_NOTE=12` is a deliberate offset comment ("Quarter-step index in ES table" at vendor/goertzel.h:28). Whether SB's original origin was C, A, or D# is outside this audit's scope (SB is an external upstream).

---

## 4. Chroma / Chromagram Computation

### 4.1 Canonical fold (vendor `get_chromagram()`, vendor/goertzel.h:282–297)

```cpp
inline void get_chromagram(){
    profile_function([&]() {
        memset(chromagram, 0, sizeof(float) * 12);

        float max_val = 0.2f;
        for (uint16_t i = 0; i < 60; i++) {
            chromagram[ i % 12 ] += (spectrogram_smooth[i] / 5.0f);
            max_val = fmaxf(max_val, chromagram[ i % 12 ]);
        }

        float auto_scale = 1.0f / max_val;
        for (uint16_t i = 0; i < 12; i++) {
            chromagram[i] *= auto_scale;
        }
    }, __func__ );
}
```

**Properties verified from source:**

- **Fold span:** 60 (bins 0..59).
- **Bins excluded:** 60..63 (D#7, E7, F7, F#7).
- **Modulo expression:** `i % 12` (no offset constant).
- **Pitch-class offset:** none. No `(i + OFFSET) % 12`, no lookup table, no compensation in any chroma writer in the canonical path (verified across `vendor/goertzel.h`, `EsV11Adapter.cpp:141–167`, `ControlBus.cpp` chroma sections).
- **Divisibility:** 60 / 12 = 5 — each output index receives exactly 5 bins. The fold is musically clean *with respect to even contribution count*.
- **Normalisation:** auto-AGC by max with floor 0.2 (vendor/goertzel.h:286, 292–295) plus an additional adapter-side AGC follower (`m_chromaMaxFollower`) at EsV11Adapter.cpp:164–166.

### 4.2 Bin-0 → chroma[0] alignment (the core finding)

**Bin 0 detector frequency = 77.78 Hz (D#2, pitch class 3) → chroma[0] (because fold uses `i % 12` with offset 0).**

Each chroma index accumulates one specific pitch class:

| chroma index | Receives bins | Pitch class (in source) | Captain-intended (per ControlBus.h:42) |
|---|---|---|---|
| chroma[0] | 0, 12, 24, 36, 48 | **D#** | C |
| chroma[1] | 1, 13, 25, 37, 49 | **E** | C# |
| chroma[2] | 2, 14, 26, 38, 50 | **F** | D |
| chroma[3] | 3, 15, 27, 39, 51 | **F#** | D# |
| chroma[4] | 4, 16, 28, 40, 52 | **G** | E |
| chroma[5] | 5, 17, 29, 41, 53 | **G#** | F |
| chroma[6] | 6, 18, 30, 42, 54 | **A** | F# |
| chroma[7] | 7, 19, 31, 43, 55 | **A#** | G |
| chroma[8] | 8, 20, 32, 44, 56 | **B** | G# |
| chroma[9] | 9, 21, 33, 45, 57 | **C** | A |
| chroma[10] | 10, 22, 34, 46, 58 | **C#** | A# |
| chroma[11] | 11, 23, 35, 47, 59 | **D** | B |

**The implementation is internally D#-origin. The contract is C-origin. They contradict.**

### 4.3 heavy_chroma

Slow exponential smoothing of `chroma[]` (EsV11Adapter.cpp:171–178). Same pitch-class origin as `chroma[]`; no independent fold. Inherits the same offset.

### 4.4 SB sidecar (`sb_note_chromagram[]`, EsV11Adapter.cpp:245–268)

```cpp
for (uint8_t octave = 0; octave < 6; ++octave) {
    for (uint8_t note = 0; note < CONTROLBUS_NUM_CHROMA; ++note) {
        uint16_t noteIndex = static_cast<uint16_t>(12 * octave + note);
        if (noteIndex < ControlBusFrame::BINS_64_COUNT) {
            float val = out.bins64Adaptive[noteIndex];
            m_sbNoteChroma[note] += val;
            ...
        }
    }
}
```

The SB sidecar uses explicit `(octave, note)` indexing into `bins64Adaptive[]` rather than `i % 12`. **However, `bins64Adaptive[]` IS the same offset bin array.** `bins64Adaptive[0]` is bin 0 = D#2. So `m_sbNoteChroma[0]` accumulates `bins64Adaptive[0, 12, 24, 36, 48]` — identical D# content to canonical `chroma[0]`. Same defect.

### 4.5 sb_chromagram_smooth

Field declared at `ControlBus.h:152`. **No writer found in EsV11Adapter.cpp.** PARTIAL — possibly dead code, possibly external. Flagged in §10.

### 4.6 Other chroma paths

- `ChromaAnalyzer.cpp:93–105` — separate Goertzel-based chroma path folding 48 frequencies via `note % 12`. **Not on canonical ESV11_32kHz path.** Mentioned for completeness; out of scope for canonical correctness.

---

## 5. Consumer Assumptions

### 5.1 EffectContext audio API (firmware-v3/src/plugins/api/EffectContext.h)

| Method | Line | Returns | Call-sites |
|---|---|---|---|
| `getChroma(uint8_t i)` | 277 | `controlBus.chroma[i]` | 7 |
| `chroma()` | 285 | pointer to `controlBus.chroma[]` | **44** |
| `getHeavyChroma(uint8_t i)` | 281 | `controlBus.heavy_chroma[i]` | 0 (used via heavyChroma()) |
| `heavyChroma()` | 287 | pointer to `controlBus.heavy_chroma[]` | 9 |
| `rootNote()` | 249 | `controlBus.chordState.rootNote` (doc: "0-11: C=0...B=11") | **11** |
| `chordConfidence()` | 252 | `controlBus.chordState.confidence` | 13 |
| `chordState()` | 243 | full ChordState struct | 2 |
| `bins64()` | 385 | `controlBus.bins64[]` | 2 |
| `bins64Adaptive()` | 396 | `controlBus.bins64Adaptive[]` | 5 |
| `bins256()` | 438 | `controlBus.bins256[]` (PipelineCore only) | 1 |
| `musicalBin(idx)` | 399 | delegates to `binAdaptive()` | 0 |
| `musicalRange(lo, hi)` | 402–413 | mean over 64-bin slice | 0 |
| `bin(idx)` (legacy) | 377–382 | `controlBus.bins64[index]` | 0 |
| `hfEnergy()` (FEATURE_AUDIO_HF_SEMANTICS) | 336 | `controlBus.hfEnergy` (else `treble()` fallback) | 0 in effects |
| `hatEvent()` | 340 | `hatEvent.strength / 65535.0f` | 0 in effects |
| `cymbalSustain()` | 344 | `controlBus.cymbalSustain` | 0 in effects |
| `airEnergy()` | (per stub map) | `controlBus.airEnergy` (else `air()`) | 0 in effects |
| `spectralBrightness()` | (per stub map) | `controlBus.spectralBrightness` (else `treble()`) | 0 in effects |

### 5.2 Effects with hardcoded C-origin assumptions (DEFECT CARRIERS)

1. **SbK1WaveformHarmonicEffect** — `firmware-v3/src/effects/sensorybridge_reference/SbK1WaveformHarmonicEffect.cpp:10–14, 177–182`
   - Comment: *"Bin 0 (C) → pixel 80 (centre); Bin 6 (F#) → pixel ~120; Bin 11 (B) → pixel ~153"*.
   - Code: `posF = kCenterRight + (c / 12.0f) * (kHalfLength - 1)` — fixed pitch-class-to-position map.
   - Visual consequence if origin defect holds: harmonic dots map to the wrong strip positions. Currently C input would illuminate the position labelled "A" on the strip; D# input would illuminate the position labelled "C". Severity: HIGH (geometry is the entire effect).

2. **LGPExperimentalAudioPack** — `firmware-v3/src/effects/ieffect/LGPExperimentalAudioPack.cpp` (NOTE_HUES[12] table)
   - `static constexpr uint8_t NOTE_HUES[12] = { 0, 12, 24, 40, 56, 74, 92, 112, 134, 154, 178, 202 }` — implicit C-origin (NOTE_HUES[0] = "C hue").
   - Severity: MEDIUM (colour authenticity).

3. **BeatPulseBloomEffect** — `firmware-v3/src/effects/ieffect/BeatPulseBloomEffect.cpp`
   - `paletteShift = (uint8_t)(ctx.audio.rootNote() * 21)` — linear palette mapping assuming rootNote[0] = C.
   - Severity: MEDIUM.

4. **RippleEffect** — `firmware-v3/src/effects/ieffect/RippleEffect.cpp`
   - `hue = (uint8_t)((ctx.audio.rootNote() * 21) + chordHueShift)` — same linear assumption.
   - Severity: MEDIUM.

### 5.3 Effects safe under transposition

≥11 effects use `circularChromaHueSmoothed()` / `circularChromaHue()` (`ChromaUtils.h:94–110`). The pattern computes hue via `atan2(Σ chroma[i] · sin(i · 30°), Σ chroma[i] · cos(i · 30°))` — output rotates with the data uniformly, so a global pitch-class shift only rotates the resulting hue. Effects: LGPInterferenceScannerEffectEnhanced, LGPWaveCollisionEffect, LGPStarBurstEffect (and Enhanced), LGPPerlinInterferenceWeaveEffect, ChevronWavesEffect, LGPTalbotCarpetAREffect, LGPSpirographCrownAREffect, LGPPhotonicCrystalEffectEnhanced, plus enhanced variants of Ripple. Severity for these: LOW (only shape matters).

### 5.4 Stale comments / contracts implying C-origin

| File | Line | Text |
|---|---|---|
| ControlBus.h | 42 | `///< 0-11 (C=0, C#=1, D=2, ..., B=11)` |
| EffectContext.h | 248 | `/// Get root note (0-11: C=0, C#=1, D=2, ..., B=11)` |
| EffectContext.h | 276 | `/// Get single chroma bin value (0-11: C=0, C#=1, ..., B=11)` |
| SbK1WaveformHarmonicEffect.cpp | 10 | "Bin 0 (C) → pixel 80 (centre)" |
| SbK1WaveformHarmonicEffect.cpp | 17 | "C major = C, E, G" |

---

## 6. HF Semantic Fields

### 6.1 Fields and writers

| Field | Declared | Writer | Source bins | Computation |
|---|---|---|---|---|
| `hihatEnergy` | ControlBus.h:74, 166 | EsV11Adapter.cpp:283–287 | bins[50..60] | mean, clamped (triggers disabled) |
| `hfEnergy` | ControlBus.h:182 (FEATURE_AUDIO_HF_SEMANTICS) | EsV11Adapter.cpp:358 | bins64Adaptive[50..63] | hfSum / 14.0; one-pole α attack 0.35s, release 0.08s |
| `hfFlux` | ControlBus.h:183 | EsV11Adapter.cpp:359 | bins64Adaptive[50..63] | clamp01((hfRaw − prev) × 4.0) |
| `hatEvent` | ControlBus.h:184 | EsV11Adapter.cpp:360–363 | bins64Adaptive[50..63] + bins64Adaptive[58..63] + bands[0..4] | ratio detector, Q15 packed event |
| `cymbalSustain` | ControlBus.h:185 | EsV11Adapter.cpp:364 | weighted blend bins64Adaptive[50..63] (0.65) + bins64Adaptive[58..63] (0.35) | one-pole; attack 0.20s, release 0.015s |
| `airEnergy` | ControlBus.h:186 | EsV11Adapter.cpp:365 | bins64Adaptive[58..63] | mean / 6.0; one-pole; attack 0.12s, release 0.025s |
| `spectralBrightness` | ControlBus.h:187 | EsV11Adapter.cpp:366 | bins64Adaptive[0..63] | weighted_sum(bin · i) / (energy · 63) |
| `spectralBrightnessDelta` | ControlBus.h:188 | (downstream of brightness) | derivative of brightness | signed [-1, 1] |

Stage B (`ControlBus::applyDerivedFeatures`) provides identical-source fallback writers gated by an `alreadyPopulated` check (ControlBus.cpp:586–664).

### 6.2 Actual frequency ranges (computed from §3 lattice)

| Field | Source bin range | Actual frequency range | Comment claim | Mismatch |
|---|---|---|---|---|
| `hihatEnergy` | 50..60 | **1396.91 Hz – 2489.02 Hz** (F6 – D#7) | "(6-12 kHz)" | YES — claim is ≈ 4× too high |
| `hfEnergy` | 50..63 | **1396.91 Hz – 2959.96 Hz** (F6 – F#7) | "high-frequency content" | YES — ceiling is ~3 kHz, not HF |
| `hfFlux` | 50..63 | 1397 Hz – 2960 Hz | "high-frequency change" | YES — same |
| `hatEvent` | 50..63 + 58..63 | 1397 Hz – 2960 Hz | "Short hat-like event" | YES — bins do not represent typical hihat air content |
| `cymbalSustain` | 50..63 (weighted) | 1397 Hz – 2960 Hz | "Sustained noisy HF envelope" | YES — naming implies cymbal air (3–10 kHz) |
| `airEnergy` | 58..63 | **2217.46 Hz – 2959.96 Hz** (C#7 – F#7) | "upper-air shimmer" | YES — "air" colloquially implies 6–12 kHz |
| `spectralBrightness` | 0..63 (weighted centroid) | 77.78 Hz – 2960 Hz centroid | "spectral centroid / upper-balance proxy" | PARTIAL — centroid is real, but "upper-balance" overstates the limited HF span |
| `spectralBrightnessDelta` | derivative | — | "signed brightness movement" | NONE |

> **Cross-SSA arithmetic note:** SSA-5's report cited HF bin frequencies as 988–2093 Hz (using `f = 55 × 2^(bin/12)`). That formula omits the `BOTTOM_NOTE = 12` quarter-tone offset in the source `notes[]` table. The correct formula given `notes[]` is quarter-tone-spaced is `f = notes[12 + bin·2] = 55 × 2^((12 + 2·bin) / 24) = 77.78 × 2^(bin/12)`. SSA-5's qualitative conclusion (HF naming is mislabelled) holds; the numeric range is **1397–2960 Hz**, not 988–2093 Hz. This audit uses the corrected values, which match SSA-2's directly-read frequency table.

### 6.3 6–12 kHz energy on canonical path

**Verified absent.** Evidence:

- All HF semantic writers source from `bins64Adaptive[]`, which covers 77.78 Hz – 2960 Hz only (§3).
- `bins256[]` declared at `ControlBus.h:93–94, 196–197` (256 floats, comment claims "62.5 Hz spacing @ 32kHz/512-pt", giving 0–16 kHz range), **but no writer found in EsV11Adapter.cpp or ControlBus.cpp on the canonical path**. Comment at EsV11Adapter.cpp:303 explicitly states: "no wider projections, no bins256 dependency".
- Effect consumer count for `bins256()`: 1 call-site, gated to PipelineCore (per SSA-4); inactive on canonical env.

**Conclusion:** No true 6–12 kHz signal information enters ControlBus on the canonical ESV11_32kHz path.

### 6.4 Consumer count for HF semantic fields

**Zero consumer effects** in `firmware-v3/src/effects/` call any of `hfEnergy()`, `airEnergy()`, `cymbalSustain()`, `hatEvent()`, `spectralBrightness()`, or `spectralBrightnessDelta` (per SSA-5 grep). When `FEATURE_AUDIO_HF_SEMANTICS` is disabled, EffectContext stubs map these accessors to legacy fallbacks (`treble()`, `air()`, etc.). **Mislabelling currently has no visual impact** because nothing reads the fields — the risk is downstream developers building on the fields under their stated semantics.

---

## 7. 60 / 64 / 72 Relationship

| Value | Status | Source / Document | Meaning |
|---|---|---|---|
| **60** | compiled, canonical | vendor/goertzel.h:287 (fold loop bound) | Chroma fold span. Bins 0..59 included; bins 60..63 excluded. 60/12 = 5 (clean per-class count). |
| **64** | compiled, canonical | vendor/global_defines.h:6; ControlBus.h:88; TempoTracker.h:48 | NUM_FREQS / BINS_64_COUNT — canonical Goertzel detector count. |
| **Bins 60–63** | compiled, canonical | vendor/goertzel.h:35–53 | Musically valid bins. Frequencies: 2489 Hz (D#7), 2637 Hz (E7), 2794 Hz (F7), 2960 Hz (F#7). EXCLUDED from chroma fold; INCLUDED in bins64[] / bins64Adaptive[] for HF semantic writers. |
| **72** | UNRESOLVED in source | research/AFSv2.md:553, 649, 1280, 1376, 1667, 1698 (deferred) | No source occurrence. Documented in research as deliberately deferred. The "old 72" reference in effect-ID files is a legacy index, not a bin count. |
| **96** | compiled (but NOT spectral) | vendor/global_defines.h:28 | `NUM_TEMPI = 96` — tempo resonator count (BPM bins, 1 BPM/bin from 48–144 BPM). NOT a Goertzel spectrum extension. |
| **128** | compiled (but NOT Goertzel) | ControlBus.h:14; STMExtractor.cpp:264 | `CONTROLBUS_WAVEFORM_N = 128` (waveform sample count) and a 128-bin mel filterbank applied to bins256. NOT canonical Goertzel spectrum. |

**Hypothetical extension arithmetic (source-grounded but no compiled implementation):**

- Extending current lattice **upward** by one octave (12 semitones) would push the ceiling from 2960 Hz (F#7) to ≈ 5920 Hz (F#8) — still below the 6–12 kHz "air" range; insufficient to reach true HF on its own.
- Extending **downward** by one octave would lower the floor from 77.78 Hz to ≈ 38.89 Hz.
- Re-anchoring to **C2** (≈ 65.41 Hz) is referenced only as a hypothetical; **no source occurrence** of C2 anchoring or `65.41` Hz constant in the canonical path.

---

## 8. Existing Fixtures / Tests

| Test category | Status | File | CI? |
|---|---|---|---|
| Pure-tone 261.63 Hz (C4) → chroma[0] dominance | **NOT EXIST** | — | n/a |
| Pure-tone 277.18 Hz (C#4) → chroma[1] dominance | **NOT EXIST** | — | n/a |
| Chromatic scale C→B → chroma peak walks 0→11 | **NOT EXIST** | — | n/a |
| C major chord → chroma[0,4,7] dominance | **NOT EXIST** (chroma values set for unrelated velocity tests at test_attack_only_pitch_velocity.cpp:230–231) | — | n/a |
| A minor chord → chroma[9,0,4] dominance | **NOT EXIST** | — | n/a |
| Hihat / cymbal / air semantic correctness | **NOT EXIST** | — | n/a |
| bins64 frequency-mapping correctness | PARTIAL (8-band only) | test/test_audio/test_goertzel_basic.cpp:89–113 | yes (native_test) |
| ESV11 pipeline parity (220 Hz sine + 120 BPM envelope) | EXISTS | test/test_esv11_audio/test_esv11_parity.cpp | yes (native_test_esv11_audio) |
| Real-music tempo validation (drum loops, EDM) | EXISTS (external WAV corpus, not committed) | test/test_esv11_music/test_esv11_real_music.cpp | yes (native_test_esv11_music) |
| Sine-wave generator | EXISTS | test/test_audio/test_goertzel_basic.cpp:13–20 | yes |
| Impulse-train generator | EXISTS | test/test_audio/test_goertzel_basic.cpp:44–81 | yes |
| Chirp / noise / metronome fixtures | NOT EXIST as dedicated generators | — | n/a |
| `test_pitch_correctness.cpp` or analogous | **NOT EXIST** | — | n/a |

**Summary:** The canonical pitch-class-correctness surface has zero direct test coverage. Existing audio tests cover legacy-band Goertzel targets, ESV11 parity (signal stability), and tempo tracking — none of which would catch a chroma-origin defect.

---

## 9. Risk Register

| Risk | Evidence | Affected fields/effects | Severity | Confidence | Captain decision required? |
|---|---|---|---|---|---|
| **chroma[0..11] is D#-origin (D#, E, F, …, D), not C-origin as documented** | §3, §4.2, vendor/goertzel.h:287–288, ControlBus.h:42, EffectContext.h:248,276 | All chroma[] readers, heavy_chroma[] readers, chordState.rootNote (pending §10), 4 hardcoded-C-origin effects (§5.2) | **HIGH** | HIGH | YES |
| **chordState.rootNote semantics inherit chroma offset** | UNRESOLVED — chord detector internals not traced this audit; ControlBus.cpp:670–672 cited but writer not opened | rootNote(), chordConfidence(), chordState — 26 total call-sites across effects | HIGH | MEDIUM (writer location known, derivation logic unaudited) | YES — cannot answer chord correctness without trace |
| **HF-semantic field naming claims 6–12 kHz; actual source bins cover 1397–2960 Hz** | §6.1, §6.2; ControlBus.h:74,166 ("6-12 kHz"); EsV11Adapter.cpp:273 | hihatEnergy, hfEnergy, hfFlux, hatEvent, cymbalSustain, airEnergy, spectralBrightness | MEDIUM (no current consumers in effects/) | HIGH | YES — comments are stale; new consumers will misuse |
| **bins256 declared but unwritten; no true 6–12 kHz energy reaches ControlBus on canonical path** | §6.3; ControlBus.h:93–94; EsV11Adapter.cpp:303 | Any future "true HF" feature; PipelineCore migration | MEDIUM | HIGH | YES — gap between contract and implementation |
| **SbK1WaveformHarmonicEffect spatial map assumes Bin 0 = C** | §5.2 item 1; SbK1WaveformHarmonicEffect.cpp:10–14, 177–182 | Effect visual correctness | HIGH (geometry-defining) | HIGH | YES |
| **NOTE_HUES / palette-shift effects assume C-origin** | §5.2 items 2–4 | LGPExperimentalAudioPack, BeatPulseBloomEffect, RippleEffect | MEDIUM | HIGH | YES |
| **No fixture detects pitch-class-origin defects** | §8 | All audio musical-correctness validation | HIGH (governance) | HIGH | YES |
| **SB sidecar `sb_chromagram_smooth[]` has no visible writer** | §4.5; ControlBus.h:152 | Possibly dead code consumer; possibly unwritten field read by an effect | LOW–MEDIUM | MEDIUM (writer not located in this audit) | YES — clarify if dead |
| **vendor/goertzel.h is upstream Sensory Bridge code** | EsV11Adapter.h:49–54; vendor/goertzel.h header comments | Modifying canonical fold means diverging from SB lineage | (governance) | HIGH | YES — origin alignment is a Captain-level decision because it affects upstream parity |

---

## 10. Unresolved Questions

These cannot be answered from the source already inspected; they require additional tracing or Captain decision.

1. **chordState.rootNote derivation.** ControlBus.cpp:670–672 was cited as the chord-state writer, but the function chain that produces the root index from chroma was not opened in this audit. **Question:** Does the chord detector compute `argmax(chroma)` directly (in which case rootNote inherits the D#-origin defect), does it use a key-finding template that compensates, or does it consult the original bin frequencies? Trace required.

2. **`sb_chromagram_smooth[]` writer location.** Field declared at ControlBus.h:152 with no writer found in EsV11Adapter or ControlBus.cpp. Either (a) dead code, (b) unwritten field nonetheless read, or (c) writer elsewhere. Trace required.

3. **Sensory Bridge upstream pitch-class origin.** The vendor goertzel.h is described as ES v1.1_320 (Emotiscope) with Sensory Bridge 3.1.0 parity. Whether the upstream original chroma[0] was meant to be C, A, or D# is outside this audit's scope. Captain decision needed on whether canonical K1 musical semantics must follow upstream or assert independent C-origin convention.

4. **PipelineCore chroma path.** The PipelineCore backend is inactive on canonical env, but it has its own NUM_FREQS=64, its own (ChromaAnalyzer-based) chroma writer, and its own bins256 producer. Whether PipelineCore's chroma is C-origin or also D#-origin was not traced. Affects feature-flag flips and any future migration.

5. **Whether contracts at ControlBus.h:42 / EffectContext.h:248,276 reflect intent or aspiration.** The comments explicitly state "C=0". Source contradicts. Captain must establish whether the documented contract is the design intent (and the implementation is wrong) or whether the comments are the wrong artefacts (and the implementation reflects accepted SB-lineage behaviour).

---

## 11. Non-Decisions

This audit explicitly does NOT make the following determinations:

- **No production migration decision made.** No stance on whether to keep the current bin lattice, extend, or re-anchor.
- **No 72-bin adoption decision made.** No source occurrence to migrate from; documented in research only.
- **No 96-bin / 128-bin Goertzel adoption decision made.** Same.
- **No code changes made.** Zero file modifications; zero renames; zero patches.
- **No architecture proposed.** No competing pitch-class layout, palette mapping, chord-detector design, or HF-spectrum design suggested.
- **No claim that current behaviour should be preserved because effects depend on it.** The 4 C-origin-assuming effects (§5.2) and the 26 chordState consumers are flagged as defect carriers, not as a constraint on future direction.
- **No claim that the SB-lineage origin is correct or incorrect.** That is a Captain / system-architect decision that requires upstream inspection beyond this audit's scope.

---

## Investigator Conclusion

**The source verifies that current chroma is internally consistent (D#-origin throughout) but contradicts the documented C-origin contract.**

Supporting evidence:

1. **Bin 0 is D#2 (77.78 Hz).** `BOTTOM_NOTE = 12` (vendor/goertzel.h:28), `NOTE_STEP = 2` (vendor/goertzel.h:29), `notes[]` is quarter-tone spaced with `notes[0] = 55 Hz = A1` (vendor/goertzel.h:35–53). Therefore bin 0 = `notes[12 + 0·2] = notes[12]` = 55 × 2^(12/24) = 55 × √2 = 77.78 Hz, which is D#2 (six semitones above A1).

2. **The fold uses `i % 12` with no offset.** vendor/goertzel.h:288: `chromagram[ i % 12 ] += (spectrogram_smooth[i] / 5.0f);`. Verified absent: any `(i + OFFSET) % 12` expression, any pitch-class-offset constant, any compensation in EsV11Adapter.cpp:141–167 or ControlBus.cpp chroma sections.

3. **The implementation is internally consistent.** Every chroma writer on the canonical path — vendor `get_chromagram()` for `chroma[]`, the smoother for `heavy_chroma[]`, the SB sidecar's `(12·octave + note)` indexing for `sb_note_chromagram[]` — agrees on the same offset bin lattice and produces the same D#-origin output.

4. **The contract documents C-origin.** ControlBus.h:42 (`"0-11 (C=0, C#=1, D=2, ..., B=11)"`), EffectContext.h:248 (`"0-11: C=0, C#=1, D=2, ..., B=11"`), EffectContext.h:276 (same), SbK1WaveformHarmonicEffect.cpp:10 (`"Bin 0 (C) → pixel 80 (centre)"`), and 4 effects with hardcoded C-origin maps (§5.2).

5. **Therefore, source consistency holds (D# throughout) but contradicts external semantics (C-origin contract).** The defect candidate is the contract/implementation mismatch. Whether the implementation or the contract should yield is **not decided in this audit** — that is a Captain / system-architect decision pending §10 question 5.

6. **HF-semantic field naming further misrepresents source content.** Comments at ControlBus.h:74,166 and EsV11Adapter.cpp:273 claim 6–12 kHz coverage; source bins 50–63 cover 1397–2960 Hz only (§6.2). `bins256[]` exists but is unwritten on canonical path; no true HF energy reaches ControlBus.

---

### Update — 2026-04-28: Lattice fix applied (C-origin restored)

The pitch-class-origin defect documented in §0, §3, §4.2, and §9 row 1 has been resolved at the lattice anchor itself. **Source change:** `firmware-v3/src/audio/backends/esv11/vendor/goertzel.h:28` `BOTTOM_NOTE 12 → 6`. NOTE_STEP, NUM_FREQS, and the chroma fold (`i % 12`) are all unchanged. Bin 0 now targets `notes[6] = 65.40639 Hz = C2`; bins 0..59 cover 5 complete octaves C2..B6 folding cleanly into chroma[0..11] = C..B; bins 60..63 (C7, C#7, D7, D#7) remain spectrum-only extras excluded from the fold.

**Why this is the right surface, not output rotation:** Captain's design intent locked the public chroma contract as the authoritative semantic. Re-anchoring the detector lattice (rather than rotating chroma[] / patching detectChord output / patching WebServerBroadcast NOTE_NAMES) propagates the fix uniformly through every downstream path — including the SB sidecar fold, detectChord interval templates (relative offsets work over any C-origin or non-C-origin lattice; correctness is preserved), the 4 hardcoded-C-origin effects (audit §5.2), the WebSocket `key` field broadcast (§4.1), and MusicalSaliency.prevChordRoot — without introducing a single compensation point that could be missed or double-applied.

**Verification:** New native test `firmware-v3/test/test_esv11_lattice/test_lattice_origin.cpp` asserts the C-origin frequency map at 8 anchor bins (C2/C3/C4/C5/C6/B6/C7/D#7) plus 3 chroma-pitch-class identity tests. TDD red→green confirmed: pre-fix all 11 assertions failed with the audit's verified D#-origin values (bin 0 = 77.78175 Hz, bin 63 = 2959.956 Hz, etc.); post-fix all 11 pass. Existing `test_esv11_parity_synthetic_120bpm` golden values were re-baselined because the 220 Hz (A3) test signal now lands on bin 21 instead of bin 18 (semantically correct: A pitch class now at chroma[9]). Canonical env `esp32dev_audio_esv11_k1v2_32khz` builds clean (RAM 43.5%, Flash 33.0%). K1 V2 (MAC `b4:3a:45:a5:87:f8`) flashed and behaviour-verified by Captain on hardware.

**Status of audit sections:**
- §0 Executive Summary — *"verified pitch-class origin status"* row: was DEFECT, now RESOLVED.
- §9 Risk Register — rows 1, 2, 4, 5 (rootNote +3 offset, WebSocket key mislabel, SbK1 spatial map, NOTE_HUES palette) all close as a consequence of the lattice flip.
- §10 Unresolved Questions — Q5 ("contract vs implementation: which yields") resolved: Captain locked the C-origin contract as authoritative; implementation now matches.
- §11 Non-Decisions — three items now superseded ("No production migration decision made", "No code changes made", "No architecture proposed" no longer apply).

Items NOT resolved by this fix (still open per audit):
- §6 HF semantic field naming/comments still claim 6–12 kHz; actual bins now cover ≈1.40–2.96 kHz (numbers shifted slightly by the lattice flip but the mismatch direction is unchanged — naming still misleading).
- §6.3 `bins256[]` declared but unwritten on canonical path — no true 6–12 kHz energy reaches ControlBus.
- §10 Q3 Sensory Bridge upstream pitch-class origin question — K1 lattice now diverges from upstream ES `BOTTOM_NOTE = 12`; SB-parity sidecar visual behaviour against the original SB reference is informational follow-up, not a defect.
- §8 Test coverage gap — 0 chord-detector contract fixtures still in place. New lattice tests cover frequency map only.

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-28 | orchestrator-claude (synthesised from 6 read-only SSAs) | Created. Audit synthesised from SSA-1 (build path), SSA-2 (Goertzel bank), SSA-3 (chroma fold), SSA-4 (consumers), SSA-5 (HF semantics; numeric values corrected against SSA-2's verified frequency table), SSA-6 (60/64/72 + fixtures). All claims traced to file:line; arithmetic independently verified by orchestrator. No source modified. |
| 2026-04-28 | orchestrator-claude (engineering pass) | Lattice fix applied: vendor/goertzel.h:28 `BOTTOM_NOTE 12 → 6`. C-origin restored. New `test_esv11_lattice` env passes 11/11; parity test goldens re-baselined. K1 V2 hardware-verified by Captain. See `### Update — 2026-04-28` above. |
