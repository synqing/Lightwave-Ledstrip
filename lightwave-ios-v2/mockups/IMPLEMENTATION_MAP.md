# LightwaveOS Redesign — SwiftUI Implementation Map

This document maps mockup sections to SwiftUI components and specifies the changes needed for implementation.

---

## 1. Design Tokens (DesignTokens.swift)

| Token | Compact | Balanced | Current |
|-------|---------|----------|---------|
| Card padding | 4–6pt | 10–14pt | — |
| Section spacing | 3pt | 8pt | 4pt (RootView) |
| Slider track height | 4pt | 6pt | — |
| Slider thumb size | 14pt | 18pt | — |
| Label font size | 7–8pt | 9–10pt | 9pt |
| Value font size | 10–11pt | 12–13pt | 13pt |

**Action:** Add a layout mode (compact/balanced) or apply the chosen design’s values directly to existing tokens.

---

## 2. Status Bar (StatusBarView.swift)

**Mockup section:** Top status row with connection, BPM, effect.

**Current:** Uses `ConnectionDot`, status text, BPM with `BeatPulse`, effect name.

**Changes:**
- Align layout with mockup (dot + text left, BPM centre, effect right)
- Apply font sizes from chosen design (9–11pt)
- No structural changes to connection flow

---

## 3. Effect + Palette Dropdowns (RootView + LWDropdown)

**Mockup section:** Two side-by-side dropdowns.

**Current:** `LWDropdown` with title, selected text, subtitle, prev/next.

**Changes:**
- Adjust font sizes and padding to match mockup
- Ensure `currentEffectName` and `currentPaletteName` are populated from API/WS
- Fix EffectViewModel to pass `audioReactive` for filtering

---

## 4. Main Sliders (SliderCardView + LWSlider)

**Mockup section:** Brightness + Speed side by side.

**Current:** `LWSlider` for both.

**Changes:**
- Reduce slider track/thumb size for compact mode
- Apply spacing from chosen design
- Keep debounce and pending logic unchanged

---

## 5. Advanced Parameters (AdvancedParamsView)

**Mockup section:** Collapsible block with 7 sliders.

**Current:** Collapsible `LWCard` with `ParameterSlider` for each param.

**Changes:**
- Adjust padding and font sizes
- Keep collapse animation and parameter binding

---

## 6. Zone Mode (ZoneModeView + LEDStripView)

**Mockup section:** Toggle, **Zones count selector** (1/2/3/4), **Preset selector** (Unified, Dual Split, Triple Rings, Quad Active, Heartbeat Focus, Custom), LED strip, per-zone cards with effect/palette/blend + speed.

**Zone count and preset (from dashboard/firmware):**
- **Zones** dropdown: 1 Zone, 2 Zones, 3 Zones, 4 Zones — drives layout and how many zone cards render
- **Preset** dropdown: Custom | Unified (0) | Dual Split (1) | Triple Rings (2) | Quad Active (3) | Heartbeat Focus (4) — loads firmware presets via `zones.loadPreset` or equivalent REST
- Firmware GET /api/v1/zones returns `presets: [{id, name}, ...]`; use for Preset dropdown options

**Current:**
- Toggle and `LEDStripView` exist
- `segments` is never populated (see API_AUDIT.md)
- Per-zone: only speed slider; effect/palette/blend not wired

**Changes (critical):**

1. **ZoneViewModel.loadZones():**
   - Decode `segments` from `ZonesResponse`
   - Map to `ZoneSegment` and set `self.segments`
   - Decode `effectName`, `blendModeName` in zone model

2. **ZonesResponse (RESTClient):**
   - Add `Segment` with `zoneId`, `s1LeftStart`, `s1LeftEnd`, `s1RightStart`, `s1RightEnd`, `totalLeds`
   - Add `segments: [Segment]` to `ZonesData`
   - Add `effectName: String?`, `blendModeName: String?` to `Zone`

3. **ZoneConfig model:**
   - Add `effectName: String?`, `blendModeName: String?` for display

4. **ZoneControlView (individual zone parameter layout):**
   - Each zone in its own card with distinct coloured border (zone 0 = cyan, 1 = green, etc.)
   - Zone header: zone index + LED count and range (e.g. "80 LEDs · L[40-79] R[80-119]")
   - Three stacked parameter rows per zone:
     - **Effect**: dropdown/sheet with current effect name
     - **Palette**: dropdown/sheet with current palette name
     - **Blend Mode**: dropdown with current blend mode name
   - Speed slider below
   - All parameters exposed individually per zone (no shared/inline controls)

5. **LEDStripView:**
   - Confirm it receives `segments` from `ZoneViewModel` (after loadZones fix)
   - Optional: add centre marker if not present

---

## 7. Debug Log (DebugLogView)

**Mockup section:** Collapsible log with entries.

**Current:** Collapsible `LWCard`, `DebugLogEntry` with timestamp/category/message.

**Changes:**
- Adjust font sizes (7–9pt) and padding
- Keep expand/collapse and log content logic

---

## 8. Connection Flow (Unchanged)

- `AppViewModel.connectManual()` — no changes
- `AppViewModel.setupWebSocketCallbacks()` — no changes
- `WebSocketManager` — no changes
- `DeviceDiscovery` — no changes
- `ConnectionSheet` — no changes (except optional visual tweaks)

---

## 9. Effect Filtering (EffectViewModel + RESTClient)

**From API_AUDIT.md:**

1. Add to `EffectsResponse.Effect`: `categoryId: Int?`, `isAudioReactive: Bool?`
2. Add CodingKeys: `isAudioReactive`, `categoryId`
3. In `loadEffects()`, map `effect.isAudioReactive` and `effect.categoryId` into `EffectMetadata`
4. Ensure `EffectMetadata` CodingKeys map `isAudioReactive` correctly (firmware sends `isAudioReactive`)

---

## 10. Palette Source (PaletteViewModel)

**From API_AUDIT.md:**

1. Call `getPalettes(limit: 100)` when loading (e.g. on connect or when opening palette selector)
2. Replace or merge with `PaletteStore.all` using API `id`/`name`/`category`
3. Fall back to hardcoded list if the API call fails

---

## Implementation Order

1. API/model fixes (EffectsResponse, ZonesResponse, ZoneConfig) — unblocks features
2. ZoneViewModel segments + ZoneControlView effect/palette/blend UI
3. PaletteViewModel API loading
4. EffectViewModel audioReactive mapping
5. Design token and layout updates (padding, fonts, slider sizes)
6. Visual polish pass
