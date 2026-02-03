# LightwaveOS iOS v2 — Design Specification

## Design Tokens (Revised)

### Typography Scale

| Role | Size | Font | Weight | Usage |
|------|------|------|--------|-------|
| Hero numeral | 48pt | SF Pro Rounded | Bold | BPM display |
| Effect title | 22pt | SF Pro Display | Semibold | Current effect name on hero |
| Body value | 15pt | SF Pro Text | Regular | Selected effect/palette name |
| Section header | 13pt | SF Pro Text | Semibold | "PATTERN", "ZONES" — uppercase, +0.08em tracking |
| Card label | 11pt | SF Pro Text | Medium | Slider labels, metadata |
| Caption | 11pt | SF Pro Text | Regular | IDs, counts, timestamps |
| Monospace | 11pt | SF Mono | Regular | Debug log only |

### Spacing Rules

| Context | Value |
|---------|-------|
| Between major sections | 24pt (`lg`) |
| Between cards within a section | 12pt |
| Card internal padding | 16pt horizontal, 12pt vertical |
| Between related elements (label → slider) | 8pt (`sm`) |
| Inline (icon → text) | 4pt (`xs`) |

### Corner Radius Hierarchy

| Surface | Radius | Usage |
|---------|--------|-------|
| Hero container | 24pt | LED preview, BPM hero |
| Primary card | 16pt | Control cards, zone cards |
| Nested element | 12pt | Sliders, dropdowns, badges |
| Pill/chip | capsule | Category badges, filter pills |

### Shadows

| Level | Shadow | Usage |
|-------|--------|-------|
| Ambient card | `0 2px 12px rgba(0,0,0,0.4)` | Standard cards |
| Elevated | `0 4px 20px rgba(0,0,0,0.5)` | Active/focused cards |
| Hero glow | `0 0 40px <accent>.opacity(0.15)` | LED preview |
| Beat pulse | `0 0 24px lwGold.opacity(0.6)` | On beat events |

### New Colour Tokens

```swift
// Gradient card background (not flat)
static let lwCardGradient = LinearGradient(
    colors: [Color(hex: "1E2535"), Color(hex: "1A2030")],
    startPoint: .top, endPoint: .bottom
)

// Hero background glow
static let lwGoldRadial = RadialGradient(
    colors: [Color.lwGold.opacity(0.08), Color.clear],
    center: .center, startRadius: 0, endRadius: 200
)

// Beat accent (brighter than lwGold)
static let lwBeatAccent = Color(hex: "FFCC33")

// Zone colours (explicit)
static let lwZone0 = Color(hex: "00FFFF")  // Cyan
static let lwZone1 = Color(hex: "00FF99")  // Green
static let lwZone2 = Color(hex: "9900FF")  // Purple
static let lwZone3 = Color(hex: "FF6600")  // Orange

// Tab bar
static let lwTabBarBackground = Color(hex: "0F1219").opacity(0.95)
static let lwTabBarActive = Color.lwGold
static let lwTabBarInactive = Color(hex: "6B7280")
```

---

## Play Tab Layout

```
+--------------------------------------------------+
|  PersistentStatusBar (44pt)                       |
|  [green dot] Connected  •  LGP Holo... • 124 BPM |
+--------------------------------------------------+
|                                                    |
|  HeroLEDPreview (120pt)                            |
|  [===320 LED strip via Canvas===]                  |
|  Centre marker (gold line at LED 79/80)            |
|  Effect name overlay (22pt, bottom-left)           |
|  BPM ring overlay (48pt, bottom-right)             |
|  Swipe L/R to change effect with transition        |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  PatternPill (full-width card)                     |
|  PATTERN                           42/101  >      |
|  LGP Holographic Auto-Cycle                        |
|  [waveform] Audio-Reactive                         |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  PalettePill (full-width card)                     |
|  PALETTE                            5/75  >       |
|  [====gradient swatch====] Spectral Nova           |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  BRIGHTNESS                              178       |
|  [========================O========]               |
|  10pt track, 24pt thumb, gold gradient fill        |
|                                                    |
|  SPEED                                    15       |
|  [=========O=======================]               |
|  Cyan→gold gradient fill                           |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  EXPRESSION PARAMETERS                     v       |
|  (collapsible, starts expanded on first launch)    |
|                                                    |
|  COLOUR                                            |
|  Hue        [=====O====]  64                       |
|             Rainbow-tint track gradient             |
|  Saturation [======O====]  200                     |
|             Greyscale→vivid track gradient          |
|  Mood       [====O======]  100                     |
|             "Reactive <-------> Smooth"             |
|                                                    |
|  MOTION                                            |
|  Trails     [========O==]  200                     |
|             "Short <----------> Long"              |
|  Intensity  [=====O=====]  128                     |
|             "Subtle <---------> Bold"              |
|  Complexity [====O======]  100                     |
|             "Minimal <-------> Dense"              |
|  Variation  [=O=========]   20                     |
|             "Uniform <--------> Chaotic"           |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  TRANSITION                                        |
|  Type: [Iris v]  Duration: [===O==] 1200ms         |
|  [Trigger Transition]                              |
|                                                    |
+--------------------------------------------------+
```

