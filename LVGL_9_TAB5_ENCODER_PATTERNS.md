# LVGL 9.3 + M5Stack Tab5 + Dual M5ROTATE8 Integration Patterns

## Overview

This document covers specific patterns for integrating LVGL 9.3 dashboards with the M5Stack Tab5 (ESP32-P4, 800x480 ILI9881C display) and dual M5ROTATE8 encoder units (16 rotary encoders total).

**Hardware Setup:**
- **Tab5:** ESP32-P4 RISC-V, 800x480 display
- **Unit A (I2C 0x42):** M5ROTATE8 with 8 encoders for global parameters
- **Unit B (I2C 0x41):** M5ROTATE8 with 8 encoders for preset/zone selection
- **WiFi:** ESP32-C6 co-processor via SDIO (custom pins)

---

## Architecture: Dual Encoder System

### Input Domain Separation

The dual M5ROTATE8 units allow **independent control domains**:

```
┌─────────────────────────────────────────┐
│     LVGL Touch Screen (800x480)         │
│  ┌──────────────────────────────────┐   │
│  │   Touch Focus Group              │   │
│  │  (Touch selects widget focus)    │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘

┌──────────────────────┐  ┌──────────────────────┐
│  Unit A (0x42)       │  │  Unit B (0x41)       │
│  8 Encoders          │  │  8 Encoders          │
│  Global Parameters   │  │  Zone/Preset Control │
│  - Brightness (E0)   │  │  - Zone A (E0)       │
│  - Speed (E1)        │  │  - Zone B (E1)       │
│  - Effect (E2)       │  │  - Preset 1 (E2)     │
│  - HUE (E3)          │  │  - Preset 2 (E3)     │
│  - Saturation (E4)   │  │  - Preset 3 (E4)     │
│  - Value (E5)        │  │  - Preset 4 (E5)     │
│  - Param1 (E6)       │  │  - Preset 5 (E6)     │
│  - Param2 (E7)       │  │  - Preset 6 (E7)     │
└──────────────────────┘  └──────────────────────┘
```

### Encoder Group Strategy

**Option 1: Single Primary Group (Recommended)**
```c
// One encoder group controls UI focus
// Unit A: Updates focused widget's value (if slider/spinbox)
// Unit B: Cycles through presets (separate mechanism)

lv_group_t *primary_group = lv_group_create();
// Add all interactive widgets to primary group
// Encoder rotation changes focus or value
```

**Option 2: Dual Group System (Advanced)**
```c
// Two independent groups
lv_group_t *ui_focus_group = lv_group_create();    // Touch/UI focus
lv_group_t *param_edit_group = lv_group_create();  // Encoder parameter edit

// Unit A linked to param_edit_group
// Unit B linked to separate mechanism (preset selection)
```

---

## M5ROTATE8 Integration Code

### Reading M5ROTATE8 Encoders

**Standard M5ROTATE8 Library Pattern:**

```c
#include "M5Module_ROTATE8.h"

M5Module_ROTATE8 rotate8_unit_a(0x42);  // I2C address 0x42
M5Module_ROTATE8 rotate8_unit_b(0x41);  // I2C address 0x41

/**
 * Read encoder delta from M5ROTATE8
 * Returns signed integer: positive = clockwise, negative = counter-clockwise
 */
int read_encoder_delta(M5Module_ROTATE8 &rotate8, uint8_t encoder_id) {
    // M5ROTATE8 getEncoderIncrement() returns rotational steps
    int delta = rotate8.getEncoderIncrement(encoder_id);
    return delta;  // Could be 0, ±1, ±2, etc.
}

/**
 * Read button state from M5ROTATE8
 * Each encoder has an integrated push button
 */
bool read_encoder_button(M5Module_ROTATE8 &rotate8, uint8_t encoder_id) {
    return rotate8.getButtonState(encoder_id);
}
```

### LVGL Encoder Device Registration with M5ROTATE8

