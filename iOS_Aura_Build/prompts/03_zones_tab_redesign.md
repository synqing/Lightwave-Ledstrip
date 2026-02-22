# aura.build Prompt 3: Zone Composition Redesign

## Attachments (2 max)
1. **Screenshot of current Zones tab** → `../03_zones_tab.png`
2. **DESIGN_SPEC.md** → `../reference/DESIGN_SPEC.md`

## Prompt

Redesign this iOS zone composition interface for a 320-LED strip controller.
Users split their LED strip into 2-4 independent zones with different effects.

Current issues: Zone count selector is unclear, no blend mode visualization,
no solo/mute per zone. Zone cards are visually dense with 6 controls each.

Improve:
- Replace zone count stepper with 3 visual LED strip diagrams (2/3/4 zones)
  showing how the strip divides, using zone colours: Cyan #00FFFF, Green #00FF99,
  Purple #9900FF, Orange #FF6600
- Each zone card: effect name, palette swatch, blend mode dropdown,
  brightness/speed sliders, solo/mute toggles
- LED strip visualization at top showing all zones with draggable boundaries
- Compact zone pills with zone colour accent for switching between zone detail cards

Dark mode, glassmorphic cards, gold accent #FFB84D.
Corner radius: 16pt cards. Spacing: 8pt between elements, 24pt between sections.
iPhone 15 Pro frame.

## Design Goals
- Visual zone count selector showing LED strip division diagrams
- LED strip visualisation at top with colour-coded zone segments
- Draggable zone boundary sliders on the strip visualisation
- Zone cards with colour-coded headers matching zone colours
- Blend mode picker row (Additive, Screen, Multiply, Overlay)
- Solo/Mute toggles per zone for isolated preview
- Zone presets section with 12-slot grid (save/load/delete)
- Empty state when zones are disabled explaining what zones are

## Current Issues Being Addressed
- Zone count selector is a plain stepper with no visual preview
- No blend mode controls despite backend support
- No way to solo/preview a single zone in isolation
- Zone cards have 6 controls each creating visual density
- Navigation arrows are 26pt (below 44pt accessibility minimum)
- No empty state when zones are disabled -- just blank space
