#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <M5GFX.h>
#include <cstdint>

/**
 * @brief Neon Cyberpunk Display UI for K1.8encoderS3
 *
 * Full-screen 128x128px grid layout with 8 parameter cells (2x4 grid).
 * Each cell displays a parameter name, value, and progress bar with
 * glowing neon borders. Includes scanline overlay effect and swipe
 * detection for page navigation.
 */
class DisplayUI {
public:
    DisplayUI(M5GFX& display);

    /**
     * @brief Initialize the display UI
     * Sets up display, clears screen, draws initial grid
     */
    void begin();

    /**
     * @brief Update a single parameter cell
     * @param paramIndex Parameter index (0-7)
     * @param value Parameter value (0-255)
     */
    void update(uint8_t paramIndex, uint8_t value);

    /**
     * @brief Update all parameter cells
     * @param values Array of 8 parameter values (0-255)
     */
    void updateAll(const uint8_t values[8]);

    /**
     * @brief Highlight a specific parameter (brighter glow)
     * @param paramIndex Parameter index (0-7), or 255 to clear highlight
     */
    void setHighlight(uint8_t paramIndex);

    /**
     * @brief Draw scanline overlay effect
     * Adds horizontal lines across entire display for cyberpunk aesthetic
     */
    void drawScanlines();

    /**
     * @brief Process touch input for swipe detection
     * Call this in loop() if touch is detected
     * @return true if a valid swipe was detected (future: page switching)
     */
    bool handleTouch();

private:
    M5GFX& _display;
    uint8_t _currentValues[8];
    uint8_t _highlightIndex;

    // Touch tracking for swipe detection
    int16_t _touchStartX;
    int16_t _touchStartY;
    int16_t _touchEndX;
    int16_t _touchEndY;
    bool _touchActive;

    // Display constants
    static constexpr uint16_t DISPLAY_WIDTH = 128;
    static constexpr uint16_t DISPLAY_HEIGHT = 128;
    static constexpr uint8_t CELL_WIDTH = 64;
    static constexpr uint8_t CELL_HEIGHT = 32;
    static constexpr uint8_t COLS = 2;
    static constexpr uint8_t ROWS = 4;
    static constexpr uint8_t SWIPE_THRESHOLD = 30;

    // Colors (RGB565 format)
    // RGB565: RRRRR GGGGGG BBBBB (5-6-5 bits)
    static constexpr uint16_t COLOR_BG = 0x0841;           // #0a0a14 dark background
    static constexpr uint16_t COLOR_EFFECT = 0xF810;       // #ff0080 hot pink (R=31, G=16, B=16)
    static constexpr uint16_t COLOR_BRIGHTNESS = 0xFFE0;   // #ffff00 yellow (R=31, G=63, B=0)
    static constexpr uint16_t COLOR_PALETTE = 0x07FF;      // #00ffff cyan (R=0, G=63, B=31)
    static constexpr uint16_t COLOR_SPEED = 0xFA20;        // #ff4400 orange (R=31, G=20, B=0)
    static constexpr uint16_t COLOR_INTENSITY = 0xF81F;    // #ff00ff magenta (R=31, G=0, B=31)
    static constexpr uint16_t COLOR_SATURATION = 0x07F1;   // #00ff88 green (R=0, G=63, B=17)
    static constexpr uint16_t COLOR_COMPLEXITY = 0x901F;   // #8800ff purple (R=18, G=0, B=31)
    static constexpr uint16_t COLOR_VARIATION = 0x047F;    // #0088ff blue (R=0, G=17, B=31)

    // Parameter names
    static constexpr const char* PARAM_NAMES[8] = {
        "Effect", "Brightness", "Palette", "Speed",
        "Intensity", "Saturation", "Complexity", "Variation"
    };

    /**
     * @brief Get neon color for parameter index
     * @param paramIndex Parameter index (0-7)
     * @return RGB565 color value
     */
    uint16_t getParamColor(uint8_t paramIndex) const;

    /**
     * @brief Draw a single parameter cell
     * @param paramIndex Parameter index (0-7)
     * @param value Parameter value (0-255)
     * @param highlight Whether to draw with brighter glow
     */
    void drawCell(uint8_t paramIndex, uint8_t value, bool highlight = false);

    /**
     * @brief Draw glowing border around cell
     * @param x Cell X position
     * @param y Cell Y position
     * @param w Cell width
     * @param h Cell height
     * @param color Border color
     * @param highlight Whether to draw brighter glow
     */
    void drawGlowBorder(int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color, bool highlight = false);

    /**
     * @brief Draw progress bar with glow effect
     * @param x Bar X position
     * @param y Bar Y position
     * @param w Bar width
     * @param h Bar height
     * @param value Progress value (0-255)
     * @param color Bar color
     */
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint8_t value, uint16_t color);

    /**
     * @brief Calculate cell position
     * @param paramIndex Parameter index (0-7)
     * @param outX Output X coordinate
     * @param outY Output Y coordinate
     */
    void getCellPosition(uint8_t paramIndex, int16_t& outX, int16_t& outY) const;

    /**
     * @brief Dim color for glow effect
     * @param color Original RGB565 color
     * @param factor Dimming factor (0.0 = black, 1.0 = original)
     * @return Dimmed RGB565 color
     */
    uint16_t dimColor(uint16_t color, float factor) const;
};

#endif // DISPLAY_UI_H