```c
/**
 * Global encoder state tracking
 */
struct {
    M5Module_ROTATE8 *rotate8_a;
    M5Module_ROTATE8 *rotate8_b;
    uint8_t active_encoder_a;  // Currently focused encoder from Unit A
    uint8_t active_encoder_b;  // Currently focused encoder from Unit B
    int32_t accumulated_delta_a;
    int32_t accumulated_delta_b;
    bool button_pressed_a;
    bool button_pressed_b;
} g_encoder_state = {0};

/**
 * Unit A Encoder Read Callback (Global Parameters)
 * Called periodically by LVGL
 */
void encoder_a_read_callback(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (!g_encoder_state.rotate8_a) return;

    // Check which Unit A encoder is "active" (has focus in LVGL group)
    // This could be determined by which widget is currently focused
    uint8_t active_id = g_encoder_state.active_encoder_a;

    // Read encoder delta
    int delta = read_encoder_delta(*g_encoder_state.rotate8_a, active_id);
    g_encoder_state.accumulated_delta_a += delta;

    // Read button
    bool pressed = read_encoder_button(*g_encoder_state.rotate8_a, active_id);

    // Set LVGL input data
    data->enc_diff = g_encoder_state.accumulated_delta_a;
    g_encoder_state.accumulated_delta_a = 0;  // Reset for next cycle
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/**
 * Unit B Encoder Read Callback (Preset/Zone Selection)
 */
void encoder_b_read_callback(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (!g_encoder_state.rotate8_b) return;

    // Unit B: Preset selection (could be independent of focus)
    // OR use encoder 0 for preset cycling
    uint8_t preset_encoder = 0;

    int delta = read_encoder_delta(*g_encoder_state.rotate8_b, preset_encoder);
    g_encoder_state.accumulated_delta_b += delta;

    bool pressed = read_encoder_button(*g_encoder_state.rotate8_b, preset_encoder);

    data->enc_diff = g_encoder_state.accumulated_delta_b;
    g_encoder_state.accumulated_delta_b = 0;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/**
 * Initialize M5ROTATE8 units
 */
void init_m5rotate8_units(void) {
    // Initialize Unit A (0x42)
    g_encoder_state.rotate8_a = &rotate8_unit_a;
    if (!rotate8_unit_a.begin()) {
        Serial.println("Failed to initialize Unit A");
        return;
    }
    Serial.println("Unit A initialized");

    // Initialize Unit B (0x41)
    g_encoder_state.rotate8_b = &rotate8_unit_b;
    if (!rotate8_unit_b.begin()) {
        Serial.println("Failed to initialize Unit B");
        return;
    }
    Serial.println("Unit B initialized");
}

/**
 * Register both encoder input devices with LVGL
 */
void register_dual_encoders(void) {
    // Register Unit A encoder
    {
        static lv_indev_drv_t drv;
        lv_indev_drv_init(&drv);
        drv.type = LV_INDEV_TYPE_ENCODER;
        drv.read_cb = encoder_a_read_callback;
        lv_indev_t *indev_a = lv_indev_drv_register(&drv);

        lv_group_t *group_a = lv_group_create();
        lv_indev_set_group(indev_a, group_a);

        g_encoder_group_a = group_a;
    }

    // Register Unit B encoder
    {
        static lv_indev_drv_t drv;
        lv_indev_drv_init(&drv);
        drv.type = LV_INDEV_TYPE_ENCODER;
        drv.read_cb = encoder_b_read_callback;
        lv_indev_t *indev_b = lv_indev_drv_register(&drv);

        lv_group_t *group_b = lv_group_create();
        lv_indev_set_group(indev_b, group_b);

        g_encoder_group_b = group_b;
    }
}
```

### Advanced: Per-Encoder LVGL Widgets

**Approach:** Map each physical encoder to a specific LVGL widget

