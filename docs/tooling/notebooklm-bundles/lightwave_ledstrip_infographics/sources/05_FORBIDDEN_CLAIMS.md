---
abstract: "Forbidden-claims register for the K1v2 infographic series. Enumerates eight banned technical claims (Kalman tempo, DMA-backed LED, exact ControlBusFrame byte equality, 512 FFT bins, bare 'LED 80' centre, 4-zone audio AGC, K1v2 LED timing without provenance, 350-LED / 30-LED status strip), seven structural bans for NotebookLM image output (microtext, wide tables, callout overload, edge-margin violations, invented identifiers, hidden uncertainty, marketing language), and the substitute wording each ban resolves to. Constrains both content and composition before any image generation."
---

# 05 — Forbidden Claims

This file enumerates what the K1v2 infographic series **must never** claim — both at the level of technical assertions (Section A) and at the level of NotebookLM's known image-output failure modes (Section B). Every ban resolves to a substitute already specified in `04_LABEL_WHITELIST.md` and `01_VERIFIED_FIRMWARE_FACTS.md`.

**Read order:** Skim Section A before reviewing any panel draft for accuracy. Skim Section B before reviewing any panel draft for composition.

**Severity scale (used in both sections):**

| Level | Meaning |
|---|---|
| **HARD-BAN** | Reject the panel. Regenerate or redraft. No exceptions. |
| **REJECT-ON-DETECT** | If a reviewer sees this in a generated panel, mark for rework before any external surface consumes it. |
| **WARN-AND-FIX** | Acceptable to fix in manual overlay rather than regenerate, provided the fix is verified before publication. |

---

## Section A — Forbidden technical claims

Eight locked bans from the approved Rev 3 plan, plus one structural firmware-source ban (A9) surfaced during authoring. Each entry: (a) banned phrasing, (b) why it is wrong (with citation), (c) substitute phrasing.

### A1 — "Kalman-filtered" tempo / beat / pitch tracking

**Severity:** HARD-BAN.

**Banned phrasings (any context):**
- `Kalman-filtered tempo tracker`
- `Kalman tracker`
- `Kalman beat tracker`
- `Kalman-smoothed pitch`
- Any label containing the word `Kalman`.

**Why it is wrong:** The K1v2 firmware contains no Kalman filter of any kind. The tempo subsystem is a Goertzel-bank novelty / energy estimator with smoothed winner selection. Source: `firmware-v3/src/audio/tempo/TempoTracker.cpp:1–17` ("Implementation of Goertzel-based tempo tracker"). Citing Kalman is a fabrication, not a simplification.

**Use instead:** `Goertzel-based tempo tracker using novelty/energy history and smoothed winner selection` (whitelisted in `04_LABEL_WHITELIST.md`). Short form: `Goertzel-based tempo tracker (novelty + smoothed winner)`.

### A2 — "DMA-backed" LED output

**Severity:** HARD-BAN.

**Banned phrasings:**
- `DMA-backed FastLED.show()`
- `DMA-backed LED output`
- `CPU DMA streams the framebuffer`
- `DMA peripheral pushes LED frames`

**Why it is wrong:** LED output runs on the **ESP32-S3 RMT4 peripheral** (Remote Transceiver Module), not on a CPU-DMA path. Functionally similar — CPU returns immediately after queuing the buffer; wire time runs in parallel — but the terminology matters because any reader cross-checking against ESP-IDF documentation will find no DMA-backed FastLED path on the S3. Source: `firmware-v3/src/core/actors/RendererActor.cpp` ("patched FastLED RMT4: CPU returns quickly; wire time runs in parallel").

**Use instead:** `RMT-peripheral-driven LED output (CPU returns immediately; wire time runs in parallel)`. Short form: `RMT4-driven (not CPU DMA)`.

### A3 — Exact byte-equality for ControlBusFrame

**Severity:** HARD-BAN.

**Banned phrasings:**
- `ControlBusFrame is 5120 bytes`
- `5120-byte ControlBusFrame`
- `sizeof(ControlBusFrame) == 5120`
- Any phrasing that asserts equality between the frame size and 5120.

**Why it is wrong:** The firmware enforces an **upper bound**, not equality. Source: `firmware-v3/src/audio/contracts/ControlBus.h:261–262` — `static_assert(sizeof(ControlBusFrame) <= 5120, ...)`. The actual sizeof at any given commit may be smaller than 5120 and is permitted to grow up to that ceiling. Asserting 5120 as the size is a precision claim the firmware does not back.

**Use instead:** `ControlBusFrame constrained to ≤5120 bytes` (whitelisted). When a panel needs visual emphasis on the budget, use the explicit "≤" symbol — never a bare `5120 bytes` label.

