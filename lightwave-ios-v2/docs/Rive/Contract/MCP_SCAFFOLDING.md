# Rive MCP Scaffolding Runbook (LightwaveOS iOS Contract)

This runbook drives Rive MCP to scaffold artboards, state machines, view models, and input bindings based on the iOS contract.

## Source of truth
- `LightwaveOS/Utilities/Rive/RiveAssetRegistry.swift`
- Generated spec: `docs/Rive/Contract/mcp_scaffold_spec.json`
- **iOS design → Rive:** `docs/Rive/IOS_TO_RIVE_TRANSFER.md` (artboards, tokens, screen layouts)

## Preconditions
- Rive Early Access running
- Single shared `.riv` file open and saved
- MCP connected in Cursor

## Baseline layout
- Artboard size: **390 x 844 pt** (iPhone 13 Pro baseline)
- Layout: scale-to-fit, maintain aspect

## View model naming
- `VM_<ArtboardName>`

## Recommended prompt (Cursor → Rive MCP)
Paste the following into Cursor with the Rive MCP connected. After the prompt, type **End Prompt** in Cursor.

```text
You are editing the currently open Rive file. Use the MCP tools to scaffold the artboards, state machines, view models, and inputs described below. Follow these rules strictly:
- Create a single shared file (use the current file).
- For each asset, create an artboard with the exact name.
- Set artboard size to 390 x 844.
- Create a state machine with the exact name.
- Create a view model named VM_<ArtboardName> and add properties matching the inputs.
- Bind each state machine input 1:1 to the corresponding view model property.
- Do not rename or remove any existing artboards if already present; add missing ones.
- Keep names exact and case-sensitive.

Assets:
1) Artboard: ConnectionIndicator | State Machine: Connection | View Model: VM_ConnectionIndicator
   Inputs: isConnected (bool), isConnecting (bool), isDiscovering (bool), hasError (bool)
2) Artboard: Discovery | State Machine: Discovery | View Model: VM_Discovery
   Inputs: isSearching (bool)
3) Artboard: BeatPulse | State Machine: Beat | View Model: VM_BeatPulse
   Inputs: isBeating (bool), isDownbeat (bool), confidence (number)
4) Artboard: HeroOverlay | State Machine: Hero | View Model: VM_HeroOverlay
   Inputs: bpm (number), confidence (number), isBeating (bool)
5) Artboard: PatternPill | State Machine: Accent | View Model: VM_PatternPill
   Inputs: index (number), count (number)
6) Artboard: PalettePill | State Machine: Accent | View Model: VM_PalettePill
   Inputs: index (number), count (number)
7) Artboard: AudioMicro | State Machine: Audio | View Model: VM_AudioMicro
   Inputs: rms (number), flux (number), confidence (number), isSilent (bool), isBeating (bool)
```

## Artboards for full UI reference
Create additional artboards for the full screen reference and component library. Use the naming scheme below.

### Screens (390 x 844)
- `Screen_Play`
- `Screen_Zones`
- `Screen_Audio`
- `Screen_Device`
- `Screen_ConnectionSheet`
- `Screen_EffectSelector`
- `Screen_PaletteSelector`
- `Screen_Presets_Effect`
- `Screen_Presets_Zone`
- `Screen_Shows`
- `Screen_ColourCorrection`
- `Screen_DebugLog`
- `Screen_WebSocketInspector`

### Components (use intrinsic height, 358pt width unless noted)
- `Component_PersistentStatusBar` (390 x 64)
- `Component_HeroLEDPreview` (358 x 140)
- `Component_PatternPill`
- `Component_PalettePill`
- `Component_PrimaryControlsCard`
- `Component_ExpressionParametersCard`
- `Component_ColourCorrectionCard`
- `Component_ZoneHeaderCard`
- `Component_ZoneCard`
- `Component_ZoneSplitSlider`
- `Component_LEDStripView` (390 x 80)
- `Component_AudioMicroInteraction` (358 x 72)
- `Component_SpectrumVisualisation_Waterfall` (358 x 180)
- `Component_SpectrumVisualisation_DotRidge` (358 x 180)
- `Component_SpectrumVisualisation_DotMatrix` (358 x 180)
- `Component_EnergyCard`
- `Component_BeatTempoCard`
- `Component_StateCard`
- `Component_WaveformSpectrumCard`
- `Component_DSPHealthCard`
- `Component_BeatHapticsCard`
- `Component_AudioParametersCard`
- `Component_ControlBusSmoothingCard`
- `Component_BandEnvelopeCard`
- `Component_SilenceDetectionCard`
- `Component_NoveltyOnsetCard`
- `Component_CategoryFilterPill`
- `Component_EffectCard` (358 x 72)
- `Component_FilterChip`
- `Component_PaletteCell` (108 x 68)
- `Component_PaletteGradientSwatch` (108 x 40)
- `Component_DeviceInfoRow` (358 x 44)
- `Component_ConnectionDot` (24 x 24)

## Verification checklist
- All artboards exist with exact names.
- All state machines exist with exact names.
- Each state machine has matching input set.
- Each view model has matching properties.
- Artboards set to 390 x 844.

## Executed from Cursor (Rive MCP)

The following was run via Rive MCP in Cursor; no paste required for view model creation.

- **View models created** (all 7, matching spec):
  - `VM_ConnectionIndicator` — isConnected, isConnecting, isDiscovering, hasError (boolean)
  - `VM_Discovery` — isSearching (boolean)
  - `VM_BeatPulse` — isBeating, isDownbeat (boolean), confidence (number)
  - `VM_HeroOverlay` — bpm, confidence (number), isBeating (boolean)
  - `VM_PatternPill` — index, count (number)
  - `VM_PalettePill` — index, count (number)
  - `VM_AudioMicro` — rms, flux, confidence (number), isSilent, isBeating (boolean)

**Still required in Rive (manual or per-artboard MCP):**
- Create artboards with exact names; set size to 390×844.
- For each artboard, create the state machine (Connection, Discovery, Beat, Hero, Accent, Accent, Audio) and bind each state machine input 1:1 to the corresponding view model property (use `listViewModels` to get property IDs for conditions/bindings).

## Notes
If Rive MCP reports that something already exists, keep it and proceed to the next item. Avoid renaming existing objects to prevent breaking the iOS contract.
