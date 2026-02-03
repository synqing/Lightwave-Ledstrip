# LightwaveOS iOS v2

SwiftUI dashboard for the LightwaveOS ESP32-S3 LED controller. Music visualisation interface for a dual-strip 320-LED Light Guide Plate.

## Quick Start

### Prerequisites

- **Xcode 15+** (for Swift 6.0 support)
- **iOS 17.0+** deployment target
- **XcodeGen** (optional, for project file generation)

### Build and Run

#### Option 1: Use XcodeGen (Recommended)

```bash
# Install XcodeGen if not already installed
brew install xcodegen

# Generate Xcode project
cd lightwave-ios-v2
xcodegen

# Open in Xcode
open LightwaveOS.xcodeproj
```

#### Option 2: Manual Xcode Project Setup

If you don't have XcodeGen, create a new Xcode project manually:

1. Open Xcode
2. Create new iOS App project
3. Set **Product Name**: `LightwaveOS`
4. Set **Organisation Identifier**: `com.spectrasynq`
5. Set **Interface**: SwiftUI
6. Set **Language**: Swift
7. Drag the `LightwaveOS` folder into the project navigator

### Project Structure

```
lightwave-ios-v2/
├── LightwaveOS/
│   ├── App/
│   │   └── LightwaveOSApp.swift          # App entry point
│   ├── Views/
│   │   ├── ContentView.swift             # Root TabView
│   │   ├── Shared/
│   │   │   ├── PersistentStatusBar.swift # 44pt status bar
│   │   │   └── ConnectionSheet.swift     # Device discovery
│   │   ├── Tabs/
│   │   │   ├── PlayTab.swift             # Main performance tab
│   │   │   ├── ZonesTab.swift            # Multi-zone controls
│   │   │   ├── AudioTab.swift            # BPM and audio config
│   │   │   └── DeviceTab.swift           # Settings and diagnostics
│   │   ├── Play/                         # Play tab components
│   │   ├── Zones/                        # Zone tab components
│   │   ├── Audio/                        # Audio tab components
│   │   └── Device/                       # Device tab components
│   ├── ViewModels/
│   │   ├── AppViewModel.swift            # Root ViewModel
│   │   ├── EffectViewModel.swift         # Effect state
│   │   ├── PaletteViewModel.swift        # Palette state
│   │   ├── ParametersViewModel.swift     # Parameter state
│   │   ├── ZoneViewModel.swift           # Zone state
│   │   └── AudioViewModel.swift          # Audio state
│   ├── Models/                           # Data models
│   ├── Network/                          # REST + WebSocket clients
│   ├── Components/                       # Reusable UI components
│   ├── Theme/
│   │   └── DesignTokens.swift            # Colour, typography, spacing
│   └── Resources/
│       └── Info.plist
├── docs/
│   └── DESIGN_SPEC.md                    # Full design specification
├── project.yml                           # XcodeGen configuration
├── CLAUDE.md                             # Project context (this file)
└── README.md                             # This file
```

## Architecture

### State Management

- **`@Observable` macro** for all ViewModels (Swift 6)
- **`@MainActor`** for all UI-bound state
- **Environment injection** via `.environment()` on ContentView
- **No ObservableObject** (deprecated in favour of `@Observable`)

### Navigation

- **TabView** with 4 tabs: Play | Zones | Audio | Device
- **PersistentStatusBar** always visible above TabView (44pt)
- **NavigationStack** for Device tab's settings drill-down

### Network

- **REST API** for device discovery and configuration (`/api/v1/*`)
- **WebSocket** for real-time LED data stream and beat events (`ws://device/ws`)
- **Dual-channel architecture**: REST for setup, WebSocket for live updates

## Current Implementation Status

### ✅ Completed

- App entry point (LightwaveOSApp.swift)
- ContentView with TabView shell
- PersistentStatusBar (connection + effect + BPM)
- All 4 tab scaffolds (Play, Zones, Audio, Device)
- Component stubs for all major views
- Design tokens (colours, typography, spacing, shadows)
- XcodeGen project configuration
- Info.plist with local network permissions

### 🚧 In Progress (Stubs)

All views are scaffolded but need full implementation:

- **Play Tab**: LED preview, pattern/palette pills, controls, parameters, transitions
- **Zones Tab**: Zone header, LED strip view, per-zone cards
- **Audio Tab**: BPM hero card, audio parameters, effect grid
- **Device Tab**: Settings list, presets, colour correction, debug log

### ⏳ To Do

- Port ViewModels from v1 (replace stubs)
- Implement HeroLEDPreview with Canvas rendering (320 LEDs)
- Implement effect/palette selector sheets
- Wire up all network calls (REST + WebSocket)
- Add LED data stream parsing
- Implement zone visualization
- Add beat pulse animations
- Implement transition commands
- Port debug log from v1

## Key Design Decisions