```c
/**
 * Map encoder ID to LVGL widget
 * Unit A: Parameters (sliders, spinboxes)
 * Unit B: Presets (button matrix, dropdown)
 */
typedef struct {
    uint8_t encoder_id;
    lv_obj_t *widget;
    int min_value;
    int max_value;
    int current_value;
    const char *label;
} EncoderMapping;

static EncoderMapping g_encoder_mappings_a[] = {
    {0, NULL, 0, 255, 128, "Brightness"},
    {1, NULL, 1, 50, 25, "Speed"},
    {2, NULL, 0, 45, 0, "Effect"},
    {3, NULL, 0, 360, 180, "Hue"},
    {4, NULL, 0, 255, 128, "Saturation"},
    {5, NULL, 0, 255, 200, "Value"},
    {6, NULL, 0, 100, 50, "Param1"},
    {7, NULL, 0, 100, 50, "Param2"},
};

/**
 * Enhanced encoder callback with per-encoder mapping
 */
void encoder_a_read_advanced(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (!g_encoder_state.rotate8_a) return;

    // Find which encoder has focus
    // For now: use encoder 0 (can be extended to check which widget is focused)
    uint8_t active_id = 0;

    int delta = read_encoder_delta(*g_encoder_state.rotate8_a, active_id);
    EncoderMapping *mapping = &g_encoder_mappings_a[active_id];

    // Update value based on delta
    mapping->current_value += delta;
    if (mapping->current_value < mapping->min_value) {
        mapping->current_value = mapping->min_value;
    }
    if (mapping->current_value > mapping->max_value) {
        mapping->current_value = mapping->max_value;
    }

    // Update LVGL widget if it's a slider or spinbox
    if (mapping->widget) {
        if (lv_obj_check_type(mapping->widget, &lv_slider_class)) {
            lv_slider_set_value(mapping->widget, mapping->current_value, LV_ANIM_ON);
        }
        else if (lv_obj_check_type(mapping->widget, &lv_spinbox_class)) {
            lv_spinbox_set_value(mapping->widget, mapping->current_value);
        }
    }

    // Set LVGL encoder data for focus navigation
    data->enc_diff = delta;
    data->state = read_encoder_button(*g_encoder_state.rotate8_a, active_id) ?
                  LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
```

---

## Tab5 Dashboard Patterns

### Layout: Tab-Based Navigation with Parameter Control

```c
/**
 * Complete Tab5 dashboard structure
 */
void create_tab5_dashboard(void) {
    // Screen setup
    lv_obj_clean(lv_scr_act());

    // ---- Header ----
    lv_obj_t *header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 800, 60);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "LightwaveOS Tab5");

    // ---- Tab View Navigation ----
    lv_obj_t *tabview = lv_tabview_create(lv_scr_act());
    lv_obj_set_size(tabview, 800, 420);
    lv_obj_set_pos(tabview, 0, 60);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 60);

    // Tab 1: Global Parameters
    lv_obj_t *tab_global = lv_tabview_add_tab(tabview, "Global");
    create_global_parameters_page(tab_global);

    // Tab 2: Zone Control
    lv_obj_t *tab_zones = lv_tabview_add_tab(tabview, "Zones");
    create_zones_control_page(tab_zones);

    // Tab 3: Presets
    lv_obj_t *tab_presets = lv_tabview_add_tab(tabview, "Presets");
    create_presets_page(tab_presets);

    // Tab 4: Settings
    lv_obj_t *tab_settings = lv_tabview_add_tab(tabview, "Settings");
    create_settings_page(tab_settings);

    // ---- Footer ----
    lv_obj_t *footer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(footer, 800, 40);
    lv_obj_set_pos(footer, 0, 440);

    lv_obj_t *status = lv_label_create(footer);
    lv_label_set_text(status, "Status: Ready");

    // Add tab buttons to encoder group
    lv_obj_t *tab_buttons = lv_tabview_get_tab_btns(tabview);
    lv_group_add_obj(g_encoder_group_a, tab_buttons);
}
```

### Global Parameters Page (Unit A Control)

