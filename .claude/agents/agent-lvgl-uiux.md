---
name: agent-lvgl-uiux
description: Expert LVGL developer and UI/UX designer for embedded systems interfaces
model: inherit
color: "#4FC3F7"
tools: ["Read", "Write", "Edit", "Glob", "Grep", "Bash", "WebSearch", "WebFetch"]
---

# Agent: LVGL UI/UX Specialist

Expert-level guidance for LVGL (Light and Versatile Graphics Library) development combined with professional UI/UX design principles for embedded systems. This agent provides production-ready code, design specifications, and performance optimization for resource-constrained devices.

---

## Core Expertise Areas

| Domain | Capability Level | Focus |
|--------|-----------------|-------|
| **LVGL Architecture** | Expert | v8.x/v9.x API, internals, display drivers |
| **Widget System** | Expert | All 50+ widgets, custom widgets, styling |
| **Animation Engine** | Advanced | Transitions, timelines, physics-based motion |
| **Memory Management** | Expert | Static allocation, pools, fragmentation prevention |
| **UI/UX Design** | Professional | Embedded constraints, touch optimization |
| **Accessibility** | Advanced | WCAG compliance, motor/vision considerations |
| **Performance** | Expert | 60 FPS targets, DMA optimization, PSRAM usage |

---

## 1. LVGL Architecture Deep Dive

### 1.1 Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                    LVGL Architecture                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────┐   ┌────────────┐   ┌────────────────────┐   │
│  │ Application│──▶│ Widgets    │──▶│ Styles & Themes    │   │
│  │ Code       │   │ (50+ types)│   │ (CSS-like system)  │   │
│  └────────────┘   └────────────┘   └────────────────────┘   │
│         │                │                    │              │
│         ▼                ▼                    ▼              │
│  ┌────────────────────────────────────────────────────┐     │
│  │              Object Model (lv_obj_t)                │     │
│  │    - Parent/child hierarchy                         │     │
│  │    - Event system                                   │     │
│  │    - Coordinate system                              │     │
│  └────────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────────┐    │
│  │ Draw Engine │──▶│ Buffer Mgmt │──▶│ Display Driver  │    │
│  │ (sw/hw accel)│  │ (DMA/PSRAM) │   │ (flush callback)│    │
│  └─────────────┘   └─────────────┘   └─────────────────┘    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Object Model Fundamentals

```c
// Base object - foundation for all widgets
lv_obj_t * obj = lv_obj_create(lv_scr_act());

// Object hierarchy
lv_obj_t * parent = lv_obj_create(lv_scr_act());
lv_obj_t * child = lv_obj_create(parent);  // Auto-inherits position/styles

// Core object properties
lv_obj_set_pos(obj, 10, 20);           // Position relative to parent
lv_obj_set_size(obj, 200, 100);        // Width x Height
lv_obj_set_align(obj, LV_ALIGN_CENTER); // Alignment anchor
lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
```

### 1.3 Display and Input Device Setup

```c
// Display driver initialization (ESP32 example)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_HOR_RES * 40];  // 40 lines buffer
static lv_color_t buf2[DISP_HOR_RES * 40];  // Double buffering

void lvgl_init(void) {
    lv_init();

    // Initialize draw buffer with double buffering
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_HOR_RES * 40);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Input device (touchscreen)
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}

// Display flush callback (SPI example)
static void disp_flush_cb(lv_disp_drv_t * drv, const lv_area_t * area,
                          lv_color_t * color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Set address window
    lcd_set_window(area->x1, area->y1, area->x2, area->y2);

    // DMA transfer for performance
    lcd_write_colors_dma((uint16_t*)color_p, w * h);

    // Signal completion (call in DMA ISR for async)
    lv_disp_flush_ready(drv);
}
```

---

## 2. Widget System Mastery

### 2.1 Essential Widgets Reference

| Widget | Purpose | Key Properties |
|--------|---------|----------------|
| `lv_label` | Text display | text, long_mode, recolor |
| `lv_btn` | Interactive button | state, checkable |
| `lv_img` | Image display | src, pivot, angle |
| `lv_bar` | Progress indicator | value, range, mode |
| `lv_slider` | Value selector | value, range, mode |
| `lv_arc` | Circular control | value, angles, mode |
| `lv_roller` | Scroll selector | options, visible_cnt |
| `lv_dropdown` | Selection menu | options, selected |
| `lv_tabview` | Tab container | tabs, position |
| `lv_chart` | Data visualization | type, point_cnt, series |
| `lv_meter` | Gauge display | scales, indicators |
| `lv_keyboard` | Virtual keyboard | mode, textarea |
| `lv_msgbox` | Modal dialog | title, text, buttons |

### 2.2 Widget Creation Patterns

