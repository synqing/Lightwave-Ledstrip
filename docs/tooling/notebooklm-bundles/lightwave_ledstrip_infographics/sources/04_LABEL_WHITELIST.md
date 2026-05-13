---
abstract: "Label whitelist for the K1v2 infographic series. Defines the exact British-English terms NotebookLM is allowed to use across all six panels. Rejection of any unlisted label (e.g. 'colour' yes, 'color' no; 'centre' yes, 'center' no for body labels). Includes the public-safe phrasing table that resolves every Captain-flagged ambiguity from Rev 3 plan."
---

# 04 — Label Whitelist

NotebookLM has a tendency to invent labels, mix dialects, and produce plausible-but-wrong technical terms. This file constrains its label vocabulary.

**Rule:** A panel may use labels from this file. A panel may use exact firmware constant names that appear in `01_VERIFIED_FIRMWARE_FACTS.md`. Anything else requires explicit Captain authorisation.

## British English (mandatory)

The K1v2 firmware uses British English in all comments, docs, logs, and UI strings (`firmware-v3/CLAUDE.md` hard constraint). Infographic labels follow the same rule.

| Use | NOT |
|---|---|
| centre, centred, centring | center, centered, centering |
| colour, coloured | color, colored |
| behaviour | behavior |
| initialise, initialiser | initialize, initializer |
| serialise | serialize |
| analogue | analog |
| organise, organised | organize, organized |
| optimisation | optimization |
| visualise | visualize |
| recognised | recognized |
| catalogue | catalog |

**Exception:** Exact firmware identifiers retain their source spelling. `CENTER_POINT = 80` in code stays `CENTER_POINT` in the panel — that is a *symbol*, not a *body word*. Body labels around the symbol use British English.

## Hardware labels

| Label | Use this exact phrasing |
|---|---|
| ESP32-S3 microcontroller | `ESP32-S3` (acceptable shortform), or `ESP32-S3 microcontroller` for first appearance |
| Microphone input | `MEMS microphone` or `MEMS mic` |
| I2S capture | `I2S DMA capture` |
| LED strip count | `Dual 160-LED LGP strips` (320 LEDs total) |
| Centre origin (visual seam) | `Centre seam between LEDs 79 and 80, implemented as CENTER_POINT = 80` |
| LED output peripheral | `RMT4 peripheral` or `ESP32-S3 RMT4 peripheral` |
| LED output behaviour | `RMT-peripheral-driven (CPU returns immediately; wire time runs in parallel)` |
| Strip topology (do not mix with K1v1) | `K1v2 topology: dual 160-LED LGP strips. No status strip on K1v2.` |

## Audio capture labels

| Label | Use this exact phrasing |
|---|---|
| Sample rate | `32 kHz audio sample rate` or `32 kHz` |
| Frame rate | `125 Hz audio frame rate` or `125 Hz` |
| Hop / window | `256-sample hop (= 8 ms at 32 kHz)` |
| Chunk size | `128-sample chunk (= 4 ms at 32 kHz)` |
| DC handling | `DC block` |
| Pre-processing | `Pre-gain / normalisation` |

## Spectral analysis labels

| Label | Use this exact phrasing |
|---|---|
| FFT | `512-sample FFT window yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz)` |
| FFT (short form) | `512-sample FFT → 256 bins` (acceptable in compressed callouts) |
| Goertzel | `64-bin Goertzel bank` |
| Chroma | `12-class chroma vector` |
| Octave bands | `8 octave bands` |
| Onset | `Onset detection` |
| Saliency | `Musical saliency` |

**FORBIDDEN:** `512-bin FFT`, `512 FFT bins`, `512-point FFT yielding 512 bins`. The 512 is the *window*; the bin count is *256*.

## Tempo / beat labels

| Label | Use this exact phrasing |
|---|---|
| Tempo tracker | `Goertzel-based tempo tracker using novelty/energy history and smoothed winner selection` |
| Tempo tracker (short form) | `Goertzel-based tempo tracker (novelty + smoothed winner)` |
| Beat | `Beat phase`, `Beat detection`, `Beat trigger` |
| Tempo estimate | `Tempo estimate (BPM)` |
| Percussion | `Percussion triggers` |

**FORBIDDEN:** `Kalman-filtered tempo`, `Kalman tracker`, anything containing the word `Kalman`. There is no Kalman filter in the firmware.

## Cross-core publication labels

