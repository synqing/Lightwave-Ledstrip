---
abstract: "Six-panel breakdown for the K1v2 infographic series. Per panel: title (locked), style family, purpose, required content, whitelisted labels, forbidden items, continuity anchors (left/right), and layout-specific guidance for the NotebookLM Studio prompt. Encodes the Captain Gate 1 expanded zone strike (Amendment Z — ALL zone numbering: Zone 0, 1, 2, 3, 4), the K1v2 320-LED-only topology, and the source-documented LED show path wording (Amendment L: ~6.3 ms; K1v2 topology-specific measurement pending). Read alongside 03_STYLE_BIBLE.md and 04_LABEL_WHITELIST.md before any Studio generation."
---

# 06 — Part Outlines

This file specifies what each of the six panels must contain, what it must not, and how it joins the panels on either side. It is the controlling source for every NotebookLM Studio prompt in Phase 2 (calibration pair) and Phase 4 (full series).

Read this file with `03_STYLE_BIBLE.md` (style families, palette, layout skeleton) and `04_LABEL_WHITELIST.md` (allowed wording) open. Both are referenced inline rather than restated.

## Series-wide continuity rules (apply to every panel)

These are locked across all six panels per `03_STYLE_BIBLE.md` § 1.

1. **Title bar at the same y-position** across panels 1–6. Each title bar carries a numbered badge `[01]`–`[06]` (white-on-safety-orange).
2. **Cross-core / Snapshot rail at the same y-position** across panels 1–6. Even panels that do not "publish" the contract still carry a violet horizontal rail at that y-position, used as the continuity anchor.
3. **8–10% horizontal edge margin reserved for stitching** on every panel. No critical text inside the margin. Continuation stubs (in/out of frame) exit through these margins, never the centre.
4. **Bottom strip at the same y-position** across panels 1–6. Bottom strip carries timing / contract / safety annotations (its content varies per panel; its position does not).
5. **Palette by data class is fixed:** warm amber `#E89F3C` = audio capture, coral `#E8517A` = musical features, violet `#8B5CF6` = cross-core, cyan `#3DB5E8` = render, cool white `#F0F4FA` = hardware, safety orange `#FF7A29` = accent, muted yellow `#D4B842` = Verify badges.
6. **Any zone numbering is STRUCK from all six panels** per Captain Gate 1 Amendment Z. The strike covers Zone 0, Zone 4 (per the BACKLOG F-6 `CONTROLBUS_NUM_ZONES = 4` firmware-bug excess), AND the user/API-facing identifiers Zone 1 / Zone 2 / Zone 3. The previous narrow exception that allowed user-facing zone identifiers (1/2/3) inside the EffectContext card on Panel 5 is now CLOSED. Each panel below explicitly notes the exclusion under "Forbidden in this panel" as `any zone numbering`.
7. **K1v2 hardware = 320 LGP LEDs only.** No 30-LED status strip in any current-state panel. K1v1 history may appear only in Panels 1 and 6 with a `Historical (K1v1)` grey-accent badge, visually separated from current K1v2 facts.
8. **LED show timing on K1v2 uses Captain's exact wording in Panel 6 only** (Amendment L): `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` This phrasing is verbatim — do not abbreviate to "Verify (K1v2). Measurement pending.", do not assert a "4.8 ms invariant", and do not append parentheticals about strip topology to this line. The wording is topology-neutral. No other LED-show timing appears anywhere in the series.

---

## Panel 1 — System Overview

**Title (locked):** `[01] System Overview — K1 Lightwave audio-to-LED pipeline`

**Style family:** A — Neon Hardware Explainer (`03_STYLE_BIBLE.md` § 2)

**Purpose (one sentence):** Show the entire K1v2 signal chain from microphone to LEDs in a single hero diagram, with the ESP32-S3 as the central object and the five subsystems annotated outward.

**Required content:**
- Central hero: realistic ESP32-S3 module (cool-white `#F0F4FA`).
- Five subsystem callouts arranged radially around the SoC, each in its data-class colour:
  1. **MEMS microphone → I2S DMA capture** (warm amber).
  2. **Spectral analysis** (warm amber → coral transition): FFT + Goertzel + chroma + octave bands.
  3. **Cross-core publication** (violet): ControlBusFrame + lock-free SnapshotBuffer.
  4. **Visual rendering** (cyan): 120 FPS render loop, EffectContext, centre-origin LED mapping.
  5. **LED output** (cool white with cyan glow): RMT4-driven dual 160-LED LGP strips.
