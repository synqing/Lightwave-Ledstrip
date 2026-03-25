---
abstract: "Research document for adding EdgeMixer mode controls to the iOS Play tab. Contains firmware API surface (WS commands, all 7 modes, 5 params), current Play tab insertion point (after PrimaryControlsCard), proposed UI design, and full implementation plan (3 new files, 2 modified files)."
---

# EdgeMixer Integration — iOS Play Tab Research

**Date:** 2026-03-25
**Task:** Add EdgeMixer mode controls to the Play tab, positioned just below the brightness + speed (PrimaryControlsCard).

---

## 1. Firmware EdgeMixer API Surface

### 1.1 Source

`firmware-v3/src/effects/enhancement/EdgeMixer.h` — header-only singleton, ~496 lines.
`firmware-v3/src/network/webserver/ws/WsEdgeMixerCommands.cpp` — WS handler registration.

### 1.2 What EdgeMixer Does

EdgeMixer is a 3-stage post-processor applied to Strip 2 (the second 160-LED strip) after the effect render. Strip 1 is always left untouched. It creates perceived depth through the Light Guide Plate by colour-differentiating the two strips.

Three composable stages:
- **Stage T (Temporal):** Per-frame modulation — static or RMS-gated (audio-reactive).
- **Stage C (Colour):** Per-pixel 3x3 RGB matrix transform. 5 non-mirror modes, each with a different hue rotation and desaturation blend.
- **Stage S (Spatial):** Per-pixel positional mask — uniform or centre gradient (0 at LED 76, 255 at edges).

Effective strength = `scale8(scale8(globalStrength, temporalAmount), spatialAmount)` — multiplicative.

### 1.3 Colour Mode Enum (EdgeMixerMode)

| ID | Name | API string | Description |
|----|------|-----------|-------------|
| 0 | Mirror | `mirror` | No processing. Default. Strip 2 identical to Strip 1. |
| 1 | Analogous | `analogous` | Hue shift +spread degrees on Strip 2. Spread 0-60. |
| 2 | Complementary | `complementary` | 180deg hue shift + mild desaturation (sat retain 0.851). |
| 3 | Split Complementary | `split_complementary` | +150deg near-complement, lighter desaturation (sat retain 0.902). |
| 4 | Saturation Veil | `saturation_veil` | No hue rotation. Desaturates Strip 2 by spread amount (spread 0 = none, spread 60 = near-greyscale). |
| 5 | Triadic | `triadic` | 120deg hue shift; spread controls saturation (0=full, 60=70%). |
| 6 | Tetradic | `tetradic` | 90deg hue shift; spread controls saturation (0=full, 60=70%). |

### 1.4 Spatial Mode Enum (EdgeMixerSpatial)

| ID | Name | API string | Description |
|----|------|-----------|-------------|
| 0 | Uniform | `uniform` | All pixels equal weight. Default. |
| 1 | Centre Gradient | `centre_gradient` | 0 at LED 76 (centre), 255 at edges. Creates a "frame" effect. |

### 1.5 Temporal Mode Enum (EdgeMixerTemporal)

| ID | Name | API string | Description |
|----|------|-----------|-------------|
| 0 | Static | `static` | No modulation. temporalAmount always 255. Default. |
| 1 | RMS Gate | `rms_gate` | Strength tracks smoothed audio RMS with ~55ms time constant. Has kTemporalFloor=15 (~6%) to prevent hard-kill during silence. |

### 1.6 Parameters

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| mode | uint8 | 0-6 | 0 (mirror) | Colour transform mode |
| spread | uint8 | 0-60 | 30 | Hue spread in degrees. Meaning varies per mode. |
| strength | uint8 | 0-255 | 255 | Global mix strength. 0 = no effect, 255 = full transform. |
| spatial | uint8 | 0-1 | 0 (uniform) | Spatial mask mode |
| temporal | uint8 | 0-1 | 0 (static) | Temporal modulation mode |

### 1.7 NVS Persistence

`edge_mixer.save` persists all 5 fields to NVS namespace `"edgemixer"`. NVS is auto-loaded at boot. iOS does not need to issue a save command — the firmware applies and saves separately. The iOS app should send `edge_mixer.save` after the user finishes interacting (on slider release, or immediately for picker selections).

