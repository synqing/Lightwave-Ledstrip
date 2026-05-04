---
abstract: "Project overview for the K1v2 LightwaveOS infographic source pack. Defines what K1v2 is, who the infographic audience is, the build scope (`esp32dev_audio_esv11_k1v2_32khz`), and the operating doctrine: NotebookLM is a visual-synthesis engine, not the technical-truth authority. The truth lives in this source pack and the upstream 127-source firmware notebook. Read first."
---

# 00 — Project Overview

## What K1v2 is

K1v2 is the second-generation hardware revision of the **K1 Lightwave LED controller**, a real-time audio-reactive lighting system. The hardware is a custom **ESP32-S3** module driving a **dual-strip Light Guide Plate (LGP)** with 320 individually-addressable WS2812 LEDs (160 LEDs per strip, 2 strips). The firmware (LightwaveOS) is built from `firmware-v3/` in this repository, target environment `esp32dev_audio_esv11_k1v2_32khz`.

The product transforms ambient or line-input audio into spatial light behaviour — beat-synchronised pulses, chord-aware colour shifts, transient-aware bloom — rendered symmetrically outward from the centre seam between LEDs 79 and 80.

## Who the infographic audience is

The six-panel infographic series this source pack feeds is aimed at three readers:

1. **Firmware engineers** — to understand the actor-model architecture, cross-core data publication, and timing budgets without reading source code.
2. **Technical founders / engineering leadership** — to evaluate the system as a deployable real-time embedded product, with confidence that timings, signal flow, and safety invariants are real-engineered rather than aspirational.
3. **Product reviewers** — to see "what's inside the box" with sufficient detail to write informed technical commentary, but not so much that the infographic becomes a documentation dump.

The infographics are **not** intended for end-users (consumer audience) or marketing surfaces. They sit one tier above a sales deck — comparable to a published architecture diagram in a hardware press kit.

## Build scope

This bundle covers **exactly one** firmware build target:

```
esp32dev_audio_esv11_k1v2_32khz
```

That environment is the canonical K1 V2 production target. The ESV11 audio backend runs at 32 kHz with a 125 Hz frame rate. The calibrated tempo / beat-tracking constants are applied via `EsV11_32kHz_Shim.h`. Other PlatformIO environments (`pipelinecore`, bare ESV11, native test envs, K1 V1 boards) are **out of scope** for this infographic series.

K1 V1 is mentioned in this bundle only as historical context — see the K1v1 status-strip note in `01_VERIFIED_FIRMWARE_FACTS.md`. K1 V1 hardware is deactivated; do not document its features as current.

## Operating doctrine: NotebookLM is a visual-synthesis engine

NotebookLM is being used here for **composition and styling** — backgrounds, hardware illustrations, layout grammar, glow-trace aesthetics — not as the source of technical truth. Google's NotebookLM help page explicitly warns that generated infographics may contain visual or factual inaccuracies. The discipline of this bundle reflects that warning:

- **All technical claims are sourced from this bundle's own files** (`01_VERIFIED_FIRMWARE_FACTS.md`, `07_TIMING_BUDGETS.md`).
- **NotebookLM is not allowed to invent** algorithm names, byte sizes, coefficients, function names, struct names, or timing measurements. The label whitelist (`04_LABEL_WHITELIST.md`) and forbidden-claims list (`05_FORBIDDEN_CLAIMS.md`) constrain its output.
- **Final technical text is overlaid manually** in downstream design tooling. NotebookLM PNGs supply background and composition; labels, tables, and numbers are typeset against the verified-facts file, not against the model's generated text.

## Relationship to the upstream firmware-corpus notebook

This bundle is **not** a replacement for the firmware-corpus NotebookLM notebook (`92d45c0b-83c7-4971-aa9a-2c9ee13b06d4`, "Lightwave-Ledstrip — K1 Project Knowledge Base — forensic-corrected 2026-05-04"). That notebook holds 127 sterilised firmware sources and is the authoritative truth archive. This bundle is a **tightly-curated derivative** built downstream of it, scoped to ~8 sources for infographic generation only.

The two are **not interchangeable**. Cross-checking a public claim should always hit the upstream notebook (or, ideally, the firmware source itself) — never this bundle's distilled summary alone.

## What's in this bundle

| File | Purpose |
|---|---|
| `00_PROJECT_OVERVIEW.md` | This file. Read first. |
| `01_VERIFIED_FIRMWARE_FACTS.md` | Confirmed values + source citations + confidence tier. |
| `02_PIPELINE_PARTITIONING.md` | Core 0 / Core 1 separation; what crosses the SnapshotBuffer boundary. |
| `03_STYLE_BIBLE.md` | Five style families with prompt phrases. **Agent-authored, not LLM-extracted from images.** |
| `04_LABEL_WHITELIST.md` | British-English exact terms; public-safe phrasing table. |
| `05_FORBIDDEN_CLAIMS.md` | Falsifications + structural bans + zone-conflation footgun. |
| `06_PART_OUTLINES.md` | Six-panel breakdown with required content per panel. |
| `07_TIMING_BUDGETS.md` | Confirmed / Derived / Budgeted / Verify classification per number. |

## What this bundle does NOT contain

- Audio AGC zone discussion (a known firmware bug, scheduled for fix — see BACKLOG F-6).
- K1v1 hardware features documented as current (the 30-LED status strip is deactivated; mentioned only as historical context).
- Effect-by-effect catalogue (handled by the upstream firmware-corpus notebook; would dilute the 6-panel architecture focus).
- Setup or pairing UX (out of scope; the audience is technical, not consumer).

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Bundle-overview file authored from approved Rev 3 plan (`/Users/spectrasynq/.claude/plans/quirky-tinkering-falcon.md`). |