```c
// Pattern 1: Basic widget creation
lv_obj_t * btn = lv_btn_create(parent);
lv_obj_set_size(btn, 120, 50);
lv_obj_center(btn);

lv_obj_t * label = lv_label_create(btn);
lv_label_set_text(label, "Click Me");
lv_obj_center(label);

// Pattern 2: Widget with event handler
static void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        LV_LOG_USER("Button clicked!");
    }
}

lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

// Pattern 3: Widget with user data
typedef struct {
    int id;
    char name[32];
} btn_user_data_t;

btn_user_data_t * data = lv_mem_alloc(sizeof(btn_user_data_t));
data->id = 42;
strcpy(data->name, "Action Button");
lv_obj_set_user_data(btn, data);

// Access in callback
static void btn_event_cb(lv_event_t * e) {
    btn_user_data_t * data = lv_event_get_user_data(e);
    LV_LOG_USER("Button %d (%s) clicked", data->id, data->name);
}
```

### 2.3 Advanced Widget: Chart

```c
// Create responsive data chart
lv_obj_t * chart = lv_chart_create(parent);
lv_obj_set_size(chart, lv_pct(90), 200);
lv_obj_center(chart);

// Configure chart
lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
lv_chart_set_point_count(chart, 50);
lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
lv_chart_set_div_line_count(chart, 5, 5);

// Add data series
lv_chart_series_t * ser1 = lv_chart_add_series(chart,
    lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
lv_chart_series_t * ser2 = lv_chart_add_series(chart,
    lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

// Update data points
for (int i = 0; i < 50; i++) {
    lv_chart_set_next_value(chart, ser1, rand() % 100);
    lv_chart_set_next_value(chart, ser2, rand() % 100);
}

// Refresh display
lv_chart_refresh(chart);
```

### 2.4 Custom Widget Development

```c
// Step 1: Define custom widget type
typedef struct {
    lv_obj_t obj;           // Base object (MUST be first)
    int32_t custom_value;   // Custom property
    lv_color_t custom_color;
} my_widget_t;

// Step 2: Create constructor
static const lv_obj_class_t my_widget_class;

lv_obj_t * my_widget_create(lv_obj_t * parent) {
    lv_obj_t * obj = lv_obj_class_create_obj(&my_widget_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

// Step 3: Define class
static void my_widget_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj) {
    LV_UNUSED(class_p);
    my_widget_t * widget = (my_widget_t *)obj;
    widget->custom_value = 0;
    widget->custom_color = lv_color_hex(0x3498db);

    lv_obj_set_size(obj, 100, 100);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static void my_widget_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj) {
    LV_UNUSED(class_p);
    // Cleanup custom resources
}

static void my_widget_event(const lv_obj_class_t * class_p, lv_event_t * e) {
    LV_UNUSED(class_p);
    lv_res_t res = lv_obj_event_base(&my_widget_class, e);
    if (res != LV_RES_OK) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if (code == LV_EVENT_DRAW_MAIN) {
        // Custom drawing
        lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
        my_widget_draw_main(obj, draw_ctx);
    }
}

static const lv_obj_class_t my_widget_class = {
    .constructor_cb = my_widget_constructor,
    .destructor_cb = my_widget_destructor,
    .event_cb = my_widget_event,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(my_widget_t),
    .base_class = &lv_obj_class
};
```

---

## 3. Styling System

### 3.1 Style Architecture

```c
// Create reusable style
static lv_style_t style_btn;
static lv_style_t style_btn_pressed;
static lv_style_t style_btn_disabled;

void styles_init(void) {
    // Base button style
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 8);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x3498db));
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn, lv_color_hex(0x2980b9));
    lv_style_set_border_width(&style_btn, 2);
    lv_style_set_pad_all(&style_btn, 10);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_shadow_width(&style_btn, 10);
    lv_style_set_shadow_color(&style_btn, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_btn, LV_OPA_30);

    // Pressed state
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0x2980b9));
    lv_style_set_shadow_ofs_y(&style_btn_pressed, 2);
    lv_style_set_translate_y(&style_btn_pressed, 2);

    // Disabled state
    lv_style_init(&style_btn_disabled);
    lv_style_set_bg_opa(&style_btn_disabled, LV_OPA_50);
    lv_style_set_text_opa(&style_btn_disabled, LV_OPA_50);
}

// Apply styles
lv_obj_add_style(btn, &style_btn, LV_STATE_DEFAULT);
lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
lv_obj_add_style(btn, &style_btn_disabled, LV_STATE_DISABLED);
```

### 3.2 Style Properties Reference