---

## Effect Gallery Sheet

```
+--------------------------------------------------+
| X  Select Effect                                  |
+--------------------------------------------------+
| [Search effects...]                               |
| [All] [Audio] [Ambient] [LGP] [Particles] [Waves]|
+--------------------------------------------------+
|                                                    |
| LIGHT GUIDE PLATE (13)                            |
|                                                    |
| +----------------------------------------------+  |
| | [subtle interference pattern bg]             |  |
| | LGP Holographic Auto-Cycle                   |  |
| | Light Guide Plate           [waveform] Audio |  |
| +----------------------------------------------+  |
|                                                    |
| +----------------------------------------------+  |
| | LGP Chromatic Shear              [selected]  |  |
| | Light Guide Plate                            |  |
| +----------------------------------------------+  |
|                                                    |
| PARTICLES (11)                                     |
| ...                                                |
```

Effect cards: 72pt tall, full-width, subtle category-specific pattern background, 16pt corners. Selected = gold border + glow. Tap = instant apply + dismiss.

---

## Palette Grid Sheet

```
+--------------------------------------------------+
| X  Select Palette                                 |
+--------------------------------------------------+
|                                                    |
| CPT-CITY ARTISTIC (33)                            |
|                                                    |
| [====red-orange-yellow====] Spectral Nova          |
| [====grey-blue-purple=====] Urban Twilight         |
| [====red-orange-brown=====] Canyon Heat    [check] |
| [====mint-teal-green======] Mint Aurora            |
|                                                    |
| CRAMERI SCIENTIFIC (24)                            |
| [====earth tones==========] Batlow                 |
| [====warm-cool diverging==] Roma                   |
| ...                                                |
```

Each row: full-width gradient swatch (12pt tall, capsule radius) + name. Selected = gold border + check.

---

## Zones Tab Layout

```
+--------------------------------------------------+
|  ZoneHeaderCard                                    |
|  ZONE MODE                              [ON/OFF]   |
|  Zones: [1] [2] [3] [4]  Preset: [Dual Split v]  |
+--------------------------------------------------+
|                                                    |
|  LEDStripView (60pt tall, full-width)              |
|  L [████zone1████|████zone2████] R                 |
|  L [████zone1████|████zone2████] R                 |
|                 CENTRE                              |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  Zone 0 (cyan border)                              |
|  80 LEDs · L[40-79] R[80-119]                      |
|  Effect:  [Ripple Enhanced        v]               |
|  Palette: [Copper                 v]               |
|  Blend:   [Additive              v]               |
|  Speed:   [========O====] 25                       |
|  Bright:  [===========O=] 200                      |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  Zone 1 (green border)                             |
|  80 LEDs · L[0-39] R[120-159]                      |
|  Effect:  [LGP Holographic       v]               |
|  Palette: [Sunset Real           v]               |
|  Blend:   [Screen                v]               |
|  Speed:   [=====O=======] 18                       |
|  Bright:  [===========O=] 200                      |
|                                                    |
+--------------------------------------------------+
```

---

## Audio Tab Layout

```
+--------------------------------------------------+
|                                                    |
|  BPMHeroCard (180pt)                               |
|                                                    |
|           .-~~~-.                                  |
|         /  beat   \                                |
|       |   ring     |                               |
|       |   fills    |                               |
|        \ clockwise/                                |
|          '-...-'                                   |
|                                                    |
|            124                                     |
|            BPM                                     |
|      confidence: 95%                               |
|                                                    |
|  Beat position: [●] [○] [○] [○]                   |
|  Haptic feedback: [ON]                             |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  AUDIO PARAMETERS                                  |
|  Mic Type: [I2S v]                                 |
|  Gain:     [======O====] 0.8                       |
|  Threshold:[===O=======] 0.3                       |
|                                                    |
+--------------------------------------------------+
|                                                    |
|  AUDIO-REACTIVE EFFECTS                            |
|  Quick-select grid of reactive effects             |
|  [Starfield] [Beat Pulse] [LGP Holo] [Ripple]    |
|  [Neural]    [Biolumi]    [Shockwave] [Aurora]    |
|                                                    |
+--------------------------------------------------+
```

---

## Device Tab Layout

NavigationStack + List (`.insetGrouped`, `.scrollContentBackground(.hidden)`, `.background(Color.lwBase)`):

- **Device**: firmware version, uptime, FPS, free heap, WiFi RSSI
- **Presets**: Effect Presets (nav), Zone Presets (nav), Shows (nav)
- **Colour Correction**: Gamma toggle + value, Auto-exposure toggle + target, Brown guardrail, Mode (OFF/HSV/RGB/BOTH)
- **Network**: Connection settings, WiFi setup
- **Debug**: Debug Log (full-screen), WebSocket Inspector
- **About**: App version

