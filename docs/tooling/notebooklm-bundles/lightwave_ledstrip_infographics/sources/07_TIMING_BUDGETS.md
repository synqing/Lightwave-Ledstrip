---
abstract: "Timing-budget source of truth for the K1v2 infographic series — feeds Panel 6 directly. Every timing value carries a tier (Confirmed / Derived / Budgeted / Verify) and a source citation. Audio at 32 kHz / 125 Hz / 8 ms hop; render at 120 FPS / 8.33 ms frame; 2.0 ms effect-render ceiling (Budgeted). LED show timing: source-documented ~6.3 ms; K1v2 topology-specific measurement pending. Audio-AGC partitioning entries struck pending BACKLOG F-6 — no partition numbering invoked anywhere."
---

# 07 — Timing Budgets

This file is the canonical timing reference for Panel 6 (Timing & Safety Budget) and any timing value referenced in Panels 1–5. Every value below is classified with a confidence tier; no value may appear in a panel without its tier and source citation.

All values must agree with `01_VERIFIED_FIRMWARE_FACTS.md`. Drift between these two files is a defect — fix here AND there in the same change.

Build target: `esp32dev_audio_esv11_k1v2_32khz` (canonical K1 V2 production).

## Confidence tiers

| Tier | Symbol | Meaning |
|---|:---:|---|
| **Confirmed** | C | Direct source citation. Reading the cited line shows the value. |
| **Derived** | D | Computed from a Confirmed value via simple arithmetic. Citation = the source plus the computation. |
| **Budgeted** | B | Architectural target stated in code, comments, or hard-constraint documentation. Not a measurement. |
| **Verify** | V | Stated somewhere but not yet validated for K1v2 hardware. Do not cite as fact in any public surface. |

Tier rules:
- A `Derived` value is only as defensible as the `Confirmed` value it was derived from. If the upstream changes, recompute.
- A `Budgeted` value is a *ceiling* or *target*, not an observation. It must be labelled as such on every panel.
- A `Verify` value MUST carry the `Verify (K1v2)` badge on any panel that displays it. No exceptions.

## Master timing table

Every row carries: value, units, tier, source citation, and notes. Panel 6 must mirror this table exactly.

### Audio capture

| # | Value | Units | Tier | Source | Notes |
|---|---|---|:---:|---|---|
| AT1 | 32 000 | Hz | C | `firmware-v3/include/EsV11_32kHz_Shim.h:19` (`#define SAMPLE_RATE (32000)`) | Audio sample rate. ESV11 backend at 32 kHz is the canonical K1 build path. |
| AT2 | 125 | Hz | C | `EsV11_32kHz_Shim.h:9` ("32kHz / 256-hop = 125 Hz frame rate") | Audio analysis frame rate. One ControlBus frame published every 8 ms. |
| AT3 | 8.000 | ms | D | 32 000 ÷ 256 = 125 Hz → period 1 ÷ 125 = 8.000 ms | Audio hop period. Cross-checked against `EsV11_32kHz_Shim.h:9` shim comment. |
| AT4 | 256 | samples | C | `EsV11_32kHz_Shim.h` (hop size = 256 samples) | Samples per audio hop. Drives `AT2` and `AT3`. |
| AT5 | 4.000 | ms | C | `EsV11_32kHz_Shim.h:20, 26` (`CHUNK_SIZE (128)`) | Audio chunk size = 128 samples = 4 ms at 32 kHz. Two chunks per hop. |
| AT6 | 128 | samples | C | `EsV11_32kHz_Shim.h:20, 26` | Chunk size in samples. |

### Spectral analysis

| # | Value | Units | Tier | Source | Notes |
|---|---|---|:---:|---|---|
| ST1 | 512 | samples | C | `firmware-v3/src/audio/pipeline/PipelineCore.h:40` (`windowSize = 512`) | FFT window size (Hann-windowed). |
| ST2 | 256 | bins | C | `PipelineCore.h:40` (`kNumBins = kMaxWindow / 2`) | FFT magnitude bin count (real-FFT N/2). Field name `bins256[]`. **Do NOT call this 512 bins.** |
| ST3 | 62.5 | Hz/bin | D | 32 000 ÷ 512 = 62.5 Hz | FFT bin spacing at 32 kHz with 512-sample window. |
| ST4 | 64 | bins | C | `firmware-v3/CLAUDE.md` audio backend section + Goertzel bank source under `firmware-v3/src/audio/` | Goertzel bank size — sparse-spectrum measurement at musically meaningful frequencies. |
| ST5 | 12 | classes | C | `firmware-v3/src/audio/contracts/ControlBus.h` (`chroma[12]` field) | Chroma vector — one per semitone. |
| ST6 | 8 | bands | C | `ControlBus.h` (`bands[8]` field) | Octave-band summed energy. |

### Tempo / beat tracking

