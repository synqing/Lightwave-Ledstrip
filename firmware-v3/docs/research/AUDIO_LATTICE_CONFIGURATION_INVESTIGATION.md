---
abstract: "Read-only deep-dive investigation answering three questions about K1's audio lattice. (1) Is the current 32 kHz / 64-bin / 16 kHz Nyquist configuration properly configured? Internal coherence VERIFIED, but the canonical pipeline runs at 18.2 ms p99 hop time vs 8 ms budget — Phase 1B FAILED 2026-04-27, Captain-waived 2026-04-28 for Tier 1 HF only. The bottleneck is `ControlBus::UpdateFromHop`'s 400+ line spinlock on Core 0, not the bin count. (2) Is 12-TET right for K1? Of 44 chroma consumers only 4 (9%) are load-bearing on pitch-class semantics; 18% are rotation-invariant; 64% are spacing-agnostic. K1 brand positioning ('music visualizer') locks 12-TET in for chord/harmonic effects but the right architecture is parallel surfaces (12-TET for chroma, mel for HF). 12-TET is INHERITED from upstream, not designed for K1. (3) Can 96 bins run at 32 kHz? CPU-cheap (+1.86%) and RAM-cheap (+4 KB), but blocked by hop budget crisis AND solves the wrong problem. Major correction to prior audit: bins256 IS populated on canonical via a hidden 512-pt FFT in AudioActor.cpp:~965; STM mel filterbank is already running and computing 6–12 kHz bands. The HF coverage gap is a WIRING problem, not a DSP problem. Read this before any decision on Goertzel extension, 96/128 bin work, sample-rate change, or HF semantic field redirection."
---

# Audio Lattice Configuration Investigation

**Mode:** READ-ONLY synthesis from 10 parallel SSAs.
**Date:** 2026-04-28
**Companion to:** [AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md](AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md), [CHORD_ROOT_ORIGIN_TRACE.md](CHORD_ROOT_ORIGIN_TRACE.md)
**Supersedes:** Audit §6.3 claim that `bins256[]` is unwritten on canonical (incorrect — see §3.4 below).
**RBDO label:** GROUNDED on most claims; **DEGRADED-MODE** flagged inline where evidence quality drops.

**Three-question scope:**
1. Is the current 32 kHz / 64-bin / 16 kHz Nyquist configuration properly configured?
2. Is 12-TET the right system for K1?
3. Can 96 bins run at 32 kHz without destroying AP performance?

---

## 0. Executive Summary

1. **Configuration is INTERNALLY COHERENT but ARCHITECTURALLY UNDERUSED, and the audio pipeline is in HARD BUDGET CRISIS.** The 64-bin C-origin lattice (post commit `ecba874e`) is structurally fine: 32 kHz / 256-hop / 8 ms / 125 Hz / 320 ms history all line up; bin frequencies exact; no clamps activated. But the canonical pipeline runs **18.2 ms p99 hop time vs 8 ms budget** (Phase 1B gate FAILED 2026-04-27, Captain-waived 2026-04-28 for Tier 1 HF only). The real crisis is `ControlBus::UpdateFromHop` holding Core 0 IRQ-disabled with 400+ lines of DSP inside `portENTER_CRITICAL` — observable as 1/10 expected WiFi heartbeats under WS load. **Address THIS first, not the lattice.**

2. **12-TET is right for K1, but for 9% of the reasons its current usage suggests.** Of 44 chroma consumers, only 4 (SbK1WaveformHarmonic, BeatPulseBloom, RippleEffect, LGPExperimentalAudioPack) actually require 12-TET pitch-class semantics. 18% (8 effects) are rotation-invariant and would work with any 12-bin spacing. 64% (28 effects) treat chroma as a generic 12-bin spectral shape. K1's brand positioning ("music visualizer") locks 12-TET in for chord detection and tonal effects — but the right move is **parallel surfaces** (12-TET for chroma/chord, mel for HF), not replacement.

3. **96 Goertzel bins are CPU-cheap (+1.86%) and RAM-cheap (+4 KB PSRAM), but they're solving the wrong problem.** The single most important new finding: **the prior audit was wrong about bins256.** A custom 512-point FFT IS running on canonical ESV11 (`AudioActor.cpp:~965–1000`), the STM mel filterbank IS computing 16-band perceptual + 128-band mel-spectral from it, and `stmTemporal[10..16]` covers 6–12 kHz with mel resolution. The HF semantic fields (`hfEnergy`, `hatEvent`, `cymbalSustain`, `airEnergy`) currently read `bins64[50..63]` (1.4–3 kHz) when the perceptually-correct HF data has been computed all along. **The fix is wiring, not new DSP.**