| Label | Use this exact phrasing |
|---|---|
| ControlBus contract | `ControlBusFrame` |
| ControlBus size | `ControlBusFrame constrained to ≤5120 bytes` |
| Snapshot bridge | `Lock-free double-buffered SnapshotBuffer with atomic sequence publication` |
| Snapshot bridge (short form) | `Lock-free SnapshotBuffer (atomic seq + double buffer)` |
| Cross-core boundary | `Cross-core boundary (Core 0 → Core 1)` |
| Publication direction | `Core 0 publisher → Core 1 consumer` |

## Render path labels

| Label | Use this exact phrasing |
|---|---|
| Render rate | `120 FPS render loop` |
| Frame budget | `8.33 ms frame budget` |
| Effect render budget | `2.0 ms effect-render ceiling` |
| Heap discipline | `Zero-heap render contract` |
| EffectContext | `EffectContext` (exact code identifier) |
| Centre-origin behaviour | `Symmetric outward expansion from the centre seam (LEDs 79/80)` |

## Output / final-frame labels

| Label | Use this exact phrasing |
|---|---|
| Framebuffer push | `RMT-peripheral-driven LED output` |
| LED count | `320 LGP LEDs (dual 160 strips)` |
| LED wire time (K1v2) | `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` |

## Hard-constraint labels (when shown as guarantees)

| Label | Use this exact phrasing |
|---|---|
| Centre origin | `All effects originate from / propagate to the centre seam (LEDs 79/80)` |
| No rainbow | `No rainbow cycling or full hue-wheel sweeps` |
| No heap | `No heap allocation in render() or transitively` |
| 120 FPS / 2.0 ms | `120 FPS target; per-frame effect under 2.0 ms` |

## Public-safe phrasing table (resolves every Rev 3 ambiguity)

| Topic | Exact public-safe wording |
|---|---|
| FFT | `512-sample FFT window yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz)` |
| Tempo | `Goertzel-based tempo tracker using novelty/energy history and smoothed winner selection` |
| Centre origin | `Centre seam between LEDs 79 and 80, implemented as CENTER_POINT = 80` |
| Snapshot bridge | `Lock-free double-buffered SnapshotBuffer with atomic sequence publication` |
| ControlBus size | `ControlBusFrame constrained to ≤5120 bytes` |
| LED output peripheral | `RMT-peripheral-driven (not CPU DMA)` |
| LED wire time (K1v2) | `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` |
| LED topology (K1v2) | `Dual 160-LED LGP strips = 320 LEDs total. No status strip on K1v2.` |
| K1v1 historical note | `K1v1 alt-build added a 30-LED status strip (350 LEDs total). Deactivated due to performance degradation. Historical only — not present on K1v2.` |
| Zone numbering (any context) | **STRIKE entirely.** Do not mention Zone 0, Zone 4, or any user/API zone numbering (1/2/3) in any panel — labels, layout, or generated text. The narrow exception that previously allowed user-facing zone identifiers in `EffectContext` is now CLOSED. Zone numbering may appear only in source-evidence citations (e.g. `01_VERIFIED_FIRMWARE_FACTS.md`), forbidden-claims notes (`05_FORBIDDEN_CLAIMS.md`), or excluded-topology caveats — never in panel output. |

## Numeric-classification labels (for timing tables)

When a timing value appears in a table, tag it with one of:

| Tag | Meaning |
|---|---|
| `Confirmed` | Direct source citation. |
| `Derived` | Computed from a Confirmed value. |
| `Budgeted` | Architectural target, not measured. |
| `Verify` | Stated but not yet validated for K1v2 hardware. |

Always classify. Do not show a timing number without its tag.

## Reserved badges

| Badge | When to apply |
|---|---|
| `[01]`–`[06]` (panel-number badges, white-on-orange) | Title bar of each panel |
| `Verify (K1v2)` (yellow accent) | Any timing value not yet measured on K1v2 |
| `Historical (K1v1)` (grey accent) | Any value from K1v1 that does not transfer |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Label whitelist authored from Rev 3 plan public-safe phrasing table; British-English mandate enforced; Kalman-tempo and 512-bin-FFT explicitly banned; audio AGC zones struck pending BACKLOG F-6. |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 amendments. Amendment L: replaced "Verify (K1v2). Measurement pending." LED wire-time wording (two locations: Output / final-frame labels table and Public-safe phrasing table) with Captain's exact phrasing — "Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending." (topology-neutral; no contradictory parentheticals). Amendment Z: expanded zone strike to ALL zone numbering (Zone 0, Zone 4, user/API zone numbering 1/2/3). Replaced "Audio AGC zones" entry with broader "Zone numbering (any context)" entry; the previous narrow exception for user-facing zone identifiers in `EffectContext` is now CLOSED. No zone numbering anywhere in panel output. |