### 1.8 WebSocket Commands

All commands go over the existing WebSocket connection via `ws.send(type:params:)`. The WS command router is registered at `registerWsEdgeMixerCommands()`.

#### `edge_mixer.get`

Request:
```json
{ "type": "edge_mixer.get" }
```

Response:
```json
{
  "type": "edge_mixer.get",
  "data": {
    "mode": 1,
    "modeName": "analogous",
    "spread": 30,
    "strength": 255,
    "spatial": 0,
    "spatialName": "uniform",
    "temporal": 0,
    "temporalName": "static"
  }
}
```

#### `edge_mixer.set`

Partial update — send only the fields you want to change. Firmware validates all, then applies all.

Request:
```json
{ "type": "edge_mixer.set", "mode": 2, "spread": 45 }
```

Response mirrors sent fields merged with unchanged current state:
```json
{
  "type": "edge_mixer.set",
  "data": {
    "mode": 2, "modeName": "complementary",
    "spread": 45, "strength": 255,
    "spatial": 0, "spatialName": "uniform",
    "temporal": 0, "temporalName": "static"
  }
}
```

Error codes: `OUT_OF_RANGE` if mode > 6, spread > 60, spatial > 1, temporal > 1. `MISSING_FIELD` if no fields provided.

#### `edge_mixer.save`

Persists current singleton state to NVS.

Request:
```json
{ "type": "edge_mixer.save" }
```

Response:
```json
{ "type": "edge_mixer.save", "data": { "saved": true } }
```

### 1.9 No REST Endpoint

EdgeMixer is **WebSocket-only**. There is no REST endpoint. All get/set/save operations go through WS. Confirmed by searching `firmware-v3/src/network/webserver/handlers/` — no EdgeMixer REST handlers exist.

---

## 2. Current iOS Play Tab Structure

### 2.1 PlayTab.swift Layout

File: `lightwave-ios-v2/LightwaveOS/Views/Tabs/PlayTab.swift`

```swift
ScrollView {
    VStack(spacing: Spacing.lg) {           // 24pt gaps
        HeroLEDPreview()                    // 120pt — live strip preview
        EffectPill()                        // Current effect name + change button
        PalettePill()                       // Current palette + change button
        PrimaryControlsCard()              // [BRIGHTNESS slider] [SPEED slider]
        ExpressionParametersCard()          // 7 collapsible params (mood, fade, hue, etc.)
        ColourCorrectionCard()             // Gamma, auto-exposure, brown guardrail, mode
    }
    .padding(.horizontal, 16)
    .padding(.vertical, Spacing.lg)
}
```

### 2.2 Insertion Point

EdgeMixer card goes **between PrimaryControlsCard and ExpressionParametersCard**. This positions it:
- Immediately below brightness + speed (primary controls)
- Before the 7-parameter expression card (which is already collapsible)
- Before colour correction (post-processing pipeline group)

The EdgeMixer conceptually belongs near colour correction (it IS a colour post-processor), but user priority dictates it sits higher — it directly affects the visible appearance of the second strip in real time and is a primary creative control, not a fine-tuning parameter.

### 2.3 No Existing EdgeMixer Code in iOS

Grep for `edgeMixer`, `edge_mixer`, `EdgeMixer` across the entire iOS project returns zero matches. This is a greenfield addition.

---

## 3. Existing Patterns to Follow

### 3.1 ViewModel Pattern

From `ColourCorrectionViewModel.swift` and `ParametersViewModel.swift`:

```swift
@MainActor
@Observable
class EdgeMixerViewModel {
    var restClient: RESTClient?     // Not used — EdgeMixer is WS-only
    // ws injected from AppViewModel
}
```

Key pattern: `@MainActor @Observable class`. No exceptions.

The ColourCorrectionViewModel uses `debouncedSave()` with 150ms sleep + task cancellation. This is the exact pattern to follow for the spread and strength sliders.

### 3.2 Debounce Pattern (from ColourCorrectionViewModel)

```swift
private var saveDebounceTask: Task<Void, Never>?

private func debouncedSend() {
    saveDebounceTask?.cancel()
    saveDebounceTask = Task {
        do {
            try await Task.sleep(nanoseconds: 150_000_000)  // 150ms
            guard !Task.isCancelled else { return }
            await sendToDevice()
        } catch is CancellationError { }
    }
}
```