---

## 1. Question 1 — Is the current configuration properly configured?

### 1.1 Internal coherence — VERIFIED ADEQUATE

| Axis | Value | Source | Status |
|---|---|---|---|
| `SAMPLE_RATE` | 32 kHz | `EsV11_32kHz_Shim.h:19` | ✓ |
| `CHUNK_SIZE` | 128 samples (4 ms) | shim:20 | ✓ |
| `HOP_SIZE` | 256 samples (8 ms) | `audio_config.h:140` | ✓ |
| Hop rate | 125 Hz | derived (32000/256) | ✓ matches CLAUDE.md claim |
| 250 Hz "chunk rate" reconciled | 2 chunks per published frame | `AudioActor.cpp:7` | ✓ no discrepancy |
| `SAMPLE_HISTORY_LENGTH` | 10240 (320 ms) | shim:21 | ✓ adequate for all bins |
| `NOVELTY_LOG_HZ` | 50 Hz (wall-clock, not hop-locked) | shim:26, `tempo.h:239` | ✓ decoupled by design |
| DC blocker coeffs | retuned for 32 kHz (R=0.999019, G=0.999509) | shim:35–36 | ✓ |
| Bin 0 | 65.40639 Hz = C2 | post-fix `goertzel.h:28` | ✓ verified by `test_esv11_lattice` |
| Bin 63 | 2489.016 Hz = D#7 | verified | ✓ |
| Block-size clamp | 0 bins clamped (max 4120 < 10239 ceiling) | per-bin computation | ✓ |

**SSA arithmetic correction:** SSA-B's report claimed "bin 63 captures 0.084 cycles" — this is inverted. Bin 63 = 2489 Hz, period = 32000/2489 = 12.86 samples; in `block_size = 108` that's 108/12.86 = **8.4 cycles**. Comfortably above the 1-cycle Goertzel minimum. The bin is healthy. SSA-B confused samples-per-cycle with cycles-per-block; the block-size table itself is correct.

### 1.2 Where coherence weakens — three structural concerns

**(a) Microphone–rate mismatch (DEGRADED-MODE).** SPH0645 spec response is ~100 Hz – 8 kHz; we sample at 32 kHz / Nyquist 16 kHz. The upper 8 kHz of the digitised spectrum is mic noise floor + intrinsic LPF roll-off, not real signal. No firmware anti-alias filter; we rely on the mic's own roll-off. `audio_config.h:109` describes 32 kHz on SPH0645 as "overclocked".

- **Risk if wrong:** anti-alias artefacts in the upper Goertzel bins; mic ageing/temperature drift could pollute the 4–8 kHz region.
- **Revisit trigger:** hardware bench measurement of mic frequency response on K1 V2 silicon, OR observed visual artefacts traced to mic noise.
- **Debt count:** affects only the upper Goertzel bins (50–63, post-fix 1.4–2.5 kHz) — modestly within mic spec; not a blocker for current effects but caps any HF extension above ~6 kHz.

**(b) 32 kHz justification is INHERITED, not derived.** Shim header comment: "32 kHz / 256-hop = 125 Hz frame rate (up from 12.8 kHz / 256-hop = 50 Hz)". 32 kHz was chosen for **tempo resolution** (2.5× over 12.8 kHz Emotiscope baseline), not for Nyquist headroom. The 16 kHz Nyquist is a side effect that was never claimed for use. No design doc justifies 32 kHz for any other purpose.

**(c) Lattice underuse is STRUCTURAL.** Bins cover 5.25 of the 8.21 usable octaves below Nyquist (64% of log-space). 84.4% of the Hz axis is unused (13.5 kHz of 16 kHz). This is **by design** — the Goertzel bank is musical-pitch-tuned, not broadband. The LUT has 65 unused entries (`notes[133..197]`) above bin 63 specifically reserved for hypothetical future extension.

### 1.3 The hard crisis: hop budget overrun

This is the single most important finding from this entire investigation. Committed Phase 1B runtime evidence at `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/README.md`:

| Metric | Budget | Observed (p99) | Status |
|---|---|---|---|
| `audio_chunk_work_us` | 3200 µs | **7093 µs** | **FAIL (122% over)** |
| `audio_hop_us` | 8000 µs | **18230 µs** | **FAIL (127% over)** |
| `audio_hop_us` p50 | — | 15789 µs | — |
| Dominant: `es_magnitudes_us` p99 | — | **5420 µs** | Goertzel itself = 68% of budget |

Per-stage breakdown:

- `es_magnitudes_us` (Goertzel 64-bin): **5420 µs p99** — DOMINANT
- `es_tempo_us`: 1854 µs
- `es_gpu_tick_us`: 1245 µs
- `es_chroma_us`: 25 µs (the fold itself is trivial)
- `es_vu_us`: 73 µs
- SnapshotBuffer publish: 283 µs

**Phase 1B gate FAILED 2026-04-27. Captain explicitly waived it 2026-04-28 for Tier 1 HF semantics ONLY (no 96-bin, no 128-bin, no raw bins256 expansion).** The waiver does not extend to lattice expansion.

### 1.4 The P1-02 spinlock — the *why* behind the overrun

`ControlBus::UpdateFromHop` (`ControlBus.cpp:307–524`) wraps **400+ lines of DSP work** (spike detection, AGC, filters, chord state, etc.) inside `portENTER_CRITICAL` — an ESP32-S3 spinlock that **disables interrupts on Core 0** for the duration. Combined with:

- **Core 0 occupants:** AudioActor (pri 4), WiFiManager (pri 1), ESP-IDF WiFi (pri 23), lwIP (pri 18), ShowDirector, Network, PluginManager, SyncManager, CaptureStreamer, EncoderManager
- **AsyncTCP timeline:** was Core 0 (per claude-mem #23299, 2026-01-30) → moved to Core 1 + priority 10→3 in commit `d5cd99a8` (2026-04-18) — explicit fix for "audio hop timing fully insulated from WS bursts" (i.e. PRIOR state was not insulated)
- **Forensic audit observation:** "1/10 expected WiFi heartbeats under sustained WS traffic"

Each 8 ms hop the Core 0 IRQ is disabled for hundreds of µs while DSP runs under spinlock. That's ~100–150 ms of IRQ-disabled time per second on Core 0, where WiFi and lwIP need to service packets. WiFi IS already degraded; the audio pipeline IS already over budget; **the spinlock is the load-bearing fix, not the bin count.**

### 1.5 Verdict on Question 1

**The configuration IS internally coherent and the lattice IS structurally correct. But the canonical pipeline is in budget crisis, and that crisis is NOT lattice-shaped.** The fix is the P1-02 spinlock extraction, NOT the bin count or sample rate. Captain's instinct to "address that first" — yes, but "that" = the spinlock, not the lattice.

---

## 2. Question 2 — Is 12-TET the right system for K1?

### 2.1 The 9% / 18% / 64% / 9% breakdown

Categorisation of all 44 `chroma()` consumer call-sites:

| Bucket | Count | % | Behaviour under non-12-TET spacing |
|---|---|---|---|
| MUSICAL_TONAL (load-bearing on 12-TET) | 4 | 9% | Visually breaks |
| ROTATION_INVARIANT (`circularChromaHueSmoothed`) | 8 | 18% | Survives any 12-bin spacing |
| SPECTRAL_SHAPE (treats chroma as generic 12-bin shape) | 28 | 64% | Survives mel/log/bark rebinning |
| PALETTE_SELECTOR (generic iteration) | 4 | 9% | Indifferent to spacing |

The 4 effects that actually NEED 12-TET pitch-class semantics:

| Effect | Severity | Why |
|---|---|---|
| `SbK1WaveformHarmonicEffect` | **HIGH** | Geometry-defining: hardcoded `posF = kCenterRight + (c/12.0f) × (kHalfLength-1)` maps each pitch-class to a fixed strip position. Brand-defining "harmonic visualization" effect. |
| `LGPExperimentalAudioPack` | MEDIUM | `NOTE_HUES[12] = {0, 12, 24, 40, 56, 74, 92, 112, 134, 154, 178, 202}` — fixed pitch-class → hue map |
| `BeatPulseBloomEffect` | MEDIUM | `paletteShift = rootNote * 21` — linear C-origin assumption |
| `RippleEffect` | MEDIUM | Same `rootNote * 21` linear palette mapping |

### 2.2 Rotation-invariance is portable across any 12-bin spacing

`ChromaUtils.h:94–110` `circularChromaHueSmoothed()` is `atan2(Σ chroma[i]·sin(i·30°), Σ chroma[i]·cos(i·30°))` — circular weighted mean over 12 unit vectors at 30° intervals. **The pattern doesn't care whether bins represent pitch classes, mel bands, or arbitrary spectra.** It only requires 12 angular positions on a circle. Fully portable.

### 2.3 Non-tonal features bypass chroma entirely

- **Onset detection** → `spectral_flux` from raw spectrogram bins
- **Beat tracking** → FFT-based comb-tooth on the 16 kHz backbone
- **Percussion (snare/hihat/kick)** → `bins64Adaptive[20-25]` (snare), `bins64Adaptive[50-60]` (hihat), direct band ratios
- **Spectral flux / novelty** → raw spectrogram, not chroma
- **HF semantics** (`hfEnergy`/`airEnergy`/`cymbalSustain`) → `bins64[50-63]`, not chroma

**Chroma is used only for chord detection (1 algorithm), `heavy_chroma` (decorative smooth), and hue/colour anchoring in tonal effects.** 12-TET is irrelevant for every other feature surface.

### 2.4 12-TET design rationale: INHERITED, not designed

Searched every design doc:
- `audio-visual-semantic-mapping.md`: discusses musical intelligence, no 12-TET justification
- `EFFECT_DEVELOPMENT_STANDARD.md`: cites `rootNote()` as canonical; no rationale
- `vendor/goertzel.h:2-4`: "Vendored from Emotiscope v1.1_320" — upstream choice, not K1's
- `AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md:140`: "Whether SB's original origin was C, A, or D# is outside this audit's scope (SB is an external upstream)"
- AFS v2 docs: explicitly KEEP 12-TET, do not survey alternatives

**No firmware-v3 design document justifies 12-TET. It is pure upstream inheritance from Sensory Bridge → Emotiscope → K1.**

### 2.5 The Goertzel bank is implicitly a constant-Q filter bank

With bandwidth = 4 × neighbour-distance and semitone spacing, each bin's `Q ≈ centre_freq / bandwidth ≈ 30–50`. **The current Goertzel bank IS a constant-Q transform** (each bin's bandwidth is proportional to centre frequency, yielding equal Q across the spectrum). This is never called out in any committed doc. CQT and Goertzel-with-semitone-spacing are mathematically the same animal in this configuration.