```c
/**
 * Global parameters page controlled by Unit A encoders
 */
void create_global_parameters_page(lv_obj_t *parent) {
    // Grid layout: 2 columns x 4 rows
    static int32_t cols[] = {400, 400, LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {90, 90, 90, 90, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, 800, 360);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, cols, rows);

    // ---- Row 0: Brightness ----
    create_parameter_row(grid, 0, "Brightness", "128", g_encoder_mappings_a[0]);

    // ---- Row 1: Speed ----
    create_parameter_row(grid, 1, "Speed", "25", g_encoder_mappings_a[1]);

    // ---- Row 2: Effect Selection ----
    lv_obj_t *effect_dropdown = lv_dropdown_create(grid);
    lv_dropdown_set_options(effect_dropdown,
        "Fire\nOcean\nPlasma\nPulse\nWave\nSpiral");
    lv_obj_set_grid_cell(effect_dropdown,
        LV_GRID_ALIGN_STRETCH, 0, 1,
        LV_GRID_ALIGN_CENTER, 2, 1);
    lv_group_add_obj(g_encoder_group_a, effect_dropdown);
    g_encoder_mappings_a[2].widget = effect_dropdown;

    // ---- Row 3: HSV Controls ----
    create_parameter_row(grid, 3, "Hue", "180", g_encoder_mappings_a[3]);
}

/**
 * Create a parameter row with title and value slider
 */
void create_parameter_row(lv_obj_t *parent, int row, const char *label, const char *initial_value, EncoderMapping &mapping) {
    // Title
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, label);
    lv_obj_set_grid_cell(title,
        LV_GRID_ALIGN_START, 0, 1,
        LV_GRID_ALIGN_CENTER, row, 1);

    // Value slider
    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, mapping.min_value, mapping.max_value);
    lv_slider_set_value(slider, mapping.current_value, LV_ANIM_OFF);
    lv_obj_set_grid_cell(slider,
        LV_GRID_ALIGN_STRETCH, 1, 1,
        LV_GRID_ALIGN_CENTER, row, 1);

    // Link encoder mapping
    mapping.widget = slider;

    // Add to encoder group for control
    lv_group_add_obj(g_encoder_group_a, slider);

    // Event handler for value changes
    lv_obj_add_event_cb(slider, [](lv_event_t *e) {
        lv_obj_t *slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        // Update application parameter
        // on_parameter_changed(mapping.encoder_id, value);
    }, LV_EVENT_VALUE_CHANGED, &mapping);
}
```

### Zones Control Page (Unit B Integration)

```c
/**
 * Zone control page: Select active zone, then Unit A controls it
 */
void create_zones_control_page(lv_obj_t *parent) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 800, 360);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);

    // Zone selector (Unit B controls this)
    lv_obj_t *zone_label = lv_label_create(container);
    lv_label_set_text(zone_label, "Select Zone:");

    const char *zone_map[] = {
        "Zone A", "Zone B", "Zone C", "Zone D", ""
    };

    lv_obj_t *zone_btnm = lv_buttonmatrix_create(container);
    lv_buttonmatrix_set_map(zone_btnm, zone_map);
    lv_buttonmatrix_set_one_check(zone_btnm, true);
    lv_obj_set_size(zone_btnm, 800, 100);

    // Add zone buttons to Unit B encoder group
    lv_group_add_obj(g_encoder_group_b, zone_btnm);

    lv_obj_add_event_cb(zone_btnm, [](lv_event_t *e) {
        uint16_t selected = lv_buttonmatrix_get_selected_btn((lv_obj_t *)lv_event_get_target(e));
        // Switch zone context
        // Unit A encoders now control selected zone parameters
        on_zone_selected(selected);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Zone parameters (controlled by Unit A after zone selection)
    lv_obj_t *zone_params = lv_obj_create(container);
    lv_obj_set_size(zone_params, 800, 200);
    lv_obj_set_layout(zone_params, LV_LAYOUT_GRID);

    static int32_t cols[] = {400, 400, LV_GRID_TEMPLATE_LAST};
    static int32_t rows[] = {100, 100, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(zone_params, cols, rows);

    // These parameters are controlled by Unit A after zone selection
    create_parameter_row(zone_params, 0, "Brightness", "128", g_encoder_mappings_a[0]);
    create_parameter_row(zone_params, 1, "Effect", "Fire", g_encoder_mappings_a[2]);
}
```

