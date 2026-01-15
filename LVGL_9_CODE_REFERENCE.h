/**
 * LVGL 9.x Dashboard Implementation Reference
 * Code examples for 800x480 touch screen with rotary encoder control
 * For M5Stack Tab5 (ESP32-P4) with dual M5ROTATE8 encoder units
 */

#ifndef LVGL_9_CODE_REFERENCE_H
#define LVGL_9_CODE_REFERENCE_H

#include "lvgl.h"

// ============================================================================
// SECTION 1: STYLE DEFINITIONS (Reusable Across All Widgets)
// ============================================================================

/**
 * Initialize all reusable styles once during setup
 * This reduces memory usage by sharing styles across multiple widgets
 */
static lv_style_t style_btn_default;
static lv_style_t style_btn_focused;
static lv_style_t style_btn_pressed;
static lv_style_t style_btn_focused_pressed;

static lv_style_t style_label_title;
static lv_style_t style_label_value;

void init_styles(void) {
    // ---- Button Styles ----

    // Default state: dark gray background
    lv_style_init(&style_btn_default);
    lv_style_set_bg_color(&style_btn_default, lv_color_hex(0x2E2E2E));
    lv_style_set_bg_opa(&style_btn_default, LV_OPA_100);
    lv_style_set_text_color(&style_btn_default, lv_color_hex(0xCCCCCC));
    lv_style_set_text_font(&style_btn_default, &lv_font_montserrat_16);
    lv_style_set_border_width(&style_btn_default, 1);
    lv_style_set_border_color(&style_btn_default, lv_color_hex(0x555555));
    lv_style_set_pad_all(&style_btn_default, 8);
    lv_style_set_radius(&style_btn_default, 4);

    // Focused state: orange highlight with outline
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_color_hex(0xFF6B00));
    lv_style_set_bg_opa(&style_btn_focused, LV_OPA_100);
    lv_style_set_text_color(&style_btn_focused, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&style_btn_focused, 2);
    lv_style_set_border_color(&style_btn_focused, lv_color_hex(0xFFDD00));
    lv_style_set_outline_color(&style_btn_focused, lv_color_hex(0x00CCFF));
    lv_style_set_outline_width(&style_btn_focused, 2);
    lv_style_set_outline_pad(&style_btn_focused, 2);
    lv_style_set_scale_x(&style_btn_focused, 102);
    lv_style_set_scale_y(&style_btn_focused, 102);

    // Pressed state: darker orange
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0xCC5500));
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_100);
    lv_style_set_text_color(&style_btn_pressed, lv_color_hex(0xFFFFFF));

    // Focused + Pressed: most prominent
    lv_style_init(&style_btn_focused_pressed);
    lv_style_set_bg_color(&style_btn_focused_pressed, lv_color_hex(0xFF0000));
    lv_style_set_bg_opa(&style_btn_focused_pressed, LV_OPA_100);
    lv_style_set_text_color(&style_btn_focused_pressed, lv_color_hex(0xFFFFFF));
    lv_style_set_scale_x(&style_btn_focused_pressed, 105);
    lv_style_set_scale_y(&style_btn_focused_pressed, 105);

    // ---- Label Styles ----

    // Title labels: bold, larger
    lv_style_init(&style_label_title);
    lv_style_set_text_font(&style_label_title, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_label_title, lv_color_hex(0xFFFFFF));

    // Value labels: large numbers
    lv_style_init(&style_label_value);
    lv_style_set_text_font(&style_label_value, &lv_font_montserrat_28);
    lv_style_set_text_color(&style_label_value, lv_color_hex(0x00FF00));
}

// ============================================================================
// SECTION 2: BUTTON CREATION HELPERS
// ============================================================================

/**
 * Create a styled button with automatic style application
 */
lv_obj_t *create_styled_button(lv_obj_t *parent, const char *text) {
    lv_obj_t *btn = lv_button_create(parent);

    // Apply reusable styles
    lv_obj_add_style(btn, &style_btn_default, 0);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &style_btn_focused_pressed, LV_STATE_FOCUSED | LV_STATE_PRESSED);

    // Add label
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

