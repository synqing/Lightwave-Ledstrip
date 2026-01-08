# Zone Composer Interactive UI Design
**Date:** 2026-01-09
**Platform:** Tab5.encoder (ESP32-P4, LVGL 9.3.0, 800×480 touch screen)
**Status:** Design Complete, Implementation Pending

---

## Overview

Transform Tab5's Zone Composer from display-only visualization to full interactive zone control. Users can adjust per-zone effects, palettes, speed, and brightness using touch-to-select + encoder-to-adjust paradigm.

---

## Design Goals

1. **Full zone configuration** - Zone count selection, preset loading, per-zone parameter control
2. **Touch-to-select, encoder-to-adjust** - Direct manipulation with visual feedback
3. **Mode-based encoders** - 4 parameter modes (Effect, Palette, Speed, Brightness)
4. **Real-time updates** - WebSocket commands sent immediately on value change
5. **Consistent with GLOBAL screen** - Header/footer remain identical across screens

---

## Screen Layout (800×480)

```
┌─────────────────────────────────────────────────┐
│ Header (80px)                                   │ ← Existing (status, audio metrics)
├─────────────────────────────────────────────────┤
│                                                 │
│ LED Strip Visualization (180px)                 │ ← Existing (colored zone segments)
│                                                 │
├─────────────────────────────────────────────────┤
│ Zone Count: 3                    [touch here]  │ ← NEW (30px)
│ Preset: Triple Rings             [touch here]  │ ← NEW (30px)
├─────────────────────────────────────────────────┤
│ Zone Parameter Display (140px)                  │ ← NEW: 4 zone rows (35px each)
│  [■] Zone 0  LED 65-79/80-94          Fire     │
│  [■] Zone 1  LED 20-64/95-139         Ocean    │
│  [■] Zone 2  LED 0-19/140-159         Plasma   │
│  [■] Zone 3  LED --/--                --       │
├─────────────────────────────────────────────────┤
│ [← Back] [Effect] [Palette] [Speed] [Bright]   │ ← NEW: Mode buttons (60px)
├─────────────────────────────────────────────────┤
│ Footer (20px)                                   │ ← Existing (BPM, KEY, MIC, WS)
└─────────────────────────────────────────────────┘
```

**Total Height:** 80 + 180 + 60 + 140 + 60 + 20 = **540px** (exceeds 480px!)

**CORRECTED Layout:**
- Header: 80px
- LED Strip: 160px (reduced from 180px to fit)
- Zone/Preset rows: 50px (25px each)
- Zone Parameters: 140px (35px × 4)
- Mode Buttons: 50px (reduced from 60px)
- Footer: 0px (hide on Zone Composer screen OR overlap with mode buttons)
- **Total: 480px** ✓

---

## Component Details

### 1. Mode Button Row (5 buttons × 160px each)

**Buttons:**
- `[← Back]` - Return to GLOBAL screen (distinct color: Theme::ACCENT)
- `[Effect]` - Effect selection mode
- `[Palette]` - Palette selection mode
- `[Speed]` - Speed adjustment mode (1-50)
- `[Brightness]` - Brightness adjustment mode (0-255)

**Visual States:**
- **Active mode:** Theme::ACCENT background, Theme::TEXT_BRIGHT text
- **Inactive mode:** Theme::BG_PANEL background, Theme::TEXT_DIM text
- **Touch feedback:** Brief brightness increase on tap

### 2. Zone Count & Preset Selection Rows

**Zone Count Row (25px):**
- Label: "Zone Count: 3" (Font2, Theme::TEXT_DIM)
- Touch interaction: Tap anywhere on row → becomes selected (Theme::ACCENT border)
- Encoder 0 cycles: 1 → 2 → 3 → 4 → 1 (wraps)
- WebSocket command: `{"type": "zone.count.set", "count": 3}`

**Preset Row (25px):**
- Label: "Preset: Triple Rings" (Font2, Theme::TEXT_DIM)
- Touch interaction: Tap anywhere on row → becomes selected (Theme::ACCENT border)
- Encoder 0 cycles: Unified → Dual Split → Triple → Quad → Heartbeat → Unified
- WebSocket command: `{"type": "zone.preset.load", "presetId": 2}`

**Preset Definitions:**
- 0: Unified (3 zones)
- 1: Dual Split (3 zones)
- 2: Triple Rings (3 zones)
- 3: Quad Active (4 zones)
- 4: Heartbeat Focus (3 zones)

### 3. Zone Parameter Display (4 rows × 35px)

**Row Layout (per zone):**
```
[Color] Zone 0    LED 65-79 / 80-94              Fire
[8px]  [80px]    [200px]                    [512px right-align]
```

