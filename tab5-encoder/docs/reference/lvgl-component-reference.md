---
abstract: "LVGL 9.3 component reference for Tab5 encoder UI on ESP32-P4 (1280x720). Covers display init, screen lifecycle, design system colours, font table, widget trees, layout patterns, event handling, anti-patterns, and copy-paste templates. Read before creating or modifying any Tab5 LVGL screen."
---

# Tab5 LVGL Component Reference

## 1. Architecture Overview

| Property | Value |
|----------|-------|
| Display | 1280x720 IPS capacitive touch (M5Stack Tab5) |
| SoC | ESP32-P4, dual-core |
| LVGL version | 9.3 |
| Colour depth | RGB565 (16-bit) |
| Draw buffer | 1280 x 64 lines, single buffer, PSRAM (`MALLOC_CAP_SPIRAM`) |
| Render mode | `LV_DISPLAY_RENDER_MODE_PARTIAL` |
| Display push | SPI via M5GFX (`M5.Display.pushImage`) |
| Touch input | `M5.Touch.getDetail()` mapped to `LV_INDEV_TYPE_POINTER` |
| Tick source | `millis()` via `lv_tick_set_cb` |
| Build flag | `TAB5_ENCODER_USE_LVGL=1` (legacy M5GFX path exists but inactive) |

**Render:** `LVGLBridge::update()` calls `lv_timer_handler()` in Arduino `loop()` on CPU1. No explicit FPS cap.

**Thread safety:** All `lv_*` calls MUST be on CPU1 (loopTask). NEVER from CPU0. Flush callback resets WDT around SPI transfers.

**Key files:** `lvgl_bridge.cpp` (display init, flush, touch), `DisplayUI.cpp` (widget tree, card factory), `ControlSurfaceUI.cpp` (cleanest template), `ZoneComposerUI.cpp` (user_data cleanup), `ConnectivityTab.cpp` (deferred loading), `Theme.h` (legacy RGB565 -- do not use in LVGL), `fonts/experimental_fonts.h` + `fonts/bebas_neue_fonts.h` (font declarations).

---

## 2. Screen Lifecycle

Four screens are created once in `DisplayUI::begin()` and are **never destroyed** during runtime:

```
_screen_global           = lv_obj_create(nullptr);   // UIScreen::GLOBAL
_screen_zone             = lv_obj_create(nullptr);   // UIScreen::ZONE_COMPOSER
_screen_connectivity     = lv_obj_create(nullptr);   // UIScreen::CONNECTIVITY
_screen_control_surface  = lv_obj_create(nullptr);   // UIScreen::CONTROL_SURFACE
```

**Switching:** `lv_scr_load(targetScreen)` -- instant, no animation. Previous screen stays in memory.

**Sub-UIs:** GLOBAL is built inline. Others delegate: `ZoneComposerUI` (zone screen), `ConnectivityTab` (connectivity screen), `ControlSurfaceUI` (control surface screen). Each receives the screen via `begin(lv_obj_t* parent)`.

**Adding a new screen:** (1) add `UIScreen` enum, (2) add `_screen_xxx` member, (3) `lv_obj_create(nullptr)` in `begin()`, (4) create sub-UI class, (5) instantiate + wire back button + call `begin()`, (6) add `setScreen()` case.

---

## 3. Design System Colours

All LVGL code uses `lv_color_hex(0xRRGGBB)`. The colour constants are defined as `static constexpr uint32_t` at the top of each `.cpp` file.

| Token | Hex | Usage |
|-------|-----|-------|
| `TAB5_COLOR_BG_PAGE` | `0x0A0A0B` | Screen background (near-black charcoal) |
| `TAB5_COLOR_BG_SURFACE_BASE` | `0x121214` | Card base background |
| `TAB5_COLOR_BG_SURFACE_ELEVATED` | `0x1A1A1C` | Header, footer, elevated card background |
| `TAB5_COLOR_BORDER_BASE` | `0x2A2A2E` | Default card/bar borders |
| `TAB5_COLOR_FG_PRIMARY` | `0xFFFFFF` | Primary text (white) |
| `TAB5_COLOR_FG_SECONDARY` | `0x9CA3AF` | Label text, secondary text (cool grey) |
| `TAB5_COLOR_FG_DIMMED` | `0x4B5563` | Inactive/unused slot text |
| `TAB5_COLOR_BRAND_PRIMARY` | `0xFFC700` | Brand yellow (bar indicators, active borders) |
| `TAB5_COLOR_STATUS_SUCCESS` | `0x22C55E` | Connected / success (green) |
| `TAB5_COLOR_STATUS_ERROR` | `0xEF4444` | Error / disconnect (red) |
| `TAB5_COLOR_STATUS_WARNING` | `0xF59E0B` | Warning (amber) |