### A4 — "512 FFT bins" / "512-bin FFT"

**Severity:** HARD-BAN.

**Banned phrasings:**
- `512 FFT bins`
- `512-bin FFT`
- `512-point FFT yielding 512 bins`
- `FFT produces 512 magnitude bins`

**Why it is wrong:** The 512 figure is the **window size in samples**, not the bin count. Real-FFT halves the output: a 512-sample real input produces 256 magnitude bins (N/2). The ControlBus field that carries the magnitudes is named `bins256[]`, not `bins512[]`. Source: `firmware-v3/src/audio/pipeline/PipelineCore.h:40` (`windowSize = 512`, `kNumBins = kMaxWindow / 2`).

**Use instead:** `512-sample FFT window yielding 256 magnitude bins (62.5 Hz spacing at 32 kHz)`. Short form: `512-sample FFT → 256 bins`.

### A5 — Bare "LED 80" as the centre description

**Severity:** REJECT-ON-DETECT.

**Banned phrasings:**
- `Centre at LED 80`
- `Origin: LED 80`
- `All effects radiate from LED 80`
- `Centre LED is index 80`

**Why it is wrong:** The C symbol `CENTER_POINT = 80` is real (`firmware-v3/src/core/system/OtaLedFeedback.h:53`), but the *visual centre of a 160-LED strip* is the **seam between LEDs 79 and 80**, not LED 80 alone. Render fill logic walks left from `CENTER_POINT - 1` (LED 79) and right from `CENTER_POINT` (LED 80) symmetrically. A bare "LED 80" caption misrepresents what the user actually sees as the centre and breaks the symmetry illustration in any centre-origin diagram.

**Use instead:** `Centre seam between LEDs 79 and 80, implemented as CENTER_POINT = 80`. The substitute keeps the firmware constant visible while making the visual seam explicit.

### A6 — Zone numbering (any context, any panel)

**Severity:** HARD-BAN.

**Banned phrasings (Captain Gate 1: expanded to ALL zone numbering, not just audio AGC):**
- `Four audio AGC zones`
- `4-zone AGC partitioning`
- `Per-zone followers (4)`
- `Zone 0`, `Zone 4` (any framing)
- `Zone 1 / Zone 2 / Zone 3` (user-facing or API zone numbering, in any panel context)
- `User-facing zone IDs`
- `API zone IDs`
- Any infographic graphic that depicts numbered audio, render, or effect zones.
- Any reference to user-facing zone identifiers in `EffectContext` (the narrow exception that previously permitted this is now CLOSED).

**Why it is wrong:** Two independent reasons compound. (1) `firmware-v3/src/audio/contracts/ControlBus.h:22` currently defines `CONTROLBUS_NUM_ZONES = 4`, which **violates the Captain-defined hard rule** of three user-facing zones (1 / 2 / 3, max 3, no Zone 0, no Zone 4) and is tracked as a firmware bug in `BACKLOG.md` § F-6. (2) Captain Gate 1 has expanded the strike from "audio AGC zones only" to "ALL zone numbering anywhere in the infographic set" — the rationale being that zone numbering in any panel risks (a) cementing the F-6 bug into public surfaces, (b) confusing readers about Zone 1/2/3 vs Zone 0/3 indexing semantics, and (c) leaking a UX-layer abstraction into the firmware-architecture series, which is not what these panels document.

**Use instead:** Strike entirely. The infographic series does **not** mention zone numbering at any level — labels, layout, generated text, EffectContext callouts, or audio-AGC partitioning. If a panel would otherwise show zone partitioning, it must instead refer to the spectral analysis in aggregate: `512-sample FFT → 256 bins → 12-class chroma + 8 octave bands + 64-bin Goertzel + onset / tempo`. No zone count, no zone identifier, no zone-numbered EffectContext field anywhere.

**Reference:** `BACKLOG.md` § F-6 tracks the firmware-fix prerequisite for the underlying audio-AGC mismatch. Captain Gate 1 (2026-05-04) expanded the scope of A6 to all zone numbering. A6 stays HARD-BAN regardless of F-6 closure — the broader strike is independent of the firmware fix.

### A7 — K1v2 LED-driver timing without K1v2 measurement provenance

**Severity:** HARD-BAN.

**Banned phrasings:**
- `LED show: 6.3 ms`
- `~6.3 ms LED wire time`
- `Approximate LED frame: 6 ms`
- Any K1v2 LED-output timing number that lacks an explicit "measured on K1v2 on YYYY-MM-DD" provenance line.

