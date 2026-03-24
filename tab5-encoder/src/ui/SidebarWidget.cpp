// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// SidebarWidget - Three-tab sidebar with backlit glow treatment
// ============================================================================

#include "SidebarWidget.h"
#include "DesignTokens.h"
#include "fonts/experimental_fonts.h"

// ============================================================================
// Tab definitions: label, neon colour, dim border colour
// ============================================================================
// Per-tab inactive opacity: tuned for perceptual brightness on #060607 dark bg.
// Purple has lowest perceived brightness at equal opacity — gets a 10% boost.
// Gold and teal are sufficiently visible at 25% (spec value).
// Vertical stacked text: one character per line, centred in 70px slot
const SidebarWidget::TabDef SidebarWidget::kTabDefs[TAB_COUNT] = {
    {"F\nX",             DesignTokens::NEON_GOLD,   DesignTokens::DIM_BORDER_GOLD,   64},    // 25%
    {"Z\nO\nN\nE\nS",   DesignTokens::NEON_PURPLE, DesignTokens::DIM_BORDER_PURPLE, 89},    // 35%
    {"P\nR\nE\nS\nE\nT\nS", DesignTokens::NEON_TEAL, DesignTokens::DIM_BORDER_TEAL, 64},    // 25%
};

// ============================================================================
// Construction / destruction
// ============================================================================

SidebarWidget::SidebarWidget() = default;

SidebarWidget::~SidebarWidget() {
    // LVGL objects are children of the screen and cleaned up when the
    // screen is deleted. No heap user_data to free.
}