- Lower 1/3 block diagram strip showing the same chain linearly: `Mic → I2S → Spectral → Cross-core → Render → RMT → LEDs`.
- Continuity rail (violet) running horizontally across the panel at the locked y-position.
- Title bar `[01] System Overview` and bottom strip with the system-wide constants (`32 kHz`, `125 Hz`, `120 FPS`, `≤5120 bytes`, `320 LEDs`).

**Required labels (drawn ONLY from `04_LABEL_WHITELIST.md`):**
- `ESP32-S3 microcontroller`
- `MEMS microphone`
- `I2S DMA capture`
- `512-sample FFT → 256 bins` (compressed callout form acceptable here)
- `64-bin Goertzel bank`
- `12-class chroma vector`
- `8 octave bands`
- `ControlBusFrame`
- `Lock-free SnapshotBuffer (atomic seq + double buffer)`
- `120 FPS render loop`
- `RMT4 peripheral`
- `Dual 160-LED LGP strips` and `320 LGP LEDs (dual 160 strips)`
- `Cross-core boundary (Core 0 → Core 1)`

**Optional historical badge (Panel 1 only):**
- A small grey-accent badge in the bottom strip noting the K1v1 historical context, using the exact whitelisted phrasing: `K1v1 alt-build added a 30-LED status strip (350 LEDs total). Deactivated due to performance degradation. Historical only — not present on K1v2.` This is the only place in panels 1–5 where any K1v1 hardware reference is allowed, and it must be visually segregated from the K1v2 current-state diagram.