**Why it is wrong:** The 6.3 ms LED-show figure (recorded 2026-02-28) was measured on **K1v1 hardware with the now-deactivated 30-LED status alt-build**. Two confounders make it untransferable to K1v2 as a *measured* fact: (a) different total LED count (350 vs 320), (b) different RMT serialisation pattern (3 strips vs 2). The figure cannot be cited as a K1v2 measured fact without a fresh K1v2 measurement.

**Use instead (Captain Gate 1 exact wording):** `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` This is the canonical replacement for the previous `Verify (K1v2). Measurement pending.` hedge — Captain has authorised citing the source-documented figure provided the wording remains topology-neutral (no inline parentheticals about "320 LEDs plus status strip" in the LED-show line). Do **not** describe the figure as a "4.8 ms invariant." The K1v1-era 6.3 ms figure may appear additionally as an explicitly labelled "Historical (K1v1)" callout. See `07_TIMING_BUDGETS.md` for the canonical entry and `01_VERIFIED_FIRMWARE_FACTS.md` § L2 for the tier classification.

### A8 — "350 LEDs" / "30-LED status strip" as a K1v2 fact

**Severity:** HARD-BAN.

**Banned phrasings (in any K1v2 context):**
- `350 LEDs`
- `350-LED total`
- `30-LED status strip`
- `Three-strip topology`
- `Tri-strip output`

**Why it is wrong:** K1v2 hardware has **dual 160-LED LGP strips, total 320 LEDs**, no status strip. The 350-LED / 30-LED-status configuration was a K1v1 alt-build. **It was deactivated due to performance degradation caused by the third strip's RMT serialisation cost.** Documenting it as current would misrepresent the shipping topology and bake a known-bad configuration into public surfaces.

**Use instead:** `Dual 160-LED LGP strips = 320 LEDs total. No status strip on K1v2.` The 30-LED status strip may appear **only** as historical context in a clearly labelled "Historical (K1v1)" panel callout, with the deactivation reason stated. It must never appear in a K1v2 hardware-topology diagram, BOM, or callout list.

### A9 — Invented firmware identifiers (struct, function, field names)

**Severity:** HARD-BAN. (Captain-confirmed at Gate 1, 2026-05-04. Previously flagged as authoring-note pending Captain confirmation; that question is now closed.)

**Hard-banned identifiers (the six explicit Captain Gate 1 entries — REJECT-ON-DETECT for any panel that contains these as code identifiers in labels, layout, or generated text):**

1. `bins512`
2. `KalmanTempo`
3. `DMABuffer`
4. `512 FFT bins`
5. `Kalman-filtered tempo`
6. `DMA-backed FastLED.show()`

**Banned (broader rule — the Captain Gate 1 list is illustrative, not exhaustive):**
- Any struct, class, function, namespace, or field name not listed in `01_VERIFIED_FIRMWARE_FACTS.md` or recoverable via clangd against the firmware tree.
- Other plausibly-named-but-wrong identifiers in the same failure class: e.g. `AGCZone[4]`, `centerLED`, `BeatKalmanFilter`, `pushFrameDMA()`, `processAudioFrame()`, `kalmanUpdate()`.

**StatusStrip / StatusStripTouch handling (Captain Gate 1 Amendment SS):** `StatusStrip` and `StatusStripTouch` are **REJECT-ON-DETECT** for infographic *labels* — i.e. they must not appear in any panel's required-labels, layout, or generated text. No Gate 1 panel is about status-strip hardware, so these identifiers have no surface in panel output. They MAY appear in the following three contexts only:

1. **Source evidence** — e.g. firmware-source citations in `01_VERIFIED_FIRMWARE_FACTS.md` that document the K1v1 historical alt-build.
2. **Forbidden-claims notes** — i.e. this file's own bans where `StatusStrip` is named as a banned identifier (the present paragraph is itself one such permitted occurrence).
3. **Excluded-topology caveats** — e.g. "K1v1 historical, not present on K1v2" callouts that explicitly disclaim status-strip hardware as out of scope.

Any other appearance of `StatusStrip` / `StatusStripTouch` is a REJECT-ON-DETECT failure: regenerate or redraft.

**Why it is wrong:** NotebookLM is a composition engine, not a code reader. Any identifier it produces that is not pinned to a verified-facts citation is fabrication. Readers will (correctly) cross-check any C-style identifier against the firmware tree, and a single wrong name breaks credibility for the whole series. The six explicit Captain Gate 1 identifiers are the highest-probability fabrications for the Part 4 cross-core panel and the Part 1 audio-pipeline panel; they are flagged individually so a reviewer can grep the panel for them rather than relying on judgement alone.

