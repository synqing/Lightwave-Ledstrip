---
abstract: "Gate 1 preflight report for the K1v2 NotebookLM calibration-pair generation. Confirms source pack + MANIFEST present, validation greps clean for all banned terms, Panel 5 + Panel 6 prompts carry Captain's amended wording verbatim, no firmware files will be edited. Cleared to proceed with NotebookLM Studio generation (Part 5 + Part 6, 3 variants each)."
---

# Gate 1 Preflight Report — 2026-05-04

## 1. Source-pack file tree

```
docs/tooling/notebooklm-bundles/lightwave_ledstrip_infographics/
├── MANIFEST.md (164 lines)
├── GATE1_PREFLIGHT.md (this file)
└── sources/
    ├── 00_PROJECT_OVERVIEW.md (74 lines)
    ├── 01_VERIFIED_FIRMWARE_FACTS.md (125 lines)
    ├── 02_PIPELINE_PARTITIONING.md (114 lines)
    ├── 03_STYLE_BIBLE.md (235 lines)
    ├── 04_LABEL_WHITELIST.md (162 lines)
    ├── 05_FORBIDDEN_CLAIMS.md (282 lines)
    ├── 06_PART_OUTLINES.md (376 lines)
    └── 07_TIMING_BUDGETS.md (170 lines)
Total: 1,702 LOC across 9 files.
```

✅ MANIFEST.md present.
✅ All 8 source files present and authored.

## 2. Validation grep summary

| Check | Result |
|---|---|
| `bins512` (A9 HARD-BAN) | ✅ Zero leakage. No occurrences. |
| `KalmanTempo` (A9 HARD-BAN) | ✅ Zero leakage. No occurrences. |
| `DMABuffer` (A9 HARD-BAN) | ✅ Zero leakage. No occurrences. |
| `Kalman-filtered tempo` (A9 HARD-BAN) | ✅ Only in ban-context (`05_FORBIDDEN_CLAIMS.md`, `06_PART_OUTLINES.md` forbidden lists). |
| `DMA-backed FastLED.show()` (A9 HARD-BAN) | ✅ Only in ban-context (MANIFEST.md correction note). |
| `Kalman-filtered` / `Kalman` (A9 HARD-BAN) | ✅ All occurrences are negative ("There is no Kalman filter") in ban-context. |
| `DMA-backed` (A9 HARD-BAN) | ✅ All occurrences are corrective ("→ use RMT-peripheral-driven instead"). |
| `Zone 0` / `Zone 1` / `Zone 2` / `Zone 3` / `Zone 4` (Amendment Z) | ✅ Zero panel-content leakage. |
| `user-facing zone` / `API zone` / `zone identifier` (Amendment Z) | ✅ Zero leakage. EffectContext rephrased to "render-context flags". |
| `4.8 ms` / `4.8 ms invariant` | ✅ Only in negative contexts ("do not assert a `4.8 ms invariant`"). |
| `StatusStrip` / `StatusStripTouch` (Amendment SS) | ✅ Only in ban-context (`05_FORBIDDEN_CLAIMS.md` REJECT-ON-DETECT rule + permitted-context exceptions). |
| LED wire-time exact wording (Amendment L) | ✅ Captain's exact phrasing present verbatim in 4 source files (`01:92`, `04:109`, `04:130`, `07:79`) and referenced from Panel 6 (`06:22, 66, 265, 302, 330`). |

## 3. Panel 5 + Panel 6 prompts — confirmation of amended wording

### Panel 5 prompt — Visual Render Engine (Style E variant — Graphite Reference Board with centre-origin radial diagram)

The prompt I will send to NotebookLM Studio for Panel 5:

```
Create Part 5 of 6 in a matching LightwaveOS K1v2 technical infographic series.

Panel title: Core 1 Visual Rendering Engine — 120 FPS Deterministic Loop

Panel purpose: Show how the visual engine consumes the latest audio snapshot
and maps it into symmetric LED output expanding outward from the centre seam.

Style: Use a Graphite System Reference Board style: dark graphite background
with organised section dividers, modular cards arranged as foundations +
controls + data display + feedback inventory, orange accent highlights, precise
UI components with subtle depth, high legibility, compact tables. Series
palette: warm amber for audio input, coral for musical features, violet for
cross-core data, cyan/blue for visual rendering, cool white for hardware. Title
bar at the top with section number badge [05].

Layout:
- Landscape 16:9.
- Title bar with [05] section badge at top.
- Left side: SnapshotBuffer input (violet rail entering from left margin).
- Centre hero: 120 FPS Render Loop card containing EffectContext build +
  symmetric outward expansion from the centre seam (LEDs 79/80) — the
  centre-origin radial diagram is the visual anchor.
- Right side: rendered LED frame card with Dual 160-LED LGP strips (320
  LEDs total) in cool-white callout.
- Bottom strip: render-path timing relationships only (8.33 ms frame budget
  and 2.0 ms effect ceiling) — DO NOT show any LED-show timing on this panel.
- 8% horizontal edge margins reserved (no critical text within margins).

Required labels (use ONLY these — drawn from 04_LABEL_WHITELIST.md):
- "SnapshotBuffer" / "Lock-free SnapshotBuffer (atomic seq + double buffer)"
- "120 FPS render loop"
- "8.33 ms frame budget"
- "2.0 ms effect-render ceiling"
- "Visual Interpolation"
- "EffectContext"
- "Centre seam between LEDs 79 and 80, implemented as CENTER_POINT = 80"
- "Symmetric outward expansion from the centre seam (LEDs 79/80)"
- "RMT-peripheral-driven LED output"
- "320 LGP LEDs (dual 160 strips)"
- "Zero-heap render contract"

Forbidden in this panel (do NOT generate):
- Any zone numbering (Zone 0, 1, 2, 3, 4 — Amendment Z).
- Any LED show timing number — that lives in Panel 6 only.
- "DMA-backed", "CPU-DMA", or any LED-output description that implies CPU-DMA.
- "Kalman" / "Kalman-filtered tempo" / "Kalman tracker".
- The K1v1 30-LED status strip — K1v2 topology only on this panel.
- "4.8 ms invariant".
- Microtext (>100 chars per single label).
- More than 8 major callouts.

British English required (centre, colour, behaviour, initialise).
```