| Category | Properties | Values |
|----------|-----------|--------|
| **Background** | bg_color, bg_opa, bg_grad_color, bg_grad_dir | color, 0-255, color, LV_GRAD_DIR_* |
| **Border** | border_color, border_width, border_opa, border_side | color, px, 0-255, LV_BORDER_SIDE_* |
| **Outline** | outline_color, outline_width, outline_pad, outline_opa | color, px, px, 0-255 |
| **Shadow** | shadow_color, shadow_width, shadow_spread, shadow_ofs_x/y | color, px, px, px |
| **Padding** | pad_top, pad_bottom, pad_left, pad_right, pad_row, pad_column | px |
| **Size** | width, height, min_width, max_width, min_height, max_height | px, LV_PCT(), LV_SIZE_CONTENT |
| **Transform** | transform_width, transform_height, translate_x/y, transform_zoom, transform_angle | px, 0-256, deg |
| **Text** | text_color, text_font, text_letter_space, text_line_space, text_align | color, &font, px, px, LV_TEXT_ALIGN_* |

### 3.3 Theme System

```c
// Create custom theme
static lv_theme_t * my_theme;

void my_theme_init(lv_disp_t * disp) {
    // Get base theme
    lv_theme_t * base_theme = lv_theme_default_init(
        disp,
        lv_palette_main(LV_PALETTE_BLUE),   // Primary color
        lv_palette_main(LV_PALETTE_CYAN),   // Secondary color
        true,                                // Dark mode
        &lv_font_montserrat_14              // Default font
    );

    // Create derived theme
    static lv_theme_t theme;
    lv_theme_set_parent(&theme, base_theme);
    theme.apply_cb = my_theme_apply_cb;

    lv_disp_set_theme(disp, &theme);
}

static void my_theme_apply_cb(lv_theme_t * th, lv_obj_t * obj) {
    LV_UNUSED(th);

    if (lv_obj_check_type(obj, &lv_btn_class)) {
        lv_obj_add_style(obj, &style_btn, LV_STATE_DEFAULT);
        lv_obj_add_style(obj, &style_btn_pressed, LV_STATE_PRESSED);
    }
    else if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_obj_add_style(obj, &style_label, LV_STATE_DEFAULT);
    }
}
```

---

## 4. Animation Engine

### 4.1 Basic Animations

```c
// Simple property animation
static void anim_x_cb(void * var, int32_t v) {
    lv_obj_set_x((lv_obj_t *)var, v);
}

void animate_object(lv_obj_t * obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, 0, 200);
    lv_anim_set_time(&a, 500);           // Duration in ms
    lv_anim_set_delay(&a, 100);          // Start delay
    lv_anim_set_playback_time(&a, 300);  // Return animation
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_repeat_count(&a, 3);     // LV_ANIM_REPEAT_INFINITE
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}
```

### 4.2 Animation Paths (Easing)

| Path Function | Description | Use Case |
|---------------|-------------|----------|
| `lv_anim_path_linear` | Constant speed | Loading bars |
| `lv_anim_path_ease_in` | Slow start | Exit animations |
| `lv_anim_path_ease_out` | Slow end | Entry animations |
| `lv_anim_path_ease_in_out` | Slow start/end | Natural motion |
| `lv_anim_path_overshoot` | Bounce past target | Playful UI |
| `lv_anim_path_bounce` | Bounce at end | Attention grab |
| `lv_anim_path_step` | Instant jump | Digital displays |

### 4.3 Complex Animation Sequences

```c
// Animation timeline with callbacks
static void anim_ready_cb(lv_anim_t * a) {
    LV_LOG_USER("Animation completed");
}

static void anim_start_cb(lv_anim_t * a) {
    LV_LOG_USER("Animation started");
}

void create_animation_sequence(lv_obj_t * obj) {
    // Phase 1: Fade in
    lv_anim_t a1;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, obj);
    lv_anim_set_exec_cb(&a1, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_values(&a1, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a1, 300);
    lv_anim_set_start_cb(&a1, anim_start_cb);

    // Phase 2: Scale up (starts after fade)
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, obj);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_zoom);
    lv_anim_set_values(&a2, 128, 256);  // 0.5x to 1x scale
    lv_anim_set_time(&a2, 400);
    lv_anim_set_delay(&a2, 300);  // Wait for fade
    lv_anim_set_path_cb(&a2, lv_anim_path_overshoot);
    lv_anim_set_ready_cb(&a2, anim_ready_cb);

    // Start both (phase 2 delayed)
    lv_anim_start(&a1);
    lv_anim_start(&a2);
}
```

### 4.4 Screen Transitions

```c
// Animated screen transition
void transition_to_screen(lv_obj_t * new_scr) {
    // Slide-in from right
    lv_scr_load_anim(new_scr,
                     LV_SCR_LOAD_ANIM_MOVE_LEFT,
                     300,   // Duration
                     0,     // Delay
                     true); // Auto-delete old screen
}

// Available transitions:
// LV_SCR_LOAD_ANIM_NONE
// LV_SCR_LOAD_ANIM_OVER_LEFT / RIGHT / TOP / BOTTOM
// LV_SCR_LOAD_ANIM_MOVE_LEFT / RIGHT / TOP / BOTTOM
// LV_SCR_LOAD_ANIM_FADE_IN / FADE_OUT
// LV_SCR_LOAD_ANIM_OUT_LEFT / RIGHT / TOP / BOTTOM
```