/**
 * Create a button matrix (memory-efficient multiple buttons)
 *
 * Example usage:
 *   const char *effects[] = {"Fire", "Ocean", "Plasma", "\n", "Pulse", "Wave", ""};
 *   lv_obj_t *btnm = create_button_matrix(parent, effects);
 */
lv_obj_t *create_button_matrix(lv_obj_t *parent, const char **map) {
    lv_obj_t *btnm = lv_buttonmatrix_create(parent);
    lv_buttonmatrix_set_map(btnm, map);

    // Apply default styles
    lv_obj_add_style(btnm, &style_btn_default, 0);
    lv_obj_add_style(btnm, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btnm, &style_btn_pressed, LV_STATE_PRESSED);

    return btnm;
}

// ============================================================================
// SECTION 3: GROUP AND ENCODER SETUP
// ============================================================================

static lv_group_t *g_encoder_group = NULL;
static lv_group_t *g_touch_group = NULL;

/**
 * Initialize input groups for separate control domains
 */
void init_input_groups(void) {
    // Encoder group: for rotary encoder navigation
    g_encoder_group = lv_group_create();

    // Touch group: for touch screen navigation
    g_touch_group = lv_group_create();
}

/**
 * Encoder read callback
 * Called periodically by LVGL to read encoder state
 */
static int32_t encoder_rotation = 0;
static bool encoder_pressed = false;

void encoder_read_callback(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    // Read hardware encoder
    // IMPORTANT: This is YOUR implementation - interfacing with M5ROTATE8

    // Example pseudo-code:
    // int delta = m5rotate8_get_encoder_delta(0x42, 0);  // Read encoder delta
    // encoder_rotation += delta;
    // encoder_pressed = m5rotate8_get_button_state(0x42, 0);

    // Set LVGL encoder data
    data->enc_diff = encoder_rotation;
    encoder_rotation = 0;
    data->state = encoder_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/**
 * Register encoder input device with LVGL
 */
void register_encoder_input(void) {
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read_callback;

    lv_indev_t *encoder_indev = lv_indev_drv_register(&indev_drv);

    // Link encoder to group
    lv_indev_set_group(encoder_indev, g_encoder_group);
}

/**
 * Add object to encoder focus group
 * Objects in group cycle on encoder rotation
 */
void add_to_encoder_group(lv_obj_t *obj) {
    lv_group_add_obj(g_encoder_group, obj);
}

// ============================================================================
// SECTION 4: LAYOUT BUILDERS
// ============================================================================

#define SCREEN_W 800
#define SCREEN_H 480
#define HEADER_H 60
#define NAVBAR_H 60
#define FOOTER_H 40
#define CONTENT_H (SCREEN_H - HEADER_H - NAVBAR_H - FOOTER_H)

/**
 * Create header with title and status area
 */
lv_obj_t *create_header(void) {
    lv_obj_t *header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);

    // Header uses flex layout for responsive sizing
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);

    // Style header background
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_hor(header, 16, 0);

    // Add title
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "LightwaveOS Tab5");
    lv_obj_add_style(title, &style_label_title, 0);

    // Add status area (will be filled by caller)
    lv_obj_t *status = lv_obj_create(header);
    lv_obj_set_size(status, 150, LV_SIZE_CONTENT);
    lv_obj_set_layout(status, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, 0);

    return header;
}

/**
 * Create grid-based content area for parameter display
 *
 * Example:
 *   static int32_t cols[] = {400, 400, LV_GRID_TEMPLATE_LAST};
 *   static int32_t rows[] = {90, 90, 90, 90, LV_GRID_TEMPLATE_LAST};
 *   lv_obj_t *content = create_grid_content(cols, rows);
 */
lv_obj_t *create_grid_content(int32_t *col_dsc, int32_t *row_dsc) {
    lv_obj_t *content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, SCREEN_W, CONTENT_H);
    lv_obj_set_pos(content, 0, HEADER_H);

    lv_obj_set_layout(content, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(content, col_dsc, row_dsc);

    // Style background
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_style_gap(content, 8, 0);

    return content;
}

/**
 * Create flex-based content area (responsive)
 */
