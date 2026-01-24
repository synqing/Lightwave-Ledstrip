# Zone Enable Toggle Implementation Plan

## Problem Statement

The Zone Composer UI on Tab5 lacks a way to enable/disable zone mode on the LightwaveOS controller. Users can select zones, change parameters, and adjust presets, but zones remain disabled at the firmware level because there's no UI element to send the `sendZoneEnable(true)` command.

From serial logs:
- Zone presets are being selected: `[ZoneComposer] Preset → Dual Split (3 zones)`
- Zone Count is being touched and selected: `[ZoneComposer] Selected Zone Count (Encoder 0)`
- But no zone enable/disable commands are being sent

## Current State

### Existing Infrastructure (Already Implemented)
1. ✅ `WebSocketClient::sendZoneEnable(bool enable)` - WebSocket command exists
2. ✅ `ButtonHandler::toggleZoneMode()` - Toggle function exists
3. ✅ `ZoneComposerUI::_zonesEnabled` (line 304) - State tracking exists
4. ✅ ButtonHandler integration in ZoneComposerUI via `setButtonHandler()`

### Missing Component
❌ **No UI button to trigger zone enable/disable**

The current header has:
- Left: Back button (works)
- Center: "ZONE COMPOSER" title
- Right: Empty spacer (transparent 120x44px placeholder)

## Proposed Solution

Replace the right-side spacer with an "ENABLE ZONES" toggle button.

### UI Design

```
┌──────────────────────────────────────────────────────────────┐
│  [< BACK]     ZONE COMPOSER          [ZONES: OFF]            │
│                                       └─ Toggle button        │
└──────────────────────────────────────────────────────────────┘
```

**Visual States:**
- **DISABLED** (default): `[ZONES: OFF]` - Red border, white text
- **ENABLED**: `[ZONES: ON]` - Green border, green text

### Implementation Steps

#### 1. Add Widget References (ZoneComposerUI.h)

**Location:** After line 275 (`lv_obj_t* _backButton = nullptr;`)

```cpp
lv_obj_t* _backButton = nullptr;
lv_obj_t* _zoneEnableButton = nullptr;  // NEW: Toggle button container
lv_obj_t* _zoneEnableLabel = nullptr;   // NEW: "ZONES: ON/OFF" label
```

#### 2. Add Static Callback Declaration (ZoneComposerUI.h)

**Location:** With other callbacks (around line 1394 in .cpp, declare in .h private section)

```cpp
static void zoneEnableButtonCb(lv_event_t* e);
```

#### 3. Replace Spacer with Toggle Button (ZoneComposerUI.cpp)

**Location:** Lines 1103-1107 (replace spacer creation)

**OLD CODE:**
```cpp
// Spacer (right side to balance)
lv_obj_t* spacer = lv_obj_create(header);
lv_obj_set_size(spacer, 120, 44);
lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, LV_PART_MAIN);
lv_obj_set_style_border_width(spacer, 0, LV_PART_MAIN);
```

**NEW CODE:**
```cpp
// Zone Enable Toggle Button (right side)
_zoneEnableButton = make_zone_card(header, true);
lv_obj_set_size(_zoneEnableButton, 160, 44);
lv_obj_set_style_border_width(_zoneEnableButton, 2, LV_PART_MAIN);
lv_obj_set_style_border_color(_zoneEnableButton,
                              lv_color_hex(_zonesEnabled ? 0x00FF00 : 0xFF0000),
                              LV_PART_MAIN);
lv_obj_add_flag(_zoneEnableButton, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_event_cb(_zoneEnableButton, zoneEnableButtonCb, LV_EVENT_CLICKED, this);

_zoneEnableLabel = lv_label_create(_zoneEnableButton);
lv_label_set_text(_zoneEnableLabel, _zonesEnabled ? "ZONES: ON" : "ZONES: OFF");
lv_obj_set_style_text_font(_zoneEnableLabel, RAJDHANI_BOLD_20, LV_PART_MAIN);
lv_obj_set_style_text_color(_zoneEnableLabel,
                            lv_color_hex(_zonesEnabled ? 0x00FF00 : 0xFFFFFF),
                            LV_PART_MAIN);
lv_obj_center(_zoneEnableLabel);
```

