---
abstract: "Style bible for the K1v2 infographic series. Five reusable style families (A–E) with palette, layout grammar, typography direction, icon style, density level, and prompt phrases. Agent-authored from the original style references — NotebookLM does NOT extract this from images per Amendment 3. Used as the controlling source for every Studio generation prompt."
---

# 03 — Style Bible

This bible defines the visual language for the K1v2 six-panel infographic series. **It is authored by the agent, not extracted by NotebookLM from reference images.** Reference images, if uploaded, supplement the bible — they do not replace it.

The bible has three sections:

1. **Series-wide grammar** — colour, typography, layout skeleton, connector system. These apply to every panel.
2. **Five style families** — each panel is generated against one family. Families are reusable; multiple panels may share one family.
3. **Per-family prompt phrases** — drop-in text for the NotebookLM Studio prompt's "Style:" field.

## 1. Series-wide grammar

### Palette by data class

The series uses a fixed palette mapping. Same data class → same colour across every panel.

| Data class | Colour | Hex | Usage |
|---|---|---|---|
| Audio capture / I2S / spectral analysis | Warm amber | `#E89F3C` | Mic, FFT bins, spectral magnitude flow |
| Musical features (chroma, beat, tempo, saliency) | Coral / pink | `#E8517A` | Chroma vector, beat line, tempo estimate |
| Cross-core publication (ControlBus, SnapshotBuffer) | Violet | `#8B5CF6` | The cross-core rail, ControlBusFrame card, atomic-publish indicators |
| Visual rendering / 120 FPS loop / EffectContext | Cyan / blue | `#3DB5E8` | Render-loop card, EffectContext, LED mapping |
| Hardware (LEDs, RMT peripheral, ESP32-S3) | Cool white | `#F0F4FA` | Strip illustrations, peripheral icons |
| Background | Dark graphite with faint circuit traces | `#0F1217` | Every panel |
| Accent / emphasis | Safety orange | `#FF7A29` | Title number badges, "Measured" tags |
| Warning / Verify | Muted yellow | `#D4B842` | "Verify" classification on timing values |

### Typography

| Role | Family | Weight | Case |
|---|---|:---:|---|
| Section title bar | Condensed sans-serif (e.g. "Inter Tight", "Bebas Neue", or close approximation) | 700 | UPPERCASE |
| Subsection headers | Same family | 600 | Sentence case |
| Body labels | Clean geometric sans-serif (e.g. "Inter", "Manrope") | 500 | Sentence case |
| Numeric values (tables, timing) | Tabular-numerals sans-serif (e.g. "Inter" tabular variant) | 500 | — |
| Code-style identifiers (constants, struct names) | Monospaced (e.g. "JetBrains Mono", "IBM Plex Mono") | 500 | As written in source |

NotebookLM may not render the exact font choices; the bible specifies *direction*, not exact typeface. Manual cleanup downstream substitutes the final typography per the locked palette + weight scheme.

### Layout skeleton (every panel uses this)

```
┌─────────────────────────────────────────────────────────────────────┐
│ [01]  PANEL TITLE                                       Series mark │  ← Title bar
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─INPUT──┐    ┌─TRANSFORM──────┐    ┌─OUTPUT────┐                  │
│  │        │ ─▶ │                │ ─▶ │           │                  │
│  └────────┘    └────────────────┘    └───────────┘                  │
│                                                                     │
│ ┌──── ControlBus / Snapshot rail (continuity line, same y across) ──┐│
│ └────────────────────────────────────────────────────────────────────┘│
│                                                                     │
│  Bottom strip: timing / data contract / safety invariants           │
└─────────────────────────────────────────────────────────────────────┘
   ↑                                                          ↑
   8% margin (left continuation stub)        8% margin (right continuation stub)
```

**Continuity rules:**

- Title bar at the same y-position across all 6 panels.
- The cross-core / Snapshot rail at the same y-position across all 6 panels.
- 8–10% horizontal padding on left and right edges. Continuation stubs from previous / to next panel exit through these margins, not through the centre.
- Bottom strip at the same y-position across all 6 panels.
- No critical text within the 8% edge margins (safe zone for stitching panoramas).