### Presets Page (Unit B Primary Control)

```c
/**
 * Presets page: Unit B encoders select presets
 */
void create_presets_page(lv_obj_t *parent) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 800, 360);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);

    // Preset selector (8 presets, controlled by Unit B)
    lv_obj_t *preset_label = lv_label_create(container);
    lv_label_set_text(preset_label, "Presets (8 Banks):");

    const char *preset_map[] = {
        "Bank 1", "Bank 2", "Bank 3", "Bank 4", "\n",
        "Bank 5", "Bank 6", "Bank 7", "Bank 8", ""
    };

    lv_obj_t *preset_btnm = lv_buttonmatrix_create(container);
    lv_buttonmatrix_set_map(preset_btnm, preset_map);
    lv_buttonmatrix_set_one_check(preset_btnm, true);
    lv_obj_set_size(preset_btnm, 800, 180);

    // Add to Unit B encoder group
    lv_group_add_obj(g_encoder_group_b, preset_btnm);

    lv_obj_add_event_cb(preset_btnm, [](lv_event_t *e) {
        uint16_t selected = lv_buttonmatrix_get_selected_btn((lv_obj_t *)lv_event_get_target(e));
        // Load preset
        on_preset_selected(selected);
        Serial.printf("Preset selected: %d\n", selected);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Preset info
    lv_obj_t *preset_info = lv_label_create(container);
    lv_label_set_text(preset_info, "Press encoder to load preset");
    lv_obj_set_style_text_font(preset_info, &lv_font_montserrat_14, 0);
}
```

---

## Advanced Patterns

### Encoder-Controlled Slider with Direct Value Update

```c
/**
 * Create a slider that responds to encoder input with visual feedback
 */
lv_obj_t *create_encoder_slider(lv_obj_t *parent, const char *label, int min, int max, int initial) {
    // Title
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, label);

    // Slider
    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, initial, LV_ANIM_OFF);
    lv_obj_set_width(slider, 300);

    // Value label
    lv_obj_t *value_label = lv_label_create(parent);
    lv_label_set_text_fmt(value_label, "%d", initial);

    // Event handler: update value label and apply style feedback
    lv_obj_add_event_cb(slider, [](lv_event_t *e) {
        lv_obj_t *slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);

        // Update label
        lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
        lv_label_set_text_fmt(label, "%d", value);

        // Flash animation
        lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
    }, LV_EVENT_VALUE_CHANGED, value_label);

    // Add to encoder group for control
    lv_group_add_obj(g_encoder_group_a, slider);

    return slider;
}
```

### Real-Time Parameter Monitoring

```c
/**
 * Monitor values from all Unit A encoders
 */
void update_encoder_parameters(void) {
    for (int i = 0; i < 8; i++) {
        int delta = read_encoder_delta(rotate8_unit_a, i);

        if (delta != 0) {
            g_encoder_mappings_a[i].current_value += delta;

            // Clamp value
            if (g_encoder_mappings_a[i].current_value < g_encoder_mappings_a[i].min_value) {
                g_encoder_mappings_a[i].current_value = g_encoder_mappings_a[i].min_value;
            }
            if (g_encoder_mappings_a[i].current_value > g_encoder_mappings_a[i].max_value) {
                g_encoder_mappings_a[i].current_value = g_encoder_mappings_a[i].max_value;
            }

            // Update UI
            if (g_encoder_mappings_a[i].widget) {
                if (lv_obj_check_type(g_encoder_mappings_a[i].widget, &lv_slider_class)) {
                    lv_slider_set_value(
                        g_encoder_mappings_a[i].widget,
                        g_encoder_mappings_a[i].current_value,
                        LV_ANIM_ON
                    );
                }
            }

            Serial.printf("Encoder %d: %d\n", i, g_encoder_mappings_a[i].current_value);
        }
    }
}
```

