// ============================================================================
// ActionRowWidget - Touch action buttons row
// ============================================================================

#include "ActionRowWidget.h"

#ifdef SIMULATOR_BUILD
    #include "../M5GFX_Mock.h"
#else
    #include <M5Unified.h>
#endif

#include <cstdio>

ActionRowWidget::ActionRowWidget(M5GFX* display, int x, int y, int w, int h)
    : _display(display), _sprite(display), _x(x), _y(y), _w(w), _h(h)
{
    _sprite.setColorDepth(16);
    _sprite.setPsram(true);
    _spriteOk = _sprite.createSprite(_w, _h);
}

ActionRowWidget::~ActionRowWidget() {
    _sprite.deleteSprite();
}

void ActionRowWidget::setGamma(float value, bool enabled) {
    if (_gammaEnabled != enabled || _gammaValue != value) {
        _gammaEnabled = enabled;
        _gammaValue = value;
        _dirty = true;
    }
}

void ActionRowWidget::setColourMode(uint8_t mode) {
    if (_colourMode != mode) {
        _colourMode = mode;
        _dirty = true;
    }
}

void ActionRowWidget::setAutoExposure(bool enabled) {
    if (_autoExposureEnabled != enabled) {
        _autoExposureEnabled = enabled;
        _dirty = true;
    }
}

void ActionRowWidget::setBrownGuardrail(bool enabled) {
    if (_brownGuardrailEnabled != enabled) {
        _brownGuardrailEnabled = enabled;
        _dirty = true;
    }
}

void ActionRowWidget::markDirty() {
    _dirty = true;
}

const char* ActionRowWidget::colourModeLabel(uint8_t mode) {
    switch (mode) {
        case 0: return "OFF";
        case 1: return "HSV";
        case 2: return "RGB";
        case 3: return "BOTH";
        default: return "UNK";
    }
}

void ActionRowWidget::drawButton(int index, const char* label, const char* value, uint16_t accent, bool active) {
    const int btnW = _w / 4;
    const int btnH = _h;
    const int x = index * btnW;
    const int y = 0;

    uint16_t border = active ? accent : Theme::dimColor(accent, 120);
    uint16_t bg = Theme::BG_PANEL;
    uint16_t text = active ? Theme::TEXT_BRIGHT : Theme::TEXT_DIM;

    _sprite.fillRect(x + 2, y + 2, btnW - 4, btnH - 4, bg);
    _sprite.drawRect(x + 1, y + 1, btnW - 2, btnH - 2, border);
    _sprite.drawRect(x, y, btnW, btnH, Theme::dimColor(border, 180));

    // Label at top-center
    _sprite.setTextDatum(textdatum_t::top_center);
    _sprite.setFont(&fonts::FreeSans9pt7b);
    _sprite.setTextColor(text);
    _sprite.drawString(label, x + btnW / 2, y + 8);

    // Value - use larger font
    _sprite.setTextDatum(textdatum_t::middle_center);
    _sprite.setFont(&fonts::FreeSansBold12pt7b);
    _sprite.setTextColor(active ? accent : Theme::TEXT_DIM);
    _sprite.drawString(value, x + btnW / 2, y + btnH / 2 + 12);
}

void ActionRowWidget::render() {
    if (!_dirty || !_display) {
        return;
    }

    char gammaValue[8];
    if (_gammaEnabled) {
        snprintf(gammaValue, sizeof(gammaValue), "%.1f", _gammaValue);
    } else {
        snprintf(gammaValue, sizeof(gammaValue), "OFF");
    }

    const char* colourMode = colourModeLabel(_colourMode);
    const char* aeState = _autoExposureEnabled ? "ON" : "OFF";
    const char* brownState = _brownGuardrailEnabled ? "ON" : "OFF";

    if (_spriteOk) {
        _sprite.startWrite();
        _sprite.fillSprite(Theme::BG_DARK);
        drawButton(0, "GAMMA", gammaValue, Theme::ACCENT, _gammaEnabled);
        drawButton(1, "COLOUR", colourMode, Theme::ACCENT, _colourMode != 0);
        drawButton(2, "EXPOSURE", aeState, Theme::STATUS_OK, _autoExposureEnabled);
        drawButton(3, "BROWN", brownState, Theme::STATUS_ERR, _brownGuardrailEnabled);
        _sprite.endWrite();
        _sprite.pushSprite(_x, _y);
    } else {
        _display->startWrite();
        _display->fillRect(_x, _y, _w, _h, Theme::BG_DARK);
        _display->endWrite();
    }

    _dirty = false;
}
