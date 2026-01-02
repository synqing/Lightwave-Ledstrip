#include "DisplayUI.h"
#include <cmath>

// Static member definition required for C++14/C++17
constexpr const char* DisplayUI::PARAM_NAMES[8];

DisplayUI::DisplayUI(M5GFX& display)
    : _display(display)
    , _highlightIndex(255)
    , _touchStartX(0)
    , _touchStartY(0)
    , _touchEndX(0)
    , _touchEndY(0)
    , _touchActive(false) {
    // Initialize all values to 0
    for (uint8_t i = 0; i < 8; i++) {
        _currentValues[i] = 0;
    }
}

void DisplayUI::begin() {
    _display.startWrite();
    _display.fillScreen(COLOR_BG);
    _display.setTextDatum(textdatum_t::top_left);
    _display.setFont(&fonts::Font0); // Small font for labels
    _display.endWrite();

    // Draw initial grid with all values at 0
    updateAll(_currentValues);

    // Draw scanline overlay
    drawScanlines();
}

void DisplayUI::update(uint8_t paramIndex, uint8_t value) {
    if (paramIndex >= 8) return;

    _currentValues[paramIndex] = value;
    drawCell(paramIndex, value, paramIndex == _highlightIndex);

    // Redraw scanlines over the updated cell
    drawScanlines();
}

void DisplayUI::updateAll(const uint8_t values[8]) {
    _display.startWrite();

    for (uint8_t i = 0; i < 8; i++) {
        _currentValues[i] = values[i];
        drawCell(i, values[i], i == _highlightIndex);
    }

    _display.endWrite();

    // Draw scanlines over entire display
    drawScanlines();
}

void DisplayUI::setHighlight(uint8_t paramIndex) {
    uint8_t prevHighlight = _highlightIndex;
    _highlightIndex = paramIndex;

    // Redraw previously highlighted cell (if any)
    if (prevHighlight < 8) {
        drawCell(prevHighlight, _currentValues[prevHighlight], false);
    }

    // Redraw newly highlighted cell
    if (paramIndex < 8) {
        drawCell(paramIndex, _currentValues[paramIndex], true);
    }

    // Redraw scanlines
    drawScanlines();
}

void DisplayUI::drawScanlines() {
    _display.startWrite();

    // Draw every 2nd horizontal line with semi-transparent dark color
    for (int16_t y = 0; y < DISPLAY_HEIGHT; y += 2) {
        // Create a subtle scanline effect by drawing thin dark lines
        _display.drawFastHLine(0, y, DISPLAY_WIDTH, 0x0000); // Black scanline
    }

    _display.endWrite();
}

bool DisplayUI::handleTouch() {
    lgfx::touch_point_t tp;
    int nums = _display.getTouchRaw(&tp, 1);

    if (nums > 0) {
        if (!_touchActive) {
            // Touch started
            _touchStartX = tp.x;
            _touchStartY = tp.y;
            _touchActive = true;
        } else {
            // Touch ongoing - update end position
            _touchEndX = tp.x;
            _touchEndY = tp.y;
        }
        return false; // Still touching
    } else if (_touchActive) {
        // Touch ended - check for swipe using last known position
        _touchActive = false;

        int16_t deltaX = _touchEndX - _touchStartX;

        if (abs(deltaX) >= SWIPE_THRESHOLD) {
            // Valid swipe detected
            // Future: implement page switching logic here
            return true;
        }
    }

    return false;
}

uint16_t DisplayUI::getParamColor(uint8_t paramIndex) const {
    switch (paramIndex) {
        case 0: return COLOR_EFFECT;
        case 1: return COLOR_BRIGHTNESS;
        case 2: return COLOR_PALETTE;
        case 3: return COLOR_SPEED;
        case 4: return COLOR_INTENSITY;
        case 5: return COLOR_SATURATION;
        case 6: return COLOR_COMPLEXITY;
        case 7: return COLOR_VARIATION;
        default: return COLOR_BG;
    }
}

