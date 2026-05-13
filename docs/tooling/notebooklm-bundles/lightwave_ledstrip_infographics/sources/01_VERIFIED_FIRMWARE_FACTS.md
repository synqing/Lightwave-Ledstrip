---
abstract: "Verified firmware facts for the K1v2 infographic series. Every claim carries a file:line source citation and a confidence tier (Confirmed/Derived/Budgeted/Verify). Includes the K1v1 30-LED status-strip historical note and the deferred audio-AGC partitioning audit (BACKLOG F-6). Single source of technical truth for all six panels."
---

# 01 — Verified Firmware Facts

## Confidence tiers

Every claim in this file is tagged with one of four confidence tiers:

| Tier | Meaning |
|---|---|
| **Confirmed (C)** | Direct source citation. Reading the cited line shows the value. |
| **Derived (D)** | Computed from a Confirmed value via simple arithmetic. |
| **Budgeted (B)** | An architectural target stated in code or constraints, not a measurement. |
| **Verify (V)** | Stated somewhere but not yet verified for K1v2 hardware. Do not cite as fact. |

Build target this file applies to: `esp32dev_audio_esv11_k1v2_32khz` (canonical K1 V2 production).

## Hardware

| # | Claim | Tier | Source |
|---|---|:---:|---|
| H1 | ESP32-S3 microcontroller (dual-core, Tensilica LX7) | C | `firmware-v3/platformio.ini` (board configuration) |
| H2 | LGP strips: 2 × 160 WS2812 LEDs = 320 total | C | `firmware-v3/src/test_strip_hw.cpp` (`NUM_LEDS`); per-strip count of 160 in render-path comments |
| H3 | Per-strip centre constant: `CENTER_POINT = 80` | C | `firmware-v3/src/core/system/OtaLedFeedback.h:53` |
| H4 | Centre origin is the **seam between LEDs 79 and 80** (left side renders from `CENTER_POINT - 1`, right side from `CENTER_POINT`) | C | Render fill logic + Captain hard-rule documentation |
| H5 | MEMS microphone via I2S (capture-side details depend on hardware revision; consult firmware for exact pinout) | C | `firmware-v3/src/audio/AudioActor.cpp` (I2S init) |
| H6 | LED output via ESP32-S3 **RMT4 peripheral** (Remote Transceiver Module). CPU returns immediately after queuing buffer; wire time runs in parallel. | C | `firmware-v3/src/core/actors/RendererActor.cpp` ("patched FastLED RMT4: CPU returns quickly; wire time runs in parallel") |

**Important:** Do NOT describe LED output as "DMA-backed" — RMT is a hardware peripheral, not CPU-DMA. Functionally equivalent for LED streaming, but the terminology matters when readers cross-check against the ESP-IDF documentation.

## Audio capture

| # | Claim | Tier | Source |
|---|---|:---:|---|
| A1 | Audio sample rate: **32 kHz** | C | `firmware-v3/include/EsV11_32kHz_Shim.h:19` (`#define SAMPLE_RATE (32000)`) |
| A2 | Frame rate: **125 Hz** (one audio analysis frame every 8 ms) | C | `EsV11_32kHz_Shim.h:9` ("32kHz / 256-hop = 125 Hz frame rate") |
| A3 | Hop size: **256 samples** = 8 ms at 32 kHz | D | 32 000 ÷ 256 = 125 Hz; cross-checked against shim comment |
| A4 | Chunk size: **128 samples** = 4 ms | C | `EsV11_32kHz_Shim.h:20, 26` (`CHUNK_SIZE (128)`) |

## Spectral analysis