**Elements:**
- **Color bar:** 8px vertical bar using ZONE_COLORS (0x6EE7F3, 0x22DD88, 0xFFB84D, 0x9D4EDD)
- **Zone label:** "Zone 0" (Font2, Theme::TEXT_BRIGHT)
- **LED range:** "LED 65-79 / 80-94" (Font2, Theme::TEXT_DIM)
- **Parameter value:** Depends on active mode:
  - Effect Mode: Effect name ("Fire", "Ocean", "LGP Quantum")
  - Palette Mode: Palette name ("Heat", "Ocean", "Rainbow")
  - Speed Mode: "25 / 50" (current / max)
  - Brightness Mode: "128 / 255" (current / max)

**Touch Interaction:**
- Tap parameter value → becomes selected (Theme::ACCENT border around value text)
- Only one parameter can be selected at a time
- Selection persists until another parameter is touched

**Encoder Interaction:**
- Encoder 0 adjusts Zone 0's selected parameter
- Encoder 1 adjusts Zone 1's selected parameter
- Encoder 2 adjusts Zone 2's selected parameter
- Encoder 3 adjusts Zone 3's selected parameter
- Encoders 4-7 are unused (reserved for future features)

**Visual Feedback on Encoder Turn:**
- Parameter value flashes Theme::ACCENT for 300ms
- Zone color bar brightness increases for 300ms
- Value updates immediately (no confirmation needed)

---

## Interaction Flow

### Mode Switching
1. User taps **[Effect]** button → Effect mode activates
2. Mode button highlights with Theme::ACCENT background
3. Zone parameter display updates to show effect names
4. Encoders 0-3 now control zone effects

### Parameter Adjustment
1. User taps "Fire" in Zone 0 row → "Fire" gets Theme::ACCENT border
2. User turns Encoder 0 → cycles through effects: Fire → Ocean → Plasma → ...
3. Display updates in real-time
4. WebSocket sends: `{"type": "zone.effect.set", "zoneId": 0, "effectId": 42}`

### Zone Count Change
1. User taps "Zone Count: 3" row → row gets Theme::ACCENT border
2. User turns Encoder 0 → cycles: 3 → 4 → 1 → 2 → 3
3. Display updates zone count
4. WebSocket sends: `{"type": "zone.count.set", "count": 4}`
5. LED strip visualization redraws with 4 zones
6. Zone parameter display shows 4 active rows

### Preset Loading
1. User taps "Preset: Triple Rings" row → row gets Theme::ACCENT border
2. User turns Encoder 0 → cycles through preset names
3. Display updates preset name
4. WebSocket sends: `{"type": "zone.preset.load", "presetId": 2}`
5. LED strip visualization redraws with preset layout
6. Zone parameter display updates with preset zone count

---

## State Management

### UI State Variables

```cpp
enum class ZoneParameterMode : uint8_t {
    EFFECT = 0,
    PALETTE = 1,
    SPEED = 2,
    BRIGHTNESS = 3
};

struct ZoneComposerState {
    ZoneParameterMode currentMode = ZoneParameterMode::EFFECT;

    // Selection tracking
    enum class SelectionTarget : uint8_t {
        NONE = 0,
        ZONE_COUNT = 1,
        PRESET = 2,
        ZONE_0_PARAM = 3,
        ZONE_1_PARAM = 4,
        ZONE_2_PARAM = 5,
        ZONE_3_PARAM = 6
    };
    SelectionTarget selectedTarget = SelectionTarget::NONE;

    // Current values
    uint8_t zoneCount = 3;
    uint8_t presetId = 2;  // Triple Rings

    // Zone states (from WebSocket updates)
    ZoneState zones[4];
};
```

### Encoder Routing Logic

```cpp
void handleEncoderTurn(uint8_t encoderIndex, int32_t delta) {
    if (encoderIndex > 3) return;  // Only encoders 0-3 used

    switch (_state.selectedTarget) {
        case SelectionTarget::ZONE_COUNT:
            if (encoderIndex == 0) adjustZoneCount(delta);
            break;

        case SelectionTarget::PRESET:
            if (encoderIndex == 0) adjustPreset(delta);
            break;

        case SelectionTarget::ZONE_0_PARAM:
            if (encoderIndex == 0) adjustZoneParameter(0, delta);
            break;

        case SelectionTarget::ZONE_1_PARAM:
            if (encoderIndex == 1) adjustZoneParameter(1, delta);
            break;

        case SelectionTarget::ZONE_2_PARAM:
            if (encoderIndex == 2) adjustZoneParameter(2, delta);
            break;

        case SelectionTarget::ZONE_3_PARAM:
            if (encoderIndex == 3) adjustZoneParameter(3, delta);
            break;

        default:
            break;
    }
}

void adjustZoneParameter(uint8_t zoneId, int32_t delta) {
    switch (_state.currentMode) {
        case ZoneParameterMode::EFFECT:
            adjustZoneEffect(zoneId, delta);
            break;
        case ZoneParameterMode::PALETTE:
            adjustZonePalette(zoneId, delta);
            break;
        case ZoneParameterMode::SPEED:
            adjustZoneSpeed(zoneId, delta);
            break;
        case ZoneParameterMode::BRIGHTNESS:
            adjustZoneBrightness(zoneId, delta);
            break;
    }
}
```