**Additional semantic colours:**

| Hex | Usage |
|-----|-------|
| `0x00FF99` | Active preset border (green) |
| `0x00FF00` | Battery > 50%, zones enabled |
| `0xFF0000` | Zones disabled border, battery critical |
| `0x3B82F6` | Occupied preset slot (blue, ControlSurface) |
| `0xEF4444` | Camera mode REC active |
| `0x6B7280` | Camera mode standby |

**Rule:** Always use `lv_color_hex(0xRRGGBB)`. NEVER use `Theme.h` RGB565 values in LVGL code -- `Theme.h` is the legacy M5GFX colour system and its values are in a different format.

---

## 4. Font Table

Three font families at sizes 24/32/40/48px (Bebas Neue also has 56/64/72). Declared in `src/ui/fonts/`.

**Macro pattern:** `FAMILY_WEIGHT_SIZE` -- e.g., `RAJDHANI_MED_24`, `JETBRAINS_MONO_BOLD_32`, `BEBAS_BOLD_40`.

| Family | Role | Weights | Sizes (px) |
|--------|------|---------|------------|
| Bebas Neue | Brand/display titles | Regular (`&bebas_neue_Npx`), Bold (`BEBAS_BOLD_N`), Light (`BEBAS_LIGHT_N`) | 24, 32, 40, 48, 56, 64, 72 |
| Rajdhani | Labels, UI text, buttons | Reg (`RAJDHANI_REG_N`), Med (`RAJDHANI_MED_N`), Bold (`RAJDHANI_BOLD_N`) | 24, 32, 40, 48 |
| JetBrains Mono | Numeric values | Reg (`JETBRAINS_MONO_REG_N`), Bold (`JETBRAINS_MONO_BOLD_N`) | 24, 32, 40, 48 |

Bebas aliases: `BEBAS_LABEL_SMALL`(24), `BEBAS_LABEL_MEDIUM`(32), `BEBAS_LABEL_LARGE`(40), `BEBAS_VALUE_SMALL`(48), `BEBAS_VALUE_MEDIUM`(56), `BEBAS_VALUE_LARGE`(64), `BEBAS_HERO`(72).

**Usage convention:** Bebas Bold 40 = screen titles. Bebas 24 = header info labels. Rajdhani Med 24 = card parameter names, footer labels, back buttons. JetBrains Mono Reg 32 = gauge values. JetBrains Mono Reg 24 = FX row values. Rajdhani Bold 32 = action row values.

**Rule:** NEVER use `lv_font_default()`. Always use a project font from the tables above.

---

## 5. Widget Tree Diagrams

### GLOBAL Screen (primary dashboard)

```
_screen_global (lv_obj, bg=0x0A0A0B)
|
+-- _header (lv_obj, flex ROW, bg=0x1A1A1C, 1260x66, border=2px white, r=14)
|   +-- _header_effect_container (lv_obj, 220x24, clickable -> CONTROL_SURFACE)
|   |   +-- _header_effect (lv_label, bebas_neue_24, scroll_circular)
|   +-- _header_palette_container (lv_obj, 220x24, transparent)
|   |   +-- _header_palette (lv_label, bebas_neue_24, scroll_circular)
|   +-- title_container (lv_obj, IGNORE_LAYOUT, centred absolute)
|   |   +-- _header_title_main (lv_label, "LIGHTWAVE", bebas_neue_40, white)
|   |   +-- _header_title_os (lv_label, "OS", bebas_neue_40, 0xFFC700)
|   +-- right_spacer (lv_obj, flex_grow=1, transparent)
|   +-- _header_net_container (lv_obj, clickable -> CONNECTIVITY, flex ROW)
|   |   +-- _header_net_ssid (lv_label, bebas_neue_24)
|   |   +-- _header_net_rssi (lv_label, bebas_neue_24)
|   |   +-- _header_net_ip (lv_label, bebas_neue_24)
|   +-- _header_retry_button (lv_label, hidden, clickable)
|
+-- _gauges_container (lv_obj, GRID 8-col, transparent, 1232x125)
|   +-- _gauge_cards[0..7] (make_card, bg=0x121214)
|       +-- _gauge_labels[i] (lv_label, BEBAS_BOLD_24, param name)
|       +-- _gauge_values[i] (lv_label, JETBRAINS_MONO_REG_32, value)
|       +-- _gauge_bars[i] (lv_bar, 0-255, 90%x10px)
|
+-- _fx_row_a_container (lv_obj, GRID 8-col, transparent, 1232x78)
|   +-- _fxa_cards[0..7] (make_card)
|       +-- _fxa_labels[i] (lv_label, RAJDHANI_MED_24)
|       +-- _fxa_values[i] (lv_label, JETBRAINS_MONO_REG_24)
|       +-- _fxa_bars[i] (lv_bar, 0-255, 90%x8px)
|
+-- _fx_row_b_container (lv_obj, GRID 8-col, transparent, 1232x78)
|   +-- _fxb_cards[0..7] (same structure as Row A)
|
+-- _preset_container (lv_obj, GRID 8-col, transparent, 1232x85)
|   +-- _preset_cards[0..7] (make_card elevated)
|       +-- _preset_labels[i] (lv_label, bebas_neue_24, "P1".."P8")
|       +-- _preset_values[i] (lv_label, "--" or "E{id} P{id} {bri}")
|
+-- _action_container (lv_obj, GRID 5-col, transparent, 1232x100)
|   +-- _action_buttons[0..4] (make_card, clickable)
|       +-- _action_labels[i] (lv_label, RAJDHANI_MED_24, EVENT_BUBBLE)
|       +-- _action_values[i] (lv_label, various fonts, EVENT_BUBBLE)
|
+-- _footer (lv_obj, flex ROW space-between, bg=0x1A1A1C, 1274x66, border=2px white)
    +-- leftGroup: BPM(95px) | KEY(112px) | MIC(125px) | UPTIME(145px) -- each title+value in flex ROW
    +-- _footer_k1_group: 2x lv_btn (80x32)
    +-- rightGroup(345px): WS status label + battery label + battery bar(60x8)
```