| # | Claim | Tier | Source |
|---|---|:---:|---|
| S1 | FFT window: **512 samples** (Hann-windowed) | C | `firmware-v3/src/audio/pipeline/PipelineCore.h:40` (`windowSize = 512`) |
| S2 | FFT output: **256 magnitude bins** (real-FFT N/2) | C | `PipelineCore.h:40` (`kNumBins = kMaxWindow / 2`) |
| S3 | Bin spacing: **62.5 Hz at 32 kHz** | D | 32 000 ÷ 512 = 62.5 Hz |
| S4 | Goertzel bank: **64 bins** for sparse-spectrum measurement | C | `CLAUDE.md` audio backend section + Goertzel bank source in `firmware-v3/src/audio/` |
| S5 | Chroma vector: **12 pitch classes** (one per semitone) | C | `firmware-v3/src/audio/contracts/ControlBus.h` (`chroma[12]` field) |
| S6 | Octave bands: **8 bands** of summed energy | C | `ControlBus.h` (`bands[8]` field) |

**Important:** Do NOT claim "512 FFT bins". The FFT *window* is 512 samples; the *output bin count* is 256. Real-FFT halves the output. The ControlBus field that carries the magnitudes is named `bins256[]`, not `bins512[]`.

## Tempo and beat tracking

| # | Claim | Tier | Source |
|---|---|:---:|---|
| T1 | Beat tracker: **Goertzel-based** with novelty-envelope detection | C | `firmware-v3/src/audio/tempo/TempoTracker.cpp:1–17` ("Implementation of Goertzel-based tempo tracker") |
| T2 | Novelty decay coefficient: **0.999 per frame** | C | TempoTracker source |
| T3 | Magnitude smoothing applied to candidate peaks | C | TempoTracker source |
| T4 | Tempo estimate published to ControlBus per audio frame | C | ControlBus.h tempo fields |

**Important:** Do NOT describe the tempo tracker as "Kalman-filtered". There is no Kalman filter in the firmware. It is a Goertzel-bank tempo estimator with novelty/energy history and smoothed winner selection.

## Cross-core publication

| # | Claim | Tier | Source |
|---|---|:---:|---|
| X1 | ControlBusFrame size: **≤ 5120 bytes** (compile-time upper bound) | C | `firmware-v3/src/audio/contracts/ControlBus.h:261–262` (`static_assert(sizeof(ControlBusFrame) <= 5120, ...)`) |
| X2 | ControlBusFrame fields include: spectral magnitudes (`bins256[]`), chroma (12), bands (8), RMS, beat, onset, tempo, and percussion triggers (audio-AGC partition entries deferred — see section below) | C | `ControlBus.h:119–259` |
| X3 | SnapshotBuffer is **lock-free**: double-buffered (`m_buf[2]`) with `std::atomic<uint32_t>` for the active-index and sequence number, `memory_order_release` on publish, `memory_order_acquire` on read | C | `firmware-v3/src/audio/contracts/SnapshotBuffer.h:27–52` |
| X4 | Publication direction: **Core 0 (AudioActor) → Core 1 (RendererActor)** | C | Actor model documented in `firmware-v3/CLAUDE.md` |

**Important:** Use the upper-bound wording for ControlBusFrame size: "constrained to ≤5120 bytes". The static_assert is an upper bound, not equality. The actual sizeof may be smaller; do not claim an exact byte count.

## Render path

| # | Claim | Tier | Source |
|---|---|:---:|---|
| R1 | Target render rate: **120 FPS** | C | `firmware-v3/src/core/actors/RendererActor.h:112-113` (`static constexpr uint16_t TARGET_FPS = 120`) |
| R2 | Frame budget: **8333 µs ≈ 8.33 ms** per frame | C | `RendererActor.h:113` (`FRAME_TIME_US = 1000000 / TARGET_FPS`) |
| R3 | Per-frame effect-render budget: **2.0 ms** (architectural ceiling) | B | `firmware-v3/CLAUDE.md` hard constraint; effect-class comments documenting "no heap allocation in hot path" |
| R4 | Zero-heap render contract: no `new`/`malloc`/`String` in `render()` or in any function transitively called from it | C | Hard constraint enforced via static-buffer patterns across effect classes |
| R5 | Centre-origin rendering: all effects originate from / propagate to the seam between LEDs 79 and 80 | C | Hard constraint documented in `firmware-v3/CLAUDE.md`; `CENTER_POINT = 80` constant |

