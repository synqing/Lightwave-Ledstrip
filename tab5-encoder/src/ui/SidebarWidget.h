// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

// ============================================================================
// SidebarWidget - Three-tab sidebar with backlit glow treatment
// ============================================================================
// Fixed 70px-wide sidebar on the left edge of the GLOBAL screen.
// Display-only at this phase; tab switching stores state but does
// not yet change any content panels.
// ============================================================================

#include <lvgl.h>
#include <cstdint>

/**
 * Sidebar tab identifiers.
 * Order matches visual top-to-bottom arrangement.
 */
enum class SidebarTab : uint8_t {
    FX_PARAMS = 0,
    ZONES     = 1,
    PRESETS   = 2
};

typedef void (*SidebarTabCallback)(SidebarTab tab);

/**
 * Three-slot sidebar widget with per-tab neon glow treatment.
 *
 * Visual layers per slot:
 *   1. Dim-tinted border (always visible) -- identity trace colour
 *   2. Shadow emitter pill (active only) -- LVGL outer shadow
 *   3. Text label -- full neon (active) or per-tab tuned opacity (inactive)
 *   4. Left accent bar (active only) -- 3px solid neon strip
 */
class SidebarWidget {
public:
    SidebarWidget();
    ~SidebarWidget();

    void begin(lv_obj_t* parent);
    void setActiveTab(SidebarTab tab);
    SidebarTab getActiveTab() const { return _activeTab; }
    void setTabCallback(SidebarTabCallback cb) { _callback = cb; }

    lv_obj_t* getRoot() const { return _root; }

private:
    static constexpr uint8_t TAB_COUNT = 3;

    // Layout constants
    static constexpr int32_t SIDEBAR_WIDTH  = 70;
    static constexpr int32_t SIDEBAR_Y      = 73;   // Below header (y=7 + h=66)
    static constexpr int32_t SIDEBAR_HEIGHT = 578;   // 651 (footer top) - 73
    static constexpr int32_t SLOT_HEIGHT    = SIDEBAR_HEIGHT / TAB_COUNT;  // ~192px

    // Accent bar inset from top/bottom of slot (mockup 29: 12px)
    static constexpr int32_t ACCENT_INSET = 12;
    static constexpr int32_t SLOT_RADIUS  = 6;

    // Per-tab colour and opacity definitions
    struct TabDef {
        const char* label;
        uint32_t    neonColour;
        uint32_t    dimBorder;
        lv_opa_t    inactiveOpa;  // Per-tab: tuned for perceptual brightness on dark bg
    };
    static const TabDef kTabDefs[TAB_COUNT];

    // Widget tree
    lv_obj_t* _root = nullptr;
    lv_obj_t* _tabSlots[TAB_COUNT]   = {nullptr};
    lv_obj_t* _tabLabels[TAB_COUNT]  = {nullptr};
    lv_obj_t* _tabPills[TAB_COUNT]   = {nullptr};   // Shadow emitter pills
    lv_obj_t* _tabAccents[TAB_COUNT] = {nullptr};   // Left accent bars

    SidebarTab _activeTab = SidebarTab::FX_PARAMS;
    SidebarTabCallback _callback = nullptr;

    void updateVisuals();
    static void onTabClicked(lv_event_t* e);
};