### ZONE_COMPOSER Screen

```
_screen_zone (lv_obj, flex COLUMN, bg=0x0A0A0B)
+-- header (lv_obj, flex ROW space-between, 1240x50)
|   +-- _backButton (card, 120x44, clickable)
|   |   +-- "< BACK" (lv_label, RAJDHANI_BOLD_24, 0xFFC700)
|   +-- "ZONE COMPOSER" (lv_label, BEBAS_BOLD_40)
|   +-- _zoneEnableButton (card, 160x44, clickable)
|       +-- "ZONES: ON/OFF" (lv_label, RAJDHANI_BOLD_24)
+-- controlsRow (lv_obj, flex ROW space-evenly, 1240x80)
|   +-- _zoneCountRow (card, 280x70, clickable)
|   |   +-- "ZONES" (lv_label) + _zoneCountValueLabel (JETBRAINS_MONO_BOLD_32)
|   +-- _presetRow (card, 400x70, clickable)
|       +-- "PRESET" (lv_label) + _presetValueLabel (RAJDHANI_BOLD_32)
+-- [zone parameter grid and LED strip visualisation]
```

### CONNECTIVITY Screen

```
_screen_connectivity (lv_obj, bg=0x0A0A0B)
+-- "NETWORK SETTINGS" (lv_label, BEBAS_BOLD_40, centred)
+-- Back button (lv_btn)
+-- Status label
+-- Available Networks card + SCAN/ADD buttons
+-- Saved Networks card + CONNECT/DISCONNECT/DELETE buttons
```

### CONTROL_SURFACE Screen

```
_screen_control_surface (lv_obj, bg=0x0A0A0B)
+-- Row A container (GRID 8-col, 1232x110)
|   +-- cards[0..7] (card with label + value + bar, RAJDHANI_MED_24 / JETBRAINS_MONO_REG_24)
+-- Row B container (GRID 8-col, 1232x110)
|   +-- cards[0..5] (param cards)
|   +-- cards[6] (CAMERA card)
|   +-- cards[7] (PRESET card)
+-- Global Row container (GRID 8-col, 1232x50)
|   +-- cells[0..7] (read-only: label + value)
+-- Back button (lv_btn, 100x40)
```

---

## 6. Layout Patterns

### Grid Layout (primary pattern)

8-col equal-width grid. Descriptor arrays **MUST be `static`** (LVGL stores pointer):

```cpp
static lv_coord_t col_dsc[9] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
static lv_coord_t row_dsc[2] = {ROW_HEIGHT, LV_GRID_TEMPLATE_LAST};
lv_obj_set_layout(container, LV_LAYOUT_GRID);
lv_obj_set_grid_dsc_array(container, col_dsc, row_dsc);
lv_obj_set_style_pad_column(container, 14 /*TAB5_GRID_GAP*/, LV_PART_MAIN);
// Place: lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
```

### Flex Row (header, footer, control rows)