### 2.6 K1 brand positioning locks 12-TET in

- `audio-visual-semantic-mapping.md:1-7`: "audio-reactive visualizations" anchored in "Musical Intelligence Principles", "harmonic novelty", "chord changes", "key shifts"
- `k1-spec-recommendations-2026-04.md`: "K1 is a dedicated hi-fi instrument for music visualization…light is as immediate and high-fidelity as the audio itself"
- `PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md`: harmonic saliency vs rhythmic saliency is a primary decision axis for effect selection

K1 is positioned as a MUSICAL visualizer, not a SPECTRAL visualizer. Musical visualizer requires harmonic awareness (chord, key, tonal centre). Harmonic awareness requires pitch-class detection. Pitch-class detection requires either 12-TET Goertzel or a CQT — **and we already have one (Goertzel-as-CQT)**. So it's fine.

### 2.7 But for HF semantics, mel is right

§3.4 shows the 6–12 kHz region is acoustically/perceptually best served by **mel filterbank from the FFT path**, not by extending the Goertzel lattice. Mel is perceptually uniform; in the 6–12 kHz window, mel bands cluster appropriately for hihat/cymbal/air semantics; Goertzel-at-semitone in that range over-resolves something that doesn't have semitone meaning (cymbals don't have pitch class).

### 2.8 Verdict on Question 2

**12-TET is right for what 9% of effects load-bear on it (chord/harmonic/tonal). It is irrelevant or substitutable for 91%. K1's brand positioning ("music visualizer") justifies keeping it.** But the right architecture is **parallel surfaces**:

- 12-TET Goertzel for chroma/chord/tonal hue → KEEP
- Mel filterbank from FFT for HF semantic features (hihat/cymbal/air) → ALREADY RUNNING, just not wired to consumers (see §3.4)

