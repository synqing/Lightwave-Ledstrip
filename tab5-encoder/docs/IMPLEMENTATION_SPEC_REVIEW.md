---
abstract: "LVGL 9.3 feasibility review of IMPLEMENTATION_SPEC.md. Covers 11 areas: layout, screen architecture, backlit slots, neon glow, colour switching, bar fill, separator, mode buttons, phase ordering, WDT, and gaps. 5 BLOCKERs, 6 RISKs, 6 CLARIFICATIONs, 5 OKs found. Read before implementing any phase."
---

# Implementation Spec Review — LVGL 9.3 Feasibility

**Reviewer:** LVGL UI/UX Specialist Agent
**Date:** 2026-03-22
**Spec reviewed:** `tab5-encoder/docs/IMPLEMENTATION_SPEC.md`
**Reference:** `tab5-encoder/docs/reference/lvgl-component-reference.md`
**Source files:** `DisplayUI.cpp`, `DisplayUI.h`, `lvgl_bridge.cpp`, `ControlSurfaceUI.h`, `lv_conf.h`

---

## 1. LVGL Layout Feasibility

### 1.1 Nested Flex Containers

**Verdict: OK**

LVGL 9.3 fully supports nested flex containers. The spec's proposed layout is:

```
outer (flex COLUMN)
  +-- header (full width)
  +-- middle (flex ROW)
  |     +-- sidebar (70px fixed)
  |     +-- content area (flex-grow 1)
  +-- footer (full width)
```

This works. `lv_conf.h` has `LV_USE_FLEX 1` and `LV_USE_GRID 1`. The outer container uses `LV_FLEX_FLOW_COLUMN`, the middle section uses `LV_FLEX_FLOW_ROW`. LVGL resolves nested flex layouts correctly.

**However**, the current codebase does NOT use flex for the outer layout. It uses absolute positioning (`lv_obj_set_pos`, `lv_obj_align` with `LV_ALIGN_TOP_MID`). The spec does not state which approach to use.

### 1.2 Sidebar Height Calculation

**Verdict: RISK**

The sidebar must span from below the header to above the footer. In LVGL flex layout, this can be achieved by giving the middle section `lv_obj_set_flex_grow(middle, 1)` so it fills the remaining vertical space. Then the sidebar inside the middle section gets `lv_obj_set_height(sidebar, lv_pct(100))`.

**Risk:** `lv_pct(100)` resolves to the content area of the parent, which accounts for padding. If the middle section has padding, the sidebar will be shorter than expected. Alternatively, calculate explicitly:

```c
int32_t middle_h = 720 - header_h - footer_h - header_gap - footer_gap;
lv_obj_set_height(sidebar, middle_h);
```

The explicit pixel approach is safer for a fixed 1280x720 display and matches the codebase's existing pattern. The spec should state which approach.

### 1.3 Flex vs Absolute Positioning

**Verdict: CLARIFICATION**

**Current approach:** Absolute positioning. Every widget has hardcoded pixel coordinates. `_gauges_container` is placed at `(TOP_MID, 0, contentTop)`, `_fx_row_a_container` at `nextRowY`, etc.

**Flex approach (spec implies):** Outer column flex for the header/middle/footer stacking.

**Recommendation for this project:** Use absolute positioning with hardcoded coordinates, matching the existing codebase pattern. Reasons:

1. The display is fixed at 1280x720 — no responsive scaling needed (anti-pattern #12 in the reference doc)
2. The existing code is entirely absolute-positioned — introducing flex for the outer shell while keeping absolute inside creates two layout paradigms
3. Debugging absolute layout on hardware is simpler — every pixel is deterministic
4. Flex layout adds computation overhead in `lv_timer_handler()` for layout recalculation

**The spec must decide: flex or absolute.** Do not leave this to the implementer.

---

## 2. Screen Architecture

### 2.1 Tab Switching via LV_OBJ_FLAG_HIDDEN

**Verdict: OK**

Using `lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN)` / `lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN)` is the correct LVGL approach for tab-like content switching within a single screen. This is:

- Cheaper than `lv_scr_load()` (no screen transition overhead)
- Cheaper than destroying/recreating widgets (no allocation/deallocation)
- Used throughout the LVGL ecosystem for exactly this purpose

Hidden objects are excluded from layout calculations and rendering. They consume memory but zero CPU during `lv_timer_handler()`.

`lv_tabview` (`LV_USE_TABVIEW`) is disabled in `lv_conf.h` (line 83: `#define LV_USE_TABVIEW 0`). Even if enabled, `lv_tabview` adds its own tab bar UI and animation overhead that conflicts with the custom sidebar design. Using `LV_OBJ_FLAG_HIDDEN` is the correct choice.

### 2.2 Memory Impact: ~40 Widgets

**Verdict: OK**

Widget memory footprint in LVGL 9.3:
- Base `lv_obj_t`: ~100-140 bytes (depends on flags/styles)
- `lv_label`: +60-80 bytes (text buffer is separate, usually static strings)
- `lv_bar`: +40-60 bytes
- Style data: shared via `lv_style_t` pointers (no per-widget copy)

Estimate for the proposed layout:
- 24 bottom-zone cards (3 panels x 8): ~24 x (140 + 80 + 80 + 60) = ~8.6 KB
- 8 gauge cards (existing): ~2.9 KB
- 6 mode buttons: ~1.2 KB
- Sidebar (3 slots + container): ~0.8 KB
- Zone selector (3 buttons): ~0.6 KB
- Containers/panels: ~1.0 KB
- **Total new allocation: ~15 KB**

`lv_conf.h` allocates 200 KB for LVGL heap (`LV_MEM_SIZE = 200 * 1024U`). The draw buffer is allocated from PSRAM separately. Current usage is approximately 40-60 KB (4 screens, ~80 existing widgets). Adding 15 KB is well within budget.

**No memory concern on ESP32-P4 with PSRAM.**

### 2.3 Control Surface Screen Fate

**Verdict: BLOCKER**

The spec says "FX PARAMS tab" absorbs the ControlSurface functionality (effect-specific parameters on encoders 8-15). But:

1. The spec's file manifest lists `ControlSurfaceUI.cpp` as MODIFIED (to use DesignTokens.h), not REMOVED
2. The spec does not say whether `_screen_control_surface` and the `ControlSurfaceUI` class are eliminated, retained as a separate screen, or merged
3. The header's effect container currently navigates to `UIScreen::CONTROL_SURFACE` on tap (line 200-205 of DisplayUI.cpp). This navigation target will be invalid if the screen is removed
4. `ControlSurfaceUI` has its own encoder handling (`handleEncoderChange`, `handleEncoderButton`) and effect parameter metadata system (`onEffectParametersReceived`). The FX PARAMS tab needs this logic but the spec does not say how to reuse it

**The spec MUST explicitly state:**
- Is `ControlSurfaceUI` class deleted, retained, or refactored?
- Does the header effect-tap navigate somewhere else now?
- How does the FX PARAMS tab obtain effect parameter metadata? Does it reuse `ControlSurfaceUI`'s `CSEffectParam` structures and `onEffectParametersReceived` handler?

---

## 3. Backlit Slots Implementation

### 3.1 Inset Shadow (Recessed Groove)

**Verdict: BLOCKER**

The spec calls for: `inset 0 1px 3px rgba(0,0,0,0.6)`

**LVGL 9.3 does NOT support inset shadows.** The shadow system (`lv_obj_set_style_shadow_*`) only draws outer drop shadows. This was confirmed as an unimplemented feature request on the [LVGL Forum](https://forum.lvgl.io/t/inset-shadow-style-property/8056) and has never been added.

**Closest approximation:**

Option A — Layered objects (recommended):
```c
// Dark border + slightly lighter inner area = visual recess
lv_obj_t* groove = lv_obj_create(sidebar);
lv_obj_set_size(groove, 56, 56);
lv_obj_set_style_bg_color(groove, lv_color_hex(0x040405), 0);  // Darker than sidebar bg
lv_obj_set_style_border_width(groove, 1, 0);
lv_obj_set_style_border_color(groove, lv_color_hex(0x000000), 0);
lv_obj_set_style_border_opa(groove, LV_OPA_60, 0);
lv_obj_set_style_radius(groove, 6, 0);
```

Option B — Top/left dark border + bottom/right lighter border using `lv_obj_set_style_border_side()`:
```c
// LVGL 9 supports per-side borders
lv_obj_set_style_border_side(groove, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT, 0);
lv_obj_set_style_border_color(groove, lv_color_hex(0x000000), 0);
// Then overlay a second object for bottom/right highlight... (complex, not recommended)
```

**Recommendation:** Use Option A. Accept that the visual depth will be a flattened approximation (dark recessed background + subtle border) rather than a true inset shadow. The mockup's CSS inset shadow has no direct LVGL equivalent.

### 3.2 Radial Gradient Backlight

**Verdict: BLOCKER**

The spec calls for: "radial gradient backlight at 12% opacity"

LVGL 9.x added radial gradient support via `lv_grad_radial_init()`, BUT this requires `LV_DRAW_SW_COMPLEX_GRADIENTS 1` in `lv_conf.h`. The current `lv_conf.h` does NOT define this flag.

**Two sub-issues:**

1. **Config missing:** Adding `#define LV_DRAW_SW_COMPLEX_GRADIENTS 1` to `lv_conf.h` enables the feature but increases code size and draw time. On ESP32-P4 this is likely acceptable, but it must be tested.

2. **API complexity:** Radial gradients in LVGL 9.x use `lv_grad_radial_init()` + `lv_style_set_bg_grad()` which requires creating a gradient descriptor with colour stops. This is significantly more complex than `lv_obj_set_style_bg_color()`.

**Closest approximation without radial gradient:**

```c
// Solid colour at low opacity behind the active tab slot
lv_obj_t* glow_bg = lv_obj_create(tab_slot);
lv_obj_set_size(glow_bg, 60, 50);
lv_obj_center(glow_bg);
lv_obj_set_style_bg_color(glow_bg, neon_colour, 0);
lv_obj_set_style_bg_opa(glow_bg, LV_OPA_12, 0);  // 12% = ~30 of 255
lv_obj_set_style_radius(glow_bg, 25, 0);  // Large radius = pill shape, simulates diffusion
lv_obj_set_style_border_width(glow_bg, 0, 0);
```

This creates a soft coloured pill behind the tab label, approximating a radial glow.

**Recommendation:** Use the solid-colour-pill approximation. If the Captain requires the full radial gradient, add `LV_DRAW_SW_COMPLEX_GRADIENTS 1` to `lv_conf.h` and implement using `lv_grad_radial_init()`. The spec must decide.

### 3.3 Text Shadow Glow

**Verdict: BLOCKER**

The spec calls for: `text-shadow: 0 0 8px <colour>`

**LVGL 9.3 does NOT have a text shadow property.** There is no `lv_style_set_text_shadow_*` function. The `shadow_*` properties apply only to the object's rectangle, not to glyph outlines.

**Closest approximation:**

Option A — Duplicate label behind the primary label at 1-2px offset with lower opacity:
```c
// Shadow label (rendered first, behind)
lv_obj_t* shadow_label = lv_label_create(slot);
lv_label_set_text(shadow_label, "FX");
lv_obj_set_style_text_color(shadow_label, neon_colour, 0);
lv_obj_set_style_text_opa(shadow_label, LV_OPA_30, 0);
lv_obj_set_style_text_font(shadow_label, RAJDHANI_BOLD_24, 0);  // Slightly larger = blur sim
lv_obj_align(shadow_label, LV_ALIGN_CENTER, 0, 0);

// Primary label (rendered on top)
lv_obj_t* label = lv_label_create(slot);
lv_label_set_text(label, "FX");
lv_obj_set_style_text_color(label, neon_colour, 0);
lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, 0);
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
```

Option B — Object shadow on a tight-fit container behind the text:
```c
// Container that wraps text tightly, with coloured shadow
lv_obj_t* glow_container = lv_obj_create(slot);
lv_obj_set_size(glow_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
lv_obj_set_style_shadow_width(glow_container, 8, 0);
lv_obj_set_style_shadow_color(glow_container, neon_colour, 0);
lv_obj_set_style_shadow_opa(glow_container, LV_OPA_COVER, 0);
lv_obj_set_style_bg_opa(glow_container, LV_OPA_TRANSP, 0);
lv_obj_set_style_border_width(glow_container, 0, 0);
```

Option B is simpler but produces a rectangular glow rather than glyph-shaped. For 14px sidebar labels, the difference is negligible at viewing distance.

**Recommendation:** Use Option B (object shadow on a transparent container). The visual effect is close enough for a 70px-wide sidebar with small text. The spec must acknowledge this is an approximation.

### 3.4 Left Accent Bar with Glow

**Verdict: OK**

The 3px left accent bar is achievable:
```c
lv_obj_t* accent = lv_obj_create(slot);
lv_obj_set_size(accent, 3, 40);
lv_obj_align(accent, LV_ALIGN_LEFT_MID, 0, 0);
lv_obj_set_style_bg_color(accent, neon_colour, 0);
lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
lv_obj_set_style_shadow_width(accent, 6, 0);
lv_obj_set_style_shadow_color(accent, neon_colour, 0);
lv_obj_set_style_shadow_opa(accent, LV_OPA_50, 0);
lv_obj_set_style_radius(accent, 1, 0);
```

LVGL outer shadows work well for this effect.

---

## 4. Neon Glow Effects

### 4.1 Box Shadow on Active Cards

**Verdict: RISK**

The spec calls for: `box-shadow: 0 0 4px <colour at 30%>`

LVGL equivalent:
```c
lv_obj_set_style_shadow_width(card, 4, 0);
lv_obj_set_style_shadow_color(card, neon_colour, 0);
lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
lv_obj_set_style_shadow_spread(card, 0, 0);
```

This is natively supported in LVGL 9.3. `shadow_spread` creates expansion, `shadow_width` creates blur. Setting `shadow_spread` to 0 with `shadow_width` at 4 produces a soft outer glow.

**Performance risk:** Shadow rendering is CPU-intensive in the software renderer (`LV_USE_DRAW_SW 1`). For each widget with shadow, LVGL draws the shadow as a separate pass with Gaussian-like blurring. With 8 cards in the bottom zone having active shadows simultaneously, plus the sidebar accent bar, that is 9 shadow-rendered objects per frame.

On ESP32-P4 (dual-core, ~400 MHz), this is likely acceptable for a 16ms refresh period. But:

- Only the ACTIVE card (one at a time) should have the neon shadow. The spec says "active card" (singular), which suggests only the card currently being adjusted glows.
- If ALL 8 bottom-zone cards have shadows simultaneously, measure frame time. Reduce `shadow_width` to 2 if over budget.
- The partial render mode (`LV_DISPLAY_RENDER_MODE_PARTIAL`) helps — only dirty areas are redrawn, so shadows only cost when the card area is invalidated.

**Recommendation:** Apply shadow to maximum 1-2 cards at a time. Verify render time does not exceed 16ms per frame.

### 4.2 Cheaper Active Highlight Alternative

**Verdict: OK (as fallback)**

If shadow performance is a problem:
```c
// Coloured border only, no shadow
lv_obj_set_style_border_color(card, neon_colour, 0);
lv_obj_set_style_border_width(card, 2, 0);  // 2px instead of 1px for visibility
```

This costs essentially nothing. The visual effect is less dramatic but sufficient for indicating the active card. Consider this the fallback if shadow causes frame drops.

---

## 5. Dynamic Colour Switching

**Verdict: OK — Approach A is correct**

The three approaches:

- **Approach A (direct restyle):** Call `lv_obj_set_style_*` on each widget when the tab changes. This is what the spec implies.
- **Approach B (custom states):** Define `LV_STATE_USER_1`, `LV_STATE_USER_2`, `LV_STATE_USER_3` for the three tabs, pre-build styles, apply via `lv_obj_add_state()` / `lv_obj_remove_state()`.
- **Approach C (themes):** Overkill for this use case.

**Approach A is correct for this codebase.** Reasons:

1. Only ~8 bars + 1 active card border change colour on tab switch — that is 9 `lv_obj_set_style_bg_color()` calls, which completes in microseconds
2. The codebase exclusively uses Approach A already (direct style calls everywhere)
3. Approach B has higher setup cost and debugging complexity for minimal benefit with only 3 states
4. Tab switching is a user-initiated action (touch), not a per-frame operation — performance is irrelevant

Implementation:
```c
void onTabSwitch(uint8_t tabIndex) {
    static const uint32_t tabColours[3] = {0xFFD000, 0xBF00FF, 0x00FFCC};
    uint32_t colour = tabColours[tabIndex];

    // Update all Unit B bars
    for (int i = 0; i < 8; i++) {
        lv_obj_set_style_bg_color(bottom_bars[i], lv_color_hex(colour), LV_PART_INDICATOR);
    }
    // Update active card border
    lv_obj_set_style_border_color(active_card, lv_color_hex(colour), 0);
    // Update sidebar glow
    updateSidebarActiveTab(tabIndex);
}
```

---

## 6. Bar Fill Colour

**Verdict: OK**

`lv_obj_set_style_bg_color(bar, colour, LV_PART_INDICATOR)` is the correct API call for changing the bar fill colour at runtime. `LV_PART_INDICATOR` targets the filled portion of `lv_bar`.

The existing code already uses this pattern (DisplayUI.cpp line 415):
```c
lv_obj_set_style_bg_color(_gauge_bars[i], lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
```

No issues.

---

## 7. Separator Line

**Verdict: OK**

Both approaches work:

**Option A — `lv_line_create()`:**
```c
static lv_point_precise_t line_points[] = {{0, 0}, {1210, 0}};
lv_obj_t* sep = lv_line_create(content_area);
lv_line_set_points(sep, line_points, 2);
lv_obj_set_style_line_color(sep, lv_color_hex(0x1E1E24), 0);
lv_obj_set_style_line_width(sep, 1, 0);
```

**Option B — 1px-tall `lv_obj`:**
```c
lv_obj_t* sep = lv_obj_create(content_area);
lv_obj_set_size(sep, 1210, 1);
lv_obj_set_style_bg_color(sep, lv_color_hex(0x1E1E24), 0);
lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
lv_obj_set_style_border_width(sep, 0, 0);
lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
```

**Recommendation: Option B.** It participates correctly in flex/grid layout (has width and height). `lv_line` with `LV_USE_LINE 1` (enabled in lv_conf.h) works but needs absolute point coordinates and does not participate in flex flow. Since the content area may use flex column layout, a 1px-tall object is simpler.

---

## 8. Mode Button Row

### 8.1 Grid Descriptor Change

**Verdict: RISK**

The current action row uses a 5-column grid:
```c
static lv_coord_t action_col_dsc[6] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
```

The spec changes to 6 buttons (ZONES removed, EM SPREAD + EM STRENGTH added). This requires a new 6-column descriptor:
```c
static lv_coord_t mode_col_dsc[7] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};
```

**The `static` keyword is mandatory** (LVGL stores a pointer to the array — anti-pattern #4). The existing 5-column `action_col_dsc` cannot be reused; a new 6-column one is needed.

**Risk: the existing `action_col_dsc` is defined at function scope inside `begin()`.** It is `static` (correct), so the pointer remains valid. However, if the implementer accidentally reuses the old 5-column array for 6 buttons, the 6th button will be placed at column index 5, which is past `LV_GRID_TEMPLATE_LAST` — undefined behaviour, likely a visual corruption or crash.

### 8.2 Button Count Change

**Verdict: RISK**

Current arrays: `_action_buttons[5]`, `_action_labels[5]`, `_action_values[5]`.
New requirement: 6 buttons.

The spec's file manifest says to MODIFY `DisplayUI.h`, but does not explicitly call out changing the array sizes from `[5]` to `[6]`. This is an easy mistake to overlook.

**The spec should explicitly state:** "Change `_action_buttons[5]` to `_action_buttons[6]` (and labels/values arrays) in `DisplayUI.h`."

### 8.3 ZONES Button Removal

**Verdict: CLARIFICATION**

The current ZONES button (index 4) navigates to `UIScreen::ZONE_COMPOSER` via a special-cased click handler:
```c
if (index == 4) {
    ui->setScreen(UIScreen::ZONE_COMPOSER);
}
```

The spec removes ZONES from the mode buttons because zones are now accessible via the ZONES sidebar tab. But:

1. Does the ZONES sidebar tab navigate to `UIScreen::ZONE_COMPOSER` (the existing separate screen)?
2. Or does the ZONES tab show zone controls inline within the GLOBAL screen's bottom zone?

The spec says bottom zone (option 2), which means the full `ZoneComposerUI` screen with its LED strip visualisation and complex zone parameter grid is NOT accessible from the new layout. This is a significant functionality change that the spec does not call out.

**Decision needed:** Is the `UIScreen::ZONE_COMPOSER` screen retained as a secondary navigation target (e.g., long-press on ZONES tab), or is it being replaced entirely by the simpler 8-card zone interface in the bottom zone?

---

## 9. Phase Ordering Risks

### 9.1 P5 (Colour) After P4 (Content)

**Verdict: OK**

P4 creates the 3 content panels and the tab switch logic (show/hide via `LV_OBJ_FLAG_HIDDEN`). P5 adds the colour-switching behaviour on tab change.

This ordering works because:
- P4 can use a placeholder colour (e.g., gold for all bars) during initial implementation
- P5 then adds the dynamic colour switching on top
- The tab switching mechanism itself does not need to know about colours — it only shows/hides panels

No hidden dependency.

### 9.2 P7 (Borders) After P4 (Content)

**Verdict: OK**

P4 creates cards using the existing `make_card()` which applies 2px white borders. P7 changes these to 1px `#1E1E24` borders. This ordering is correct because:
- P4 produces a working (if ugly) layout with white borders
- P7 is a pure visual refinement pass
- Cards must exist (P4) before their borders can be changed (P7)

However, P7 could also be done during P4 by modifying `make_card()` at card creation time. The spec's separation is valid but adds a second pass over the same widgets.

### 9.3 Hidden Dependency: P2 (Sidebar) Needs P1 (Tokens)

**Verdict: RISK**

The spec says P1 and P2 are independent. But:
- P2 creates the sidebar with neon colours (`#FFD000`, `#BF00FF`, `#00FFCC`)
- P1 centralises all colour tokens into `DesignTokens.h`

If P2 is implemented before P1, the sidebar will use inline hex values. When P1 is later merged, the sidebar must be updated to use tokens. This is not a true dependency (P2 can work without P1), but it creates unnecessary rework if P2 does not use the token header.

**Recommendation:** If running P1 and P2 in parallel, P2 should define its colours as local `constexpr` values that match the token names, then a trivial find-replace swaps them to `#include "DesignTokens.h"` references when P1 lands.

---

## 10. Watchdog Concerns

**Verdict: RISK**

The spec adds approximately 35 new widgets in `begin()`. The current `begin()` already requires `esp_task_wdt_reset()` calls (visible at lines 814, 822, 837, 849, 860, 863 of DisplayUI.cpp). The reference doc (anti-pattern #6) confirms that heavy widget creation exceeds the 5s watchdog.

**Current WDT reset points in `begin()`:**
1. Before zone composer screen creation (line 814)
2. After connectivity screen creation (line 822)
3. Before ConnectivityTab::begin (line 837)
4. Before control surface screen creation (line 849)
5. Before lv_scr_load (line 860)
6. After lv_scr_load (line 863)

**New widget creation that needs WDT resets:**
- Sidebar creation (3 slots with styled containers) — ADD reset before/after
- Top zone: 8 gauge cards (existing, already covered) + 6 mode buttons (existing, but changing from 5 to 6)
- Bottom zone: 3 content panels x 8 cards = 24 new widget trees — ADD reset between each panel creation
- Zone selector strip (3 buttons) — within ZONES panel, should be covered by per-panel reset

**Specific recommendation:**
```c
// Phase 3 restructured begin()
esp_task_wdt_reset();
// ... create sidebar ...
esp_task_wdt_reset();
// ... create top zone (gauges + mode buttons) ...
esp_task_wdt_reset();
// ... create FX panel (8 cards) ...
esp_task_wdt_reset();
// ... create ZONES panel (selector + 8 cards) ...
esp_task_wdt_reset();
// ... create PRESETS panel (8 cards) ...
esp_task_wdt_reset();
// ... create footer ...
esp_task_wdt_reset();
// ... create other screens ...
```

The spec should mandate WDT reset placement, not leave it to the implementer to discover via runtime panics.

---

## 11. What the Spec Misses

### 11.1 Scroll Behaviour on Content Panels

**Verdict: CLARIFICATION**

The spec does not state whether content panels are scrollable. At 1280x720 with 8 cards in a horizontal grid, scrolling should not be needed horizontally. But vertically?

The ZONES panel has a zone selector strip (3x 220x44px buttons) ABOVE 8 parameter cards. If the vertical space between header and footer minus the top zone is insufficient for both rows, vertical scrolling would be needed.

**Calculation:**
- Screen height: 720
- Header: ~66px + 7px top margin = 73px
- Footer: ~66px + 3px bottom margin = 69px
- Top zone (gauges + mode buttons): ~125 + 14 + 100 = 239px (estimated from current layout)
- Separator: 1px
- Available for bottom zone: 720 - 73 - 69 - 239 - 1 = 338px

The zone selector strip (44px) + 8 cards in one row (~100px) = 144px. This fits in 338px. No scrolling needed.

**The spec should explicitly state: `lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE)` on all content panels.**

### 11.2 Touch Event Handling on Sidebar Tabs

**Verdict: CLARIFICATION**

The spec says sidebar tabs are touchable (P2: "tabs switch on touch"). But:
- Are the tab slots `lv_obj` with `LV_OBJ_FLAG_CLICKABLE`?
- Do the tab labels need `LV_OBJ_FLAG_EVENT_BUBBLE` (anti-pattern #8)?
- What is the touch target size? The sidebar is 70px wide. Tab slots must be at minimum 48px tall for accessibility.

The spec should specify: each tab slot is a `lv_obj` at minimum 48px tall, with `LV_OBJ_FLAG_CLICKABLE`, and child labels must have `EVENT_BUBBLE` + `!CLICKABLE`.

### 11.3 Animation on Tab Switch

**Verdict: CLARIFICATION**

The spec does not mention animation. Options:
1. **Instant swap** (show/hide) — simplest, matches current screen switching (`lv_scr_load` with no animation)
2. **Fade transition** — would require animating opacity on outgoing and incoming panels
3. **Slide transition** — most visually polished but highest complexity

**Recommendation:** Instant swap. The current codebase uses no animations for screen switching, and the reference doc says "Clean, readable, no bullshit animations" (DisplayUI.h line 9).

### 11.4 Existing Screen Cleanup

**Verdict: CLARIFICATION**

The spec does not address:
- What happens to `_fx_row_a_container`, `_fx_row_b_container`, `_preset_container`? These are the current bottom-zone rows. Phase 3 says "REMOVE from DisplayUI: FX Row A, FX Row B, Preset Row (these move to bottom zone in Phase 4)".
- Does "REMOVE" mean `lv_obj_del()` on the containers + NULL the pointers + remove member variables from `.h`?
- Or does "REMOVE" mean they are no longer created in `begin()` but the member variables remain?

The spec should say: "Delete `_fx_row_a_container`, `_fx_row_b_container`, and `_preset_container` and their child arrays from both `DisplayUI.h` and `DisplayUI.cpp`. Replace with the new tab panel structure."

### 11.5 Z-Ordering of Sidebar vs Content

**Verdict: OK**

In LVGL, child creation order determines Z-order (later = on top). If the sidebar is created before the content area within the middle section, the content area renders on top of the sidebar. If overlapping occurs (e.g., shadow from content area bleeding onto sidebar), this matters.

For non-overlapping flex-row children, Z-order is irrelevant. As long as the sidebar and content area do not overlap (they should not if sized correctly), this is fine.

### 11.6 `LV_USE_BTN 0` in lv_conf.h

**Verdict: BLOCKER**

`lv_conf.h` line 58: `#define LV_USE_BTN 0`

The current code creates `lv_btn` widgets in the footer for K1 device selection (DisplayUI.cpp line 714):
```c
_footer_k1_buttons[i] = lv_btn_create(_footer_k1_group);
```

With `LV_USE_BTN 0`, `lv_btn_create()` should not compile. This means either:
1. The build flag is overridden elsewhere (e.g., platformio.ini `-D LV_USE_BTN=1`)
2. LVGL 9.3 compiles `lv_btn` regardless of this flag
3. This code path is guarded by a preprocessor condition I have not found

Regardless, the spec proposes no change to `lv_conf.h`, which means any new button widgets must use `lv_obj_create()` + `LV_OBJ_FLAG_CLICKABLE` (which is actually the pattern used for the action row already). The existing `lv_btn_create` in the footer may be a latent issue.

The spec should clarify: if new "buttons" are created for the zone selector strip or sidebar tabs, use `lv_obj_create()` + `LV_OBJ_FLAG_CLICKABLE`, NOT `lv_btn_create()`.

### 11.7 Font Size Discrepancy

**Verdict: RISK**

The spec's typography table says:
- Gauge labels: Rajdhani Medium 18px
- Gauge values: JetBrains Mono Bold 28px
- Sidebar tab labels: Rajdhani Bold 14px

The available font sizes (from `lvgl-component-reference.md` section 4): 24, 32, 40, 48 px.

**There are no 14px, 18px, or 28px fonts in the project.** The font table shows only sizes 24, 32, 40, 48 (plus Bebas Neue at 56, 64, 72).

The current implementation uses:
- Gauge labels: `BEBAS_BOLD_24` (24px, not 18px Rajdhani)
- Gauge values: `JETBRAINS_MONO_REG_32` (32px, not 28px)
- Mode button labels: `RAJDHANI_MED_24` (24px, not 18px)

Either the spec's typography table is aspirational (new fonts to be generated), or it is wrong. Generating new font sizes at 14px, 18px, and 28px requires running the LVGL font converter and adding new font files to `src/ui/fonts/`.

**The spec must clarify:** Are these new font sizes to be generated, or should the implementer use the nearest available size?

### 11.8 Colour Token Drift

**Verdict: RISK**

The spec defines colour tokens that differ from the existing codebase:

| Token | Spec Value | Current Codebase Value | Delta |
|-------|-----------|----------------------|-------|
| BG_DEEP | `#08080A` | `TAB5_COLOR_BG_PAGE = 0x0A0A0B` | Different |
| SURFACE | `#111114` | `TAB5_COLOR_BG_SURFACE_BASE = 0x121214` | Different |
| BORDER_SUBTLE | `#1E1E24` | `TAB5_COLOR_BORDER_BASE = 0x2A2A2E` | Significantly different |
| FG_PRIMARY | `#F0F0F0` | `TAB5_COLOR_FG_PRIMARY = 0xFFFFFF` | Different (not pure white) |
| FG_SECONDARY | `#8891A0` | `TAB5_COLOR_FG_SECONDARY = 0x9CA3AF` | Different |
| BRAND_PRIMARY | `#FFD000` | `TAB5_COLOR_BRAND_PRIMARY = 0xFFC700` | Different (slightly oranger) |

Phase 1 says "replace inline hex with token references" and "zero functional change." But the spec's token values DO NOT match the current codebase values. If Phase 1 introduces the spec's token values, it will change colours — violating its own "pixel-identical before and after" constraint.

**Resolution:** Phase 1 must use the CURRENT codebase values as the tokens (pixel-identical refactor). The colour VALUE changes (0x0A0A0B -> 0x08080A, etc.) are a separate step that should happen in Phase 5 or a dedicated colour-change phase. Do not conflate refactoring (extracting tokens) with redesigning (changing values).

---

## Summary Table

| # | Area | Verdict | Issue |
|---|------|---------|-------|
| 1.1 | Nested flex | **OK** | Supported in LVGL 9.3 |
| 1.2 | Sidebar height | **RISK** | `lv_pct(100)` behaviour with padding; use explicit pixels |
| 1.3 | Flex vs absolute | **CLARIFICATION** | Spec must decide; recommend absolute (matches codebase) |
| 2.1 | Tab switching | **OK** | `LV_OBJ_FLAG_HIDDEN` is correct approach |
| 2.2 | Memory impact | **OK** | ~15 KB additional, well within 200 KB budget |
| 2.3 | ControlSurface fate | **BLOCKER** | Spec does not say if class is deleted, retained, or merged |
| 3.1 | Inset shadow | **BLOCKER** | LVGL has no inset shadow; use dark background approximation |
| 3.2 | Radial gradient | **BLOCKER** | Requires `LV_DRAW_SW_COMPLEX_GRADIENTS 1` not in lv_conf.h; use solid pill approximation |
| 3.3 | Text shadow | **BLOCKER** | LVGL has no text shadow; use container shadow or duplicate label |
| 3.4 | Accent bar glow | **OK** | Outer shadow works for this |
| 4.1 | Card neon glow | **RISK** | Outer shadow supported; measure performance with 8+ shadows |
| 4.2 | Cheaper highlight | **OK** | Coloured border as fallback |
| 5 | Colour switching | **OK** | Approach A (direct restyle) is correct |
| 6 | Bar fill colour | **OK** | `LV_PART_INDICATOR` is correct selector |
| 7 | Separator line | **OK** | 1px-tall lv_obj recommended over lv_line |
| 8.1 | Grid descriptor | **RISK** | New 6-column static array needed; old 5-col must not be reused |
| 8.2 | Button count | **RISK** | Array sizes must change from [5] to [6] in .h file |
| 8.3 | ZONES removal | **CLARIFICATION** | ZoneComposerUI full screen — retained or replaced? |
| 9.1 | P5 after P4 | **OK** | No hidden dependency |
| 9.2 | P7 after P4 | **OK** | Valid ordering |
| 9.3 | P2 needs P1 tokens | **RISK** | Parallel execution creates rework; use local constexpr placeholder |
| 10 | WDT resets | **RISK** | Spec must mandate reset placement for 35 new widgets |
| 11.1 | Scroll behaviour | **CLARIFICATION** | Spec should explicitly disable scrolling on all panels |
| 11.2 | Sidebar touch | **CLARIFICATION** | Touch target sizes, EVENT_BUBBLE, CLICKABLE not specified |
| 11.3 | Tab animation | **CLARIFICATION** | Spec should say "instant swap, no animation" |
| 11.4 | Old widget cleanup | **CLARIFICATION** | "REMOVE" is ambiguous — delete from .h and .cpp? |
| 11.5 | Z-ordering | **OK** | Non-overlapping flex children, not an issue |
| 11.6 | LV_USE_BTN 0 | **BLOCKER** | `lv_btn` is disabled; use `lv_obj` + CLICKABLE for new buttons |
| 11.7 | Font sizes | **RISK** | 14px, 18px, 28px fonts do not exist in project |
| 11.8 | Colour token drift | **RISK** | Spec tokens differ from codebase values; P1 "pixel-identical" is violated |

### Totals

- **BLOCKER: 5** (ControlSurface fate, inset shadow, radial gradient, text shadow, LV_USE_BTN)
- **RISK: 6** (sidebar height, card shadow perf, grid descriptor, button count, P2/P1 parallel, font sizes, WDT, colour drift)
- **CLARIFICATION: 6** (flex vs absolute, ZONES screen fate, scroll, sidebar touch, animation, old widget cleanup)
- **OK: 5** (nested flex, tab switching, memory, bar fill, separator, z-order, accent bar)

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:lvgl-specialist | Created. Full feasibility review of IMPLEMENTATION_SPEC.md against LVGL 9.3 capabilities, current codebase patterns, and lv_conf.h configuration. |