lv_obj_t *create_flex_content(lv_flex_flow_t flow) {
    lv_obj_t *content = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content, SCREEN_W, CONTENT_H);
    lv_obj_set_pos(content, 0, HEADER_H);

    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, flow);
    lv_obj_set_flex_align(content,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_bg_color(content, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_style_gap(content, 8, 0);

    return content;
}

/**
 * Create parameter display cell (title + value)
 *
 * Returns array [title_label, value_label]
 */
lv_obj_t **create_parameter_cell(lv_obj_t *parent, const char *title) {
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_layout(cell, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START);

    lv_obj_set_style_bg_opa(cell, LV_OPA_0, 0);
    lv_obj_set_style_pad_all(cell, 4, 0);

    // Title label
    lv_obj_t *title_label = lv_label_create(cell);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, &style_label_title, 0);

    // Value label
    lv_obj_t *value_label = lv_label_create(cell);
    lv_label_set_text(value_label, "0");
    lv_obj_add_style(value_label, &style_label_value, 0);

    // Store references
    static lv_obj_t *result[2];
    result[0] = title_label;
    result[1] = value_label;

    return result;
}

// ============================================================================
// SECTION 5: ANIMATION UTILITIES
// ============================================================================

/**
 * Pulse animation for focused state
 */
void animate_pulse(lv_obj_t *obj) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 100, 110);
    lv_anim_set_duration(&anim, 600);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&anim, 100);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);

    lv_anim_set_exec_cb(&anim, [](void *var, int32_t v) {
        lv_obj_t *o = (lv_obj_t *)var;
        lv_obj_set_style_scale_x(o, v, LV_PART_MAIN);
        lv_obj_set_style_scale_y(o, v, LV_PART_MAIN);
    });

    lv_anim_start(&anim);
}

/**
 * Flash animation for value change feedback
 */
void animate_flash(lv_obj_t *obj) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_duration(&anim, 300);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);

    lv_anim_set_exec_cb(&anim, [](void *var, int32_t v) {
        lv_obj_t *o = (lv_obj_t *)var;
        uint8_t opacity = (v < 128) ? 100 : 200;
        lv_obj_set_style_bg_opa(o, opacity, LV_PART_MAIN);
    });

    lv_anim_start(&anim);
}

/**
 * Color transition animation
 */
void animate_color_transition(lv_obj_t *obj, lv_color_t from, lv_color_t to, uint32_t duration) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_duration(&anim, duration);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);

    lv_anim_set_exec_cb(&anim, [](void *var, int32_t v) {
        lv_obj_t *o = (lv_obj_t *)var;
        // Need to capture from/to in closure - simplified here
        lv_obj_set_style_text_color(o, lv_color_white(), LV_PART_MAIN);
    });

    lv_anim_start(&anim);
}

// ============================================================================
// SECTION 6: EVENT HANDLER PATTERNS
// ============================================================================

/**
 * Generic button event handler with state tracking
 */
typedef struct {
    int id;
    const char *name;
    void (*on_pressed)(int id);
    void (*on_focused)(int id);
} ButtonData;

void button_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    ButtonData *btn_data = (ButtonData *)lv_event_get_user_data(e);

    switch(code) {
        case LV_EVENT_PRESSED:
            if (btn_data->on_pressed) {
                btn_data->on_pressed(btn_data->id);
            }
            break;

        case LV_EVENT_FOCUSED:
            if (btn_data->on_focused) {
                btn_data->on_focused(btn_data->id);
            }
            break;

        case LV_EVENT_DEFOCUSED:
            // Optionally clear highlight
            break;

        default:
            break;
    }
}

/**
 * Create button with integrated data and handler
 */
lv_obj_t *create_button_with_handler(lv_obj_t *parent, const char *text, int id,
                                      void (*on_pressed)(int), void (*on_focused)(int)) {
    lv_obj_t *btn = create_styled_button(parent, text);

    // Allocate and fill button data
    ButtonData *data = (ButtonData *)malloc(sizeof(ButtonData));
    data->id = id;
    data->name = text;
    data->on_pressed = on_pressed;
    data->on_focused = on_focused;

    // Set as user data and register handler
    lv_obj_set_user_data(btn, data);
    lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_ALL, data);

    // Add to encoder group
    add_to_encoder_group(btn);

    return btn;
}

// ============================================================================
// SECTION 7: LABEL UPDATE PATTERNS
// ============================================================================

/**
 * Update label with integer value
 */
