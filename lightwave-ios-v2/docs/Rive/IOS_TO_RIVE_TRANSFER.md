# iOS App Design → Rive Editor Transfer

This document transfers the LightwaveOS iOS v2 app design into Rive editor terms so the UI can be built and modified in Rive. Use it as the single reference when creating artboards, layouts, and components in Rive.

**Source of truth (iOS):**
- `LightwaveOS/Theme/DesignTokens.swift`
- `docs/DESIGN_SPEC.md`
- `mockups/ui-overhaul-mockup.html`

**Rive contract:** `docs/Rive/Contract/mcp_scaffold_spec.json` and `RIVE_ASSET_CONTRACT.md`

---

## 1. Canvas and artboard baseline

| Property | Value | Notes |
|----------|--------|--------|
| **Artboard size** | 390 × 844 pt | iPhone 13 Pro baseline; scale-to-fit in app |
| **Safe area** | Top 44pt (status bar), bottom 34pt (home indicator) | Content stays within 390 × 766 pt if full-screen |
| **Layout** | Single column, top-to-bottom | Match SwiftUI `VStack` flow |

---

## 2. Design tokens (Rive-friendly)

Use these when building layouts and text in Rive so the design matches the iOS app.

### 2.1 Colours (hex, no alpha unless noted)

| Token | Hex | Usage |
|-------|-----|--------|
| **lwBase** | `#0F1219` | App background |
| **lwCard** | `#1E2535` | Card background (top of gradient) |
| **lwCardBottom** | `#1A2030` | Card gradient bottom |
| **lwElevated** | `#252D3F` | Elevated surfaces |
| **lwGold** | `#FFB84D` | Primary accent, tab active |
| **lwBeatAccent** | `#FFCC33` | Beat pulse, brighter accent |
| **lwTextPrimary** | `#E6E9EF` | Primary text |
| **lwTextSecondary** | `#9CA3B0` | Secondary text |
| **lwTextTertiary** | `#6B7280` | Tertiary, tab inactive |
| **lwSuccess** | `#4DFFB8` | Connected, success |
| **lwError** | `#FF4D4D` | Error state |
| **lwCyan** | `#00D4FF` | Zone/slider accent |
| **lwZone0** | `#00FFFF` | Zone 0 (cyan) |
| **lwZone1** | `#00FF99` | Zone 1 (green) |
| **lwZone2** | `#9900FF` | Zone 2 (purple) |
| **lwZone3** | `#FF6600` | Zone 3 (orange) |
| **Tab bar bg** | `#0F1219` @ 95% | Tab bar background |

### 2.2 Spacing (pt)

| Token | Value | Usage |
|-------|--------|--------|
| **xs** | 4 | Icon → text |
| **sm** | 8 | Label → slider |
| **md** | 12 | Between cards in section |
| **lg** | 24 | Between major sections |

### 2.3 Corner radius (pt)

| Token | Value | Usage |
|-------|--------|--------|
| **hero** | 24 | LED preview, BPM hero container |
| **card** | 16 | Control cards, zone cards |
| **nested** | 12 | Sliders, dropdowns, badges |
| **pill** | capsule | PATTERN / PALETTE pills |

### 2.4 Typography (pt)

| Role | Size | Style | Usage |
|------|------|--------|--------|
| Hero numeral | 48 | Bebas Neue Bold | BPM display |
| Effect title | 22 | Bebas Neue Bold | Effect name on hero |
| Section header | 14 | Bebas Neue Bold | Uppercase labels |
| Sub-group header | 12 | Bebas Neue Bold | COLOUR, MOTION |
| Pill label | 11 | Bebas Neue Bold | PATTERN, PALETTE |
| Body value | 15 | Rajdhani Medium | Effect/palette name |
| Card label | 12 | Rajdhani Medium | Slider labels |
| Caption | 11 | Rajdhani Medium | IDs, counts |
| Slider / value | 13 | JetBrains Mono Medium | Numeric readouts |

### 2.5 Shadows (for reference; implement as effects in Rive)

| Level | CSS-style | Usage |
|-------|-----------|--------|
| Ambient | 0 2px 12px rgba(0,0,0,0.4) | Standard cards |
| Elevated | 0 4px 20px rgba(0,0,0,0.5) | Active cards |
| Hero glow | 0 0 40px accent 15% | LED preview |
| Beat pulse | 0 0 24px lwGold 60% | On beat |

---

## 3. Artboard map

Build these artboards in Rive so the editor mirrors the iOS app. Each main screen is one artboard (390 × 844). Embedded Rive assets (connection indicator, beat pulse, etc.) are separate small assets referenced by the contract.

### 3.1 Main screen artboards (390 × 844)