**Forbidden in this panel:**
- Any zone numbering of any kind (Zone 0, 1, 2, 3, 4) or audio-AGC follower / 4-band AGC graphic. Struck per Captain Gate 1 Amendment Z (broader than the original BACKLOG F-6 strike).
- Any LED show timing number (no measurement; lives in Panel 6 only with Captain's exact Amendment L wording: `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.`).
- The word "Kalman" anywhere. There is no Kalman filter.
- "DMA-backed FastLED" or "CPU-DMA". Use `RMT4 peripheral` / `RMT-peripheral-driven`.
- "512-bin FFT" or "512 FFT bins". Bin count is 256.
- "350 LEDs" as a current K1v2 fact. The 350-LED figure is K1v1 historical and must wear the `Historical (K1v1)` badge.

**Continuity anchors:**
- **Incoming-from-left:** none (panel 1 is the leftmost). Left-edge stub is decorative continuation only — circuit traces dissolving into the margin.
- **Outgoing-to-right:** the continuity rail (violet) exits the right margin at the locked y-position, carrying the implicit "continued in Panel 2" handoff. The amber audio-flow line also exits the right margin at the same y-position as Panel 2's audio-input.

**Layout-specific guidance for the NotebookLM Studio prompt:**
- Composition: SoC centred at ~50% width, ~55% height. Five subsystem callouts placed at roughly 11, 1, 4, 7 o'clock positions plus one anchored on the lower-third strip.
- Density cap: 8 major callouts maximum (`03_STYLE_BIBLE.md` § 5).
- Lower-third block diagram occupies ~25% of the panel height.
- Bottom strip is a thin horizontal band with system-wide constants laid out as small badges.
- Treat all microtext as "label-only space"; final exact wording will be overlaid manually downstream.

---

## Panel 2 — Audio Capture & DSP

**Title (locked):** `[02] Audio Capture & DSP — 32 kHz to spectral state`

**Style family:** E — Graphite System Reference Board (`03_STYLE_BIBLE.md` § 2)

**Purpose (one sentence):** Show the audio-input front end and the spectral-analysis stage in structured detail, from microphone capture to the populated magnitude / chroma / octave-band fields ready for cross-core publication.

**Required content:**
- Left section ("Capture"): MEMS microphone, I2S DMA capture, sample-rate / chunk / hop annotations.
- Centre section ("DSP"): three stacked blocks for the parallel spectral computations:
  1. **FFT block:** `512-sample FFT window yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz)`.
  2. **Goertzel bank:** `64-bin Goertzel bank`.
  3. **Pitch / band features:** `12-class chroma vector` and `8 octave bands`.
- Right section ("Output to ControlBus"): a small ControlBus card showing the relevant fields populated (`bins256[]`, `chroma[12]`, `bands[8]`).
- Bottom strip: the audio timing badges — `32 kHz audio sample rate`, `125 Hz audio frame rate`, `256-sample hop (= 8 ms at 32 kHz)`, `128-sample chunk (= 4 ms at 32 kHz)`.
- Continuity rail (violet) at locked y-position with a `Core 0` axis label.

**Required labels (drawn ONLY from `04_LABEL_WHITELIST.md`):**
- `MEMS microphone`, `I2S DMA capture`
- `32 kHz audio sample rate`, `125 Hz audio frame rate`
- `256-sample hop (= 8 ms at 32 kHz)`, `128-sample chunk (= 4 ms at 32 kHz)`
- `DC block`, `Pre-gain / normalisation`
- `512-sample FFT window yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz)` (full form preferred here)
- `64-bin Goertzel bank`
- `12-class chroma vector`
- `8 octave bands`
- `ControlBusFrame`

**Forbidden in this panel:**
- Any zone numbering of any kind (Zone 0, 1, 2, 3, 4), audio-AGC follower diagram, 4-band AGC graphic, or per-zone visualisation. Struck per Captain Gate 1 Amendment Z (broader than the original BACKLOG F-6 strike — covers user/API zones too). The panel discusses spectral analysis in aggregate and stops at the magnitudes / chroma / bands; it does not expose AGC partitioning at all.
- "512-bin FFT" or "512 FFT bins". Always show the 512-sample → 256-bin transform explicitly.
- "Kalman" anywhere (tempo/beat tracking is in Panel 3, not here).
- LED, RMT, or render-side material. Panel 2 stops at the boundary into ControlBus.

**Continuity anchors:**
- **Incoming-from-left:** the amber audio flow line and violet continuity rail enter from the left margin at the y-positions established by Panel 1.
- **Outgoing-to-right:** the populated ControlBus card hands off to Panel 3 (musical-feature engine) via a coral signal line at the rail y-position. The amber flow continues at its own y-position to indicate the FFT/spectral output feeding the feature engine.

**Layout-specific guidance for the NotebookLM Studio prompt:**
- Three-column dense reference-board layout: Capture (~25%), DSP (~50%), ControlBus output card (~25%).
- DSP column stacks the three spectral blocks vertically with structured dividers — this is where the Graphite Reference Board family earns its keep.
- Bottom-strip timing badges sit in a thin horizontal band at the locked y-position; tag each with `Confirmed` (timing classification per `04_LABEL_WHITELIST.md` § Numeric-classification).
- Density cap: 8 major callouts (combine related items into single grouped cards rather than splitting).
- The flow direction is left-to-right; do not introduce vertical or radial pathways here.

---

## Panel 3 — Musical Feature Engine

**Title (locked):** `[03] Musical Feature Engine — beat, tempo, chroma, percussion`

**Style family:** D — Dark Skeuomorphic Control Surface (`03_STYLE_BIBLE.md` § 2)

**Purpose (one sentence):** Present the higher-level musical features extracted from the spectral state — tempo, beat phase, onset, percussion triggers, chroma — as a tactile control-board where each "control" is firmware-internal state, not a user-facing knob.

**Required content:**
- Tempo card (coral): `Goertzel-based tempo tracker using novelty/energy history and smoothed winner selection`. Show the novelty envelope as a glowing coral curve with a `0.999 per-frame decay` annotation.
- Beat card (coral): beat-phase indicator and `Beat trigger` event marker.
- Onset card (coral): `Onset detection` with a small flux-style indicator.
- Percussion card (coral): `Percussion triggers`.
- Chroma card (coral): `12-class chroma vector` shown as twelve small uniform pads (no rainbow / no full hue-wheel).
- Saliency / aggregate stats card (coral, smaller): `Musical saliency`, `RMS`, `Spectral centroid` references.
- Annotation banner: a small bottom-edge banner reminding readers that these are *internal firmware state* — no end-user control surface (per `03_STYLE_BIBLE.md` Family D risk note).
- Continuity rail (violet) at locked y-position with a `Core 0 — feature engine` axis label.

**Required labels (drawn ONLY from `04_LABEL_WHITELIST.md`):**
- `Goertzel-based tempo tracker using novelty/energy history and smoothed winner selection`
- `Goertzel-based tempo tracker (novelty + smoothed winner)` (compressed alt)
- `Beat phase`, `Beat detection`, `Beat trigger`
- `Tempo estimate (BPM)`
- `Onset detection`
- `Percussion triggers`
- `12-class chroma vector`
- `Musical saliency`
- `ControlBusFrame`

**Forbidden in this panel:**
- "Kalman", "Kalman-filtered tempo", "Kalman tracker". There is no Kalman filter.
- Any zone numbering of any kind (Zone 0, 1, 2, 3, 4) or per-zone follower visualisation. Struck per Captain Gate 1 Amendment Z.
- Rainbow chroma display or full-hue-wheel pad. Hard constraint from `firmware-v3/CLAUDE.md`. The 12 chroma pads must use a single hue family with brightness modulation (coral against graphite is the canonical option).
- End-user control framing (no "user adjusts BPM" knob narrative). These are internal computed parameters.
- Render-side or LED-side material. Panel 3 stops before the ControlBus boundary.

**Continuity anchors:**
- **Incoming-from-left:** amber spectral feed and violet rail enter from the left margin at the y-positions established by Panel 2. The amber feed connects into the tempo / beat / chroma cards as the input source; the rail continues unbroken.
- **Outgoing-to-right:** all coral feature outputs converge into a single coral merge line at the violet rail y-position, which exits the right margin to feed Panel 4 (cross-core publication). The violet rail itself continues unbroken.

**Layout-specific guidance for the NotebookLM Studio prompt:**
- Skeuomorphic card layout, soft rounded edges, subtle drop shadows. Tactile feel is the family DNA — do not over-flatten.
- Cards arranged as a 3×2 or 2×3 grid in the upper 60% of the panel.
- Single-hue chroma pads ONLY (no rainbow). Reinforce this in the prompt: "twelve uniform coral pads with brightness modulation, no rainbow hues".
- Tempo card shows a curve, but no specific BPM number — just the curve shape and the decay coefficient as a small label.
- Bottom-edge "internal state, not user control" banner is small but visible.
- Density cap: 8 cards maximum.

---

## Panel 4 — Cross-Core Publication: ControlBus → SnapshotBuffer

**Title (locked):** `[04] Cross-Core Publication — ControlBus → SnapshotBuffer`

**Style family:** C — Industrial Runtime Styleguide (`03_STYLE_BIBLE.md` § 2)

**Purpose (one sentence):** Present the cross-core contract as a structured system inventory — the ControlBusFrame shape, the lock-free SnapshotBuffer mechanics, the publish/consume direction — rather than as a flow diagram.

**Required content:**
- Centre-of-panel hero card: **ControlBusFrame inventory**, showing the listed fields as a structured table:
  - `bins256[]` (256 FFT magnitude bins)
  - `chroma[12]` (12-class chroma vector)
  - `bands[8]` (8 octave bands)
  - `RMS`, `Beat phase`, `Onset`, `Tempo estimate`, `Percussion triggers`
  - Aggregate statistics row (spectral centroid, flux, novelty)
  - Size annotation: `ControlBusFrame constrained to ≤5120 bytes`
- Left card: **Publication mechanism** — `Lock-free double-buffered SnapshotBuffer with atomic sequence publication`, with a small diagram of the two buffers and the atomic active-index swap.
- Right card: **Cross-core boundary** — `Core 0 publisher → Core 1 consumer`, with axis labels and a dashed violet boundary line.
- Bottom strip: a small "guarantees" inventory — `Reader never blocks`, `Sequence-numbered publication`, `memory_order_release on publish`, `memory_order_acquire on read`.
- Continuity rail (violet) is at full saturation across this panel — this is the panel where the rail is the subject. Both adjacent panels carry the rail at the same y-position; this panel makes it the centrepiece.

**Required labels (drawn ONLY from `04_LABEL_WHITELIST.md`):**
- `ControlBusFrame`
- `ControlBusFrame constrained to ≤5120 bytes`
- `Lock-free double-buffered SnapshotBuffer with atomic sequence publication`
- `Lock-free SnapshotBuffer (atomic seq + double buffer)` (compressed alt)
- `Cross-core boundary (Core 0 → Core 1)`
- `Core 0 publisher → Core 1 consumer`
- `Beat phase`, `Onset detection`, `Percussion triggers`, `12-class chroma vector`, `8 octave bands`, `Tempo estimate (BPM)`

**Forbidden in this panel:**
- Any zone numbering of any kind (Zone 0, 1, 2, 3, 4), `m_zones[]` array visualisation, or any per-zone field inside the ControlBusFrame inventory. The frame DOES carry zone followers internally, but the inventory shown to readers omits zone partitioning entirely per Captain Gate 1 Amendment Z (broader than the original BACKLOG F-6 strike — covers user/API zones too). Show only the spectral-aggregate fields.
- Mutex / queue / FreeRTOS-primitive imagery. The bridge is lock-free; the visual must not contradict that.
- Exact byte equality (e.g. "5120 bytes"). Always show the upper bound `≤5120 bytes`.
- Mention of LED hardware, RMT, or rendering. Panel 4 is upstream of the renderer.

**Continuity anchors:**
- **Incoming-from-left:** coral merged feature line and amber spectral line both enter the ControlBusFrame card from the left margin at their established y-positions. They terminate inside the inventory card.
- **Outgoing-to-right:** a single cyan render-handoff line exits the right margin at the cross-core-boundary y-position, indicating the SnapshotBuffer consumer side feeding Panel 5. The violet rail continues unbroken.

**Layout-specific guidance for the NotebookLM Studio prompt:**
- Treat the panel as a system-reference sheet, NOT a left-to-right pipeline. The Industrial Styleguide family handles inventory layout.
- Centre card occupies ~50% of the panel area; left and right cards take ~25% each.
- Tabular numerals for the field-list and byte-bound. No decorative typography on numbers.
- The `Lock-free` label is a defining property — show it prominently in the publication-mechanism card with safety-orange highlighting.
- Density cap: 8 cards/groups maximum.
- Use the dashed-violet cross-core boundary line described in `03_STYLE_BIBLE.md` § Connector system. The dashed style is reserved for this boundary; do not use dashing elsewhere on this panel.

---

## Panel 5 — Visual Render Engine: 120 FPS Loop

**Title (locked):** `[05] Visual Render Engine — 120 FPS centre-origin render`

**Style family:** E — Graphite System Reference Board (variant) (`03_STYLE_BIBLE.md` § 2)

**Purpose (one sentence):** Show the Core 1 render loop with a centre-origin radial diagram of the LED strip plus structured surrounding context — frame budget, EffectContext composition, zero-heap contract, and the RMT4 output handoff — without quoting any LED-show timing.

**Required content:**
- Centre-of-panel hero: a centre-origin radial diagram of the dual 160-LED LGP strips. Render direction shown as outward expansion from the centre seam between LEDs 79 and 80 (`CENTER_POINT = 80`). Cyan glow on the propagating wavefront.
- Upper-left card: **Render loop** — `120 FPS render loop`, `8.33 ms frame budget`, `2.0 ms effect-render ceiling`.
- Upper-right card: **EffectContext composition** — `EffectContext` carrying the LED buffer, frame delta-time, render-context flags, and the audio snapshot reference. (Zone numbering of any kind — including user/API identifiers 1/2/3 — is STRUCK per Captain Gate 1 Amendment Z; the previous narrow exception is now CLOSED. Describe the API surface in topology-neutral terms.)
- Lower-left card: **Heap discipline** — `Zero-heap render contract`, `No heap allocation in render() or transitively`. Show this as a guarantee badge.
- Lower-right card: **LED output handoff** — `RMT-peripheral-driven (CPU returns immediately; wire time runs in parallel)`.
- Bottom strip: the centre-origin formal statement — `Symmetric outward expansion from the centre seam (LEDs 79/80)` — and the topology label `K1v2 topology: dual 160-LED LGP strips. No status strip on K1v2.`
- Continuity rail (violet) at locked y-position. This panel reads from the rail (consumer side); the rail's `Core 1` axis label is shown.

**Required labels (drawn ONLY from `04_LABEL_WHITELIST.md`):**
- `120 FPS render loop`
- `8.33 ms frame budget`
- `2.0 ms effect-render ceiling`
- `Zero-heap render contract`
- `No heap allocation in render() or transitively`
- `EffectContext`
- `Symmetric outward expansion from the centre seam (LEDs 79/80)`
- `Centre seam between LEDs 79 and 80, implemented as CENTER_POINT = 80`
- `RMT-peripheral-driven (CPU returns immediately; wire time runs in parallel)`
- `RMT4 peripheral`
- `Dual 160-LED LGP strips` and `320 LGP LEDs (dual 160 strips)`
- `K1v2 topology: dual 160-LED LGP strips. No status strip on K1v2.`

**Forbidden in this panel:**
- Any specific LED-show timing on this panel (no `6.3 ms`, no `~6 ms`, no `~7 ms`, no measured number). The K1v2 LED-show statement, if shown at all, lives in Panel 6 only with Captain's exact Amendment L wording: `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.`
- Any 30-LED status-strip rendering. K1v2 has 320 LGP LEDs only.
- Any zone numbering of any kind (Zone 0, 1, 2, 3, 4), zone-partition graphic on the LED strip, or zone-identifier text label inside the EffectContext card. Struck per Captain Gate 1 Amendment Z. The previous narrow exception that allowed `zone identifier: 1, 2, or 3` as a text label inside EffectContext is now CLOSED. EffectContext is described in topology-neutral terms (LED buffer, delta-time, render-context flags, audio snapshot).
- Rainbow / full-hue-wheel propagation. The wavefront is single-hue (cyan) with brightness modulation.
- "DMA-backed FastLED" or "CPU-DMA" anywhere. Use `RMT-peripheral-driven`.
- Centre described as "LED 80" alone or "LED 79" alone. Always use the seam wording.

**Continuity anchors:**
- **Incoming-from-left:** cyan render-handoff line enters from the left margin at the cross-core-boundary y-position established in Panel 4. Violet rail continues unbroken.
- **Outgoing-to-right:** cool-white LED-output line exits the right margin at the bottom-strip y-position, carrying the RMT4 handoff into Panel 6. Violet rail continues unbroken.

**Layout-specific guidance for the NotebookLM Studio prompt:**
- Centre-origin radial diagram is the visual focal point. The seam between LEDs 79 and 80 is at the diagram centre; outward expansion is bilateral and symmetric. Reinforce in the prompt: "centre seam at exact midpoint, symmetric outward propagation, no offset bias".
- Surrounding cards arranged as 2×2 around the radial centre.
- Cool-white LED illustration uses the hardware palette (`#F0F4FA`); render-loop / EffectContext cards use the cyan render palette (`#3DB5E8`).
- Density cap: 8 cards maximum (count the radial diagram as 1).
- The Graphite Reference Board variant here means more visual breathing room than Panel 2 — the radial diagram needs space.

---

## Panel 6 — Timing & Safety Budget

**Title (locked):** `[06] Timing & Safety Budget — frame ledger and invariants`

**Style family:** C — Industrial Runtime Styleguide (`03_STYLE_BIBLE.md` § 2)

**Purpose (one sentence):** Present the system's timing ledger and safety invariants as a single structured table panel — every number tagged with its confidence tier, every invariant stated as a guarantee, and the LED-show row carrying Captain's exact Amendment L wording (`Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.`) under the `Verify` tier.

**Required content:**
- Centre-of-panel hero: **Timing ledger table.** Columns: `Stage`, `Value`, `Tier`, `Notes`. Rows include:
  - Audio sample rate — `32 kHz` — `Confirmed` — capture rate.
  - Audio frame rate — `125 Hz` — `Confirmed` — `256-sample hop = 8 ms`.
  - Hop / chunk — `256 / 128 samples` — `Confirmed`.
  - FFT window / output — `512 samples → 256 bins (62.5 Hz spacing at 32 kHz)` — `Confirmed`.
  - Render rate — `120 FPS` — `Confirmed` — `8.33 ms frame budget`.
  - Effect-render ceiling — `2.0 ms` — `Budgeted` — architectural hard rule.
  - ControlBusFrame size — `≤ 5120 bytes` — `Confirmed` — `static_assert` upper bound.
  - LED show — `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` — `Verify` — Captain Gate 1 Amendment L: this exact wording is mandatory. Topology-neutral. No "Verify (K1v2). Measurement pending." placeholder. No "4.8 ms invariant." No parentheticals about strip topology on this row.
- Left card: **Safety invariants** (as guarantees):
  - `All effects originate from / propagate to the centre seam (LEDs 79/80)`
  - `No rainbow cycling or full hue-wheel sweeps`
  - `No heap allocation in render() or transitively`
  - `120 FPS target; per-frame effect under 2.0 ms`
- Right card: **Cross-core guarantees:**
  - `Lock-free double-buffered SnapshotBuffer with atomic sequence publication`
  - `Reader never blocks`
  - `Core 0 publisher → Core 1 consumer`
- Bottom strip: tier legend (`Confirmed` / `Derived` / `Budgeted` / `Verify`) per `04_LABEL_WHITELIST.md` § Numeric-classification.
- Continuity rail (violet) at locked y-position with a small `series end` marker on the right margin.

**Required labels (drawn ONLY from `04_LABEL_WHITELIST.md`):**
- All timing labels: `32 kHz audio sample rate`, `125 Hz audio frame rate`, `256-sample hop (= 8 ms at 32 kHz)`, `128-sample chunk (= 4 ms at 32 kHz)`, `512-sample FFT window yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz)`, `120 FPS render loop`, `8.33 ms frame budget`, `2.0 ms effect-render ceiling`, `ControlBusFrame constrained to ≤5120 bytes`.
- LED-output classification (Captain Gate 1 Amendment L — exact wording mandatory): `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.`
- Topology label: `K1v2 topology: dual 160-LED LGP strips. No status strip on K1v2.`
- Hard-constraint guarantees (verbatim from whitelist § Hard-constraint labels):
  - `All effects originate from / propagate to the centre seam (LEDs 79/80)`
  - `No rainbow cycling or full hue-wheel sweeps`
  - `No heap allocation in render() or transitively`
  - `120 FPS target; per-frame effect under 2.0 ms`
- Cross-core: `Lock-free double-buffered SnapshotBuffer with atomic sequence publication`, `Core 0 publisher → Core 1 consumer`.

**Optional historical badge (Panel 6 only, paired with Panel 1):**
- A small grey-accent footnote on the LED-show row noting: `Source-documented ~6.3 ms figure pre-dates the K1v2 topology and is sourced from the K1v1-era alt-build with the deactivated 30-LED status strip. K1v2 topology-specific re-measurement pending.` This footnote MUST sit visually adjacent to the LED-show row (Captain Gate 1 Amendment L wording: `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.`) and MUST wear the `Historical (K1v1)` grey-accent badge so it is unmistakably segregated from current K1v2 facts. The footnote is the ONLY place that may pair the ~6.3 ms figure with K1v1 strip-topology context; the LED-show row itself remains topology-neutral.

**Forbidden in this panel:**
- Any LED-show wording other than Captain's exact Amendment L phrasing on the LED-show row: `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` Specifically forbidden on this row: the placeholder `Verify (K1v2). Measurement pending.`, the phrase `4.8 ms invariant`, any parenthetical about 320 LEDs / status strip / strip topology, or any approximation that strips the "topology-specific measurement pending" qualifier. The K1v1 historical footnote (described above) is the ONLY place the ~6.3 ms figure may be paired with strip-topology context, and it must wear the `Historical (K1v1)` grey-accent badge.
- Any zone numbering of any kind in the timing ledger or anywhere else on the panel (Zone 0, 1, 2, 3, 4) — including audio-AGC zone count rows and any user/API-facing zone identifiers. Struck per Captain Gate 1 Amendment Z (broader than the original BACKLOG F-6 strike).
- "Kalman" anywhere.
- "DMA-backed", "CPU-DMA", or any LED-output description that implies CPU-DMA.
- "5120 bytes" without the `≤` upper-bound qualifier.
- Bar-chart-style "stages stacking up to the frame budget" graphics that imply LED-show is a known fraction of the 8.33 ms budget. We do not have that K1v2 measurement, so no bar chart that claims it.

**Continuity anchors:**
- **Incoming-from-left:** cool-white LED-output line and violet rail enter from the left margin at the y-positions established by Panel 5. Both terminate within the timing ledger and bottom strip respectively.
- **Outgoing-to-right:** none (panel 6 is the rightmost). Right-edge stub is a decorative `series end` marker on the violet rail at the locked y-position.

**Layout-specific guidance for the NotebookLM Studio prompt:**
- Tabular layout dominates. Tabular-numerals typography is mandatory (`03_STYLE_BIBLE.md` § Typography).
- Centre table occupies ~55% of the panel area; left invariants card and right cross-core card take ~22% each.
- Tier badges (`Confirmed` / `Derived` / `Budgeted` / `Verify`) use the whitelist's tag colour scheme — `Verify` is muted yellow `#D4B842` per `03_STYLE_BIBLE.md` palette; the others use neutral graphite badges with accent colour matching the data class (audio = warm amber, render = cyan, cross-core = violet).
- The K1v1 historical footnote uses grey accent (`Historical (K1v1)` badge per `04_LABEL_WHITELIST.md` § Reserved badges) and is small enough to read as a footnote, not a row.
- Density cap: the table itself counts as a single inventory card; with the two side cards and the tier-legend bottom strip, the panel sits at 4 major elements — well under the 8 cap, intentionally so. Timing tables read better with breathing space.
- No flow-direction arrows on this panel. The series flow ends here; the panel is a ledger, not a pipeline.

---

## Confidence gate (per panel) — label coverage check

For each panel above, every required label is drawn from `04_LABEL_WHITELIST.md`. No required label needs an entry that is missing from the whitelist. The `[01]`–`[06]` panel-number badges and the `Historical (K1v1)` accent badge are reserved in `04_LABEL_WHITELIST.md` § Reserved badges and are used as specified there. Note: the historical `Verify (K1v2)` badge entry in the whitelist is superseded for the LED-show row by Captain Gate 1 Amendment L's exact wording (`Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.`), which appears verbatim on Panel 6 under the `Verify` tier.

The only labels appearing in the panel outlines that are not literal whitelist entries are firmware-source identifiers (`CENTER_POINT = 80`, `bins256[]`, `chroma[12]`, `bands[8]`, `m_buf[2]`, `memory_order_release`, `memory_order_acquire`, `static_assert`). These are exact code-symbol identifiers and are admitted by the whitelist's exception clause: "exact firmware identifiers retain their source spelling" (`04_LABEL_WHITELIST.md` § British English).

---

## Cross-panel summary table

| Panel | Title | Family | Carries | Continuity-out (right) |
|:---:|---|:---:|---|---|
| 1 | System Overview | A — Neon Hardware Explainer | Full pipeline hero, K1v1 historical badge optional | Amber + violet rail |
| 2 | Audio Capture & DSP | E — Graphite Reference Board | Capture front end + spectral DSP | Coral + amber + violet rail |
| 3 | Musical Feature Engine | D — Dark Control Surface | Tempo/beat/onset/chroma cards | Coral + violet rail |
| 4 | Cross-Core Publication | C — Industrial Styleguide | ControlBusFrame inventory + SnapshotBuffer | Cyan render-handoff + violet rail |
| 5 | Visual Render Engine | E — Graphite Reference Board (variant) | Centre-origin radial + render-loop cards | Cool-white LED-output + violet rail |
| 6 | Timing & Safety Budget | C — Industrial Styleguide | Timing ledger + invariants table | (series end marker) |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Six-panel breakdown authored from Rev 3 plan, locked against 03_STYLE_BIBLE.md families, 04_LABEL_WHITELIST.md vocabulary, and 01_VERIFIED_FIRMWARE_FACTS.md confidence tiers. Encodes the audio-AGC zone strike (BACKLOG F-6) per panel, K1v2 320-LED-only topology, and `Verify (K1v2)` classification for LED show timing. Continuity anchors specified left/right per panel. K1v1 historical badge restricted to Panels 1 and 6 only, with grey-accent visual segregation from current K1v2 facts. |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 amendments applied. Amendment Z: zone-numbering strike expanded to ALL zone numbering (Zone 0, 1, 2, 3, 4) across every panel; previous narrow exception allowing user-facing zone identifiers (1/2/3) inside Panel 5's EffectContext card is now CLOSED. EffectContext composition rephrased to "render-context flags". Every panel's "Forbidden in this panel" section now cites Amendment Z. Amendment L: Panel 6 LED-show row uses Captain's exact wording verbatim — `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` — replacing the previous `Verify (K1v2). Measurement pending.` placeholder. Topology-neutral; no parentheticals. Series-wide rule 8 and Panel 6 historical footnote updated to match. |
