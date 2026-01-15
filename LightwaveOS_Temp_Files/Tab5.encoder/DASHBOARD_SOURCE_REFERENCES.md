# Tab5.encoder Dashboard LVGL Implementation Reference

This document provides concrete source code references and implementation patterns for the LightwaveOS Tab5.encoder LVGL 9.3.0 dashboard. Use these references when generating mockups to ensure designs are implementable within LVGL constraints.

---

## LVGL Configuration (`src/ui/lv_conf.h`)

### Key Configuration Settings

```cpp
// Memory Configuration
#define LV_MEM_CUSTOM 1
#define LV_MEM_SIZE (200 * 1024U)  // 200KB dedicated to LVGL
#define LV_COLOR_DEPTH 16           // RGB565 color depth

// Display Settings
#define LV_DISP_DEF_REFR_PERIOD 16  // 16ms refresh (~60fps)
#define LV_COLOR_16_SWAP 0          // Don't swap bytes (M5GFX handles BGR565)

// Enabled Widgets (Critical - Only these are available!)
#define LV_USE_LABEL 1      // Text labels
#define LV_USE_LINE 1       // Lines
#define LV_USE_BAR 1        // Progress bars
#define LV_USE_FLEX 1       // Flexbox layouts
#define LV_USE_GRID 1       // Grid layouts
#define LV_USE_ANIM 1       // Basic animations

// DISABLED Widgets (Cannot use these!)
#define LV_USE_BTN 0        // Buttons disabled - use clickable labels instead
#define LV_USE_ARC 0        // Arc/radial gauges - NOT available
#define LV_USE_SLIDER 0     // Sliders - NOT available
#define LV_USE_METER 0      // Meters - NOT available
#define LV_USE_CHART 0      // Charts - NOT available

// Theme
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 1

// Drawing Engine
#define LV_USE_DRAW_SW 1            // Software rendering only
#define LV_DRAW_SW_COMPLEX 1        // Enable complex drawing
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE  // No assembly acceleration
```

### Font Configuration

**Default Font**: Montserrat 14px (only size enabled)
**Custom Fonts**: Embedded Bebas Neue, Rajdhani, JetBrains Mono (see font files)

---

## LVGL Initialization (`src/ui/lvgl_bridge.cpp`)

### Display Setup

```cpp
// Screen dimensions (from M5Stack Tab5)
static uint16_t s_screenWidth = 1280;
static uint16_t s_screenHeight = 720;
static constexpr uint16_t kBufferLines = 64;  // Partial rendering buffer

// Buffer allocation (PSRAM)
const size_t bufSizeBytes = s_screenWidth * kBufferLines * sizeof(uint16_t);
gDrawBuf = heap_caps_malloc(bufSizeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

// Display creation
gDisplay = lv_display_create(s_screenWidth, s_screenHeight);
const uint32_t bufPxCnt = s_screenWidth * kBufferLines;
lv_display_set_buffers(gDisplay, gDrawBuf, nullptr, bufPxCnt, 
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```

### Touch Input Setup

```cpp
gTouchIndev = lv_indev_create();
lv_indev_set_type(gTouchIndev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(gTouchIndev, touch_read_cb);

// Touch callback reads from M5.Touch
static void touch_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    auto t = M5.Touch.getDetail();
    if (t.isPressed()) {
        data->point.x = t.x;
        data->point.y = t.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
```

### Display Flush

```cpp
static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    
    // Push to M5GFX display (handles BGR565 conversion)
    M5.Display.startWrite();
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t*)px_map);
    M5.Display.endWrite();
    
    lv_display_flush_ready(disp);
}
```

---

## Widget Creation Patterns (`src/ui/DisplayUI.cpp`)

### Screen Structure

```cpp
// Main screen creation
_screen_global = lv_obj_create(nullptr);
lv_obj_set_style_bg_color(_screen_global, lv_color_hex(TAB5_COLOR_BG_PAGE), LV_PART_MAIN);
lv_obj_set_style_pad_all(_screen_global, 0, LV_PART_MAIN);

// Load screen
lv_scr_load(_screen_global);
```