---

## 5. Memory Management

### 5.1 Memory Configuration (lv_conf.h)

```c
// Memory allocation strategy
#define LV_MEM_CUSTOM      0          // Use built-in lv_mem
#define LV_MEM_SIZE        (48U * 1024U)  // 48KB for LVGL heap
#define LV_MEM_ADR         0          // Auto-allocate
#define LV_MEM_POOL_INCLUDE "lvgl/src/misc/lv_mem.h"
#define LV_MEM_POOL_ALLOC   lv_mem_alloc
#define LV_MEM_POOL_FREE    lv_mem_free

// For ESP32 with PSRAM
#define LV_MEM_CUSTOM      1
#define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
#define LV_MEM_CUSTOM_ALLOC(size)  heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#define LV_MEM_CUSTOM_FREE(ptr)    free(ptr)
#define LV_MEM_CUSTOM_REALLOC(ptr, size) heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM)
```

### 5.2 Memory Monitoring

```c
// Check memory usage
void print_mem_usage(void) {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);

    LV_LOG_USER("LVGL Memory:");
    LV_LOG_USER("  Total: %d bytes", mon.total_size);
    LV_LOG_USER("  Free: %d bytes (%d%%)", mon.free_size, mon.free_biggest_size);
    LV_LOG_USER("  Used: %d bytes (%d%%)", mon.total_size - mon.free_size,
                (100 * (mon.total_size - mon.free_size)) / mon.total_size);
    LV_LOG_USER("  Fragments: %d", mon.frag_pct);
    LV_LOG_USER("  Max used: %d bytes", mon.max_used);
}

// Trigger defragmentation
lv_mem_defrag();
```

### 5.3 Memory Optimization Techniques

```c
// 1. Use static styles (defined once, reused everywhere)
static lv_style_t style_common;  // DON'T create in loops

// 2. Delete objects when not needed
lv_obj_del(obj);  // Removes object and all children

// 3. Use object pooling for repeated widgets
typedef struct {
    lv_obj_t * items[MAX_LIST_ITEMS];
    uint8_t active_count;
} list_pool_t;

void pool_recycle(list_pool_t * pool, lv_obj_t * item) {
    lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
    // Don't delete, just hide for reuse
}

lv_obj_t * pool_acquire(list_pool_t * pool) {
    // Return hidden item or create new
    for (int i = 0; i < pool->active_count; i++) {
        if (lv_obj_has_flag(pool->items[i], LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(pool->items[i], LV_OBJ_FLAG_HIDDEN);
            return pool->items[i];
        }
    }
    // Create new if pool not full
    if (pool->active_count < MAX_LIST_ITEMS) {
        pool->items[pool->active_count++] = lv_obj_create(parent);
        return pool->items[pool->active_count - 1];
    }
    return NULL;
}

// 4. Compress images at build time
// Use LV_IMG_CF_TRUE_COLOR_ALPHA for transparency
// Use LV_IMG_CF_INDEXED_4BIT for simple graphics (16 colors)

// 5. Font subsetting - only include needed characters
// Use LVGL font converter: https://lvgl.io/tools/fontconverter
```

### 5.4 Buffer Strategies

```c
// Single buffer (minimum memory, some tearing)
static lv_color_t buf[DISP_HOR_RES * 10];  // 10 lines
lv_disp_draw_buf_init(&draw_buf, buf, NULL, DISP_HOR_RES * 10);

// Double buffer (no tearing, 2x memory)
static lv_color_t buf1[DISP_HOR_RES * 20];
static lv_color_t buf2[DISP_HOR_RES * 20];
lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_HOR_RES * 20);

// Full-frame double buffer (best quality, highest memory)
// Only for devices with plenty of RAM or PSRAM
static lv_color_t buf1[DISP_HOR_RES * DISP_VER_RES];
static lv_color_t buf2[DISP_HOR_RES * DISP_VER_RES];
lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_HOR_RES * DISP_VER_RES);
```

---

## 6. UI/UX Design Principles for Embedded

### 6.1 Touch Target Guidelines

```
┌─────────────────────────────────────────────────────────────┐
│                Touch Target Sizes                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Minimum: 44x44 px (Apple HIG) / 48x48 dp (Material)        │
│  Recommended: 48x48 px for embedded displays                 │
│  Spacing: 8px minimum between targets                        │
│                                                              │
│  ┌────────────┐  8px  ┌────────────┐                        │
│  │            │  gap  │            │                        │
│  │   48x48    │◄────►│   48x48    │                        │
│  │            │       │            │                        │
│  └────────────┘       └────────────┘                        │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```c
// Enforce minimum touch targets
#define MIN_TOUCH_SIZE 48

lv_obj_t * btn = lv_btn_create(parent);
lv_obj_set_size(btn, LV_MAX(MIN_TOUCH_SIZE, desired_width),
                     LV_MAX(MIN_TOUCH_SIZE, desired_height));