// ============================================================================
// begin() -- build the full widget tree on the given parent screen
// ============================================================================
void SidebarWidget::begin(lv_obj_t* parent) {

    // --- Root container ---------------------------------------------------
    _root = lv_obj_create(parent);
    lv_obj_set_size(_root, SIDEBAR_WIDTH, SIDEBAR_HEIGHT);
    lv_obj_set_pos(_root, 0, SIDEBAR_Y);
    lv_obj_set_style_bg_color(_root, lv_color_hex(DesignTokens::SIDEBAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Right-edge separator (1px, BORDER_SUBTLE) -------------------------
    lv_obj_t* rightBorder = lv_obj_create(_root);
    lv_obj_set_size(rightBorder, 1, SIDEBAR_HEIGHT);
    lv_obj_set_pos(rightBorder, SIDEBAR_WIDTH - 1, 0);
    lv_obj_set_style_bg_color(rightBorder, lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rightBorder, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(rightBorder, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rightBorder, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rightBorder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(rightBorder, LV_OBJ_FLAG_CLICKABLE);

    // --- Per-tab slots ----------------------------------------------------
    for (uint8_t i = 0; i < TAB_COUNT; ++i) {
        const TabDef& def = kTabDefs[i];

        // Slot container (clickable card area)
        _tabSlots[i] = lv_obj_create(_root);
        lv_obj_set_size(_tabSlots[i], SIDEBAR_WIDTH, SLOT_HEIGHT);
        lv_obj_set_pos(_tabSlots[i], 0, static_cast<int32_t>(i) * SLOT_HEIGHT);
        lv_obj_set_style_bg_color(_tabSlots[i], lv_color_hex(DesignTokens::SLOT_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_tabSlots[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tabSlots[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(_tabSlots[i], LV_OBJ_FLAG_SCROLLABLE);

        // Layer 1: dim-tinted border (always visible — dormant light channel)
        lv_obj_set_style_border_color(_tabSlots[i], lv_color_hex(def.dimBorder), LV_PART_MAIN);
        lv_obj_set_style_border_width(_tabSlots[i], 1, LV_PART_MAIN);
        lv_obj_set_style_radius(_tabSlots[i], SLOT_RADIUS, LV_PART_MAIN);

        // Make slot clickable
        lv_obj_add_flag(_tabSlots[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(_tabSlots[i], reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_tabSlots[i], onTabClicked, LV_EVENT_CLICKED, this);

        // Layer 2: shadow emitter pill (active only)
        // Invisible 40x4px object at the left edge, emitting an outer glow
        _tabPills[i] = lv_obj_create(_tabSlots[i]);
        lv_obj_set_size(_tabPills[i], 40, 4);
        lv_obj_align(_tabPills[i], LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_opa(_tabPills[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(_tabPills[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_tabPills[i], 2, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_tabPills[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(_tabPills[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(_tabPills[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_tabPills[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // Shadow properties (applied to all; visibility toggled in updateVisuals)
        lv_obj_set_style_shadow_color(_tabPills[i], lv_color_hex(def.neonColour), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(_tabPills[i], 25, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(_tabPills[i], 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(_tabPills[i], LV_OPA_30, LV_PART_MAIN);

        // Layer 4: accent bar REMOVED — replaced by full slot background fill.
        // IPS panel testing showed the 3px bar didn't register visually.
        // Full neon bg tint at ~10% (mockup 21: rgba(neon, 0.08)) reads clearly.
        _tabAccents[i] = nullptr;

        // Layer 3: vertical stacked text label (centred in slot)
        _tabLabels[i] = lv_label_create(_tabSlots[i]);
        lv_label_set_text(_tabLabels[i], def.label);
        lv_obj_set_style_text_font(_tabLabels[i], RAJDHANI_BOLD_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_tabLabels[i], lv_color_hex(def.neonColour), LV_PART_MAIN);
        lv_obj_set_style_text_align(_tabLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(_tabLabels[i], 2, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(_tabLabels[i], 0, LV_PART_MAIN);
        lv_obj_set_width(_tabLabels[i], SIDEBAR_WIDTH);
        lv_obj_center(_tabLabels[i]);

        // Anti-pattern #8: prevent labels from swallowing touch events
        lv_obj_clear_flag(_tabLabels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_tabLabels[i], LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    // Set initial visual state
    updateVisuals();
}

// ============================================================================
// setActiveTab() -- change the selected tab and refresh visuals
// ============================================================================
void SidebarWidget::setActiveTab(SidebarTab tab) {
    if (tab == _activeTab) return;
    _activeTab = tab;
    updateVisuals();
}

// ============================================================================
// updateVisuals() -- apply active/inactive styling to all three slots
// ============================================================================
void SidebarWidget::updateVisuals() {
    for (uint8_t i = 0; i < TAB_COUNT; ++i) {
        const bool active = (static_cast<uint8_t>(_activeTab) == i);

        const TabDef& tabDef = kTabDefs[i];

        // Shadow emitter pill: visible only when active
        if (active) {
            lv_obj_clear_flag(_tabPills[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_tabPills[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Slot background: full neon tint when active, dark when inactive
        // ~10% neon fill (26/255) — tested on IPS, reads as "illuminated slot"
        if (active) {
            lv_obj_set_style_bg_color(_tabSlots[i], lv_color_hex(tabDef.neonColour), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tabSlots[i], 51, LV_PART_MAIN);  // ~20%
        } else {
            lv_obj_set_style_bg_color(_tabSlots[i], lv_color_hex(DesignTokens::SLOT_BG), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_tabSlots[i], LV_OPA_COVER, LV_PART_MAIN);
        }

        // Label opacity: full neon (active) or per-tab tuned dormant intensity (inactive)
        lv_obj_set_style_text_opa(
            _tabLabels[i],
            active ? LV_OPA_COVER : tabDef.inactiveOpa,
            LV_PART_MAIN);
    }
}

// ============================================================================
// onTabClicked() -- static callback for slot touch events
// ============================================================================
void SidebarWidget::onTabClicked(lv_event_t* e) {
    auto* self = static_cast<SidebarWidget*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    uint8_t idx = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));

    if (idx >= TAB_COUNT) return;

    auto newTab = static_cast<SidebarTab>(idx);
    self->setActiveTab(newTab);

    if (self->_callback) {
        self->_callback(newTab);
    }
}