### 3.3 WebSocket Send Pattern

From `WebSocketService.swift`:

```swift
// actor method — must be called with await from @MainActor context
await ws.send("edge_mixer.set", params: ["mode": 2, "spread": 45])
```

The `send` method signature:
```swift
func send(_ command: String, params: [String: Any] = [:])
```

It is a non-async `func` on an actor — callers from `@MainActor` must `Task { await ws.send(...) }`.

### 3.4 Card Pattern (from ColourCorrectionCard)

```swift
struct EdgeMixerCard: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        @Bindable var em = appVM.edgeMixer   // inject child VM via @Bindable

        LWCard {
            VStack(alignment: .leading, spacing: Spacing.md) {
                // Section header
                Text("EDGE MIXER")
                    .font(.sectionHeader)
                    .foregroundStyle(Color.lwTextTertiary)
                    .textCase(.uppercase)
                    .tracking(0.8)

                // Divider pattern
                Rectangle()
                    .fill(Color.white.opacity(0.04))
                    .frame(height: 1)
            }
        }
    }
}
```

### 3.5 Picker Pattern (from ColourCorrectionCard)

```swift
Picker("", selection: $cc.mode) {
    ForEach(CCMode.allCases) { mode in
        Text(mode.name).tag(mode)
    }
}
.pickerStyle(.segmented)
.tint(.lwGold)
```

For 7 modes a `.segmented` picker is too wide. Use a `Menu`-based picker or a custom scrollable chip row. See Section 4.2.

### 3.6 Model Pattern (from ColourCorrectionConfig)

```swift
struct EdgeMixerConfig: Codable, Sendable, Equatable {
    var mode: EdgeMixerMode         // Int-backed enum
    var spread: Int                 // 0-60
    var strength: Int               // 0-255
    var spatial: EdgeMixerSpatial
    var temporal: EdgeMixerTemporal
}
```

### 3.7 AppViewModel Wiring (from AppViewModel.swift)

The init follows this pattern for every child VM:
```swift
var edgeMixer: EdgeMixerViewModel   // declared as stored property

init() {
    // ...
    self.edgeMixer = EdgeMixerViewModel()
}

// In connectManual(), after ws is connected:
edgeMixer.ws = ws
await edgeMixer.loadConfig()
```

---

## 4. Proposed UI Design

### 4.1 Controls Required

| Control | Type | Range | Purpose |
|---------|------|-------|---------|
| Mode picker | Custom chip row (7 items) | Mirror/Analogous/Complementary/Split Comp/Sat Veil/Triadic/Tetradic | Primary mode selection |
| Spread slider | LWSlider | 0-60 | Hue spread (hidden for Mirror and Complementary modes) |
| Strength slider | LWSlider | 0-255 | Mix blend amount |
| Spatial toggle | 2-segment Picker | Uniform / Centre Gradient | Positional mask |
| Temporal toggle | 2-segment Picker | Static / RMS Gate | Audio reactivity |

### 4.2 Mode Picker Design

7 items is too many for `.segmented`. The best pattern given the existing design system:

**Option A — Horizontal scroll row of chips** (recommended):
```
[ Mirror ] [ Analogous ] [ Complementary ] [ Split Comp ] [ Sat Veil ] [ Triadic ] [ Tetradic ]
```
Scrollable horizontally. Selected chip uses `lwGold` background, unselected uses `lwCard`. Same pattern as effect tags.

**Option B — Menu picker** (compact):
```
Mode   [ Analogous ▾ ]
```
Uses SwiftUI `Menu` as a dropdown. Less discoverable but compact.

Given that the modes are a primary creative feature and users benefit from seeing all options, **Option A (chip row) is recommended**.

### 4.3 Conditional Spread Visibility

Show the Spread slider only when the current mode uses spread meaningfully:
- Analogous: show (spread = hue shift degrees)
- Saturation Veil: show (spread = desaturation amount)
- Triadic: show (spread = saturation reduction control)
- Tetradic: show (spread = saturation reduction control)
- Mirror: hide (no-op)
- Complementary: hide (fixed 180deg, fixed saturation)
- Split Complementary: hide (fixed 150deg, fixed saturation)