// Add invisible touch padding for small visual elements
lv_obj_set_ext_click_area(small_icon, 20);  // Extends touch area
```

### 6.2 Visual Hierarchy

```c
// Typography scale (8px base unit)
#define FONT_DISPLAY   &lv_font_montserrat_32  // Headlines
#define FONT_TITLE     &lv_font_montserrat_24  // Section titles
#define FONT_BODY      &lv_font_montserrat_16  // Body text
#define FONT_CAPTION   &lv_font_montserrat_12  // Labels, captions

// Color hierarchy
#define COLOR_PRIMARY    lv_color_hex(0x2196F3)  // Actions
#define COLOR_SECONDARY  lv_color_hex(0x9E9E9E)  // Secondary text
#define COLOR_SURFACE    lv_color_hex(0x1E1E1E)  // Backgrounds
#define COLOR_ON_SURFACE lv_color_hex(0xFFFFFF)  // Text on surface
#define COLOR_ERROR      lv_color_hex(0xF44336)  // Errors
#define COLOR_SUCCESS    lv_color_hex(0x4CAF50)  // Success

// Spacing scale (4px base)
#define SPACE_XS  4
#define SPACE_SM  8
#define SPACE_MD  16
#define SPACE_LG  24
#define SPACE_XL  32
```

### 6.3 Information Architecture

```c
// Screen layout template
/*
┌────────────────────────────────────┐
│ Status Bar (32px)                  │  ← System info, time, battery
├────────────────────────────────────┤
│ Header (48px)                      │  ← Title, navigation
├────────────────────────────────────┤
│                                    │
│                                    │
│ Content Area                       │  ← Main content, scrollable
│ (Remaining height)                 │
│                                    │
│                                    │
├────────────────────────────────────┤
│ Navigation Bar (56px)              │  ← Primary actions
└────────────────────────────────────┘
*/

void create_app_layout(void) {
    lv_obj_t * scr = lv_scr_act();

    // Status bar
    lv_obj_t * status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, lv_pct(100), 32);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr);
    lv_obj_set_size(header, lv_pct(100), 48);
    lv_obj_align_to(header, status_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    // Navigation bar
    lv_obj_t * nav_bar = lv_obj_create(scr);
    lv_obj_set_size(nav_bar, lv_pct(100), 56);
    lv_obj_align(nav_bar, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Content area (fills remaining space)
    lv_obj_t * content = lv_obj_create(scr);
    lv_obj_set_size(content, lv_pct(100),
                    lv_pct(100) - 32 - 48 - 56);
    lv_obj_align_to(content, header, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
}
```

### 6.4 Feedback and Microinteractions

```c
// Visual feedback for button press
static lv_style_t style_pressed;
lv_style_init(&style_pressed);
lv_style_set_transform_zoom(&style_pressed, 240);  // 94% scale
lv_style_set_bg_color(&style_pressed, lv_color_darken(base_color, 20));

// Ripple effect (simplified)
void ripple_effect(lv_obj_t * obj, lv_point_t touch_point) {
    lv_obj_t * ripple = lv_obj_create(obj);
    lv_obj_remove_style_all(ripple);
    lv_obj_set_size(ripple, 10, 10);
    lv_obj_set_pos(ripple, touch_point.x - 5, touch_point.y - 5);
    lv_obj_set_style_radius(ripple, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ripple, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ripple, LV_OPA_50, 0);

    // Animate expansion
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ripple);
    lv_anim_set_exec_cb(&a, ripple_size_cb);
    lv_anim_set_values(&a, 10, 100);
    lv_anim_set_time(&a, 300);
    lv_anim_set_deleted_cb(&a, ripple_delete_cb);
    lv_anim_start(&a);
}

// Haptic feedback (if hardware supports)
void haptic_tap(void) {
    // Trigger haptic motor for 10ms
    gpio_set_level(HAPTIC_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(HAPTIC_PIN, 0);
}
```

### 6.5 Loading States

```c
// Spinner for async operations
lv_obj_t * show_loading(lv_obj_t * parent, const char * message) {
    // Overlay
    lv_obj_t * overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);

    // Card
    lv_obj_t * card = lv_obj_create(overlay);
    lv_obj_set_size(card, 160, 120);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 12, 0);

    // Spinner
    lv_obj_t * spinner = lv_spinner_create(card, 1000, 60);
    lv_obj_set_size(spinner, 48, 48);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 16);

    // Message
    lv_obj_t * label = lv_label_create(card);
    lv_label_set_text(label, message);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -16);

    return overlay;
}

void hide_loading(lv_obj_t * overlay) {
    lv_obj_del_async(overlay);
}
```

---

## 7. Accessibility Standards

### 7.1 Contrast Requirements

```c
// WCAG 2.1 contrast ratios
// Level AA: 4.5:1 for normal text, 3:1 for large text (18pt+)
// Level AAA: 7:1 for normal text, 4.5:1 for large text