#### 4. Implement Callback (ZoneComposerUI.cpp)

**Location:** After `backButtonCb` (around line 1407)

```cpp
void ZoneComposerUI::zoneEnableButtonCb(lv_event_t* e) {
    ZoneComposerUI* ui = (ZoneComposerUI*)lv_event_get_user_data(e);

    // Toggle state
    ui->_zonesEnabled = !ui->_zonesEnabled;

    Serial.printf("[ZoneComposer] Zones %s\n", ui->_zonesEnabled ? "ENABLED" : "DISABLED");

    // Update visual state
    if (ui->_zoneEnableButton) {
        lv_obj_set_style_border_color(ui->_zoneEnableButton,
                                      lv_color_hex(ui->_zonesEnabled ? 0x00FF00 : 0xFF0000),
                                      LV_PART_MAIN);
    }

    if (ui->_zoneEnableLabel) {
        lv_label_set_text(ui->_zoneEnableLabel, ui->_zonesEnabled ? "ZONES: ON" : "ZONES: OFF");
        lv_obj_set_style_text_color(ui->_zoneEnableLabel,
                                    lv_color_hex(ui->_zonesEnabled ? 0x00FF00 : 0xFFFFFF),
                                    LV_PART_MAIN);
    }

    // Send WebSocket command to LightwaveOS v2 firmware
    if (ui->_wsClient && ui->_wsClient->isConnected()) {
        ui->_wsClient->sendZoneEnable(ui->_zonesEnabled);
    }
}
```

### Expected Behavior After Fix

1. **Initial State:** Button shows "ZONES: OFF" with red border
2. **User taps button:**
   - State toggles to enabled
   - Button updates to "ZONES: ON" with green border/text
   - WebSocket sends: `{"cmd":"zones.enable","enable":true}`
   - Serial logs: `[ZoneComposer] Zones ENABLED`
3. **Main controller receives command:**
   - Enables zone mode
   - Zone parameter changes (effect, palette, speed, brightness) now take effect
4. **User taps button again:**
   - State toggles to disabled
   - Button updates to "ZONES: OFF" with red border
   - WebSocket sends: `{"cmd":"zones.enable","enable":false}`
   - Main controller disables zone mode (returns to global effect)

### Integration with Existing Code

**No breaking changes required:**
- ButtonHandler already has `toggleZoneMode()` but we bypass it (direct WebSocket send)
- Preset loading doesn't need changes (presets work regardless of enable state)
- Encoder routing already works (fixed in previous session)
- Touch callbacks already functional

### Color Scheme

Using Tab5 theme colors:
- **Red (disabled):** `0xFF0000` - clear visual indicator zones are off
- **Green (enabled):** `0x00FF00` - matches "active" state in other UIs
- **White text:** `0xFFFFFF` when disabled
- **Green text:** `0x00FF00` when enabled (matches border)

### Testing Plan

1. **Boot test:** Button shows "ZONES: OFF" (red)
2. **Enable test:** Tap button → turns green → serial shows enable command
3. **Parameter test:** With zones enabled, adjust zone parameters → verify WebSocket sends zone commands
4. **Disable test:** Tap button again → turns red → serial shows disable command
5. **Persistence test:** Zones should stay in last state across screen switches

### Files to Modify

1. `firmware/Tab5.encoder/src/ui/ZoneComposerUI.h` - Add widget references
2. `firmware/Tab5.encoder/src/ui/ZoneComposerUI.cpp` - Replace spacer, add callback

### Estimated Changes

- Lines added: ~35
- Lines modified: ~5 (spacer replacement)
- Risk level: **LOW** (isolated change, no breaking dependencies)

## Alternative Considered

**Option B:** Add toggle to controls row (below header)
- **Pros:** More space, clearer separation
- **Cons:** Takes up vertical space, less immediate visibility
- **Verdict:** Header placement is better for quick access

## Next Steps

1. Review this plan for approval
2. Implement changes in ZoneComposerUI.h and ZoneComposerUI.cpp
3. Build and upload to Tab5 device
4. Test enable/disable functionality
5. Verify zone parameters work when enabled
