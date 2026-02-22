# aura.build Prompt 2: Audio Tab Restructure

## Attachments (2 max)
1. **Screenshot of current Audio tab** → `../04_audio_tab.png`
2. **DesignTokens.swift** → `../reference/DesignTokens.swift`

## Prompt

Redesign this iOS audio analysis dashboard for an LED controller. Currently shows
14 flat cards in a single scroll -- needs information hierarchy.

Group into 5 collapsible sections:
1. SPECTRUM VISUALIZATION (3 canvas-based visualizers: waterfall, dot ridge, dot matrix)
2. BEAT & TEMPO (BPM display with ring, beat phase indicator, haptic toggle)
3. ENERGY & DIAGNOSTICS (RMS, flux, clipping, DSP health) -- collapsed by default
4. MICROPHONE TUNING (SPH0645 gain, sensitivity) -- collapsed by default
5. DSP CONFIGURATION (smoothing, band envelope, silence detection, novelty onset) -- collapsed by default

Dark mode (#0F1219 background), glassmorphic cards (#1E2535).
Beat-reactive elements pulse gold (#FFB84D) on beats.
BPM numeral: 32pt Bebas Neue Bold. Values: JetBrains Mono 13pt.
Sections should have sticky headers with collapse chevron.
iPhone 15 Pro frame, portrait only.

## Design Goals
- Clear hierarchy: visualisation at top, controls in middle, diagnostics collapsed
- Section headers with collapse/expand state (chevron indicator)
- BPM hero treatment with ring, confidence arc, phase indicator
- Spectrum visualisers as full-width canvas cards (waterfall, dot ridge, dot matrix)
- Metric cards with label/value pairs using monospace font for numeric readouts
- Persist collapse state between sessions

## Current Issues Being Addressed
- 14 cards in flat scroll with zero grouping or hierarchy
- BPM buried as card 6 of 14 -- Performer persona can't find it
- Technical DSP diagnostics mixed with user-facing controls
- Explorer persona overwhelmed by wall of unfamiliar controls
- No way to hide advanced controls for casual users
