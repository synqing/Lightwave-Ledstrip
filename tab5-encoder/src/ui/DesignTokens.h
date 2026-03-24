// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// DesignTokens - Centralised colour and layout constants for Tab5 UI
// ============================================================================
// Single source of truth. All UI files reference these tokens.
// Phase 1: exact values extracted from the existing codebase (no visual change).
// Phase 7 will migrate to final spec values.
// ============================================================================

#pragma once
#include <cstdint>

namespace DesignTokens {

// --- Core palette (from current codebase) ---
static constexpr uint32_t BG_PAGE              = 0x0A0A0B;
static constexpr uint32_t SURFACE_BASE         = 0x121214;
static constexpr uint32_t SURFACE_ELEVATED     = 0x1A1A1C;
static constexpr uint32_t BORDER_BASE          = 0x2A2A2E;
static constexpr uint32_t FG_PRIMARY           = 0xFFFFFF;
static constexpr uint32_t FG_SECONDARY         = 0x9CA3AF;
static constexpr uint32_t FG_DIMMED            = 0x4B5563;
static constexpr uint32_t BRAND_PRIMARY        = 0xFFC700;
static constexpr uint32_t STATUS_SUCCESS       = 0x22C55E;
static constexpr uint32_t STATUS_ERROR         = 0xEF4444;
static constexpr uint32_t STATUS_WARNING       = 0xF59E0B;

// --- Neon accents (for sidebar/tabs) ---
static constexpr uint32_t NEON_GOLD            = 0xFFD000;  // FX tab — spec #FFD000
static constexpr uint32_t NEON_PURPLE          = 0xBF00FF;  // ZONES tab
static constexpr uint32_t NEON_TEAL            = 0x00FFCC;  // PRESETS tab

// --- Sidebar ---
static constexpr uint32_t SIDEBAR_BG           = 0x080809;
static constexpr uint32_t SLOT_BG              = 0x060607;  // Darker than sidebar — recessed slot
static constexpr uint32_t ROW_TOP              = 0x0C0C0E;
static constexpr uint32_t ROW_BOTTOM           = 0x08080A;

// --- Dim-tinted borders (neon at ~10% on dark) ---
static constexpr uint32_t DIM_BORDER_GOLD      = 0x1A1500;  // #FFD000 at ~10% — delta <1 step from old
static constexpr uint32_t DIM_BORDER_PURPLE    = 0x0F0016;
static constexpr uint32_t DIM_BORDER_TEAL      = 0x001A14;

// --- Subtle border (standard for param cards) ---
static constexpr uint32_t BORDER_SUBTLE        = 0x1E1E24;

// --- ControlSurface-specific ---
static constexpr uint32_t CAMERA_ACTIVE        = 0xEF4444;
static constexpr uint32_t CAMERA_STANDBY       = 0x6B7280;
static constexpr uint32_t PRESET_OCCUPIED      = 0x3B82F6;
static constexpr uint32_t PRESET_ACTIVE_BORDER = 0x00FF99;

// --- Layout (DisplayUI / ControlSurface canonical values) ---
static constexpr int32_t STATUSBAR_HEIGHT      = 66;
static constexpr int32_t GRID_GAP              = 14;
static constexpr int32_t GRID_MARGIN           = 24;
static constexpr int32_t SIDEBAR_WIDTH         = 70;
static constexpr int32_t CARD_RADIUS           = 14;
static constexpr int32_t CARD_BORDER_WIDTH     = 1;

} // namespace DesignTokens

#include <lvgl.h>

// ============================================================================
// Shared card factory (matches DisplayUI / ControlSurface / ZoneComposer pattern)
// ============================================================================
// Subtle 1px border (#1E1E24), 14px radius, pad 10, non-scrollable.
// Header and footer retain their own 2px white borders (not affected by this).
// ConnectivityTab uses a local variant with BORDER_BASE border colour.
// ============================================================================
inline lv_obj_t* make_card(lv_obj_t* parent, bool elevated = false) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card,
        lv_color_hex(elevated ? DesignTokens::SURFACE_ELEVATED : DesignTokens::SURFACE_BASE),
        LV_PART_MAIN);
    lv_obj_set_style_border_width(card, DesignTokens::CARD_BORDER_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(DesignTokens::DIM_BORDER_GOLD), LV_PART_MAIN);
    lv_obj_set_style_radius(card, DesignTokens::CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}