```cpp
lv_obj_set_layout(c, LV_LAYOUT_FLEX);
lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
```

### Card Factory

The `make_card()` pattern is duplicated in each `.cpp` file. Use this form:

```cpp
static lv_obj_t* make_card(lv_obj_t* parent, bool elevated) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card,
        lv_color_hex(elevated ? 0x1A1A1C : 0x121214), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}
```

### Transparent Container

Used for invisible grouping. Also clear scrollbar/indicator parts in flex children:

```cpp
lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
// Also for flex children: clear on LV_PART_SCROLLBAR and LV_PART_INDICATOR
```

### Centre Title Over Flex Row

To centre a title in a flex row header while having left/right content:

```cpp
lv_obj_t* title_container = lv_obj_create(header);
lv_obj_add_flag(title_container, LV_OBJ_FLAG_IGNORE_LAYOUT);  // Break out of flex
lv_obj_align(title_container, LV_ALIGN_CENTER, 0, 0);         // Absolute centre
```

---

## 7. Event Handling

**Pattern A -- Static callback + `this` via user_data:** Pass `this` as the 4th arg to `lv_obj_add_event_cb`. Recover with `lv_event_get_user_data(e)`.

```cpp
lv_obj_add_event_cb(btn, backButtonCb, LV_EVENT_CLICKED, this);
// In callback: auto* self = static_cast<MyClass*>(lv_event_get_user_data(e));
```

**Pattern B -- Lambda callback:** Same pattern, inline. Used for one-off handlers in `DisplayUI::begin()`.

```cpp
lv_obj_add_event_cb(widget, [](lv_event_t* e) {
    auto* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
    if (ui) ui->setScreen(UIScreen::CONNECTIVITY);
}, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));
```

**Pattern C -- Index via widget user_data:** For indexed buttons, store index on widget, recover `this` from event user_data. These are separate: `lv_obj_get_user_data(target)` = index, `lv_event_get_user_data(e)` = `this`.

```cpp
lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
// In callback:
uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
```

**Touch bubbling:** Child labels in clickable cards swallow events. Fix: `lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE)` + `lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE)`.

---

## 8. Anti-Patterns (MUST NOT)

| # | Anti-Pattern | Why It Breaks |
|---|-------------|---------------|
| 1 | Using `Theme.h` RGB565 in LVGL code | Theme.h is `uint16_t` RGB565 for legacy M5GFX. LVGL uses `lv_color_hex(0xRRGGBB)` -- wrong format = wrong colours. |
| 2 | Creating/destroying screens at runtime | All 4 screens persist forever. Use `lv_scr_load()` to switch. |
| 3 | Forgetting `lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE)` | Touch gestures trigger scroll behaviour on cards/containers, breaking fixed layouts. |
| 4 | Stack-local grid descriptor arrays | `lv_obj_set_grid_dsc_array()` stores a pointer. Stack arrays go stale. Use `static lv_coord_t[]`. |
| 5 | Using `lv_font_default()` | Built-in LVGL fonts not included in build. Use project fonts only. |
| 6 | Missing `esp_task_wdt_reset()` in `begin()` | Heavy widget creation exceeds 5s watchdog. Reset before/after each phase. |
| 7 | Mixing M5GFX + LVGL rendering | Same SPI bus. Concurrent writes = corruption + WDT panics. LVGL is the active path. |
| 8 | Not disabling click-through on child labels | Labels swallow touch. Fix: `clear_flag(CLICKABLE)` + `add_flag(EVENT_BUBBLE)`. |
| 9 | Memory leaks from `user_data` allocations | LVGL does NOT free `user_data`. `delete` in destructor. See ZoneComposerUI. |
| 10 | Calling `lv_*` from CPU0 | LVGL not thread-safe. Use flags/queues from CPU0; apply in `loop()` on CPU1. |
| 11 | Header flex child without `IGNORE_LAYOUT` | Centre title breaks if it participates in flex. Use `LV_OBJ_FLAG_IGNORE_LAYOUT`. |
| 12 | Adding "responsive" scaling | 1280x720 is fixed. Hardcoded dimensions are intentional. Do not add percentage scaling. |

---

## 9. Copy-Paste Templates

### New Screen (in `DisplayUI::begin()`)

```cpp
esp_task_wdt_reset();
_screen_myscreen = lv_obj_create(nullptr);
lv_obj_set_style_bg_color(_screen_myscreen, lv_color_hex(0x0A0A0B), LV_PART_MAIN);
lv_obj_set_style_pad_all(_screen_myscreen, 0, LV_PART_MAIN);
_myScreen = new MyScreenUI(_display);
_myScreen->setBackButtonCallback(onMyScreenBackButton);
_myScreen->begin(_screen_myscreen);
// + add UIScreen enum, setScreen() case, destructor cleanup (see section 2)
```