### Card/Container Pattern

```cpp
static lv_obj_t* make_card(lv_obj_t* parent, bool elevated) {
    lv_obj_t* card = lv_obj_create(parent);
    
    // Background color (base vs elevated)
    lv_obj_set_style_bg_color(card,
        lv_color_hex(elevated ? TAB5_COLOR_BG_SURFACE_ELEVATED 
                               : TAB5_COLOR_BG_SURFACE_BASE),
        LV_PART_MAIN);
    
    // Border: 2px white
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);  // 14px rounded corners
    
    // Padding
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    
    // Disable scrolling (cards don't scroll)
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    return card;
}
```

### Grid Layout Pattern

```cpp
// 8-column grid for parameter gauges
_gauges_container = lv_obj_create(_screen_global);
lv_obj_set_size(_gauges_container, 1280 - 2 * TAB5_GRID_MARGIN, 125);
lv_obj_align(_gauges_container, LV_ALIGN_TOP_MID, 0, TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN);
lv_obj_set_layout(_gauges_container, LV_LAYOUT_GRID);

// Grid column descriptors (8 equal columns)
static lv_coord_t col_dsc[9] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST
};

// Grid row descriptor (single row)
static lv_coord_t row_dsc[2] = {125, LV_GRID_TEMPLATE_LAST};

lv_obj_set_grid_dsc_array(_gauges_container, col_dsc, row_dsc);
lv_obj_set_style_pad_column(_gauges_container, TAB5_GRID_GAP, LV_PART_MAIN);

// Position card in grid
lv_obj_set_grid_cell(_gauge_cards[i], 
    LV_GRID_ALIGN_STRETCH, i, 1,  // Column i, span 1
    LV_GRID_ALIGN_STRETCH, 0, 1); // Row 0, span 1
```

### Flex Layout Pattern

```cpp
// Header with flexbox layout
_header = lv_obj_create(_screen_global);
lv_obj_set_layout(_header, LV_LAYOUT_FLEX);
lv_obj_set_flex_flow(_header, LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(_header, 
    LV_FLEX_ALIGN_START,      // Main axis alignment
    LV_FLEX_ALIGN_CENTER,     // Cross axis alignment
    LV_FLEX_ALIGN_CENTER);    // Track alignment

// Spacer to push items to edges
lv_obj_t* spacer = lv_obj_create(_header);
lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, LV_PART_MAIN);
lv_obj_set_flex_grow(spacer, 1);  // Takes remaining space
```

### Label Pattern (Typography)

```cpp
// Brand title: "LIGHTWAVE"
_header_title_main = lv_label_create(_header);
lv_label_set_text(_header_title_main, "LIGHTWAVE");
lv_obj_set_style_text_font(_header_title_main, &bebas_neue_40px, LV_PART_MAIN);
lv_obj_set_style_text_color(_header_title_main, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
lv_obj_set_style_text_align(_header_title_main, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

// Accent text: "OS" in brand color
_header_title_os = lv_label_create(_header);
lv_label_set_text(_header_title_os, "OS");
lv_obj_set_style_text_font(_header_title_os, &bebas_neue_40px, LV_PART_MAIN);
lv_obj_set_style_text_color(_header_title_os, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);

// Long text with scrolling (effect/palette names)
_header_effect = lv_label_create(_header_effect_container);
lv_label_set_text(_header_effect, "--");
lv_label_set_long_mode(_header_effect, LV_LABEL_LONG_SCROLL_CIRCULAR);
lv_obj_set_width(_header_effect, 300);
```

### Progress Bar Pattern

```cpp
// Horizontal progress bar for parameter values
_gauge_bars[i] = lv_bar_create(_gauge_cards[i]);
lv_bar_set_range(_gauge_bars[i], 0, 255);
lv_bar_set_value(_gauge_bars[i], 0, LV_ANIM_OFF);  // No animation for performance
lv_obj_set_size(_gauge_bars[i], LV_PCT(90), 10);   // 90% width, 10px height

// Background (dark gray)
lv_obj_set_style_bg_color(_gauge_bars[i], 
    lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_MAIN);

// Indicator (brand yellow)
lv_obj_set_style_bg_color(_gauge_bars[i], 
    lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_INDICATOR);

// Positioning
lv_obj_align(_gauge_bars[i], LV_ALIGN_BOTTOM_MID, 0, -10);
```

