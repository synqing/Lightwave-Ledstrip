# Rive Asset Contract — LightwaveOS iOS v2

Each `.riv` file must match the contract below. Update the registry and this document together.

## Connection Indicator
- File: `lw_connection_indicator.riv`
- Artboard: `ConnectionIndicator`
- State machine: `Connection`
- Inputs:
  - `isConnected` (bool)
  - `isConnecting` (bool)
  - `isDiscovering` (bool)
  - `hasError` (bool)
- Events:
  - `tap`

## Discovery Empty State
- File: `lw_discovery_empty_state.riv`
- Artboard: `Discovery`
- State machine: `Discovery`
- Inputs:
  - `isSearching` (bool)
- Events: none

## Beat Pulse
- File: `lw_beat_pulse.riv`
- Artboard: `BeatPulse`
- State machine: `Beat`
- Inputs:
  - `isBeating` (bool)
  - `isDownbeat` (bool)
  - `confidence` (number 0–1)
- Events: none

## Hero Overlay
- File: `lw_hero_overlay.riv`
- Artboard: `HeroOverlay`
- State machine: `Hero`
- Inputs:
  - `bpm` (number)
  - `confidence` (number 0–1)
  - `isBeating` (bool)
- Events: none

## Pattern Pill Accent
- File: `lw_pattern_pill_accent.riv`
- Artboard: `PatternPill`
- State machine: `Accent`
- Inputs:
  - `index` (number)
  - `count` (number)
- Events: none

## Palette Pill Accent
- File: `lw_palette_pill_accent.riv`
- Artboard: `PalettePill`
- State machine: `Accent`
- Inputs:
  - `index` (number)
  - `count` (number)
- Events: none

## Audio Micro Interaction
- File: `lw_audio_micro.riv`
- Artboard: `AudioMicro`
- State machine: `Audio`
- Inputs:
  - `rms` (number)
  - `flux` (number)
  - `confidence` (number 0–1)
  - `isSilent` (bool)
  - `isBeating` (bool)
- Events: none