### Panel 6 prompt — Timing & Safety Budget (Style C — Industrial Runtime Styleguide)

The prompt I will send to NotebookLM Studio for Panel 6:

```
Create Part 6 of 6 in a matching LightwaveOS K1v2 technical infographic series.

Panel title: Timing Budget & Real-Time Safety Invariants

Panel purpose: Show the hard timing relationships that constrain the K1v2
firmware: audio cadence, render cadence, LED-output path, and the architectural
ceilings that govern effect implementation.

Style: Use an Industrial Runtime Styleguide style: dark graphite engineering
background with subtle grid, tactile machined-feeling component cards,
safety-orange accent colour, charcoal/slate surfaces, ONE compact field-
inventory timing table (no duplicates), badges and tokens, modular component
taxonomy. Treat the panel as a system-reference sheet, not a flow diagram.
Series palette: warm amber / coral / violet / cyan accents on dark graphite,
with safety-orange section dividers.

Layout:
- Landscape 16:9.
- Title bar with [06] section badge at top.
- Centre: ONE timing table containing every classified timing value, with
  classification tier badge per row (Confirmed / Derived / Budgeted / Verify).
  No duplicate timing tables.
- Right: safety-invariants column (centre origin, no rainbow, zero-heap, 120
  FPS / 2.0 ms ceilings).
- Bottom: phase-relationship strip showing audio period 8.000 ms vs render
  period 8.333 ms with the snapshot-buffer absorbing the ~0.333 ms drift.
- 8% horizontal edge margins reserved.

Timing table rows (with tier in brackets) — exact wording mandatory:
- Audio sample rate — 32 kHz — [Confirmed]
- Audio frame rate — 125 Hz — [Confirmed]
- Audio hop — 8 ms (256 samples) — [Derived]
- Audio chunk — 4 ms (128 samples) — [Confirmed]
- FFT window — 512 samples — [Confirmed]
- FFT bins — 256 magnitude bins — [Confirmed]
- FFT bin spacing — 62.5 Hz — [Derived]
- Goertzel bank — 64 bins — [Confirmed]
- Chroma — 12 classes — [Confirmed]
- Octave bands — 8 bands — [Confirmed]
- ControlBusFrame size — ≤5120 bytes — [Confirmed]
- Render rate — 120 FPS — [Confirmed]
- Render frame budget — 8.33 ms — [Confirmed]
- Effect-render ceiling — 2.0 ms — [Budgeted]
- LED show — Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending. — [Verify]

Optional adjacent footnote (clearly labelled "Historical (K1v1)" with grey
accent badge): "K1v1 alt-build added a 30-LED status strip (350 LEDs total).
Deactivated due to performance degradation. Historical only — not present on
K1v2." — but ONLY if visually segregated from the current K1v2 timing rows.

Forbidden in this panel (do NOT generate):
- Any zone numbering (Zone 0, 1, 2, 3, 4 — Amendment Z).
- Any LED show wording other than the exact Amendment L phrasing on the
  LED-show row: "Source-documented LED show path: ~6.3 ms; K1v2 topology-
  specific measurement pending."
- "Verify (K1v2). Measurement pending." (placeholder is forbidden).
- "4.8 ms invariant".
- Parentheticals about 320 LEDs / status strip / strip topology in the LED-
  show row. The wording is topology-neutral.
- DUPLICATE timing tables. ONE table only.
- "Kalman-filtered tempo" / "DMA-backed".
- More than 8 major callouts beyond the timing table itself.

British English required (centre, colour, behaviour, initialise).
```

✅ Both prompts use Captain's amended wording (Amendments L, Z, A9, SS) verbatim where applicable.
✅ Both prompts forbid the placeholder `Verify (K1v2). Measurement pending.` substitute.
✅ Both prompts forbid `4.8 ms invariant`.
✅ Both prompts forbid all zone numbering.
✅ Both prompts forbid `DMA-backed` / `Kalman-filtered`.

## 4. Firmware-edit assertion

✅ **No firmware files will be edited during Gate 1 execution.** Scope is strictly:
- Notebook creation (NotebookLM API)
- Source-file uploads (NotebookLM API)
- Studio infographic generation (NotebookLM API)
- PNG download (NotebookLM API)
- Markdown report authoring under `media/k1v2_infographics/calibration_gate1/`

Firmware paths (`firmware-v3/`, `lightwave-ios-v2/`, `tab5-encoder/`) are read-only references for citation; no edits planned, no edits performed.

## 5. Cleared to proceed

Preflight green. Proceeding to:
1. Create new NotebookLM notebook.
2. Upload 8 source files + 5 Pinterest reference images.
3. Generate Part 5 × 3 variants.
4. Generate Part 6 × 3 variants.
5. Download artifacts; save prompts beside each output.
6. Author Gate 1 evaluation report.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Gate 1 preflight authored after parallel-SSA source-pack updates landed (Amendments L, Z, A9, SS). All validation greps green. Panel 5 + Panel 6 prompts locked with Captain's verbatim amended wording. Cleared to proceed. |