### Gauge Card (Label + Value + Bar)

```cpp
lv_obj_t* card = make_card(grid_container, false);
lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1,
                     LV_GRID_ALIGN_STRETCH, 0, 1);

lv_obj_t* label = lv_label_create(card);
lv_label_set_text(label, "PARAM");
lv_obj_set_style_text_font(label, BEBAS_BOLD_24, LV_PART_MAIN);
lv_obj_set_style_text_color(label, lv_color_hex(0x9CA3AF), LV_PART_MAIN);
lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

lv_obj_t* value = lv_label_create(card);
lv_label_set_text(value, "--");
lv_obj_set_style_text_font(value, JETBRAINS_MONO_REG_32, LV_PART_MAIN);
lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
lv_obj_align(value, LV_ALIGN_TOP_MID, 0, 30);

lv_obj_t* bar = lv_bar_create(card);
lv_bar_set_range(bar, 0, 255);
lv_bar_set_value(bar, 0, LV_ANIM_OFF);
lv_obj_set_size(bar, LV_PCT(90), 10);
lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -10);
lv_obj_set_style_bg_color(bar, lv_color_hex(0x2A2A2E), LV_PART_MAIN);
lv_obj_set_style_bg_color(bar, lv_color_hex(0xFFC700), LV_PART_INDICATOR);
lv_obj_set_style_radius(bar, 8, LV_PART_MAIN);
lv_obj_set_style_radius(bar, 8, LV_PART_INDICATOR);
```

### Button with Callback (also used for Back button)

```cpp
lv_obj_t* btn = lv_btn_create(parent);
lv_obj_set_size(btn, 120, 44);                                          // Back: 100x40
lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1C), LV_PART_MAIN);
lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
lv_obj_set_style_border_color(btn, lv_color_hex(0xFFC700), LV_PART_MAIN); // Back: 0xFFFFFF
lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);                         // Back: 10

lv_obj_t* label = lv_label_create(btn);
lv_label_set_text(label, "ACTION");                                      // Back: "BACK"
lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);      // Back: RAJDHANI_MED_24
lv_obj_set_style_text_color(label, lv_color_hex(0xFFC700), LV_PART_MAIN); // Back: 0xFFFFFF
lv_obj_center(label);

lv_obj_add_event_cb(btn, [](lv_event_t* e) {
    auto* self = static_cast<MyClass*>(lv_event_get_user_data(e));
    if (self) self->onAction();
}, LV_EVENT_CLICKED, this);
```

### Action Row Button (Clickable Card with Label + Value)

Same as gauge card, but add `LV_OBJ_FLAG_CLICKABLE` to card, and on both child labels: `clear_flag(CLICKABLE)` + `add_flag(EVENT_BUBBLE)`. Store index with `lv_obj_set_user_data(btn, (void*)(uintptr_t)col)`. See Pattern C in section 7.

---

## 10. Watchdog and Memory

### Watchdog Safety

The ESP32-P4 task watchdog fires after 5 seconds of CPU starvation. LVGL widget creation in `begin()` can exceed this.

```cpp
void MyScreenUI::begin(lv_obj_t* parent) {
    esp_task_wdt_reset();   // Before heavy widget creation

    // ... create many widgets ...

    esp_task_wdt_reset();   // After widget creation, before lv_scr_load
}
```

Call `esp_task_wdt_reset()` at the boundary of each major section (header, grid, footer). The flush callback in `lvgl_bridge.cpp` also resets the WDT around large SPI transfers.

### user_data Cleanup

If you allocate heap memory and store it via `lv_obj_set_user_data()`, you own the lifetime. LVGL does not free it. Clean up in the destructor:

```cpp
MyScreenUI::~MyScreenUI() {
    for (uint8_t i = 0; i < WIDGET_COUNT; i++) {
        if (_widgets[i]) {
            auto* data = static_cast<MyData*>(lv_obj_get_user_data(_widgets[i]));
            if (data) delete data;
        }
    }
}
```

### No Dynamic Allocation in loop()

The `loop()` / tick methods should only update existing widget properties (`lv_label_set_text`, `lv_bar_set_value`, `lv_obj_set_style_*`). Never create or destroy widgets, allocate memory, or call `new`/`delete` in the render loop.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:lvgl-specialist | Created. Extracted patterns from DisplayUI, ControlSurfaceUI, ZoneComposerUI, ConnectivityTab, and lvgl_bridge source files. |