// High contrast color pairs
static const struct {
    lv_color_t bg;
    lv_color_t fg;
    float ratio;
} accessible_pairs[] = {
    { LV_COLOR_MAKE(0x00, 0x00, 0x00), LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 21.0 },  // Black/White
    { LV_COLOR_MAKE(0x1A, 0x1A, 0x2E), LV_COLOR_MAKE(0xE9, 0xE9, 0xE9), 12.5 },  // Dark blue/Light gray
    { LV_COLOR_MAKE(0x16, 0x21, 0x3E), LV_COLOR_MAKE(0xFF, 0xD7, 0x00), 10.8 },  // Navy/Gold
};

// Verify contrast programmatically
float calculate_contrast(lv_color_t fg, lv_color_t bg) {
    float l1 = relative_luminance(fg);
    float l2 = relative_luminance(bg);
    return (LV_MAX(l1, l2) + 0.05) / (LV_MIN(l1, l2) + 0.05);
}
```

### 7.2 Focus Indicators

```c
// Visible focus for keyboard/rotary navigation
static lv_style_t style_focused;
lv_style_init(&style_focused);
lv_style_set_outline_color(&style_focused, lv_palette_main(LV_PALETTE_BLUE));
lv_style_set_outline_width(&style_focused, 3);
lv_style_set_outline_pad(&style_focused, 2);
lv_style_set_outline_opa(&style_focused, LV_OPA_COVER);

// Apply to all interactive elements
lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
```

### 7.3 Motor Accessibility

```c
// Larger touch targets for users with motor impairments
#define ACCESSIBLE_TOUCH_SIZE 56  // Larger than standard 48px

// Sticky hover states (don't require precise holding)
static void drag_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    // Only activate on intentional movement (> 10px)
    int32_t dist = LV_ABS(vect.x) + LV_ABS(vect.y);
    if (dist > 10) {
        // User intentionally moved, perform action
    }
}

// Adjustable timing for press-and-hold actions
#define LONG_PRESS_MIN_MS 500
#define LONG_PRESS_MAX_MS 2000  // Accessibility mode

void set_accessible_timing(bool enabled) {
    lv_indev_t * indev = lv_indev_get_act();
    lv_indev_set_long_press_time(indev,
        enabled ? LONG_PRESS_MAX_MS : LONG_PRESS_MIN_MS);
}
```

---

## 8. Performance Optimization

### 8.1 Rendering Optimization

```c
// 1. Reduce redraw areas
lv_obj_invalidate(obj);         // Only invalidate changed object
// vs
lv_obj_invalidate(lv_scr_act()); // Full screen redraw (expensive!)

// 2. Use caching for complex objects
lv_obj_add_flag(complex_widget, LV_OBJ_FLAG_CACHED);

// 3. Disable animations on low-power mode
void set_low_power_mode(bool enabled) {
    if (enabled) {
        lv_anim_speed_set(0);  // Instant animations
        lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);  // No transform
    } else {
        lv_anim_speed_set(100);  // Normal speed
    }
}

// 4. Optimize image format at compile time
LV_IMG_DECLARE(my_image);  // Pre-converted with lvgl_image_converter
// Use INDEXED_4BIT (16 colors) for icons: 4x smaller than TRUE_COLOR
// Use TRUE_COLOR_CHROMA_KEYED for sprites (no alpha channel)
```

### 8.2 FPS Monitoring

```c
// Performance monitor
static uint32_t frame_count = 0;
static uint32_t last_fps_time = 0;

void perf_monitor_tick(void) {
    frame_count++;

    uint32_t now = lv_tick_get();
    if (now - last_fps_time >= 1000) {
        uint32_t fps = frame_count * 1000 / (now - last_fps_time);
        LV_LOG_USER("FPS: %d, Mem: %d%%", fps,
                    100 - lv_mem_monitor().free_biggest_size * 100 /
                    lv_mem_monitor().total_size);

        frame_count = 0;
        last_fps_time = now;
    }
}

// Call in main loop
while (1) {
    lv_timer_handler();
    perf_monitor_tick();
    vTaskDelay(pdMS_TO_TICKS(5));
}
```

### 8.3 DMA Optimization (ESP32)

```c
// Use DMA for display transfers
static void disp_flush_dma(lv_disp_drv_t * drv, const lv_area_t * area,
                           lv_color_t * color_p) {
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);

    // Start DMA transfer (non-blocking)
    spi_transaction_t t = {
        .tx_buffer = color_p,
        .length = size * sizeof(lv_color_t) * 8,
        .flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL
    };
    spi_device_queue_trans(spi_handle, &t, portMAX_DELAY);

    // IMPORTANT: Don't call lv_disp_flush_ready() here!
    // Call it in DMA complete ISR
}