### 4.4 Full Card Layout

```
┌──────────────────────────────────────────────┐
│  EDGE MIXER                                  │
│                                              │
│  [Mirror] [Analogous] [Compl] [Split] ...   │  ← horizontal scroll chips
│                         ↑ scrollable         │
│  ────────────────────────────────────────    │
│  Spread           ●━━━━━━━━━━━━━━━   30°    │  ← shown when mode uses spread
│  ────────────────────────────────────────    │
│  Strength         ●━━━━━━━━━━━━━━━  255     │
│  ────────────────────────────────────────    │
│  Spatial            [Uniform | Centre Grad]  │
│  ────────────────────────────────────────    │
│  Temporal           [Static  | RMS Gate  ]   │
└──────────────────────────────────────────────┘
```

Mirror mode collapses spread and shows a subtitle: "Strip 2 mirrors Strip 1 exactly."

### 4.5 Spread Slider Label

Display the numeric value with a `°` suffix to communicate "degrees" to users:
- "Spread — 30°"

Strength shows the raw 0-255 value (consistent with brightness slider).

---

## 5. Implementation Plan

### 5.1 Files to Create

**1. `LightwaveOS/Models/EdgeMixerConfig.swift`**
- `struct EdgeMixerConfig: Codable, Sendable, Equatable`
- `enum EdgeMixerMode: Int, Codable, Sendable, CaseIterable, Identifiable` — 7 cases with `name` and `useSpread` computed properties
- `enum EdgeMixerSpatial: Int, Codable, Sendable, CaseIterable, Identifiable` — 2 cases
- `enum EdgeMixerTemporal: Int, Codable, Sendable, CaseIterable, Identifiable` — 2 cases

**2. `LightwaveOS/ViewModels/EdgeMixerViewModel.swift`**
- `@MainActor @Observable class EdgeMixerViewModel`
- Stored properties: `config: EdgeMixerConfig`, `ws: WebSocketService?`
- Per-field debounce tasks: `spreadDebounce`, `strengthDebounce` (150ms)
- Mode, spatial, temporal setters send immediately (no debounce, they are instant discrete selections) then save
- Spread/strength setters debounce 150ms then send + save
- `loadConfig()` — sends `edge_mixer.get` and awaits via the WS send mechanism

**3. `LightwaveOS/Views/Play/EdgeMixerCard.swift`**
- `struct EdgeMixerCard: View`
- `@Environment(AppViewModel.self) private var appVM`
- `@Bindable var em = appVM.edgeMixer`
- Mode chip row (horizontal `ScrollView` of `Button` chips)
- Conditional spread `LWSlider` with `animation(.easeInOut)` on show/hide
- Strength `LWSlider`
- Spatial 2-segment `Picker`
- Temporal 2-segment `Picker`
- Dividers between sections (matches `ColourCorrectionCard` pattern)

### 5.2 Files to Modify

**4. `LightwaveOS/ViewModels/AppViewModel.swift`**

Add stored property:
```swift
var edgeMixer: EdgeMixerViewModel
```

Add to `init()`:
```swift
self.edgeMixer = EdgeMixerViewModel()
```

Add to `connectManual()` after `ws` is connected and before `connectionState = .connected`:
```swift
edgeMixer.ws = ws
await edgeMixer.loadConfig()
```

Add to `disconnect()`:
```swift
edgeMixer.reset()
```

**5. `LightwaveOS/Views/Tabs/PlayTab.swift`**

Add `EdgeMixerCard()` after `PrimaryControlsCard()`:
```swift
PrimaryControlsCard()
EdgeMixerCard()          // ← insert here
ExpressionParametersCard()
ColourCorrectionCard()
```

### 5.3 WebSocket Integration Detail

EdgeMixer is WS-only. The ViewModel holds a reference to `WebSocketService` (the `actor`).

Sending a command from `@MainActor`:
```swift
Task {
    await ws.send("edge_mixer.set", params: ["mode": config.mode.rawValue])
    await ws.send("edge_mixer.save")
}
```

Loading initial state requires a round-trip. The simplest approach is to add a typed WS send helper to `WebSocketService`:

```swift
// In WebSocketService (actor)
func sendEdgeMixerGet() {
    send("edge_mixer.get")
}

func sendEdgeMixerSet(mode: Int? = nil, spread: Int? = nil,
                       strength: Int? = nil, spatial: Int? = nil,
                       temporal: Int? = nil) {
    var params: [String: Any] = [:]
    if let v = mode     { params["mode"]     = v }
    if let v = spread   { params["spread"]   = v }
    if let v = strength { params["strength"] = v }
    if let v = spatial  { params["spatial"]  = v }
    if let v = temporal { params["temporal"] = v }
    guard !params.isEmpty else { return }
    send("edge_mixer.set", params: params)
}

func sendEdgeMixerSave() {
    send("edge_mixer.save")
}
```

For the `edge_mixer.get` response, the ViewModel needs to listen on the WS event stream. The current `AppViewModel.consumeWebSocketEvents` switch handles typed `WebSocketMessageType` cases. Add:

```swift
// In WebSocketMessageType enum
case edgeMixerGet = "edge_mixer.get"
case edgeMixerSet = "edge_mixer.set"
```

And in the `consumeWebSocketEvents` switch:
```swift
case .edgeMixerGet, .edgeMixerSet:
    self.edgeMixer.handleResponse(payload.data)
```

`handleResponse` on the ViewModel parses the JSON dict and updates `config` fields.

### 5.4 WS Response Parsing

The `edge_mixer.get` response data dict contains:
```
"mode": Int, "modeName": String,
"spread": Int, "strength": Int,
"spatial": Int, "spatialName": String,
"temporal": Int, "temporalName": String
```

Parse in `EdgeMixerViewModel.handleResponse(_:)`:
```swift
func handleResponse(_ data: [String: Any]) {
    if let raw = data["mode"] as? Int, let m = EdgeMixerMode(rawValue: raw) {
        config.mode = m
    }
    if let v = data["spread"] as? Int { config.spread = v }
    if let v = data["strength"] as? Int { config.strength = v }
    if let raw = data["spatial"] as? Int, let s = EdgeMixerSpatial(rawValue: raw) {
        config.spatial = s
    }
    if let raw = data["temporal"] as? Int, let t = EdgeMixerTemporal(rawValue: raw) {
        config.temporal = t
    }
}
```

---

## 6. Constraints Checklist

- [x] `@MainActor @Observable class` for EdgeMixerViewModel
- [x] `actor` for WebSocketService — already actor, no changes needed
- [x] 150ms debounce on spread and strength sliders
- [x] Discrete selections (mode, spatial, temporal) send immediately — no debounce needed for toggles/pickers
- [x] British English in all UI strings: "Colour", "Centre Gradient", "Saturation Veil"
- [x] `[weak self]` in Task closures capturing self — ViewModel is `@Observable` class, apply where Task captures are involved
- [x] EdgeMixer is WS-only — no REST calls needed
- [x] `edge_mixer.save` sent after each change to persist to NVS
- [x] Mirror mode hides spread slider (conditional with animation)
- [x] No REST endpoint — do not add to RESTClient

---

## 7. Open Questions

1. **Initial state load timing** — `edge_mixer.get` is a WS round-trip. If the WS is not yet connected when `loadConfig()` is called, the get will be silently dropped. Consider a retry after the `connected` event fires, similar to how stream subscriptions are retried in `startStreamSubscriptions()`.

2. **WS response routing** — The current WS event dispatch in `AppViewModel` dispatches on typed `WebSocketMessageType` enum. Adding `edge_mixer.get` and `edge_mixer.set` to that enum requires also adding them to the `Event` enum or handling them inside the `status` case. The cleanest approach is adding dedicated `Event` cases.

3. **Mode chip row width** — 7 chips at ~90pt each = ~630pt total. The horizontal `ScrollView` approach works on all iPhone screen widths, but test on iPhone SE (375pt) to confirm scrollability is obvious without a hint.

4. **Save strategy** — Send `edge_mixer.save` immediately after each `edge_mixer.set`? Or only when the user leaves the tab? Immediate save (matching firmware default behaviour where NVS is updated on set) is recommended for simplicity and robustness.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:ios-dev | Created — research for EdgeMixer Play tab integration |