### Clickable Label Pattern (Buttons)

```cpp
// Since LV_USE_BTN is disabled, use clickable labels for buttons
_action_buttons[i] = make_card(_action_container, false);
lv_obj_add_flag(_action_buttons[i], LV_OBJ_FLAG_CLICKABLE);
lv_obj_clear_flag(_action_buttons[i], LV_OBJ_FLAG_SCROLLABLE);

// Store index as user data
lv_obj_set_user_data(_action_buttons[i], 
    reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

// Event handler for clicks
lv_obj_add_event_cb(_action_buttons[i], [](lv_event_t* e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint8_t index = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
    
    if (ui && ui->_action_callback) {
        ui->_action_callback(index);
    }
}, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));

// Labels inside button (must not capture touch events)
lv_obj_clear_flag(_action_labels[i], LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_flag(_action_labels[i], LV_OBJ_FLAG_EVENT_BUBBLE);  // Bubble to parent
```

### Dynamic Border Color (State Feedback)

```cpp
// Parameter gauge highlight when encoder adjusted
lv_obj_set_style_border_color(
    _gauge_cards[displayPos],
    lv_color_hex(highlight ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
    LV_PART_MAIN);

// Preset slot border color based on state
if (isActive) {
    lv_obj_set_style_border_color(_preset_cards[i], 
        lv_color_hex(0x00FF99), LV_PART_MAIN);  // Green = active
} else if (occupied) {
    lv_obj_set_style_border_color(_preset_cards[i], 
        lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);  // Yellow = saved
} else {
    lv_obj_set_style_border_color(_preset_cards[i], 
        lv_color_hex(0xFFFFFF), LV_PART_MAIN);  // White = empty
}
```

### Animation Pattern (Feedback)

```cpp
// Preset save/recall/delete feedback (600ms flash)
void DisplayUI::showPresetSaveFeedback(uint8_t slot) {
    if (slot >= 8 || !_preset_cards[slot]) return;
    
    // Store feedback end time
    _feedback_until_ms[slot] = millis() + 600;
    _feedback_color_hex[slot] = 0xFFE066;  // Light yellow
    
    // Change border color
    lv_obj_set_style_border_color(_preset_cards[slot], 
        lv_color_hex(_feedback_color_hex[slot]), LV_PART_MAIN);
}

// In loop(), check and restore after timeout
void DisplayUI::loop() {
    const uint32_t now = millis();
    for (int i = 0; i < 8; i++) {
        if (_feedback_until_ms[i] != 0 && now >= _feedback_until_ms[i]) {
            _feedback_until_ms[i] = 0;
            // Restore appropriate border color based on state
            // ...
        }
    }
}
```

---

## Color Constants (From `src/ui/DisplayUI.cpp`)

```cpp
// Color palette (32-bit hex values, converted to RGB565 by LVGL)
static constexpr uint32_t TAB5_COLOR_BG_PAGE = 0x0A0A0B;              // Very dark gray
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_BASE = 0x121214;      // Dark gray
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_ELEVATED = 0x1A1A1C;  // Lighter dark gray
static constexpr uint32_t TAB5_COLOR_BORDER_BASE = 0x2A2A2E;          // Medium gray
static constexpr uint32_t TAB5_COLOR_FG_PRIMARY = 0xFFFFFF;           // White
static constexpr uint32_t TAB5_COLOR_FG_SECONDARY = 0x9CA3AF;         // Gray
static constexpr uint32_t TAB5_COLOR_BRAND_PRIMARY = 0xFFC700;        // Yellow/Gold

// Status colors
static constexpr uint32_t STATUS_GREEN = 0x00FF00;    // Green
static constexpr uint32_t STATUS_RED = 0xFF0000;      // Red
static constexpr uint32_t STATUS_YELLOW = 0xFFC700;   // Yellow (same as brand)

// Preset feedback colors
static constexpr uint32_t PRESET_ACTIVE = 0x00FF99;   // Green (active)
static constexpr uint32_t PRESET_SAVING = 0xFFE066;   // Light yellow (saving)
static constexpr uint32_t PRESET_DELETING = 0xFF3355; // Red (deleting)
```

