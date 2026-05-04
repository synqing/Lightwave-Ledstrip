# LightwaveOS Codebase Map

> Agent quick-reference. Source: synqtool map 2026-03-14.

## Overview

| Field | Value |
|-------|-------|
| Total files | 111 (88 Swift) |
| Total LOC | 12,200 |
| Language | Swift 100% |
| Min target | iOS 17.0, Swift 6 strict concurrency |
| Entrypoint | `App/LightwaveOSApp.swift` (`@main`) |

## Frameworks & Dependencies

| Name | Usage |
|------|-------|
| SwiftUI | All views, `@Observable`, `@Environment` |
| Foundation | Networking, JSON, data types |
| Observation | `@Observable` ViewModels (iOS 17) |
| UIKit | Audio session / haptics only |
| Network | `NWBrowser`, `NWConnection`, `NWListener` (Bonjour + UDP) |
| rive-ios | Animation runtime (SPM dependency) |

## Directory Tree

```
LightwaveOS/                          12,200 LOC
|-- App/                                  42
|   `-- LightwaveOSApp.swift
|-- Components/                        1,030
|   |-- LWSlider, BeatPulse, LWButton, StepperControl
|   |-- ConnectionDot, LWCard, MetricBar, BooleanIndicator
|-- Debug/                                38
|-- Models/                            1,512
|   |-- PaletteMetadata(214) SystemState(205) AudioMetricsFrame(181)
|   |-- ZoneConfig(174) BeatEvent(134) ZoneSegment(103)
|   `-- ColourCorrectionConfig(102) TransitionConfig(99) +6 more
|-- Network/                           1,415
|   |-- RESTClient.swift                 703
|   |-- WebSocketService.swift           340
|   |-- DeviceDiscoveryService.swift     251
|   `-- UDPStreamReceiver.swift          121
|-- Theme/                               191  (DesignTokens.swift)
|-- Utilities/                           314
|   |-- PaletteLUT(135) CatmullRom(67) Rive/(96)
|-- ViewModels/                        1,867
|   |-- AppViewModel.swift               444
|   |-- ZoneViewModel.swift              437
|   |-- AudioViewModel.swift             432
|   |-- ParametersVM(120) PaletteVM(116) EffectVM(110)
|   `-- ColourCorrectionVM(109) PresetsVM(58)
`-- Views/                             5,791
    |-- Audio/          ~1,100 (9 files)
    |-- Device/           ~264 (4 files)
    |-- Play/           ~1,000 (7 files)
    |-- Shared/            308 (PersistentStatusBar)
    |-- Tabs/              477 (PlayTab, ZonesTab, AudioTab, DeviceTab)
    |-- Zones/          ~1,250 (5 files)
    |-- PaletteSelectorView(367) ConnectionSheet(294)
    `-- EffectSelectorView(265) ContentView(89)
```

## Key Patterns

- **Architecture:** MVVM with `@Observable` ViewModels injected via `.environment()`
- **Networking:** REST (URLSession) + WebSocket (URLSessionWebSocketTask) + UDP (NWListener)
- **Discovery:** Bonjour via `NWBrowser` for `_http._tcp` services
- **Concurrency:** `actor` (WebSocketService), `@MainActor` (ViewModels), `async/await`
- **Streaming:** UDP primary for LED + audio frames, WebSocket fallback on stall
- **State:** `AppViewModel.ConnectionState` drives all connection UI
- **Navigation:** `TabView` with 4 tabs, modal `ConnectionSheet`
- **Imports:** `Network` used by `DeviceDiscoveryService` + `UDPStreamReceiver` only
- Single-target app, no internal modules, no tests, no CI/CD
