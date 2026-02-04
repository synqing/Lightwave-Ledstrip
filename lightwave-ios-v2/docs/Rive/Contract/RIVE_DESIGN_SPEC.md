# LightwaveOS iOS v2 — Rive Design Reference Spec (Exact)

This document captures the exact UI design tokens and layout rules used by the LightwaveOS iOS v2 app. It is the canonical source for reproducing the current UI in Rive, at pixel‑matched fidelity.

## Baseline
- **Device baseline:** iPhone 13 Pro
- **Artboard size:** 390 × 844 pt
- **Theme:** Dark only

## Colour Tokens (from `DesignTokens.swift`)
- `lwGold` — `#FFB84D` (primary accent)
- `lwBase` — `#0F1219` (app background)
- `lwCard` — `#1E2535` (card background)
- `lwElevated` — `#252D3F` (elevated surfaces)
- `lwTextPrimary` — `#E6E9EF`
- `lwTextSecondary` — `#9CA3B0`
- `lwTextTertiary` — `#6B7280`
- `lwSuccess` — `#4DFFB8`
- `lwError` — `#FF4D4D`
- `lwCyan` — `#00D4FF`
- `lwBeatAccent` — `#FFCC33`
- `lwZone0` — `#00FFFF`
- `lwZone1` — `#00FF99`
- `lwZone2` — `#9900FF`
- `lwZone3` — `#FF6600`

### Gradients
- `lwCardGradient`: `#1E2535` → `#1A2030` (vertical)
- `lwGoldRadial`: `lwGold` @ 8% → transparent (radial)

## Typography (from `DesignTokens.swift`)
**Bebas Neue**
- `heroNumeral`: 48pt, Bold
- `effectTitle`: 22pt, Bold
- `sectionHeader`: 14pt, Bold (uppercase, +1.5px tracking)
- `subGroupHeader`: 12pt, Bold (uppercase, +1.5px tracking)
- `statusBrand`: 16pt, Bold
- `pillLabel`: 11pt, Bold

**Rajdhani**
- `bodyValue`: 15pt, Medium
- `cardLabel`: 12pt, Medium
- `caption`: 11pt, Medium
- `hapticLabel`: 13pt, Medium
- `statusLabel`: 12pt, Medium

**JetBrains Mono**
- `sliderValue`: 13pt, Medium
- `stepperValue`: 13pt, Medium
- `monospace`: 11pt, Medium
- `metricValue`: 11pt, Medium

## Spacing
- `Spacing.xs`: 4pt
- `Spacing.sm`: 8pt
- `Spacing.md`: 12pt
- `Spacing.lg`: 24pt

## Corner Radius
- `hero`: 24pt
- `card`: 16pt
- `nested`: 12pt

## Shadows
- **Ambient card shadow:** 0 2px 12px rgba(0,0,0,0.4)
- **Elevated shadow:** 0 4px 20px rgba(0,0,0,0.5)
- **Hero glow:** 0 0 40px accent @ 15%
- **Beat pulse glow:** 0 0 24px `lwGold` @ 60%

## Global Layout Rules
- **Horizontal screen padding:** 16pt (most screens)
- **Card width (baseline):** 358pt (390 − 32)
- **Section vertical spacing:** 24pt (`Spacing.lg`)

## Component‑level Rules (selected)
### EffectSelector
- **Category pills:** 16pt horizontal padding, 8pt vertical padding, capsule radius
- **Effect card height:** 72pt
- **Card radius:** 16pt
- **Selected stroke:** 2pt `lwGold`

### PaletteSelector
- **Search bar:** 12pt horizontal, 10pt vertical, 12pt radius
- **Filter chips:** 12pt horizontal, 6pt vertical
- **Palette cell:** 10pt radius, gradient swatch 40pt height
- **Selected stroke:** 2pt `lwGold`

### Cards (LWCard)
- **Background:** `lwCard` or `lwCardGradient`
- **Corner radius:** 16pt
- **Internal padding:** 12–16pt

## Palette Swatches (Firmware‑derived)
Palette gradients are extracted from `firmware/v2/src/palettes/Palettes_MasterData.cpp` and compiled into the app as `Resources/Palettes/Palettes_Master.json`. These gradients drive the swatch rendering in `PaletteSelectorView`.

If the JSON is missing, the app falls back to names only; swatches will degrade to the default grey gradient. For exact matching, ensure `Palettes_Master.json` is present in the bundle.

## Screen Catalogue
This spec applies to all screens and components in the following catalogue:
- Play, Zones, Audio, Device
- Connection sheet
- Effect selector sheet
- Palette selector sheet
- Presets (Effect + Zone)
- Shows view
- Colour correction view
- Debug log view
- WebSocket inspector view

## Notes
- All measurements are in points.
- The reference PNG export is the authoritative visual baseline.
- Use only the fonts listed here; no substitutions.