**Note**: RGB565 has limited color precision (5 bits red, 6 bits green, 5 bits blue). Some colors may appear slightly different when converted from 24-bit hex to RGB565.

---

## Typography System

### Available Fonts

```cpp
// Bebas Neue (Brand font) - Available sizes
#include "fonts/bebas_neue_fonts.h"
&bebas_neue_24px   // Labels
&bebas_neue_32px   // Medium labels
&bebas_neue_40px   // Headings (branding)
&bebas_neue_48px   // Large headings
&bebas_neue_56px   // Hero text
&bebas_neue_64px   // Extra large
&bebas_neue_72px   // Maximum size

// Bebas Neue Bold
#include "fonts/experimental_fonts.h"
BEBAS_BOLD_24      // Bold labels (parameter names)

// Rajdhani (Secondary font)
RAJDHANI_MED_24    // Footer labels, action button labels
RAJDHANI_BOLD_32   // Action button values (text)

// JetBrains Mono (Monospace)
JETBRAINS_MONO_REG_32   // Parameter numeric values
JETBRAINS_MONO_BOLD_32  // GAMMA numeric value
```

### Font Usage Pattern

```cpp
// Parameter label (secondary text)
lv_obj_set_style_text_font(_gauge_labels[i], BEBAS_BOLD_24, LV_PART_MAIN);
lv_obj_set_style_text_color(_gauge_labels[i], 
    lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);

// Parameter value (primary numeric)
lv_obj_set_style_text_font(_gauge_values[i], JETBRAINS_MONO_REG_32, LV_PART_MAIN);
lv_obj_set_style_text_color(_gauge_values[i], 
    lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);

// Footer metrics (secondary text, smaller)
lv_obj_set_style_text_font(_footer_bpm, RAJDHANI_MED_24, LV_PART_MAIN);
lv_obj_set_style_text_color(_footer_bpm, 
    lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
```

---

## Layout Constants (From `src/ui/DisplayUI.cpp`)

```cpp
// Screen dimensions
static constexpr int32_t SCREEN_WIDTH = 1280;
static constexpr int32_t SCREEN_HEIGHT = 720;

// Component dimensions
static constexpr int32_t TAB5_STATUSBAR_HEIGHT = 66;   // Header/footer height
static constexpr int32_t TAB5_GRID_GAP = 14;           // Gap between cards
static constexpr int32_t TAB5_GRID_MARGIN = 24;        // Side margins

// Row heights
static constexpr int32_t GAUGE_ROW_HEIGHT = 125;       // Parameter gauge row
static constexpr int32_t PRESET_ROW_HEIGHT = 85;       // Preset slot row
static constexpr int32_t ACTION_ROW_HEIGHT = 100;      // Action button row

// Positioning calculations
// Header: y = 7 (top margin)
// Gauges: y = TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN
// Presets: y = TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN + GAUGE_ROW_HEIGHT + TAB5_GRID_GAP
// Actions: y = TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN + GAUGE_ROW_HEIGHT + TAB5_GRID_GAP + PRESET_ROW_HEIGHT + TAB5_GRID_GAP
// Footer: y = SCREEN_HEIGHT - TAB5_STATUSBAR_HEIGHT - 3 (bottom margin)
```

---

## Style Application Pattern