### Centre Origin

All LED visualisations reflect LEDs 79/80 as the centre point. The 320-LED strip is rendered as two 160-LED segments extending left and right from centre.

### Performance Targets

- **120 FPS** for LED rendering (Canvas-based, no SwiftUI views per LED)
- **150ms debounce** on all slider → network calls
- **Pending flags** prevent WebSocket echo-back loops

### British English

All comments and user-facing strings use British English spelling (colour, centre, visualisation, etc.).

### Dark Mode Only

App enforces `.preferredColorScheme(.dark)` — no light mode support.

## Design Tokens

### Colour Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `lwGold` | `#FFB84D` | Primary accent, tabs, highlights |
| `lwBase` | `#0F1219` | App background |
| `lwCard` | `#1E2535` | Card background |
| `lwElevated` | `#252D3F` | Highest elevation surfaces |
| `lwTextPrimary` | `#E6E9EF` | Primary text |
| `lwTextSecondary` | `#9CA3B0` | Secondary text |
| `lwTextTertiary` | `#6B7280` | Tertiary text |
| `lwSuccess` | `#4DFFB8` | Connection success |
| `lwError` | `#FF4D4D` | Errors, disconnected state |
| `lwBeatAccent` | `#FFCC33` | Beat pulse accent |

### Typography Scale

| Role | Font | Size | Weight | Usage |
|------|------|------|--------|-------|
| Hero numeral | SF Pro Rounded | 48pt | Bold | BPM display |
| Effect title | SF Pro Display | 22pt | Semibold | Current effect name |
| Body value | SF Pro Text | 15pt | Regular | Selected names |
| Section header | SF Pro Text | 13pt | Semibold | Uppercase labels (+0.08em tracking) |
| Card label | SF Pro Text | 11pt | Medium | Slider labels |
| Caption | SF Pro Text | 11pt | Regular | IDs, counts |
| Monospace | SF Mono | 11pt | Regular | Debug log only |

### Spacing

| Context | Value | Usage |
|---------|-------|-------|
| `xs` | 4pt | Inline (icon → text) |
| `sm` | 8pt | Related elements (label → slider) |
| `md` | 12pt | Between cards within section |
| `lg` | 24pt | Between major sections |

### Corner Radius

| Surface | Radius | Usage |
|---------|--------|-------|
| `hero` | 24pt | LED preview, BPM hero |
| `card` | 16pt | Control cards, zone cards |
| `nested` | 12pt | Sliders, dropdowns, badges |

## Network Protocol

### REST API (`/api/v1/`)

- `GET /effects` — Fetch all effects
- `GET /palettes` — Fetch all palettes
- `POST /effects/:id` — Set current effect
- `POST /palettes/:id` — Set current palette
- `POST /params` — Update effect parameters
- `POST /zones` — Configure zone mode
- `POST /transitions` — Trigger transition

### WebSocket (`ws://device/ws`)

Messages are newline-delimited JSON:

```json
{"type": "led_data", "data": [r,g,b,r,g,b,...]}  // 960 bytes (320 LEDs × RGB)
{"type": "beat", "bpm": 124, "confidence": 0.95, "isDownbeat": true}
{"type": "state", "effectId": 42, "brightness": 200, ...}
```

## Local Network Discovery

App uses **mDNS (Bonjour)** to discover LightwaveOS controllers:

- **Service type**: `_http._tcp`
- **Required permission**: `NSLocalNetworkUsageDescription` in Info.plist
- **Auto-connect**: Connection sheet dismisses on successful connection

## Development Notes

### Previews

All views include `#Preview` blocks for live Canvas previews in Xcode. AppViewModel is injected with mock data for preview rendering.

### STUB Comments

Views marked with `// STUB:` or `// TODO:` are scaffolds awaiting full implementation. Search for these markers to find work-in-progress items.

### Testing

To test without a physical device:
1. Use Xcode Previews for UI layout verification
2. Mock AppViewModel state in preview blocks
3. Test WebSocket/REST clients against a local mock server

## Xcode Project Generation

The `project.yml` file defines the Xcode project structure. After modifying it, regenerate the `.xcodeproj`:

```bash
xcodegen
```

This ensures project file consistency and avoids merge conflicts.

## Next Steps

1. **Port ViewModels from v1** — Replace stub ViewModels with full implementations
2. **Implement HeroLEDPreview** — Canvas-based 320-LED strip rendering
3. **Wire network layer** — Connect REST/WebSocket to ViewModels
4. **Build effect/palette selectors** — Sheet UI for browsing effects and palettes
5. **Add beat animations** — Pulse effects on BPM events
6. **Implement zone controls** — Multi-zone effect composition

---

For detailed design specifications, see [docs/DESIGN_SPEC.md](docs/DESIGN_SPEC.md).

For v1 reference implementation, see [../lightwave-ios/](../lightwave-ios/).
