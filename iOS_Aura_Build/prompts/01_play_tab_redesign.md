# aura.build Prompt 1: Play Tab Redesign

## Attachments (2 max)
1. **Screenshot of current Play tab** → `../02_play_tab.png`
2. **DesignTokens.swift** → `../reference/DesignTokens.swift`

## Prompt

Redesign this iOS LED controller's main screen. Dark mode interface (#0F1219 background).
The app controls 320 addressable LEDs on a Light Guide Plate.

Keep: LED strip preview as hero element (top), pattern/palette selection pills below,
brightness + speed sliders, collapsible expression parameters.

Improve: Move BPM ring from overlaying the LED preview to a compact pill beside it.
Add distinct icons to Pattern (play.circle) and Palette (paintpalette) pills.
Add skeleton loading states. Use glassmorphic cards with subtle inner shadows.
Fonts: Bebas Neue Bold for headings, Rajdhani Medium for body, JetBrains Mono for values.
Primary accent: #FFB84D (gold). Status: #4DFFB8 (success), #FF4D4D (error), #00D4FF (cyan).
Card background: #1E2535. Corner radius: 24pt hero, 16pt cards, 12pt nested.
iPhone 15 Pro frame, portrait only.

## Design Goals
- LED preview is the HERO element -- nothing should obscure it
- BPM display visible from arm's length but not overlapping hero
- Pattern and Palette pills visually distinguishable at a glance
- Skeleton/loading states for first connection
- Expression parameters grouped into collapsible sections (Core / Colour / Advanced)
- 8-layer "Liquid Glass" card treatment: blur + gradient + inner shadow + specular highlight + grain

## Current Issues Being Addressed
- BPM ring (80pt diameter) covers 57% of LED preview hero
- Pattern/Palette pills are visually identical (same styling, no icons)
- No loading states -- shows "0/0" counters on first load
- Expression parameters card has 7 flat sliders with no grouping
