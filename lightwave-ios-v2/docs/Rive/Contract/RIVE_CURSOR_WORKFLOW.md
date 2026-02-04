# Rive Cursor Workflow (Consolidated Prompts + Checklist)

This is the **single handoff** doc for Cursor. It contains:
1) A consolidated prompt to create **all reference artboards with exact sizes**.
2) A strict import checklist (order + locking + naming).
3) Contract scaffolding prompts (batched).

---

## 1) Consolidated Prompt — Create All Reference Artboards (Exact Sizes)

Paste this **single prompt** into Cursor with Rive MCP enabled:

```
Create artboards with the exact sizes listed. Use exact names:
- Component_AudioMicroInteraction: 358x72
- Component_AudioParametersCard: 358x165
- Component_BandEnvelopeCard: 358x205
- Component_BeatHapticsCard: 358x65
- Component_BeatTempoCard: 358x109
- Component_CategoryFilterPill: 120x32
- Component_ColourCorrectionCard: 358x252
- Component_ConnectionDot: 24x24
- Component_ControlBusSmoothingCard: 358x125
- Component_DSPHealthCard: 358x109
- Component_DeviceInfoRow: 358x44
- Component_EffectCard: 358x72
- Component_EnergyCard: 358x201
- Component_ExpressionParametersCard: 358x520
- Component_FilterChip: 100x28
- Component_HeroLEDPreview: 358x140
- Component_LEDStripView: 390x80
- Component_NoveltyOnsetCard: 358x128
- Component_PaletteCell: 108x68
- Component_PaletteGradientSwatch: 108x40
- Component_PalettePill: 358x107
- Component_PaletteSwatchSheet: 390x2044
- Component_PatternPill: 358x68
- Component_PersistentStatusBar: 390x64
- Component_PrimaryControlsCard: 358x130
- Component_SilenceDetectionCard: 358x125
- Component_SpectrumVisualisation_DotMatrix: 358x180
- Component_SpectrumVisualisation_DotRidge: 358x180
- Component_SpectrumVisualisation_Waterfall: 358x180
- Component_StateCard: 358x109
- Component_WaveformSpectrumCard: 358x197
- Component_ZoneCard: 358x319
- Component_ZoneHeaderCard: 358x117
- Component_ZoneSplitSlider: 358x319
- Screen_Audio: 390x844
- Screen_ColourCorrection: 390x844
- Screen_ConnectionSheet: 390x844
- Screen_DebugLog: 390x844
- Screen_Device: 390x844
- Screen_EffectSelector: 390x844
- Screen_PaletteSelector: 390x844
- Screen_Play: 390x844
- Screen_Presets_Effect: 390x844
- Screen_Presets_Zone: 390x844
- Screen_Shows: 390x844
- Screen_WebSocketInspector: 390x844
- Screen_Zones: 390x844
```

---

## 2) Rive Import Checklist (Idiot‑Proof)

1. Open the target `.riv` file (single shared file). Do not create multiple files.
2. Run the **Consolidated Prompt** above to create all reference artboards with exact sizes.
3. Import all PNGs from `reports/rive-reference/`.
4. For each artboard, drag the matching PNG in and **lock** it on a background layer.
   - The artboard name must match the PNG filename (without `.png`).
   - Example: `Screen_Play` uses `Screen_Play.png`.
5. Confirm artboard sizes match `reports/rive-reference/ARTBOARD_PNG_MAP.json`.
6. Keep background images locked at 100% opacity and **do not scale** the bitmap inside the artboard.
7. Build vector layers on top, matching the bitmap exactly.
8. Use the palette swatch sheet for colour tracing:
   - `Component_PaletteSwatchSheet.png`
   - Lock it as background in `Component_PaletteSwatchSheet` artboard.

---

## 3) Contract Scaffolding Prompts (Batched)

These create the **contract artboards**, state machines, and view models from `RiveAssetRegistry.swift`.

### Batch 1 — Artboards + State Machines
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

### Batch 2 — View Models + Properties
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

### Batch 3 — Bind View Models to State Machine Inputs
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

## Reference Files

- PNGs: `reports/rive-reference/`
- Mapping: `reports/rive-reference/ARTBOARD_PNG_MAP.json`
- Contract spec: `LightwaveOS/Utilities/Rive/RiveAssetRegistry.swift`