### Connector system

| Connector | Style | Use |
|---|---|---|
| Primary signal flow | Glowing thin line, 2–3 px, palette-coloured per data class, soft outer glow | Audio → spectral, spectral → features, features → ControlBus, ControlBus → render |
| Secondary annotation | Hairline 1 px, neutral grey, no glow | Numeric labels, callouts |
| Cross-core boundary | Dashed line, violet, with "core 0 / core 1" axis labels | Where data crosses the SnapshotBuffer |
| Continuation stub (left/right edges) | Glowing line entering/exiting the panel boundary | Inter-panel continuity |

### Icon style

Minimal technical line icons. No skeuomorphism (except in Style Family D where the family DNA is skeuomorphic). Icons are line-only with optional inner-glow when on a dark background.

## 2. Five style families

### Style Family A — Neon Hardware Explainer

**DNA:**
- Background: dark technical board with faint circuit traces.
- Hero element: realistic 3D ESP32-S3 module, central composition.
- Annotation: numbered callout panels around the hero, with leader lines.
- Lower 1/3: clean block diagram or pipeline strip.
- Palette: full series palette plus neon accents (cyan / orange / amber / violet).

**Best for:** Panel 1 (System Overview). The full audio-to-visual pipeline is best shown with the SoC as the central hero and subsystems annotated outward.

**Risks if used wrong:** Becomes overcrowded if every callout has paragraphs of text. Limit to ≤8 callouts per panel.

### Style Family B — Cinematic Exploded Firmware Stack

**DNA:**
- Background: deep black void.
- Hero: floating exploded layered stack (audio → features → cross-core → render → output).
- Lighting: dramatic blue / violet / coral / cyan glow per layer.
- Detail: HUD-style annotations, minimal large labels.
- Palette: high-contrast subset of the series palette, reduced colour count per panel.

**Best for:** Cover image / hero / system-vision shot. NOT for technical reference panels — text quality collapses at high cinematic density.

**Risks if used wrong:** Generated text becomes nonsense. Use for visual impact; rely on manual overlay for any factual labels.

### Style Family C — Industrial Runtime Styleguide

**DNA:**
- Background: off-white engineering grid (or dark graphite for series consistency — dark variant preferred for K1v2).
- Surface: machined-feeling tactile UI components, bolted-feeling cards.
- Palette: safety orange + gray + base-graphite. Subtler than neon; reads as "system reference sheet" not "poster".
- Composition: modular design-system board with tokens, tables, badges.

**Best for:** Panels that present a *system inventory* rather than a *flow* — Panel 4 (Cross-Core Publication, ControlBusFrame as a structured contract) and Panel 6 (Timing Budget table).

**Risks if used wrong:** Becomes generic SaaS dashboard if not constrained by the series palette.

### Style Family D — Dark Skeuomorphic Control Surface

**DNA:**
- Background: dark textured panel (graphite or matte black).
- Surfaces: soft skeuomorphic cards with rounded edges and subtle shadows.
- Lighting: blue glow on focus rings, warm cream on illuminated edges.
- Composition: control-board layout with sliders, toggles, meters.

**Best for:** Panel 3 (Musical Feature Engine) — the "live audio-reactive parameter dashboard" framing maps cleanly to a control-panel aesthetic.

**Risks if used wrong:** Reads as UI design rather than firmware architecture. Constrain by adding system-level annotations (e.g. "internal state, not user-facing") to keep the focus on signal flow.

### Style Family E — Graphite System Reference Board

**DNA:**
- Background: dark graphite with structured sectioning.
- Surfaces: realistic UI components with depth (more polished than Family C).
- Palette: charcoal / slate / orange / muted blue-gray.
- Composition: dense but organised component taxonomy. Foundations / controls / data / feedback states.

**Best for:** Panel 2 (Audio Capture & DSP) and Panel 5 (Visual Render Engine). Both panels carry a *lot* of structural detail; the Graphite Reference Board layout grammar handles density without becoming chaotic.

**Risks if used wrong:** Becomes generic enterprise design-system if not narrowed by the series palette and the 8-callout-cap rule.

