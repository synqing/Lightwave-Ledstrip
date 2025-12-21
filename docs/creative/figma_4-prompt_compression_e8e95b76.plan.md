---
name: Figma 4-prompt compression
overview: Reduce the existing 11 Figma Make prompts into 4 high-leverage prompts, anchored by the PRISM.node token spec and the UI requirements, while preserving the non-negotiable domain constraints and the full 4-tab information architecture.
todos:
  - id: synth_overview
    content: Synthesize the key non-negotiables (IA, constraints, tokens) into a short briefing usable inside prompts.
    status: completed
  - id: draft_4_prompts
    content: Write the 4 condensed prompts with explicit references to the two attached files and unambiguous output requirements.
    status: in_progress
    dependencies:
      - synth_overview
  - id: select_2_attachments
    content: Finalise the 2-file attachment set and provide exact file paths and how each prompt should reference them.
    status: pending
    dependencies:
      - draft_4_prompts
---

# 4

-Prompt Figma Make Strategy (with 2 Attachments)

## Context absorbed from the docs (what we’re building)

- **Product**: a web dashboard UI for LightwaveOS controlling a dual-strip **WS2812, 320 LEDs total** system, designed around **CENTRE ORIGIN** (effects radiate from LED 79/80).
- **Information architecture**: **4 tabs** (Control+Zones, Shows, Effects Library, System) with specific layouts and responsibilities (no “Transitions” tab; transitions live in Control’s Quick Tools column).
- **Design system**: strict PRISM.node tokens (dark canvas/surface/elevated), with **gold** primary accent and **cyan** secondary; Inter + JetBrains Mono; defined spacing/radii/shadows/animation tokens.
- **Domain constraints**:
- **No rainbow colour cycling** in previews.
- LED viz must show **centre marker**, zone boundaries, dual strip representation.
- Performance indicators and thresholds included (FPS etc.) in System.
- **Deliverable expectation** (from the prompt guide): multi-frame design including states, responsive variants, edge/empty/loading/error states, and a basic interactive prototype.

Primary sources reviewed:

- [`docs/creative/01_K1_NODE1_ARCHITECTURE.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/01_K1_NODE1_ARCHITECTURE.md)
- [`docs/creative/02_UI_REQUIREMENTS.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/02_UI_REQUIREMENTS.md)
- [`docs/creative/03_FIGMA_PROMPT_GUIDE.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/03_FIGMA_PROMPT_GUIDE.md)
- [`docs/creative/04_DESIGN_TOKENS.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/04_DESIGN_TOKENS.md)
- [`docs/creative/README.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/README.md)

## Strategy: why 4 prompts is the “minimum safe” set

- **Prompt 1 (Tokens + primitives)** locks the visual language and prevents drift.
- **Prompt 2 (Shell + LED viz + navigation)** anchors the always-visible scaffold and the domain-specific LED constraints.
- **Prompt 3 (All 4 tabs as complete frames)** generates the actual product UI in one cohesive go.
- **Prompt 4 (States + responsive + prototype + handoff)** forces completeness (edge cases, breakpoints, interactions) without needing 4 separate prompts for each.

This preserves everything the 11-prompt flow was gating for, but collapses repetitive restatement.

## Attachments (2-file limit)

Attach exactly these two files:

- [`docs/creative/04_DESIGN_TOKENS.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/04_DESIGN_TOKENS.md)
- [`docs/creative/02_UI_REQUIREMENTS.md`](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/creative/02_UI_REQUIREMENTS.md)

In each prompt, explicitly say: “Use the attached `04_DESIGN_TOKENS.md` as the single source of truth for styles; use the attached `02_UI_REQUIREMENTS.md` as the single source of truth for layout/IA/domain constraints.”

## The 4 condensed prompts (copy/paste)

### Prompt 1 — Tokens + component foundations

**Goal**: Create a token library and a minimal component set that the rest of the frames must reuse.**Prompt text**:

- Create a PRISM.node design token library for LightwaveOS.
- Use attached `04_DESIGN_TOKENS.md` as the single source of truth for all colours, typography, spacing, radii, shadows, animation tokens, z-index, and breakpoints.
- Create reusable base components aligned to the requirements in attached `02_UI_REQUIREMENTS.md`: Button variants, Slider, Input/Select, Toggle, Checkbox, Card (surface/elevated/bordered), Badge/Dot status indicator, Progress bars, basic chart container style.
- Include states for each component: default, hover, active, disabled, error/focus.
- Ensure accessibility: focus rings, touch targets, readable contrast.
- Output as a Figma page containing all tokens and a component library page.

### Prompt 2 — App shell + navigation + LED strip visualisation

**Goal**: Create the persistent layout scaffold shared by all screens.**Prompt text**:

- Build the main application shell (header + tab bar + always-visible LED strip area) using tokens from attached `04_DESIGN_TOKENS.md`.
- Implement the 4-tab navigation from attached `02_UI_REQUIREMENTS.md`: Control+Zones, Shows, Effects Library, System.
- LED strip visualisation requirements (from attached `02_UI_REQUIREMENTS.md`):
- 320 LEDs, dual 160 strips, centre marker at LED 79/80 with cyan indicator and label.
- Zone boundary markers (dashed) when zones enabled.
- No rainbow cycling.
- Create connection state variants: connected, connecting, disconnected.
- Produce frames for each tab selected state (same shell; only tab highlight changes in this prompt).

### Prompt 3 — The four tabs (complete screen frames)

**Goal**: Generate the actual UI for each tab, matching the specified layouts.**Prompt text**:

- Using the shell from Prompt 2 and components from Prompt 1, create complete tab content frames per attached `02_UI_REQUIREMENTS.md`:
- Tab 1 Control+Zones: 3-column layout (30/40/30) with global controls, zone composer (zone tabs colour-coded), and Quick Tools including Transitions.
- Tab 2 Shows: 40/60 split with show library, timeline editor, and sticky Now Playing bar.
- Tab 3 Effects Library: 50/50 split with effects browser + palettes browser, plus shared preview bar.
- Tab 4 System: 2x2 dashboard grid (Performance, Network, Device, Firmware) plus collapsible sections (WiFi config, debug options, serial monitor).
- Include realistic empty/default content placeholders, and ensure spacing alignment uses the token grid.

### Prompt 4 — Edge cases, responsive variants, prototype links, handoff notes

**Goal**: Force completeness: states, breakpoints, interactions, and developer-ready annotations.**Prompt text**:

- Add dedicated frames for: loading states, empty states, error states, disconnected state banner, long names truncation, 0-parameter effect message, all zones disabled warning.
- Create responsive variants using the breakpoints guidance in attached `04_DESIGN_TOKENS.md` and behaviour described in attached `02_UI_REQUIREMENTS.md` (desktop/laptop/tablet; mobile optional).
- Wire an interactive prototype:
- Tab navigation switches content.
- Zone tab switching.
- Effect selection populates metadata/parameter panel.
- Dropdown open/close.
- Slider hover/active behaviour.
- Add handoff annotations: token references on key elements, spacing measurements, component names matching the component library.

## Success criteria (what “done” looks like)

- All colours/typography/spacing derived from `04_DESIGN_TOKENS.md` with no drift.
- All 4 tabs exist as coherent, consistent frames, matching `02_UI_REQUIREMENTS.md` layouts.
- LED visualisation honours centre origin and forbidden patterns.
- At least: default + hover + disabled (and focus where relevant) states present for interactive components.
- Responsive variants and a basic prototype are included.

## Risks / failure modes to watch for (and how to counter)

- **Token drift**: restate “attached tokens are the single source of truth” in every prompt.
- **LED centre/origin errors**: explicitly repeat “centre marker at LED 79/80” and “no edge-originating effects”.