void DisplayUI::drawCell(uint8_t paramIndex, uint8_t value, bool highlight) {
    if (paramIndex >= 8) return;

    int16_t cellX, cellY;
    getCellPosition(paramIndex, cellX, cellY);

    uint16_t color = getParamColor(paramIndex);

    _display.startWrite();

    // Fill cell background
    _display.fillRect(cellX, cellY, CELL_WIDTH, CELL_HEIGHT, COLOR_BG);

    // Draw glowing border
    drawGlowBorder(cellX, cellY, CELL_WIDTH, CELL_HEIGHT, color, highlight);

    // Draw parameter name (top-left of cell, small font)
    _display.setFont(&fonts::Font0);
    _display.setTextColor(color);
    _display.setCursor(cellX + 3, cellY + 2);
    _display.print(PARAM_NAMES[paramIndex]);

    // Draw value (top-right of cell, slightly larger)
    _display.setFont(&fonts::Font2);
    _display.setCursor(cellX + CELL_WIDTH - 18, cellY + 2);
    _display.print(value);

    // Draw progress bar (bottom of cell)
    int16_t barX = cellX + 3;
    int16_t barY = cellY + CELL_HEIGHT - 8;
    int16_t barW = CELL_WIDTH - 6;
    int16_t barH = 5;
    drawProgressBar(barX, barY, barW, barH, value, color);

    _display.endWrite();
}

void DisplayUI::drawGlowBorder(int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t color, bool highlight) {
    // Draw multiple border layers for glow effect
    float glowFactor = highlight ? 1.0f : 0.6f;

    // Outer glow (dimmest)
    uint16_t outerColor = dimColor(color, 0.3f * glowFactor);
    _display.drawRect(x, y, w, h, outerColor);

    // Middle glow
    uint16_t middleColor = dimColor(color, 0.5f * glowFactor);
    _display.drawRect(x + 1, y + 1, w - 2, h - 2, middleColor);

    // Inner border (brightest)
    uint16_t innerColor = dimColor(color, glowFactor);
    _display.drawRect(x + 2, y + 2, w - 4, h - 4, innerColor);
}

void DisplayUI::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                               uint8_t value, uint16_t color) {
    // Calculate filled width based on value (0-255)
    int16_t filledWidth = (w * value) / 255;

    // Draw bar background (dark)
    _display.fillRect(x, y, w, h, dimColor(color, 0.2f));

    // Draw filled portion with glow
    if (filledWidth > 0) {
        _display.fillRect(x, y, filledWidth, h, color);

        // Add glow to right edge of bar
        if (filledWidth < w) {
            uint16_t glowColor = dimColor(color, 0.5f);
            _display.drawFastVLine(x + filledWidth, y, h, glowColor);
        }
    }

    // Draw bar outline
    _display.drawRect(x, y, w, h, dimColor(color, 0.5f));
}

void DisplayUI::getCellPosition(uint8_t paramIndex, int16_t& outX, int16_t& outY) const {
    if (paramIndex >= 8) {
        outX = 0;
        outY = 0;
        return;
    }

    // Grid layout: 2 columns x 4 rows
    uint8_t col = paramIndex % COLS;
    uint8_t row = paramIndex / COLS;

    outX = col * CELL_WIDTH;
    outY = row * CELL_HEIGHT;
}

uint16_t DisplayUI::dimColor(uint16_t color, float factor) const {
    if (factor >= 1.0f) return color;
    if (factor <= 0.0f) return 0x0000;

    // Extract RGB565 components
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    // Apply dimming factor
    r = static_cast<uint8_t>(r * factor);
    g = static_cast<uint8_t>(g * factor);
    b = static_cast<uint8_t>(b * factor);

    // Recombine into RGB565
    return (r << 11) | (g << 5) | b;
}