| # | Value | Units | Tier | Source | Notes |
|---|---|---|:---:|---|---|
| TT1 | 0.999 | per frame | C | `firmware-v3/src/audio/tempo/TempoTracker.cpp` (novelty-envelope decay) | Novelty decay coefficient. Goertzel-based tempo tracker — **NOT Kalman**. |

### Cross-core publication

| # | Value | Units | Tier | Source | Notes |
|---|---|---|:---:|---|---|
| XT1 | ≤ 5120 | bytes | C | `firmware-v3/src/audio/contracts/ControlBus.h:261-262` (`static_assert(sizeof(ControlBusFrame) <= 5120, ...)`) | ControlBusFrame compile-time upper bound. **Use the upper-bound wording**, not equality. The actual `sizeof` may be smaller. |

### Render path

| # | Value | Units | Tier | Source | Notes |
|---|---|---|:---:|---|---|
| RT1 | 120 | FPS | C | `firmware-v3/src/core/actors/RendererActor.h:112-113` (`static constexpr uint16_t TARGET_FPS = 120`) | Target render rate. |
| RT2 | 8.333 | ms | C | `RendererActor.h:113` (`FRAME_TIME_US = 1000000 / TARGET_FPS` = 8333 µs) | Render frame budget — total wall-time per render iteration. |
| RT3 | 2.0 | ms | **B** | `firmware-v3/CLAUDE.md` hard-constraint section ("per-frame effect code MUST complete in under 2.0 ms"); echoed in effect-class documentation | Per-frame effect-render ceiling. **Architectural ceiling, not a measurement.** Effects that exceed this are defects. |

### LED topology and output

| # | Value | Units | Tier | Source | Notes |
|---|---|---|:---:|---|---|
| LT1 | 320 | LEDs | C | `firmware-v3/src/test_strip_hw.cpp` (`NUM_LEDS`); per-strip count of 160 in render-path comments | LED count on K1v2. Dual 160-LED LGP strips. **No status strip on K1v2.** |
| LT2 | 80 | constant | C | `firmware-v3/src/core/system/OtaLedFeedback.h:53` (`CENTER_POINT = 80`) | Per-strip centre constant. Visual centre is the seam between LEDs 79 and 80 (left side renders from `CENTER_POINT - 1`; right from `CENTER_POINT`). |
| LT3 | ~6.3 | ms | **V** | Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending. | LED show timing. Display this row in Panel 6 with the exact wording from the Source column — topology-neutral, no parenthetical about LED counts or status strips on the same line. |

## Phase relationships (audio vs render)

The audio and render loops run at intentionally close-but-not-synchronous rates:

| Loop | Period | Driver |
|---|---|---|
| Audio analysis (Core 0) | **8.000 ms** (125 Hz, AT3) | I2S DMA hop boundary |
| Render (Core 1) | **8.333 ms** (120 FPS, RT2) | RendererActor frame timer |

**Phase drift:** Each render frame is approximately **0.333 ms longer** than each audio hop. Over time the two loops slip relative to one another — there is no shared clock, no interlock, and no expectation that a render frame consumes exactly one audio frame.

**Why this is safe:** the SnapshotBuffer absorbs the drift. The renderer reads whichever audio snapshot is currently published; if a new audio frame arrives mid-render, the next render iteration picks it up. If two render iterations share a single audio snapshot, the renderer interpolates internally between snapshots. The lock-free double-buffer + atomic-sequence design (`SnapshotBuffer.h:27-52`) means the renderer never blocks on the audio actor, and the audio actor never waits for the renderer.

**Why intentionally close:** at 120 FPS render and 125 Hz audio, the perceptual gap is sub-frame. The renderer effectively gets a fresh audio snapshot every render frame on average, with occasional duplicated reads (when render is fastest) or dropped reads (when audio is fastest) — both absorbed by the snapshot bridge. This is a deliberate design choice, not a bug.

## K1v1 historical note (context only — not a current K1v2 fact)

> **Historical / does not apply to K1v2 as a current measurement.**

A ~6.3 ms LED-show figure appears in earlier session notes from **K1v1 hardware** with a **30-LED status alt-build** active (third strip beyond the dual 160 LGP strips, bringing total physical LED count to 350) on **2026-02-28**. That status strip caused performance degradation — the additional strip's RMT serialisation cost stretched the LED-show wall time and was the reason the alt-build was **deactivated**. K1v2 reverts to the dual-strip 320-LED topology only.

This subsection is **context for the deactivation decision**, not an LED-show timing claim. The current public LT3 wording is the Captain-approved phrasing in the Master timing table above. Consequences for Panel 6 and any other panel that wants to show LED-show timing:

- This historical K1v1 figure must NOT be re-presented as a current K1v2 measurement, nor cited in any panel as if it transferred to K1v2 hardware.
- LT3's public wording is the only sanctioned LED-show line. Use that exact phrasing — do not paraphrase, do not append topology parentheticals, and do not blend it with the K1v1 narrative above.
- A fresh K1v2 measurement must replace the LT3 wording (with a new dated source citation) before any LED-show timing other than the Captain-approved phrasing can appear in any panel.