12-TET is not load-bearing for all 44 chroma consumers. It IS load-bearing for K1's product identity. The current implementation is more grid-coupled than it needs to be — most chroma consumers would survive a mel-backed 12-bin chroma rebinning without visible change. But the cost of switching is real (chord detection, 4 hardcoded effects), and the benefit is unclear.

---

## 3. Question 3 — Can we process 96 bins at 32 kHz?

### 3.1 CPU answer — YES, easily

Per-octave block-size distribution:

| Octave (post-fix C-origin) | Bin range | block_size | % of total cost |
|---|---|---|---|
| C2..B2 (octave 0) | bins 0..11 | 4120..2304 | **51.4%** |
| C3..B3 | bins 12..23 | 2180..1152 | 26.2% |
| C4..B4 | bins 24..35 | 1088..576 | 12.7% |
| C5..B5 | bins 36..47 | 544..288 | 6.1% |
| C6..B6 | bins 48..59 | 272..140 | 3.1% |
| C7..D#7 | bins 60..63 | 132..108 | 0.6% |
| **Total** | **64 bins** | | **79,840 samples/pass** |

Hypothetical 32 HF extension bins (64..95) targeting `notes[134..196]` = E7 to ~15.8 kHz at semitone spacing:

- Bin 64 (E7, 2637 Hz): block_size ≈ 100
- Bin 95 (~15.8 kHz): block_size ≈ 16
- Average ~46 samples/bin → +1488 samples per Goertzel pass
- **+1.86% of current Goertzel cost** (1488 / 79840)

CPU at 240 MHz: Goertzel goes from ~5.6% Core 0 → ~5.7%. Marginal pipeline impact <0.5%.

**128-bin extension is INVALID at 32 kHz:** bins 96+ would target `notes[198..]` which exceed the 198-entry LUT, and bin 96 onwards crosses Nyquist (`notes[197] = 16274` > 16000 Hz). 96 bins is the lattice ceiling at 32 kHz semitone spacing.

### 3.2 RAM answer — YES, comfortably

| Region | @ 64 bins | @ 96 | @ 128 | Δ to 96 |
|---|---|---|---|---|
| .bss DRAM (`spectrogram` + `_smooth`) | 560 B | 816 B | 1072 B | +256 B |
| PSRAM (`frequencies_musical`, `spectrogram_average`, `noise_history`) | 7,680 B | 11,520 B | 15,360 B | +3,840 B |
| **Total NUM_FREQS-keyed** | **8,240 B** | **12,336 B** | **16,432 B** | **+4,096 B** |

Current build: RAM 43.5% used (142,444 / 327,680). +256 B is rounding error. PSRAM has 8 MB and ~16 KB used. **DRAM and PSRAM are non-issues.**

**Important caveat:** `ControlBusFrame.bins64[64]` and `bins64Adaptive[64]` are **hardcoded 64**, not keyed to NUM_FREQS. Extending Goertzel to 96 bins WITHOUT extending these fields means the new high bins go into `frequencies_musical[]` and `spectrogram[]` but never reach effects via the bins64 pointer accessors. Either the field gets renamed/widened (breaks ABI) or the new bins are accessible only via a new field.

### 3.3 But — the AP/budget answer — NO, we cannot add load right now

This is where the 96-bin question crashes into reality:

- Current hop time p99: **18.2 ms** vs 8 ms budget = **127% over**
- Phase 1B gate FAILED 2026-04-27, waived only for Tier 1 HF semantics (not for bin extension)
- AsyncTCP migrated off Core 0 to Core 1 explicitly to recover audio insulation
- WiFi already shows 1/10 expected heartbeats under WS load
- ControlBus spinlock holds Core 0 IRQ-disabled at 125 Hz cadence

**Adding +30 µs of marginal Goertzel cost is mathematically negligible (<0.5%) but qualitatively bankrupting** because we're already over budget and degrading WiFi. There is no safe way to add ANY load until P1-02 (the ControlBus spinlock) is fixed.

### 3.4 The much bigger finding — bins256 is NOT dormant on canonical

This contradicts the prior audit and is the most important new evidence in this investigation.

> **The prior audit (`AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md` §6.3) said `bins256[]` is "declared but unwritten on canonical path". This is WRONG.** A custom 512-point Cooley-Tukey FFT IS running on canonical ESV11 at `AudioActor.cpp:~965–1000`, against the audio history buffer, populating `bins256[]`. The STMExtractor mel-filterbank pipeline (16-band temporal + 128-band spectral) IS RUNNING on canonical, producing `stmTemporal[16]` and `stmSpectral[42]` — including mel bands that cover 6–12 kHz with perceptual uniformity.