```cpp
// Complete style setup for a card
lv_obj_set_style_bg_color(card, lv_color_hex(color), LV_PART_MAIN);
lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);  // 100% opacity
lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
lv_obj_set_style_border_color(card, lv_color_hex(borderColor), LV_PART_MAIN);
lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
lv_obj_set_style_pad_left(card, 12, LV_PART_MAIN);   // Override all with specific
lv_obj_set_style_pad_right(card, 12, LV_PART_MAIN);
lv_obj_set_style_pad_top(card, 8, LV_PART_MAIN);
lv_obj_set_style_pad_bottom(card, 8, LV_PART_MAIN);

// Clear flags that might interfere
lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);  // Unless it's a button

// Alignment (within parent)
lv_obj_align(card, LV_ALIGN_TOP_MID, 0, yOffset);
lv_obj_align(card, LV_ALIGN_CENTER, xOffset, yOffset);
lv_obj_align(card, LV_ALIGN_BOTTOM_LEFT, xOffset, yOffset);
```

---

## Performance Considerations

### Partial Rendering

LVGL uses partial rendering mode:
- **Buffer**: 64 lines × 1280px = 81,920 pixels = 163,840 bytes (160KB)
- **Rendering**: Only dirty areas are redrawn
- **Flush**: Called per dirty region, not full screen

### Animation Disabled for Performance

```cpp
// Value updates use LV_ANIM_OFF to avoid animation overhead
lv_bar_set_value(_gauge_bars[i], value, LV_ANIM_OFF);
```

### Memory Management

- **200KB**: Dedicated to LVGL memory pool
- **160KB**: Display buffer (PSRAM)
- **Widget Objects**: ~100-200 bytes each (estimated)
- **Current Usage**: ~8 gauges + 8 presets + 5 actions + header + footer = ~50-100 widgets total

### Update Frequency

```cpp
// Footer metrics updated at 1Hz (throttled)
if (now - _lastFooterUpdate >= 1000) {
    _lastFooterUpdate = now;
    // Update battery, uptime (slow-changing values)
}

// Parameter values updated immediately (encoder events)
// Highlight feedback: 300ms (parameter gauges)
// Preset feedback: 600ms (save/recall/delete)
```

---

## LVGL Limitations to Consider

### Not Available (Disabled in Config)

1. **Buttons** (`LV_USE_BTN 0`) - Use clickable labels/cards instead
2. **Arc/Radian Gauges** (`LV_USE_ARC 0`) - Use horizontal bars only
3. **Sliders** (`LV_USE_SLIDER 0`) - Use bars with manual touch handling
4. **Meters** (`LV_USE_METER 0`) - No radial meters
5. **Charts** (`LV_USE_CHART 0`) - No graph widgets
6. **Images** (`LV_USE_IMG 0`) - No image support (except embedded fonts)
7. **Advanced Widgets** - No dropdowns, rollers, spinners, etc.

### Available Workarounds

- **Radial Gauges**: Not possible - must use horizontal progress bars
- **Buttons**: Clickable labels with event handlers work well
- **Color Pickers**: Not available - use custom grid of colored cards
- **Lists/Menus**: Use grid layout with clickable cards

### What IS Available

- Labels (with scrolling)
- Lines
- Progress bars (horizontal only)
- Flexbox layouts
- Grid layouts
- Basic animations (fade, slide)
- Custom styles (colors, borders, padding, radius)
- Event handlers (click, long press, etc.)

---

## Example: Complete Parameter Gauge Card