Create one artboard per tab; name and size must match for the transfer to stay in sync.

| Artboard name | Size | Content summary |
|---------------|------|------------------|
| **PlayScreen** | 390 × 844 | Status bar → Hero LED → PatternPill → PalettePill → Brightness/Speed → Expression → Transition |
| **ZonesScreen** | 390 × 844 | ZoneHeaderCard → LEDStripView → Zone 0–3 cards |
| **AudioScreen** | 390 × 844 | BPMHeroCard → Audio params → Audio-reactive effects grid |
| **DeviceScreen** | 390 × 844 | List: Device, Presets, Colour Correction, Network, Debug, About |

### 3.2 Embedded component assets (contract)

These are the 7 Rive assets the app embeds. They can live in the same .riv as the screens or in separate files; names and inputs must match the contract.

| Artboard | State machine | View model | Inputs |
|----------|----------------|------------|--------|
| ConnectionIndicator | Connection | VM_ConnectionIndicator | isConnected, isConnecting, isDiscovering, hasError (bool) |
| Discovery | Discovery | VM_Discovery | isSearching (bool) |
| BeatPulse | Beat | VM_BeatPulse | isBeating, isDownbeat (bool), confidence (number) |
| HeroOverlay | Hero | VM_HeroOverlay | bpm, confidence (number), isBeating (bool) |
| PatternPill | Accent | VM_PatternPill | index, count (number) |
| PalettePill | Accent | VM_PalettePill | index, count (number) |
| AudioMicro | Audio | VM_AudioMicro | rms, flux, confidence (number), isSilent, isBeating (bool) |

---

## 4. Screen-by-screen layout (for Rive layout tree)

Use these as the blueprint when building each artboard in Rive. Heights and order match the iOS layout.

### 4.1 PlayScreen (390 × 844)

| # | Block | Height | Notes |
|---|--------|--------|--------|
| 1 | **PersistentStatusBar** | 44pt | Green dot + “Connected” + device name + BPM. Embed **ConnectionIndicator** here. |
| 2 | **HeroLEDPreview** | 120pt | 320-LED strip representation; centre marker at 79/80. Overlay: effect name (22pt), BPM ring. Embed **HeroOverlay** (BPM/beat). |
| 3 | Spacer | 24pt (lg) | |
| 4 | **PatternPill** (card) | ~80pt | “PATTERN”, “42/101”, effect name, audio badge. Embed **PatternPill** accent asset or match style. |
| 5 | Spacer | 12pt (md) | |
| 6 | **PalettePill** (card) | ~80pt | “PALETTE”, “5/75”, gradient swatch, palette name. Embed **PalettePill** accent. |
| 7 | Spacer | 24pt | |
| 8 | **Brightness** row | ~48pt | Label “BRIGHTNESS”, value, 10pt track, 24pt thumb, gold fill. |
| 9 | Spacer | 8pt | |
| 10 | **Speed** row | ~48pt | Label “SPEED”, value, cyan→gold track. |
| 11 | Spacer | 24pt | |
| 12 | **ExpressionParametersCard** | Collapsible | COLOUR (Hue, Saturation, Mood), MOTION (Trails, Intensity, Complexity, Variation). 16pt corners, gradient bg. |
| 13 | **TransitionCard** | ~100pt | Type dropdown, duration slider, “Trigger Transition” button. |

Colours: card gradient `#1E2535` → `#1A2030`; text primary `#E6E9EF`; labels `#9CA3B0`; accent `#FFB84D`.

### 4.2 ZonesScreen (390 × 844)

| # | Block | Height | Notes |
|---|--------|--------|--------|
| 1 | **ZoneHeaderCard** | ~56pt | “ZONE MODE” toggle, zone count [1][2][3][4], Preset dropdown. |
| 2 | **LEDStripView** | 60pt | Two rows, zone-coloured segments; “CENTRE” at 79/80. Use lwZone0–3. |
| 3 | Spacer | 24pt | |
| 4 | **ZoneCard** × N | ~140pt each | Per zone: 80 LEDs, L[R], Effect/Palette/Blend dropdowns, Speed/Brightness sliders. Border colour = zone colour. |

### 4.3 AudioScreen (390 × 844)

| # | Block | Height | Notes |
|---|--------|--------|--------|
| 1 | **BPMHeroCard** | 180pt | Beat ring (clockwise fill), “124” 48pt, “BPM”, confidence %. Embed **BeatPulse** for beat feedback. |
| 2 | Spacer | 24pt | |
| 3 | **AudioParametersCard** | ~100pt | Mic type, Gain, Threshold sliders. |
| 4 | **AudioReactiveEffectsCard** | Grid | Quick-select effect pills. |
| 5 | Optional **AudioMicro** | — | Embed **AudioMicro** for micro-interactions if desired. |

