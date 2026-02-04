# Rive MCP Prompt Set (iOS Contract + Reference Artboards)

This file provides **ready‑to‑paste prompts** for Cursor/Rive MCP to scaffold the Rive file. The prompts are split into:

1. **Contract artboards + view models** (must match `RiveAssetRegistry.swift`).
2. **Reference artboards** that map 1:1 to exported PNGs for pixel‑matched tracing.

## Pre‑Flight (do once)
- Open the shared Rive file you want to use (e.g. `lw_design_reference.riv`).
- Confirm the Rive MCP server is connected.
- Keep this file as the **single source of truth** for scaffolding.

---

## 1) Contract Artboards + View Models (from `RiveAssetRegistry.swift`)

### Prompt A — Create all contract artboards + state machines
Paste this **single prompt** into Cursor:

```
Create the following artboards and state machines (one per asset), exact names:

1) Artboard: ConnectionIndicator, State Machine: Connection
2) Artboard: Discovery, State Machine: Discovery
3) Artboard: BeatPulse, State Machine: Beat
4) Artboard: HeroOverlay, State Machine: Hero
5) Artboard: PatternPill, State Machine: Accent
6) Artboard: PalettePill, State Machine: Accent
7) Artboard: AudioMicro, State Machine: Audio

For each artboard, ensure a default empty layout group exists (no shapes yet).
```

### Prompt B — Create view models and properties
Paste this **single prompt** into Cursor:

```
Create View Models matching the iOS contract. Use naming rule VM_<ArtboardName> and create properties with exact names/types:

VM_ConnectionIndicator:
- isConnected (bool)
- isConnecting (bool)
- isDiscovering (bool)
- hasError (bool)

VM_Discovery:
- isSearching (bool)

VM_BeatPulse:
- isBeating (bool)
- isDownbeat (bool)
- confidence (number)

VM_HeroOverlay:
- bpm (number)
- confidence (number)
- isBeating (bool)

VM_PatternPill:
- index (number)
- count (number)

VM_PalettePill:
- index (number)
- count (number)

VM_AudioMicro:
- rms (number)
- flux (number)
- confidence (number)
- isSilent (bool)
- isBeating (bool)
```

### Prompt C — Bind VM properties to inputs
Paste this **single prompt** into Cursor:

```
For each artboard state machine, create matching inputs and bind them 1:1 to the corresponding View Model properties:

ConnectionIndicator/Connection -> VM_ConnectionIndicator
Discovery/Discovery -> VM_Discovery
BeatPulse/Beat -> VM_BeatPulse
HeroOverlay/Hero -> VM_HeroOverlay
PatternPill/Accent -> VM_PatternPill
PalettePill/Accent -> VM_PalettePill
AudioMicro/Audio -> VM_AudioMicro
```

---

## 2) Reference Artboards (Pixel‑Match Targets)

These artboards are **reference‑only** and should match the exported PNGs in `reports/rive-reference/`. Create one artboard per PNG, use the exact name, and set the artboard size to the PNG’s pixel size.

### Prompt D — Create all screen artboards (390×844)
```
Create artboards with size 390x844 for each of these names:

Screen_Play
Screen_Zones
Screen_Audio
Screen_Device
Screen_ConnectionSheet
Screen_EffectSelector
Screen_PaletteSelector
Screen_Presets_Effect
Screen_Presets_Zone
Screen_Shows
Screen_ColourCorrection
Screen_DebugLog
Screen_WebSocketInspector
```

### Prompt E — Create all component artboards (use PNG sizes)
Use `reports/rive-reference/ARTBOARD_PNG_MAP.json` to set exact sizes. Paste this prompt and then set sizes to match the manifest:

```
Create component artboards with the exact names below. I will set the exact sizes from the manifest after creation:

Component_PersistentStatusBar
Component_HeroLEDPreview
Component_PatternPill
Component_PalettePill
Component_PrimaryControlsCard
Component_ExpressionParametersCard
Component_ColourCorrectionCard
Component_ZoneHeaderCard
Component_ZoneCard
Component_ZoneSplitSlider
Component_LEDStripView
Component_AudioMicroInteraction
Component_SpectrumVisualisation_Waterfall
Component_SpectrumVisualisation_DotRidge
Component_SpectrumVisualisation_DotMatrix
Component_EnergyCard
Component_BeatTempoCard
Component_StateCard
Component_WaveformSpectrumCard
Component_DSPHealthCard
Component_BeatHapticsCard
Component_AudioParametersCard
Component_ControlBusSmoothingCard
Component_BandEnvelopeCard
Component_SilenceDetectionCard
Component_NoveltyOnsetCard
Component_CategoryFilterPill
Component_EffectCard
Component_FilterChip
Component_PaletteCell
Component_PaletteGradientSwatch
Component_PaletteSwatchSheet
Component_DeviceInfoRow
Component_ConnectionDot
```

---

## 3) Importing PNGs for Tracing

1. Import PNGs from `reports/rive-reference/` into the Rive file.
2. For each artboard, drag the matching PNG into the artboard and **lock** it on a background layer.
3. Set the artboard size to match the PNG size (use the manifest).

The PNG ↔ artboard mapping is authoritative in:
- `reports/rive-reference/ARTBOARD_PNG_MAP.json`

---

## 4) Palette Swatch Sheet (Reference)

Use `Component_PaletteSwatchSheet.png` as a locked background for quick palette tracing. It includes all palette gradients extracted from firmware.
