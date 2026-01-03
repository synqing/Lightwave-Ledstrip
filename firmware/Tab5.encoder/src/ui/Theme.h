#pragma once
// ============================================================================
// Theme - Color palette and layout constants for Tab5.encoder UI
// ============================================================================

#include <cstdint>

namespace Theme {

// ============================================================================
// Colors (RGB565)
// ============================================================================

constexpr uint16_t BG_DARK     = 0x0000;   // Pure black
constexpr uint16_t BG_PANEL    = 0x0841;   // Dark gray for panels
constexpr uint16_t ACCENT      = 0x07E0;   // Cyan (green = 0x07E0)
constexpr uint16_t TEXT_BRIGHT = 0xFFFF;   // White
constexpr uint16_t TEXT_DIM    = 0x8410;   // Gray

// Status colors
constexpr uint16_t STATUS_OK   = 0x07E0;   // Green
constexpr uint16_t STATUS_CONN = 0xFD20;   // Orange (connecting)
constexpr uint16_t STATUS_ERR  = 0xF800;   // Red

// ============================================================================
// Parameter Colors (8 neon colors for global parameters)
// ============================================================================

constexpr uint16_t PARAM_COLORS[8] = {
    0xF81F,  // 0: Effect    - Magenta #FF00FF
    0x07FF,  // 1: Brightness - Cyan #00FFFF
    0xF813,  // 2: Palette   - Pink #FF0099
    0x07F3,  // 3: Speed     - Green #00FF99
    0x9C1F,  // 4: Mood      - Purple #9900FF
    0xFFE0,  // 5: Fade Amt  - Yellow #FFFF00
    0xFD20,  // 6: Complexity - Orange #FF6600
    0x04FF   // 7: Variation - Blue #0099FF
};

// ============================================================================
// Parameter Names
// ============================================================================

constexpr const char* PARAM_NAMES[16] = {
    "EFFECT",      // 0
    "BRIGHTNESS",  // 1
    "PALETTE",     // 2
    "SPEED",       // 3
    "MOOD",        // 4
    "FADE",        // 5
    "COMPLEXITY",  // 6
    "VARIATION",   // 7
    "Z0 EFFECT",   // 8
    "Z0 SPD",      // 9 - Zone 0 Speed/Palette
    "Z1 EFFECT",   // 10
    "Z1 SPD",      // 11 - Zone 1 Speed/Palette
    "Z2 EFFECT",   // 12
    "Z2 SPD",      // 13 - Zone 2 Speed/Palette
    "Z3 EFFECT",   // 14
    "Z3 SPD"       // 15 - Zone 3 Speed/Palette
};

// ============================================================================
// Layout Constants
// ============================================================================

constexpr int SCREEN_W     = 1280;
constexpr int SCREEN_H     = 720;
constexpr int STATUS_BAR_H = 80;

constexpr int GRID_ROWS = 1;  // Single row of 8 encoders
constexpr int GRID_COLS = 8;  // 8 columns for 8 global parameters

constexpr int CELL_W = SCREEN_W / GRID_COLS;                        // 160
constexpr int CELL_H = 120;  // Fixed height for horizontal bar layout

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Dim a color by a factor (0-255)
 */
inline uint16_t dimColor(uint16_t color, uint8_t factor) {
    if (factor == 0) return 0;
    if (factor == 255) return color;

    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    r = (r * factor) >> 8;
    g = (g * factor) >> 8;
    b = (b * factor) >> 8;

    return (r << 11) | (g << 5) | b;
}

}  // namespace Theme