```cpp
// 1. Create card container
_gauge_cards[i] = make_card(_gauges_container, false);

// 2. Position in grid
lv_obj_set_grid_cell(_gauge_cards[i], 
    LV_GRID_ALIGN_STRETCH, i, 1,
    LV_GRID_ALIGN_STRETCH, 0, 1);

// 3. Create label (parameter name)
_gauge_labels[i] = lv_label_create(_gauge_cards[i]);
lv_label_set_text(_gauge_labels[i], kParamNames[i]);  // "EFFECT", "PALETTE", etc.
lv_obj_set_style_text_font(_gauge_labels[i], BEBAS_BOLD_24, LV_PART_MAIN);
lv_obj_set_style_text_color(_gauge_labels[i], 
    lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
lv_obj_align(_gauge_labels[i], LV_ALIGN_TOP_MID, 0, 0);

// 4. Create value label (numeric)
_gauge_values[i] = lv_label_create(_gauge_cards[i]);
lv_label_set_text(_gauge_values[i], "--");
lv_obj_set_style_text_font(_gauge_values[i], JETBRAINS_MONO_REG_32, LV_PART_MAIN);
lv_obj_set_style_text_color(_gauge_values[i], 
    lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
lv_obj_align(_gauge_values[i], LV_ALIGN_TOP_MID, 0, 30);

// 5. Create progress bar
_gauge_bars[i] = lv_bar_create(_gauge_cards[i]);
lv_bar_set_range(_gauge_bars[i], 0, 255);
lv_bar_set_value(_gauge_bars[i], 0, LV_ANIM_OFF);
lv_obj_set_size(_gauge_bars[i], LV_PCT(90), 10);
lv_obj_set_style_bg_color(_gauge_bars[i], 
    lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
lv_obj_set_style_bg_color(_gauge_bars[i], 
    lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_MAIN);
lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_INDICATOR);
lv_obj_align(_gauge_bars[i], LV_ALIGN_BOTTOM_MID, 0, -10);

// 6. Update value dynamically
void DisplayUI::updateEncoder(uint8_t index, int32_t value, bool highlight) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    lv_label_set_text(_gauge_values[displayPos], buf);
    
    int32_t v = value;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    lv_bar_set_value(_gauge_bars[displayPos], v, LV_ANIM_OFF);
    
    // Highlight border when encoder adjusted
    lv_obj_set_style_border_color(
        _gauge_cards[displayPos],
        lv_color_hex(highlight ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
        LV_PART_MAIN);
}
```

---

## File Structure Reference

```
firmware/Tab5.encoder/src/ui/
├── lv_conf.h                    # LVGL configuration
├── lvgl_bridge.h/cpp            # M5GFX ↔ LVGL bridge
├── DisplayUI.h/cpp              # Main dashboard UI (1200+ lines)
├── Theme.h                      # Color/typography constants (legacy, M5GFX)
├── widgets/
│   ├── UIHeader.h/cpp           # Header widget (M5GFX sprite-based)
│   ├── GaugeWidget.h/cpp        # Parameter gauge (M5GFX sprite-based)
│   ├── PresetSlotWidget.h/cpp   # Preset slot (M5GFX sprite-based)
│   └── ActionRowWidget.h/cpp    # Action buttons (M5GFX sprite-based)
├── ZoneComposerUI.h/cpp         # Zone composer screen (LVGL)
└── fonts/
    ├── bebas_neue_fonts.h       # Bebas Neue declarations
    ├── bebas_neue_*.c           # Embedded font data (24-72px)
    ├── experimental_fonts.h     # Rajdhani, JetBrains Mono declarations
    └── *.c                      # Embedded font data files
```

**Note**: The current implementation uses **LVGL for the main dashboard** (`DisplayUI.cpp` when `TAB5_ENCODER_USE_LVGL` is defined) but also has **M5GFX sprite-based widgets** as a fallback/legacy implementation.

---

## Key Takeaways for Mockup Generation

1. **Widgets Available**: Labels, bars, lines, containers, flexbox, grid - that's it!
2. **No Radial Gauges**: Must use horizontal progress bars
3. **No Native Buttons**: Use clickable labels/cards with event handlers
4. **RGB565 Color Depth**: Limited color precision (5-6-5 bits)
5. **Partial Rendering**: Efficient but must consider buffer size
6. **200KB LVGL Memory**: Keep widget count reasonable
7. **Software Rendering Only**: No GPU acceleration, keep designs simple
8. **60fps Target**: Avoid complex animations or large redraws
9. **Touch Targets**: Minimum 44x44px for reliable touch (recommended 50px+)
10. **Typography**: Only embedded fonts available (Bebas, Rajdhani, JetBrains Mono)

**When designing mockups, prioritize:**
- Horizontal layouts (bars work better than circular gauges)
- Simple card-based designs (containers + labels)
- Clear visual hierarchy with spacing and typography
- Color coding for states (border colors work well)
- Minimal animations (can use basic LVGL animations sparingly)
- Efficient use of memory (reuse widgets, avoid deep nesting)