### Hybrid Touch + Encoder Focus Management

```c
/**
 * Track focused object and update encoder control
 */
static lv_obj_t *g_currently_focused = NULL;

void on_widget_focused(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);

    // Clear previous focus visual
    if (g_currently_focused) {
        lv_obj_set_style_outline_width(g_currently_focused, 0, LV_STATE_FOCUSED);
    }

    // Apply focus visual to new widget
    g_currently_focused = obj;
    lv_obj_set_style_outline_width(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(obj, lv_color_hex(0x00CCFF), LV_STATE_FOCUSED);

    // Determine which encoder(s) should control this widget
    if (lv_obj_check_type(obj, &lv_slider_class)) {
        // Unit A controls sliders
        g_encoder_state.active_encoder_a = 0;  // Assign specific encoder
    }
}

/**
 * Register focus callbacks on all interactive widgets
 */
void register_focus_callbacks(void) {
    lv_obj_t *obj = NULL;
    // For all objects in the current screen...
    // lv_obj_add_event_cb(obj, on_widget_focused, LV_EVENT_FOCUSED, NULL);
}
```

---

## Testing and Debugging

### Serial Output for Encoder Events

```c
/**
 * Debug encoder input
 */
void debug_encoder_input(void) {
    static unsigned long last_debug = 0;
    unsigned long now = millis();

    if (now - last_debug > 100) {  // Update every 100ms
        last_debug = now;

        // Unit A encoders
        Serial.print("Unit A: ");
        for (int i = 0; i < 8; i++) {
            int delta = read_encoder_delta(rotate8_unit_a, i);
            if (delta != 0) {
                Serial.printf("E%d:%+d ", i, delta);
            }
        }

        // Unit B encoders
        Serial.print(" | Unit B: ");
        for (int i = 0; i < 8; i++) {
            int delta = read_encoder_delta(rotate8_unit_b, i);
            if (delta != 0) {
                Serial.printf("E%d:%+d ", i, delta);
            }
        }

        Serial.println();
    }
}
```

### LVGL Input Debug Mode

```c
/**
 * Enable LVGL indev debug output
 */
void enable_indev_debug(void) {
    // In lv_conf.h, set:
    // #define LV_USE_LOG 1
    // #define LV_LOG_LEVEL LV_LOG_LEVEL_TRACE

    // Or programmatically:
    lv_log_register_print_cb([](lv_log_level_t level, const char *buf) {
        Serial.print(buf);
    });
}
```

---

## Summary and Recommendations

### Best Practices for Tab5 + Dual M5ROTATE8

1. **Separate Control Domains**
   - Unit A: Global parameter adjustment (brightness, speed, hue)
   - Unit B: Selection/preset management (zones, presets, effects)

2. **Use Tab View for Navigation**
   - Provides visual feedback for mode selection
   - Encoder can cycle through tabs

3. **Responsive Layouts**
   - Use Grid for precise parameter layout
   - Use Flex for responsive button placement

4. **Style Reuse**
   - Define styles once, apply to multiple widgets
   - Reduces memory usage on embedded system

5. **Event Handler Pattern**
   - One handler per widget type (button, slider, dropdown)
   - Use user data to track context (encoder ID, parameter name)

6. **Animation for Feedback**
   - Pulse animation on focused state
   - Flash animation on value change
   - Helps user understand which parameter is being adjusted

7. **Memory Optimization**
   - Use Button Matrix for preset selection (8 buttons)
   - Use static text where possible
   - Lazy-create widgets for unused tabs

---

## Resources

- [M5Stack Tab5 Documentation](https://docs.m5stack.com/)
- [M5Module ROTATE8 Library](https://github.com/m5stack/M5Module_ROTATE8)
- [LVGL 9.3 Input Devices](https://docs.lvgl.io/9.3/porting/indev.html)
- [LVGL 9.3 Groups and Focus](https://docs.lvgl.io/master/main-modules/indev/groups.html)
- [LVGL 9.3 Examples](https://docs.lvgl.io/master/examples.html)