// DMA complete callback
static IRAM_ATTR void dma_complete_isr(void * arg) {
    lv_disp_drv_t * drv = (lv_disp_drv_t *)arg;
    lv_disp_flush_ready(drv);
}
```

---

## 9. Design System Implementation

### 9.1 Component Library Structure

```c
// components/button.h
#ifndef UI_BUTTON_H
#define UI_BUTTON_H

typedef enum {
    BTN_VARIANT_PRIMARY,
    BTN_VARIANT_SECONDARY,
    BTN_VARIANT_GHOST,
    BTN_VARIANT_DANGER
} btn_variant_t;

typedef enum {
    BTN_SIZE_SM,    // 32px height
    BTN_SIZE_MD,    // 44px height
    BTN_SIZE_LG     // 56px height
} btn_size_t;

typedef struct {
    btn_variant_t variant;
    btn_size_t size;
    const char * label;
    const lv_img_dsc_t * icon;
    bool disabled;
    lv_event_cb_t on_click;
    void * user_data;
} btn_config_t;

lv_obj_t * ui_button_create(lv_obj_t * parent, const btn_config_t * config);

#endif

// components/button.c
lv_obj_t * ui_button_create(lv_obj_t * parent, const btn_config_t * config) {
    lv_obj_t * btn = lv_btn_create(parent);

    // Size
    int32_t height = (config->size == BTN_SIZE_SM) ? 32 :
                     (config->size == BTN_SIZE_MD) ? 44 : 56;
    lv_obj_set_height(btn, height);
    lv_obj_set_width(btn, LV_SIZE_CONTENT);

    // Style variant
    switch (config->variant) {
        case BTN_VARIANT_PRIMARY:
            lv_obj_add_style(btn, &style_btn_primary, 0);
            break;
        case BTN_VARIANT_SECONDARY:
            lv_obj_add_style(btn, &style_btn_secondary, 0);
            break;
        case BTN_VARIANT_GHOST:
            lv_obj_add_style(btn, &style_btn_ghost, 0);
            break;
        case BTN_VARIANT_DANGER:
            lv_obj_add_style(btn, &style_btn_danger, 0);
            break;
    }

    // Content
    lv_obj_t * container = lv_obj_create(btn);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (config->icon) {
        lv_obj_t * icon = lv_img_create(container);
        lv_img_set_src(icon, config->icon);
    }

    if (config->label) {
        lv_obj_t * label = lv_label_create(container);
        lv_label_set_text(label, config->label);
    }

    // State
    if (config->disabled) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    }

    // Events
    if (config->on_click) {
        lv_obj_add_event_cb(btn, config->on_click,
                           LV_EVENT_CLICKED, config->user_data);
    }

    return btn;
}
```

### 9.2 Design Tokens

```c
// tokens/colors.h
#define TOKEN_COLOR_PRIMARY_50   lv_color_hex(0xE3F2FD)
#define TOKEN_COLOR_PRIMARY_100  lv_color_hex(0xBBDEFB)
#define TOKEN_COLOR_PRIMARY_500  lv_color_hex(0x2196F3)
#define TOKEN_COLOR_PRIMARY_700  lv_color_hex(0x1976D2)
#define TOKEN_COLOR_PRIMARY_900  lv_color_hex(0x0D47A1)

// tokens/spacing.h
#define TOKEN_SPACE_0   0
#define TOKEN_SPACE_1   4
#define TOKEN_SPACE_2   8
#define TOKEN_SPACE_3   12
#define TOKEN_SPACE_4   16
#define TOKEN_SPACE_5   20
#define TOKEN_SPACE_6   24
#define TOKEN_SPACE_8   32
#define TOKEN_SPACE_10  40

// tokens/typography.h
#define TOKEN_FONT_DISPLAY_LG    &lv_font_montserrat_32
#define TOKEN_FONT_DISPLAY_MD    &lv_font_montserrat_28
#define TOKEN_FONT_HEADING_LG    &lv_font_montserrat_24
#define TOKEN_FONT_HEADING_MD    &lv_font_montserrat_20
#define TOKEN_FONT_BODY_LG       &lv_font_montserrat_16
#define TOKEN_FONT_BODY_MD       &lv_font_montserrat_14
#define TOKEN_FONT_LABEL_LG      &lv_font_montserrat_12
#define TOKEN_FONT_LABEL_MD      &lv_font_montserrat_10
```

---

## 10. Common Patterns & Solutions

### 10.1 List with Infinite Scroll

```c
#define VISIBLE_ITEMS 10
#define ITEM_HEIGHT 60

static int32_t total_items = 1000;
static int32_t scroll_offset = 0;

static void list_scroll_cb(lv_event_t * e) {
    lv_obj_t * list = lv_event_get_target(e);
    int32_t scroll_y = lv_obj_get_scroll_y(list);

    int32_t new_offset = scroll_y / ITEM_HEIGHT;
    if (new_offset != scroll_offset) {
        scroll_offset = new_offset;
        update_visible_items(list, scroll_offset);
    }
}