## LED output

| # | Claim | Tier | Source |
|---|---|:---:|---|
| L1 | LED output peripheral: **ESP32-S3 RMT4** (patched FastLED) | C | `RendererActor.cpp` ("patched FastLED RMT4: CPU returns quickly; wire time runs in parallel") |
| L2 | LED show timing | **V** | Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending. |
| L3 | LED count on K1v2: **320 LGP LEDs** (dual 160). No status strip. | C | Per-strip 160 + 2 strips |

**Important:** The LED-show timing line above is topology-neutral. Do NOT augment it with parenthetical claims about strip composition, status strips, or LED counts on the same line — those belong in dedicated rows or in the historical-note subsection. See `07_TIMING_BUDGETS.md` for the matching entry.

## K1v1 historical note (deactivated)

K1v1 was an earlier hardware revision. It supported a **30-LED status strip** as an alt-build (third strip, beyond the dual 160 LGP strips), bringing total physical LED count to 350 in that build. **The 30-LED status strip caused performance degradation due to the additional strip's RMT serialisation cost, and was deactivated.** K1v2 reverts to the dual-strip 320-LED topology only.

This is **historical context only** — do not document the 30-LED status strip as a current K1v2 feature. Any timing figure measured during the K1v1 + status-strip era (e.g. the 6.3 ms LED-show note from 2026-02-28) traces to that hardware combination and does not transfer to current K1v2.

## Hard constraints (enforced by code review and architecture)

| # | Constraint | Source |
|---|---|---|
| HC1 | All effects originate from the centre seam (LED 79/80) outward, or inward to that seam | `firmware-v3/CLAUDE.md` |
| HC2 | No rainbow cycling or full hue-wheel sweeps (UX choice) | `firmware-v3/CLAUDE.md` |
| HC3 | No heap allocation inside `render()` or transitively | `firmware-v3/CLAUDE.md` |
| HC4 | 120 FPS target; per-frame effect code under 2.0 ms | `firmware-v3/CLAUDE.md` |
| HC5 | British English in comments, docs, logs, and UI strings | `firmware-v3/CLAUDE.md` |

## Deferred — audio AGC partitioning audit

A firmware constant in `firmware-v3/src/audio/contracts/ControlBus.h` defines an audio-AGC partition count that does not match the Captain-defined hard rule, and is tracked as a separate firmware-fix item in `BACKLOG.md` § F-6. **Until that fix lands, the infographic series does not document audio-AGC partition counts at any level, and does not invoke any user-facing or API-level partition numbering.** Any panel that would otherwise reference audio partitioning must instead refer to the spectral analysis in aggregate (FFT + Goertzel + chroma + octave bands), without claiming a partition count or numbering.

This deferral is an active discipline, not an oversight. Do not work around it; do not document the bug as a feature; do not show partition-band AGC graphics in any panel; do not invoke specific partition identifiers anywhere in the infographic set.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Verified-facts file populated from clangd-grounded Explore-agent verification (18 files, 5 clangd queries, 12 greps) cross-confirmed with external consultant. Includes the four corrections from Rev 3 plan (Kalman→Goertzel, FFT bin count, RMT vs DMA, centre-seam wording), the K1v1 historical note (deactivated 30-LED status strip), and the deferred audio-AGC zone-count audit. |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 amendments. (Z) Expanded zone strike: removed all user-facing / API zone numbering (Zone 0/1/2/3/4) from the Deferred section, abstract, and X2 row — replaced with topology-neutral "audio-AGC partitioning" wording; narrow exception for EffectContext zone IDs is now closed. (L) LED wire-time exact wording: replaced L2 verdict with Captain's exact phrasing — "Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending." Removed contradictory parenthetical about 320 LEDs and status-strip contamination from the LED-show line itself; the K1v1 historical-note subsection retained as a dedicated context section. |