This is an active discipline — do not work around it by recycling the K1v1 figure inside the LT3 row or any panel.

## Deferred — audio AGC partitioning audit

A firmware constant in `firmware-v3/src/audio/contracts/ControlBus.h` defines an audio-AGC partition count that does not match the Captain-defined hard rule, and is tracked as a separate firmware-fix item in `BACKLOG.md` § F-6.

**Until that fix lands, all audio-AGC partitioning timing entries are STRUCK from this file, and no user-facing or API-level partition numbering is invoked anywhere in the infographic set.** Specifically:

- Per-partition AGC follower update timing — STRUCK.
- Per-partition chroma publication timing — STRUCK.
- Partition-AGC band timing in `ControlBus.cpp` — STRUCK.
- Any partition-count row in Panel 6's timing table — STRUCK.

Do NOT add an audio-AGC partitioning row that asserts any specific partition count, even with a footnote. The discipline is to keep partition counts and partition identifiers off the infographic until the firmware matches the documented hard rule.

When BACKLOG F-6 is closed and the firmware constant is corrected, this file MUST be revisited — add the partitioning timing rows then, with citations to the post-fix source.

## Cross-checking against `01_VERIFIED_FIRMWARE_FACTS.md`

Every value in this file must match the corresponding entry in `01_VERIFIED_FIRMWARE_FACTS.md`. The mapping:

| This file | `01_VERIFIED_FIRMWARE_FACTS.md` |
|---|---|
| AT1 (32 kHz) | A1 |
| AT2 (125 Hz) | A2 |
| AT3 (8 ms hop) | A3 |
| AT4 (256 samples) | A3 (cited via 32 000 ÷ 256) |
| AT5 (4 ms chunk) | A4 |
| AT6 (128 samples) | A4 |
| ST1 (512-sample window) | S1 |
| ST2 (256 bins) | S2 |
| ST3 (62.5 Hz spacing) | S3 |
| ST4 (64 Goertzel bins) | S4 |
| ST5 (12 chroma) | S5 |
| ST6 (8 bands) | S6 |
| TT1 (0.999 novelty decay) | T2 |
| XT1 (≤5120 bytes) | X1 |
| RT1 (120 FPS) | R1 |
| RT2 (8.33 ms frame budget) | R2 |
| RT3 (2.0 ms effect ceiling) | R3 |
| LT1 (320 LEDs) | H2 / L3 |
| LT2 (CENTER_POINT = 80) | H3 |
| LT3 (LED show — Verify, source-documented ~6.3 ms; K1v2 measurement pending) | L2 |

If any row in this table drifts from `01_VERIFIED_FIRMWARE_FACTS.md`, fix both files in the same change. Drift between these two files is a defect.

## What this file does NOT contain

- No measured render-frame execution times — those are not currently captured under `_trace`-environment telemetry on K1v2 and would be `Verify` if added.
- No measured effect-render times for individual effects — likewise `Verify` until MabuTrace captures exist for K1v2.
- No measured audio-pipeline execution times — likewise `Verify`.
- No audio-AGC partitioning timing rows — STRUCK pending BACKLOG F-6. No partition numbering invoked.
- No K1v1 LED-show timing as a current K1v2 fact — historical context only.

If a future panel needs any of the above, capture the measurement first via `firmware-v3/tools/capture_trace.py` against `esp32dev_audio_esv11_k1v2_32khz_trace`, then add the row here with the date of capture and the trace artefact path.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Authored from Rev 3 plan timing-budget specification cross-referenced with `01_VERIFIED_FIRMWARE_FACTS.md`. Master timing table covers audio capture, spectral analysis, tempo, cross-core publication, render path, and LED topology — every row tagged Confirmed / Derived / Budgeted / Verify with source citation. K1v1 30-LED status-strip historical note explicitly excluded from K1v2 applicability. Audio-AGC zone-count rows struck pending BACKLOG F-6. LT3 (K1v2 LED wire time) classified `Verify` — 6.3 ms K1v1-era figure does not transfer. |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 amendments. (Z) Expanded zone strike: removed all user-facing / API zone numbering (Zone 0/1/2/3/4) from the Deferred section, abstract, and "What this file does NOT contain" — replaced with topology-neutral "audio-AGC partitioning" wording. Closed the prior narrow exception that allowed user-facing zone identifiers. (L) LED wire-time exact wording: replaced LT3 row's verdict and notes with Captain's exact phrasing — "Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending." Rewrote the K1v1 historical-note subsection to frame it as context for the deactivation decision (not as a current K1v2 LED-show timing claim) and explicitly direct readers to LT3's sanctioned wording for any panel use. Updated cross-check table row and abstract to match. |