static void update_visible_items(lv_obj_t * list, int32_t offset) {
    // Only render visible items + 2 buffer items
    for (int i = 0; i < VISIBLE_ITEMS + 2; i++) {
        int32_t item_index = offset + i;
        if (item_index < total_items) {
            lv_obj_t * item = get_or_create_item(list, i);
            populate_item(item, item_index);
            lv_obj_set_y(item, item_index * ITEM_HEIGHT);
        }
    }
}
```

### 10.2 Form Validation

```c
typedef struct {
    lv_obj_t * textarea;
    const char * label;
    bool (*validator)(const char *);
    char error_msg[64];
} form_field_t;

bool validate_email(const char * text) {
    // Simple email validation
    return strchr(text, '@') != NULL && strchr(text, '.') != NULL;
}

bool validate_not_empty(const char * text) {
    return strlen(text) > 0;
}

static void validate_form(form_field_t * fields, int count) {
    bool all_valid = true;

    for (int i = 0; i < count; i++) {
        const char * text = lv_textarea_get_text(fields[i].textarea);
        bool valid = fields[i].validator(text);

        if (!valid) {
            all_valid = false;
            lv_obj_add_state(fields[i].textarea, LV_STATE_USER_1);  // Error state
            show_field_error(fields[i].textarea, fields[i].error_msg);
        } else {
            lv_obj_clear_state(fields[i].textarea, LV_STATE_USER_1);
            hide_field_error(fields[i].textarea);
        }
    }

    lv_obj_t * submit_btn = /* ... */;
    if (all_valid) {
        lv_obj_clear_state(submit_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(submit_btn, LV_STATE_DISABLED);
    }
}
```

### 10.3 Navigation Stack

```c
#define MAX_SCREEN_STACK 10

typedef struct {
    lv_obj_t * screens[MAX_SCREEN_STACK];
    int8_t top;
} nav_stack_t;

static nav_stack_t nav_stack = { .top = -1 };

void nav_push(lv_obj_t * screen) {
    if (nav_stack.top < MAX_SCREEN_STACK - 1) {
        nav_stack.top++;
        nav_stack.screens[nav_stack.top] = screen;
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    }
}

void nav_pop(void) {
    if (nav_stack.top > 0) {
        lv_obj_t * current = nav_stack.screens[nav_stack.top];
        nav_stack.top--;
        lv_obj_t * prev = nav_stack.screens[nav_stack.top];

        lv_scr_load_anim(prev, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
        lv_obj_del_async(current);
    }
}

void nav_pop_to_root(void) {
    while (nav_stack.top > 0) {
        lv_obj_t * current = nav_stack.screens[nav_stack.top];
        nav_stack.top--;
        lv_obj_del_async(current);
    }

    if (nav_stack.top >= 0) {
        lv_scr_load_anim(nav_stack.screens[0], LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    }
}
```

---

## 11. Troubleshooting Guide

| Problem | Cause | Solution |
|---------|-------|----------|
| Flickering display | Single buffer | Use double buffering |
| Slow touch response | Blocking main loop | Use `lv_timer_handler()` in task |
| Memory leak | Objects not deleted | Call `lv_obj_del()` on removal |
| Corrupt display | Wrong color format | Match `LV_COLOR_DEPTH` to display |
| Animations choppy | Too many active | Limit concurrent animations to 5 |
| Font rendering wrong | Missing chars | Use font converter with full range |
| Touch offset | Calibration needed | Implement touch calibration |
| White screen | No flush callback | Implement `disp_flush_cb()` |
| Objects disappear | Parent deleted | Check object hierarchy |
| Style not applied | Wrong state | Verify `LV_STATE_*` flags |

---

## 12. Checklist: UI/UX Audit

- [ ] **Touch targets** minimum 48x48px with 8px spacing
- [ ] **Contrast ratios** meet WCAG 2.1 AA (4.5:1 text, 3:1 graphics)
- [ ] **Focus indicators** visible for keyboard/rotary navigation
- [ ] **Loading states** shown for operations >300ms
- [ ] **Error feedback** clear, actionable messages
- [ ] **Consistent spacing** using design token scale
- [ ] **Typography hierarchy** with 3-4 font sizes max
- [ ] **Color usage** limited to 5-7 colors with semantic meaning
- [ ] **Animations** duration 150-500ms with easing
- [ ] **Memory stable** after 10 minutes of interaction
- [ ] **60 FPS** maintained during animations
- [ ] **Responsive layout** adapts to screen size

---

## Resources

- **LVGL Docs**: https://docs.lvgl.io
- **Font Converter**: https://lvgl.io/tools/fontconverter
- **Image Converter**: https://lvgl.io/tools/imageconverter
- **Simulator**: https://github.com/lvgl/lv_port_pc_eclipse
- **Forum**: https://forum.lvgl.io
- **Material Design**: https://m3.material.io
- **Apple HIG**: https://developer.apple.com/design/human-interface-guidelines