**Use instead:** Use only identifiers that appear in `01_VERIFIED_FIRMWARE_FACTS.md` or in the whitelisted-symbols list in `04_LABEL_WHITELIST.md`. When in doubt, use the descriptive label rather than a code identifier (e.g. `Cross-core publisher` rather than a made-up class name; `RMT-peripheral-driven LED output` rather than `DMA-backed FastLED.show()`).

---

## Section B — Structural bans (NotebookLM image-output failure modes)

NotebookLM Studio's infographic generator has documented failure modes that survive any amount of prompt engineering. These bans tell the reviewer what to reject at the **composition** level, independent of factual content.

### B1 — Microtext

**Severity:** REJECT-ON-DETECT.

**Ban:** No single text label may exceed **100 characters**. Long labels rasterise into illegible microtext at panel-resolution viewing.

**Why:** NotebookLM-generated panels are typically rendered at ≤2048 px on the long edge. At that resolution, anything over ~100 characters in a label compresses into pixel-noise. Readers cannot copy or verify what the label actually says, which defeats the infographic's purpose.

**Use instead:** Break long claims into a stacked label group, a short callout + footnote pair, or a numbered bullet list. The whitelisted phrases in `04_LABEL_WHITELIST.md` are pre-sized for this constraint.

### B2 — Wide tables

**Severity:** REJECT-ON-DETECT.

**Ban:** No table may exceed **5 columns** in a single panel.

**Why:** Beyond five columns, NotebookLM compresses cell text to fit, producing the same microtext failure as B1 — but harder to spot because the table looks "right" at a glance. Six- or seven-column tables also force the panel into landscape-only orientation, breaking the layout grammar from `03_STYLE_BIBLE.md`.

**Use instead:** Split into two sub-tables stacked vertically, or trim columns by merging confidence-tier and source-citation into a single tier-coded cell.

### B3 — Callout overload

**Severity:** WARN-AND-FIX.

**Ban:** No more than **8 major callouts** per panel.

**Why:** Above eight callouts, the connector lines saturate the available negative space and the panel reads as visual noise rather than as a hierarchy. The five style families in `03_STYLE_BIBLE.md` are calibrated for ≤8 callouts.

**Use instead:** Group related callouts into a single composite callout with a sub-list, or move secondary callouts into a footer strip.

### B4 — Edge-margin / panorama-stitching violations

**Severity:** HARD-BAN.

**Ban:** No critical text or critical line-art may sit within **8% of the horizontal edge margin** on either side of a panel.

**Why:** Several Phase 5 composition options stitch panels into a horizontal panorama. Anything within the safe-zone margin is at risk of being clipped, partially overlapped by the next panel's edge artwork, or hidden beneath the connector rail. This is the single most common reason a stitched poster has to be regenerated.

**Use instead:** Place all critical text and connectors within the central 84% of the panel's horizontal extent. Decorative bleed (gradients, glow falloff, rail extensions) is acceptable in the margin; load-bearing labels are not.

### B5 — Invented technical content (function names, struct sizes, coefficients, timings)

**Severity:** HARD-BAN.

**Ban:** No invented function names, struct sizes, coefficients, frequency cutoffs, or timing measurements anywhere in any panel.

**Why:** Same root cause as A9 (Section A) — NotebookLM is a composition engine, not a code reader. The firmware-truth substitution rule is: every numeric value, every code identifier, and every algorithm name must trace to a citation in `01_VERIFIED_FIRMWARE_FACTS.md`. Anything else is fabrication.

**Use instead:** Use only the values, names, and coefficients listed in the verified-facts file. When a panel composition demands a value that is not yet measured, use the `Verify (K1v2)` badge — never a placeholder number.

### B6 — Hidden uncertainty ("approximately" / "~" / "around")

**Severity:** REJECT-ON-DETECT.

**Ban:** No `approximately`, `~`, `around`, `roughly`, or `circa` prefixes that hide that a value is unmeasured.

**Why:** The four-tier confidence system (Confirmed / Derived / Budgeted / Verify — see `01_VERIFIED_FIRMWARE_FACTS.md`) exists precisely to avoid this. A `~6.3 ms` label looks calibrated but is actually unverified for K1v2; readers cannot tell. The right answer is to surface the uncertainty using the Verify tier badge, not to bury it behind a tilde.

**Use instead:** Confirmed values get the bare number. Derived values get a "(D)" tier badge or are otherwise typeset to show their derivation. Budgeted values are labelled `2.0 ms ceiling` or `120 FPS target`. Unmeasured values use `Verify (K1v2). Measurement pending.`. Pick a tier; do not hedge with `~`.