---

## Motion Design

| Trigger | Animation | Duration |
|---------|-----------|----------|
| Beat event | Hero preview scale 1.02, spring | 150ms |
| Downbeat | Hero radial glow + BPM ring reset | 200ms |
| Effect change | Hero cross-fade | 300ms |
| Palette change | Swatch gradient transition | 500ms |
| Section expand | Spring (bounce 0.15) | 300ms |
| Slider drag active | Thumb glow (lwGold, radius 8) | Continuous |
| Slider released | Glow fade out | 300ms |

All ambient animations gated by `@Environment(\.accessibilityReduceMotion)`.

---

## New Files to Create

| File | Purpose |
|------|---------|
| `Views/Tabs/PlayTab.swift` | Main performance tab |
| `Views/Tabs/ZonesTab.swift` | Multi-zone composition |
| `Views/Tabs/AudioTab.swift` | BPM + audio config |
| `Views/Tabs/DeviceTab.swift` | Settings + diagnostics |
| `Views/Play/HeroLEDPreview.swift` | Canvas-based 320-LED strip |
| `Views/Play/PatternPill.swift` | Full-width effect selector pill |
| `Views/Play/PalettePill.swift` | Full-width palette selector pill with swatch |
| `Views/Play/PrimaryControlsCard.swift` | Brightness + Speed |
| `Views/Play/ExpressionParametersCard.swift` | 7 params in 2 semantic groups |
| `Views/Play/TransitionCard.swift` | Type, duration, trigger |
| `Views/Zones/ZoneHeaderCard.swift` | Toggle + count + preset |
| `Views/Zones/ZoneCard.swift` | Full per-zone controls |
| `Views/Audio/BPMHeroCard.swift` | Large BPM with beat ring |
| `Views/Audio/AudioParametersCard.swift` | Mic, gain, threshold |
| `Views/Audio/AudioReactiveEffectsCard.swift` | Quick-select grid |
| `Views/Device/ColourCorrectionView.swift` | Gamma, AE, guardrail |
| `Views/Device/PresetsView.swift` | Effect/zone presets |
| `Views/Device/ShowsView.swift` | Show playback |
| `Views/Shared/PersistentStatusBar.swift` | 44pt status bar |
| `ViewModels/TransitionViewModel.swift` | Transition state |
| `ViewModels/ColourCorrectionViewModel.swift` | CC state |
| `ViewModels/PresetsViewModel.swift` | Scene/preset state |

## Files to Port and Modify from v1

| v1 File | Action |
|---------|--------|
| `Theme/DesignTokens.swift` | Port + expand (typography, shadows, gradients) |
| `Components/LWSlider.swift` | Port + upgrade (10pt track, 24pt thumb, glow) |
| `Components/LWCard.swift` | Port + upgrade (gradient bg, 16pt corners) |
| `Components/LWButton.swift` | Port as-is |
| `Components/BeatPulse.swift` | Port + enlarge (8pt→12pt) |
| `Components/LWDropdown.swift` | Replace with PatternPill/PalettePill |
| `ViewModels/AppViewModel.swift` | Port + add LED stream, mode, scenes |
| `ViewModels/EffectViewModel.swift` | Port + fix categoryId/audioReactive decode |
| `ViewModels/PaletteViewModel.swift` | Port + switch from hardcoded to API |
| `ViewModels/ParametersViewModel.swift` | Port as-is (debounce logic is solid) |
| `ViewModels/ZoneViewModel.swift` | Port + fix segments decode, wire all controls |
| `ViewModels/AudioViewModel.swift` | Port + add audio params |
| `Models/EffectMetadata.swift` | Port + add missing fields |
| `Models/PaletteMetadata.swift` | Port + add `colors: [Color]` |
| `Models/ZoneConfig.swift` | Port + add effectName, blendModeName |
| `Models/ZoneSegment.swift` | Port as-is |
| `Models/SystemState.swift` | Port as-is |
| `Models/APIResponse.swift` | Port as-is |
| `Models/BeatEvent.swift` | Port as-is |
| `Models/DeviceInfo.swift` | Port as-is |
| `Network/RESTClient.swift` | Port + add segment/preset/transition/CC endpoints |
| `Network/WebSocketManager.swift` | Port + add LED stream parse, transition cmds |
| `Network/DeviceDiscovery.swift` | Port as-is |
| `Views/EffectSelectorView.swift` | Port + redesign as gallery |
| `Views/PaletteSelectorView.swift` | Port + redesign as swatch grid |
| `Views/ConnectionSheet.swift` | Port as-is |
| `Views/LEDStripView.swift` | Port + enlarge to 60pt, Canvas rewrite |
| `Views/DebugLogView.swift` | Port → move to Device tab |
| `App/LightwaveOSApp.swift` | Port as-is |