## 3. Per-family prompt phrases

Drop these directly into the NotebookLM Studio prompt's "Style:" field.

### Family A — Neon Hardware Explainer

```
Use a Neon Hardware Explainer style: dark circuit-board background with faint
traces, central realistic ESP32-S3 module as the hero object, numbered technical
callout panels around the SoC, glowing thin signal lines coloured per data class
(amber for audio, coral for musical features, violet for cross-core, cyan for
visual rendering), lower-third clean block diagram. High readability. No more
than 8 major callouts. Series palette: warm amber / coral / violet / cyan on
dark graphite, with safety-orange accent badges.
```

### Family B — Cinematic Exploded Firmware Stack

```
Use a Cinematic Exploded Firmware Stack style: deep black void background,
floating exploded layered stack with five layers (audio capture, spectral
analysis, cross-core publication, visual rendering, LED output), translucent
neon signal routes between layers, dramatic but controlled lighting in blue /
violet / coral / cyan, large readable section labels only — no microtext. Premium
sci-fi firmware architecture aesthetic. Visual impact is the priority; assume
labels will be overlaid manually after generation.
```

### Family C — Industrial Runtime Styleguide

```
Use an Industrial Runtime Styleguide style: dark graphite engineering background
with subtle grid, tactile machined-feeling component cards, safety-orange accent
colour, charcoal / slate surfaces, compact field-inventory tables, badges and
tokens, modular component taxonomy. Treat the panel as a system-reference sheet,
not a flow diagram. Series palette: warm amber / coral / violet / cyan accents
on dark graphite.
```

### Family D — Dark Skeuomorphic Control Surface

```
Use a Dark Skeuomorphic Control Surface style: dark textured graphite panel
background, soft skeuomorphic cards with rounded edges and subtle drop shadows,
warm illuminated control elements, blue focus rings, sliders / toggles / meters
arranged as a tactile control board. Parameters are internal firmware state
(not end-user controls); annotate accordingly. Series palette: warm amber /
coral / violet / cyan accents.
```

### Family E — Graphite System Reference Board

```
Use a Graphite System Reference Board style: dark graphite background with
organised section dividers, modular cards arranged as a foundations + controls +
data display + feedback inventory, orange accent highlights, precise UI
components with subtle depth, high legibility, compact tables. Density is
allowed but must remain organised — no chaotic dumps. Series palette: warm
amber / coral / violet / cyan on dark graphite, with orange section accents.
```

## 4. Per-panel family assignment

| Panel | Title | Family | Reason |
|---|---|:---:|---|
| 1 | System Overview | A — Neon Hardware Explainer | SoC-centred hero with subsystem callouts |
| 2 | Audio Capture & DSP | E — Graphite Reference Board | Dense pipeline detail benefits from organised structure |
| 3 | Musical Feature Engine | D — Dark Control Surface | Feature extraction reads as a "live parameter dashboard" |
| 4 | Cross-Core Publication | C — Industrial Styleguide | Panel is a contract/inventory, not a flow |
| 5 | Visual Render Engine | E — Graphite Reference Board (variant) | Centre-origin radial diagram with structured surrounding context |
| 6 | Timing & Safety Budget | C — Industrial Styleguide | Pure tables and timing bars — best on a styleguide grid |

## 5. What this bible explicitly disallows

- **Rainbow / full-hue-wheel** in any panel. Hard constraint from `firmware-v3/CLAUDE.md`.
- **More than 8 major callouts** per panel.
- **Critical text within 8% horizontal edge margin** (would break panorama stitching).
- **Title-bar y-position drift** between panels (would break vertical alignment in a deck or panorama).
- **More than 4 saturated colours per panel** (excludes background and neutral typography). Above 4 the palette starts shouting.
- **Microtext or generated paragraphs** masquerading as factual content. NotebookLM is composition-only; final text is overlaid manually.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Style bible authored manually from the five style families per Amendment 3 of Rev 3 plan. Palette assigned per data class; layout skeleton + continuity rules locked across all 6 panels; per-family prompt phrases ready for drop-in into NotebookLM Studio. |