### B7 — Marketing language

**Severity:** REJECT-ON-DETECT.

**Banned (representative; the rule is the spirit, not just these tokens):**
- `revolutionary`
- `blazing-fast`
- `unprecedented`
- `next-generation` (as a self-applied descriptor)
- `groundbreaking`
- `world-class`
- `state-of-the-art` (without a citation to the state-of-the-art being beaten)
- `lightning-fast`, `super-fast`, `ultra-low-latency`

**Why:** The infographic audience (firmware engineers, technical leadership, product reviewers per `00_PROJECT_OVERVIEW.md`) reads marketing adjectives as a credibility signal of low calibration. The series ships engineering specifics; superlatives are noise that lowers signal density. The K1 marketing positioning file (`MEMORY.md` → `project_marketing_positioning.md`) also bans this language at the brand level.

**Use instead:** Cite the specific number. `120 FPS render loop`, `8 ms audio frame`, `lock-free SnapshotBuffer`, `RMT-peripheral-driven LED output`. Engineering details speak for themselves.

---

## Cross-reference index

| Ban | Substitute lives in | Source citation |
|---|---|---|
| A1 (Kalman) | `04_LABEL_WHITELIST.md` § Tempo / beat labels | `TempoTracker.cpp:1–17` |
| A2 (DMA-backed) | `04_LABEL_WHITELIST.md` § Output / final-frame labels | `RendererActor.cpp` (RMT4 patch comment) |
| A3 (5120 byte equality) | `04_LABEL_WHITELIST.md` § Cross-core publication labels | `ControlBus.h:261–262` |
| A4 (512 FFT bins) | `04_LABEL_WHITELIST.md` § Spectral analysis labels | `PipelineCore.h:40` |
| A5 (bare LED 80) | `04_LABEL_WHITELIST.md` § Hardware labels | `OtaLedFeedback.h:53` + Captain hard rule |
| A6 (zone numbering — any context) | **Strike entirely** (Captain Gate 1 expanded scope; independent of BACKLOG F-6) | `ControlBus.h:22` (firmware bug) + Captain Gate 1 doctrine |
| A7 (LED timing without K1v2 provenance) | `07_TIMING_BUDGETS.md` Verify entry | `01_VERIFIED_FIRMWARE_FACTS.md` § L2 |
| A8 (350 LEDs / 30-LED status) | `04_LABEL_WHITELIST.md` § K1v1 historical note | `01_VERIFIED_FIRMWARE_FACTS.md` § K1v1 historical note |
| A9 (invented identifiers) | `01_VERIFIED_FIRMWARE_FACTS.md` only | clangd against firmware tree |
| B1–B7 (structural) | `03_STYLE_BIBLE.md` layout grammar + this file | NotebookLM Studio observed failure modes |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:general-purpose | Created. Forbidden-claims register authored from the locked eight-ban list in Rev 3 plan (`quirky-tinkering-falcon.md`), cross-referenced against the verified-facts file and the label whitelist. Added A9 (invented firmware identifiers) during authoring as a structural firmware-source ban not on the locked list — flagged for Captain confirmation. Section B captures NotebookLM Studio image-output failure modes (microtext, wide tables, callout overload, edge-margin, invented values, hidden uncertainty, marketing language) with severity tiers (HARD-BAN / REJECT-ON-DETECT / WARN-AND-FIX). |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 amendments. **Amendment A9:** A9 promoted from authoring-note to Captain-confirmed HARD-BAN; six explicit identifiers enumerated (`bins512`, `KalmanTempo`, `DMABuffer`, `512 FFT bins`, `Kalman-filtered tempo`, `DMA-backed FastLED.show()`) for grep-based reviewer detection; authoring-note paragraph removed. **Amendment SS:** Added `StatusStrip` / `StatusStripTouch` REJECT-ON-DETECT rule for panel labels with three permitted contexts — source evidence, forbidden-claims notes, and excluded-topology caveats — and zero permission elsewhere. **Amendment Z:** A6 expanded from "audio AGC zones / 4-zone references" to "Zone numbering (any context, any panel)"; ban now covers Zone 0, Zone 4, and user/API zone numbering 1/2/3 in any panel surface; the previous narrow exception for user-facing zone identifiers in `EffectContext` is now CLOSED; ban is independent of BACKLOG F-6 closure. **Amendment L:** A7 substitute wording updated from `Verify (K1v2). Measurement pending.` to Captain's exact replacement `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` (topology-neutral, no inline parentheticals about 320 LEDs plus status strip; explicitly not a "4.8 ms invariant"). Cross-reference index updated for A6 scope change. |