---

## WebSocket Commands

### Zone Parameter Commands

```json
// Set zone effect
{"type": "zone.effect.set", "zoneId": 0, "effectId": 42}

// Set zone palette
{"type": "zone.palette.set", "zoneId": 0, "paletteId": 12}

// Set zone speed
{"type": "zone.speed.set", "zoneId": 0, "speed": 35}

// Set zone brightness
{"type": "zone.brightness.set", "zoneId": 0, "brightness": 200}

// Set zone count
{"type": "zone.count.set", "count": 3}

// Load preset
{"type": "zone.preset.load", "presetId": 2}
```

### WebSocket Updates (from v2)

```json
// Zone state update
{
    "type": "zones.list",
    "enabled": true,
    "zoneCount": 3,
    "zones": [
        {
            "zoneId": 0,
            "effectId": 42,
            "effectName": "Fire",
            "paletteId": 12,
            "paletteName": "Heat",
            "speed": 25,
            "brightness": 128
        },
        // ... more zones
    ]
}
```

---

## Visual Feedback

### Selection Highlighting
- **Selected element:** 2px Theme::ACCENT border around text/value
- **Unselected elements:** No border, normal colors

### Encoder Turn Feedback
- **Parameter value:** Flash Theme::ACCENT color for 300ms
- **Zone color bar:** Increase brightness/opacity for 300ms
- **Immediate update:** No delay, no confirmation needed

### Mode Button Feedback
- **Active mode:** Theme::ACCENT background
- **Inactive modes:** Theme::BG_PANEL background
- **Touch press:** Brief brightness increase

---

## Implementation Phases

### Phase 1: UI Structure
- Add Zone Count and Preset selection rows
- Add 5-button mode selector row
- Update ZoneComposerUI layout to fit 480px height

### Phase 2: Touch Interaction
- Implement touch-to-select for all interactive elements
- Add selection state management
- Add visual feedback (accent borders)

### Phase 3: Encoder Integration
- Route encoder events based on selection state and mode
- Implement parameter cycling logic
- Add encoder turn visual feedback

### Phase 4: WebSocket Integration
- Send zone parameter commands on encoder turn
- Handle zone state updates from v2
- Sync UI state with server state

### Phase 5: Testing & Refinement
- Test all 4 modes with real hardware
- Test zone count changes (1-4 zones)
- Test preset loading (5 presets)
- Verify WebSocket command format matches v2 expectations

---

## Design Decisions

### Why Touch-to-Select + Encoder-to-Adjust?
- **Direct manipulation:** User sees exactly what they're controlling
- **Muscle memory:** Hands stay on encoders after selection
- **No modal dialogs:** Keeps UI simple and responsive
- **Real-time feedback:** Immediate visual confirmation

### Why Mode Buttons Instead of Encoder Mode Switching?
- **Visual clarity:** Current mode is always obvious at a glance
- **No accidental mode changes:** Touch is deliberate, encoders can be bumped
- **Familiar pattern:** Matches web dashboard tab navigation

### Why Direct 1:1 Encoder Mapping (Enc 0-3 → Zones 0-3)?
- **Predictable:** Always know which encoder controls which zone
- **Simple:** No dynamic remapping based on zone count
- **Scalable:** Unused encoders (4-7) reserved for future features

### Why Auto-Apply Instead of Confirmation?
- **Live performance:** Musicians need immediate feedback
- **WebSocket is cheap:** Commands are small, v2 can handle rapid updates
- **User expectation:** Physical encoders feel like direct manipulation

---

## Open Questions

1. **Footer visibility:** Hide footer on Zone Composer screen, or shrink mode buttons to 50px and keep footer visible?
2. **Encoder detents:** Should encoder turn send WebSocket command on every detent, or debounce/throttle?
3. **Zone disable:** What happens when user selects Zone 3 but only 3 zones are active? Grey out? Ignore encoder turns?

---

## Success Criteria

- [ ] User can switch between 4 parameter modes using touch buttons
- [ ] User can select any parameter by touching its value
- [ ] Encoder 0-3 adjust selected parameter for Zones 0-3
- [ ] Visual feedback (accent border) indicates selected parameter
- [ ] WebSocket commands sent immediately on encoder turn
- [ ] Zone count and preset selection work via touch + encoder
- [ ] LED strip visualization updates on zone count/preset change
- [ ] Back button returns to GLOBAL screen
- [ ] Header and existing footer remain functional

---

## Next Steps

1. Search specialist researches LVGL dashboard design patterns
2. LVGL UI/UX specialist creates detailed implementation plan
3. Implement Phase 1 (UI structure and layout)
4. Implement Phase 2 (touch interaction and selection)
5. Implement Phase 3 (encoder routing and parameter adjustment)
6. Implement Phase 4 (WebSocket integration)
7. Test on real Tab5 hardware with v2 firmware