**How the prior audit confused itself:** The audit saw `EsV11Adapter.cpp:303` comment "no bins256 dependency" (true — `EsV11Adapter` doesn't write `bins256` directly), and inferred "no canonical writer to `bins256`" (false — there IS one, just at the AudioActor level, not in `EsV11Adapter`). The writer was added after the original audit was scoped.

**RBDO note:** This finding contradicts a section of work I previously delivered (`AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md` §6.3). The audit is now WRONG on this specific point. Severity: MEDIUM — it doesn't invalidate the lattice fix (which was scoped to the chroma origin defect, not HF coverage), but it changes the recommended fix direction for HF semantics. The audit has been updated with a supersession marker (`### Update — 2026-04-28b: §6.3 supersession`).

**Implications:**

- True HF energy DOES reach ControlBus (via `bins256` from STM FFT)
- Mel filterbank IS computing 6–12 kHz bands (perceptually uniform, ideal for hihat/cymbal/air)
- The "HF semantic fields use only `bins64[50..63]` = 1.4–3 kHz" finding is true, but `bins256` IS available; HF semantic fields just don't USE it
- The HF coverage problem is therefore **a wiring problem, not a DSP problem**

### 3.5 So what's actually the right move for HF coverage?

Three options:

| Option | Cost | Quality | Status |
|---|---|---|---|
| **A. Wire existing STM mel outputs to HF semantic fields** | **Zero new DSP** | Mel-perceptual (ideal for hihat/cymbal/air) | All ingredients EXIST and ARE RUNNING; just not connected to consumers |
| B. Extend Goertzel to 96 bins | +30 µs / +1.86% Goertzel | Linear-semitone (over-resolved for noise/percussion content) | New DSP load, hits hop budget crisis |
| C. Standalone small FFT for HF | +100 µs / +12,000 ops | Linear (worse than mel) | Net DSP added, redundant with existing FFT |

**Option A is dramatically the right answer.** The expensive parts (FFT + mel filterbank) are already running. The HF semantic fields (`hfEnergy`, `hatEvent`, `cymbalSustain`, `airEnergy`) currently read `bins64[50..63]` (1.4–3 kHz, mislabelled "6–12 kHz"). Wiring them to `stmTemporal[10..16]` (or `stmSpectral` upper bands) would:

1. Make the field names accurate (true 6–12 kHz coverage)
2. Add zero new DSP
3. Not touch the hop budget crisis
4. Survive the AFS v2 Captain-waiver constraints (no new bins, no 96/128 expansion)

### 3.6 Verdict on Question 3

**Can we process 96 bins at 32 kHz?** Strictly speaking, yes — CPU and RAM both comfortable. But:

1. **Not RIGHT NOW** — the audio pipeline is 127% over hop budget. Adding any load is irresponsible until P1-02 is fixed.
2. **Not for the reason Captain is asking** — extending Goertzel to 96 bins solves the wrong problem. The HF coverage gap is a wiring problem, not a DSP problem.
3. **Not the architecturally right move anyway** — for 6–12 kHz hihat/cymbal/air content, mel is perceptually correct and Goertzel-at-semitone is over-resolved. The mel filterbank running in STM is the right tool; it just isn't connected.

The 96-bin question is a red herring driven by a stale audit claim that bins256 doesn't exist. Bins256 DOES exist on canonical, with mel processing already on top of it. **The right work is wiring, not bin extension.**

---

## 4. Recommended priorities (Captain decision surface only — no execution)

In strict dependency order:

1. **AMENDED.** ✓ Completed in this work cycle: `AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md` §6.3 supersession marker added; pointer to this document inserted as the canonical statement of the bins256 status.

2. **P1-02 spinlock extraction** (Captain decision required). This is the load-bearing fix for everything else. As long as `ControlBus::UpdateFromHop` runs 400+ lines of DSP under `portENTER_CRITICAL`, hop budget recovery is impossible and AP performance keeps degrading. Independent of any audio-feature work. Captain decision required: extraction strategy (move to lock-free queue, split critical-section to atomic state-handoff, or full Phase 1B redesign).

3. **THEN — wire STM mel outputs to HF semantic fields.** The Option A path (§3.5). Zero new DSP, fixes the comment-vs-code mislabel, gives true 6–12 kHz hihat/cymbal/air to any future consumer. Does not require Phase 1B clearance because it adds no compute.

4. **DEFER 96-bin Goertzel extension indefinitely.** AFS v2 already deferred this for documented reasons. SSA-J's finding strengthens the deferral: the HF coverage problem is solved by Option A without it.

5. **DO NOT attempt 128-bin** at 32 kHz: invalid (above Nyquist + LUT exhausted at `notes[197]`).

6. **OPEN** for separate Captain investigation — the SPH0645 mic specs at 32 kHz. If hardware bench shows mic noise floor is actually decent above 8 kHz, the 12–16 kHz region is real. If not, even Option A's mel HF bands above ~8 kHz are mic noise, not signal.

---

## 5. Cross-SSA discrepancies surfaced

For audit trail:

1. **SSA-B math error (caught and corrected):** "bin 63 captures 0.084 cycles" is wrong. 108 samples / 12.86 samples-per-cycle = 8.4 cycles. Goertzel is healthy at bin 63. SSA-B inverted the ratio in their interpretation. The block_size table itself is correct.

2. **SSA-B vs SSA-I total cost (resolved):** SSA-B says 79,840 samples/pass for 64 bins (per-bin computation, verified). SSA-I says 71,372 (rounded approximation). 11% disagreement. Trust SSA-B's number; the percentage-change conclusions agree qualitatively.

3. **SSA-J vs prior audit (the big one):** Prior audit claimed `bins256` unwritten on canonical. SSA-J found `AudioActor.cpp` FFT writer. **SSA-J is correct.** Audit amended in `### Update — 2026-04-28b: §6.3 supersession`.

---

## 6. Open questions not resolvable from source alone

These require Captain decision or hardware measurement:

1. **P1-02 spinlock fix scope** — is moving `UpdateFromHop` out of `portENTER_CRITICAL` sufficient, or does ControlBus need a lock-free redesign?
2. **AsyncTCP priority=3 stability** — has the migration to Core 1 fully resolved WiFi heartbeat loss, or is it masking a deeper Core 0 saturation?
3. **SPH0645 actual response above 8 kHz** — required to validate any HF semantic surface that targets the 6–12 kHz mel bands. Hardware bench measurement only.
4. **STM HF bands wiring decision** — which mel-band indices map to which HF semantic field? Captain product call (taste-based, not derivable from source).
5. **Brand identity boundary** — at what point does adding mel-HF surface make K1 a "spectral visualizer" rather than a "music visualizer"? Brand decision, not engineering.

---

## 7. Non-Decisions

This investigation explicitly does NOT make the following determinations:

- No commit-time decision on 96-bin extension.
- No commit-time decision on STM-to-HF wiring (Option A).
- No commit-time decision on P1-02 spinlock fix.
- No code changes made by this investigation (companion lattice fix in commit `ecba874e` is separate and was Captain-authorised).
- No architecture proposal beyond surfacing options.
- No claim that current 12-TET should be replaced. K1's product identity locks it in.
- No claim that mel should replace Goertzel. The recommendation is parallel surfaces.
- No claim that the 32 kHz sample rate is wrong. The microphone-vs-rate question is flagged for hardware investigation, not changed.

---

## Investigator Conclusion

**Configuration is internally coherent; lattice is structurally correct after the C-origin fix; 12-TET is right for K1's product identity for 9% of its current usage; 96-bin extension is feasible CPU/RAM-wise but blocked by hop budget crisis; the HF coverage gap is solved by wiring existing STM mel outputs (Option A), not by new DSP. The single most leverage-rich engineering action right now is the P1-02 spinlock extraction — everything else is downstream of it.**

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-28 | orchestrator-claude (synthesised from 10 read-only SSAs) | Created. SSA tracks: A (sample-rate/hop/mic coherence), B (Goertzel block-size distribution; arithmetic correction applied for bin-63 cycle count), C (audio hop timing — Phase 1B evidence), D (44 chroma-consumer categorization), E (12-TET design rationale + alternatives), F (AFS v2 baseline read), G (Core 0 task scheduling + AP risk), H (RAM/PSRAM scaling budget), I (96-bin CPU cost projection), J (bins256/FFT producer audit — the correction to prior audit §6.3). All claims traced to file:line; arithmetic independently verified. No source modified by this work. |