void update_label_int(lv_obj_t *label, int value) {
    static char buf[32];
    lv_snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(label, buf);
}

/**
 * Update label with formatted string
 */
void update_label_fmt(lv_obj_t *label, const char *fmt, ...) {
    static char buf[64];
    va_list args;
    va_start(args, fmt);
    lv_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv_label_set_text(label, buf);
}

/**
 * Update label with animation
 */
void update_label_animated(lv_obj_t *label, int value) {
    update_label_int(label, value);
    animate_flash(label);
}

// ============================================================================
// SECTION 8: COMPLETE EXAMPLE: MODE SELECTOR WITH ENCODER
// ============================================================================

/**
 * Complete working example of mode selector button grid
 * with encoder navigation and touch support
 */

typedef struct {
    lv_obj_t **buttons;
    int count;
    int selected;
} ModeSelector;

static ModeSelector g_mode_selector = {0};

void mode_button_pressed(int mode_id) {
    // Deselect all
    for (int i = 0; i < g_mode_selector.count; i++) {
        lv_obj_clear_state(g_mode_selector.buttons[i], LV_STATE_CHECKED);
    }

    // Select this mode
    lv_obj_add_state(g_mode_selector.buttons[mode_id], LV_STATE_CHECKED);
    g_mode_selector.selected = mode_id;

    // Call application-specific mode change
    // on_mode_changed(mode_id);

    Serial.printf("Mode changed to: %d\n", mode_id);
}

void mode_button_focused(int mode_id) {
    // Visual feedback can be added here
    animate_pulse(g_mode_selector.buttons[mode_id]);
}

/**
 * Create mode selector UI
 */
void create_mode_selector(lv_obj_t *parent) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 800, 100);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container,
        LV_FLEX_ALIGN_SPACE_EVENLY,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);

    const char *modes[] = {"Global", "Zone A", "Zone B", "Zone C"};
    int num_modes = 4;

    g_mode_selector.buttons = (lv_obj_t **)malloc(sizeof(lv_obj_t *) * num_modes);
    g_mode_selector.count = num_modes;

    for (int i = 0; i < num_modes; i++) {
        g_mode_selector.buttons[i] = create_button_with_handler(
            container, modes[i], i,
            mode_button_pressed,
            mode_button_focused
        );
        lv_obj_set_size(g_mode_selector.buttons[i], 180, 60);

        // Make first button selected by default
        if (i == 0) {
            lv_obj_add_state(g_mode_selector.buttons[i], LV_STATE_FOCUSED);
        }
    }
}

// ============================================================================
// SECTION 9: MEMORY-EFFICIENT PARAMETER DISPLAY
// ============================================================================

/**
 * Parameter display using button matrix (memory efficient)
 *
 * Displays parameters in a grid format using minimal memory
 */
void create_parameter_grid(lv_obj_t *parent) {
    const char *param_map[] = {
        "Brightness:", "128", "\n",
        "Speed:", "50", "\n",
        "Effect:", "Fire", ""
    };

    lv_obj_t *btnm = lv_buttonmatrix_create(parent);
    lv_buttonmatrix_set_map(btnm, param_map);

    // Disable button click on parameters (read-only display)
    for (int i = 0; i < 6; i++) {
        lv_buttonmatrix_set_button_ctrl(btnm, i, LV_BUTTONMATRIX_CTRL_DISABLED);
    }

    lv_obj_set_size(btnm, 400, 180);
}

// ============================================================================
// SECTION 10: INITIALIZATION TEMPLATE
// ============================================================================

/**
 * Complete initialization sequence for Tab5 Dashboard
 */
void init_tab5_dashboard(void) {
    // Step 1: Initialize all reusable styles
    init_styles();

    // Step 2: Set up input groups
    init_input_groups();

    // Step 3: Register encoder input
    register_encoder_input();

    // Step 4: Create main layout
    create_header();
    create_flex_content(LV_FLEX_FLOW_COLUMN);

    // Step 5: Create mode selector
    create_mode_selector(lv_scr_act());

    // Step 6: Create parameter displays
    create_parameter_grid(lv_scr_act());

    Serial.println("Tab5 Dashboard initialized");
}

#endif // LVGL_9_CODE_REFERENCE_H