### 4.4 DeviceScreen (390 × 844)

List-style layout (`.insetGrouped` equivalent):

- **Device**: firmware, uptime, FPS, heap, RSSI
- **Presets**: Effect Presets, Zone Presets, Shows (nav rows)
- **Colour Correction**: Gamma, Auto-exposure, Guardrail, Mode
- **Network**: Connection, WiFi
- **Debug**: Debug Log, WebSocket Inspector
- **About**: App version

Background `#0F1219`; list cells elevated `#1E2535`; 16pt corners.

---

## 5. Where Rive assets plug in (iOS → Rive)

| iOS view / area | Rive asset | Binding |
|-----------------|------------|--------|
| PersistentStatusBar (connection state) | ConnectionIndicator | isConnected, isConnecting, isDiscovering, hasError |
| ConnectionSheet empty state | Discovery | isSearching |
| HeroLEDPreview (BPM ring, beat) | HeroOverlay | bpm, confidence, isBeating |
| Beat ring / BPM hero (Audio tab) | BeatPulse | isBeating, isDownbeat, confidence |
| PatternPill (selection accent) | PatternPill (Accent) | index, count |
| PalettePill (selection accent) | PalettePill (Accent) | index, count |
| Audio micro feedback | AudioMicro | rms, flux, confidence, isSilent, isBeating |

---

## 6. Steps to build in Rive

1. **Create artboards** (in Rive app): PlayScreen, ZonesScreen, AudioScreen, DeviceScreen; set each to **390 × 844**. Create or reuse the 7 component artboards per contract.
2. **Apply design tokens**: Use the colour hex values, spacing (4/8/12/24), and corner radii (24/16/12) in layouts and text.
3. **Build layout per screen**: For each main artboard, add a root layout (column), then add child layouts/text per section in the order in §4. Use flexbox-like layout (column, padding, gap) to match the iOS spacing.
4. **State machines and view models**: For each component artboard, create the state machine and VM per `mcp_scaffold_spec.json`; bind inputs 1:1. (View models already created via MCP; see `MCP_SCAFFOLDING.md`.)
5. **Placeholder content**: Use text blocks for labels and values (e.g. “124”, “BPM”, “PATTERN”, “LGP Holographic Auto-Cycle”) so the Rive file reads like the app; replace with animations/transitions as needed.

After this transfer, the Rive file is the design source for the iOS app: modify layouts, colours, and motion in Rive, then sync token and layout decisions back to `DesignTokens.swift` and the SwiftUI views as needed.

---

## 7. Executed via Rive MCP (Cursor)

The following was created in the **active artboard** of the currently open Rive file using Rive MCP tools.

### createLayout (all four screens)

All layouts created in the **active artboard**. If you use one artboard per tab, create three more artboards (ZonesScreen, AudioScreen, DeviceScreen), set each to 390×844, then **move** each root (ZonesScreenRoot, AudioScreenRoot, DeviceScreenRoot) onto its artboard in Rive.

**PlayScreen** — Root: `PlayScreenRoot` (id: 0-589)
- StatusBar, HeroLEDPreview, PatternPillCard, PalettePillCard, BrightnessRow, SpeedRow.
- Two extra sections (ids 0-751, 0-753): **Expression Parameters** and **Transition**. In Rive: rename to `ExpressionParametersCard` / `TransitionCard`, set flexDirection to column, backgroundColor `#1E2535`, cornerRadius 16, padding 16; add text: "EXPRESSION PARAMETERS" / "COLOUR … MOTION …" and "TRANSITION" / "Type: Iris …" / "Trigger Transition" if the appended content did not show.

**ZonesScreen** — Root: `ZonesScreenRoot` (id: 0-755)
- ZoneHeaderCard (ZONE MODE, Zones [1][2][3][4], Preset).
- LEDStripView (60pt, zone strip placeholder).
- ZoneCard0 (cyan), ZoneCard1 (green) with Effect/Palette/Blend/Speed/Brightness.

**AudioScreen** — Root: `AudioScreenRoot` (id: 0-905)
- BPMHeroCard (180pt, 124 BPM, confidence, beat position, Haptic).
- AudioParametersCard (Mic Type, Gain, Threshold).
- AudioReactiveEffectsCard (effect pills: Starfield, Beat Pulse, LGP Holo, Ripple, Neural, Biolumi, Shockwave, Aurora).

**DeviceScreen** — Root: `DeviceScreenRoot` (id: 0-1078)
- DeviceSection, PresetsSection, ColourCorrectionSection, NetworkSection, DebugSection, AboutSection (list-style cards).
