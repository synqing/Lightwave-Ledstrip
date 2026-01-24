// ============================================================================
// GaugeWidget - Radial encoder gauge implementation
// ============================================================================

#include "GaugeWidget.h"
#include "../../config/Config.h"

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
    #include "../../hal/SimHal.h"
#else
    #include <ESP.h>  // For heap monitoring
    #include <Arduino.h>
#endif

#include "../../hal/EspHal.h"
#include <cstdio>

GaugeWidget::GaugeWidget(M5GFX* display, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t index)
    : _display(display), _sprite(display), _x(x), _y(y), _w(w), _h(h), _index(index)
{
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] GaugeWidget ctor idx=%d size=%dx%d bytes=%u - Heap before: free=%u minFree=%u\n",
                  // index, w, h, w * h * 2, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] gauge_ctor idx=%d x=%d y=%d w=%d h=%d\n", index, x, y, w, h);
#endif

    _sprite.setColorDepth(16);
    _sprite.setPsram(true);
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] Before createSprite(idx=%d) - Heap: free=%u minFree=%u\n",
                  // index, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // #endregion
    bool spriteOk = _sprite.createSprite(_w, _h);
    _spriteOk = spriteOk;
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] After createSprite(idx=%d) ok=%d - Heap: free=%u minFree=%u\n",
                  // index, spriteOk ? 1 : 0, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // #endregion

#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] sprite_created idx=%d ok=%d\n", index, spriteOk ? 1 : 0);
#endif

    _color = (index < 8) ? Theme::PARAM_COLORS[index] : 0xFFFF;
    _title = (index < 16) ? Theme::PARAM_NAMES[index] : "???";
}

GaugeWidget::~GaugeWidget() {
    _sprite.deleteSprite();
}

void GaugeWidget::setValue(int32_t value) {
    int32_t clamped = (value < 0) ? 0 : (value > _maxValue ? _maxValue : value);
    if (_value != clamped) {
        _value = clamped;
        _dirty = true;
    }
}

void GaugeWidget::setMaxValue(uint8_t max) {
    if (_maxValue != max) {
        _maxValue = max;
        // Re-clamp current value to new max
        if (_value > _maxValue) {
            _value = _maxValue;
        }
        _dirty = true;  // Redraw to show new max value
    }
}

void GaugeWidget::setHighlight(bool active) {
    if (_highlighted != active) {
        _highlighted = active;
        _dirty = true;
    }
}

void GaugeWidget::render() {
    if (!_dirty) return;

    if (_spriteOk) {
        _sprite.startWrite();

        drawBackground();
        drawBar();
        drawValue();
        drawTitle();

        _sprite.endWrite();
        _sprite.pushSprite(_x, _y);
    } else if (_display) {
        _display->startWrite();
        drawBackgroundDirect();
        drawBarDirect();
        drawValueDirect();
        drawTitleDirect();
        _display->endWrite();
    }
    _dirty = false;
}

void GaugeWidget::drawBackground() {
    // Fill with dark background
    _sprite.fillSprite(Theme::BG_DARK);
    
    // Optimized gradient: draw 3 gradient bands instead of per-pixel
    // Top third: dimmer
    uint16_t topColor = Theme::dimColor(Theme::BG_PANEL, 128);
    _sprite.fillRect(0, 0, _w, _h / 3, topColor);
    
    // Middle third: medium
    uint16_t midColor = Theme::dimColor(Theme::BG_PANEL, 160);
    _sprite.fillRect(0, _h / 3, _w, _h / 3, midColor);
    
    // Bottom third: brighter (fades to dark at edges)
    uint16_t botColor = Theme::dimColor(Theme::BG_PANEL, 192);
    _sprite.fillRect(0, (_h * 2) / 3, _w, _h - ((_h * 2) / 3), botColor);

    // Neon border with glow effect
    uint16_t borderColor = _highlighted ? _color : Theme::dimColor(_color, 120);
    uint16_t glowColor = Theme::dimColor(_color, 40);
    
    // Outer glow (subtle)
    _sprite.drawRect(1, 1, _w - 2, _h - 2, glowColor);
    
    // Main border
    _sprite.drawRect(0, 0, _w, _h, borderColor);
    
    // Inner highlight when active
    if (_highlighted) {
        _sprite.drawRect(2, 2, _w - 4, _h - 4, Theme::dimColor(_color, 200));
    }
}

void GaugeWidget::drawBar() {
    // Horizontal progress bar at bottom
    int barY = _h - 24;  // 24px from bottom
    int barH = 16;       // 16px height
    int barW = _w - 20;  // Full width minus padding
    int barX = 10;       // 10px padding from left
    
    // Calculate fill percentage
    float pct = (_maxValue > 0) ? ((float)_value / (float)_maxValue) : 0.0f;
    int fillW = (int)(barW * pct);
    
    // Background track (dark)
    _sprite.fillRect(barX, barY, barW, barH, Theme::BG_PANEL);
    _sprite.drawRect(barX, barY, barW, barH, Theme::dimColor(_color, 60));
    
    // Active bar with gradient effect
    if (fillW > 0) {
        uint16_t barColor = _highlighted ? _color : Theme::dimColor(_color, 180);
        
        // Draw gradient fill (simplified - solid for now, can add gradient later)
        _sprite.fillRect(barX, barY, fillW, barH, barColor);
        
        // Glow effect on active bar
        if (_highlighted) {
            // Top highlight
            _sprite.drawFastHLine(barX, barY, fillW, Theme::dimColor(barColor, 250));
            // Bottom shadow
            _sprite.drawFastHLine(barX, barY + barH - 1, fillW, Theme::dimColor(barColor, 100));
        }
    }
}

void GaugeWidget::drawValue() {
    int cx = _w / 2;
    int cy = _h / 2 - 10;  // Center vertically, slightly above bar

    // Main value - use Font7 (7-segment) for large display
    _sprite.setTextDatum(textdatum_t::middle_center);
    _sprite.setFont(&fonts::Font7);  // 7-segment font, 48px height
    _sprite.setTextSize(1);
    _sprite.setTextColor(_highlighted ? Theme::TEXT_BRIGHT : Theme::dimColor(Theme::TEXT_BRIGHT, 200));
    
    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%d", _value);
    _sprite.drawString(valueStr, cx, cy);
}

void GaugeWidget::drawTitle() {
    // Parameter name at top-left
    _sprite.setTextDatum(textdatum_t::top_left);
    _sprite.setFont(&fonts::Font2);  // Smaller built-in font
    _sprite.setTextSize(2);  // Increased for readability
    
    // Add text shadow effect (draw slightly offset darker version)
    _sprite.setTextColor(Theme::dimColor(_color, 60));
    _sprite.drawString(_title, 9, 9);  // Shadow offset (adjusted for larger text)
    
    _sprite.setTextColor(_highlighted ? _color : Theme::dimColor(_color, 200));
    _sprite.drawString(_title, 8, 8);  // Main text (adjusted for larger text)
}

void GaugeWidget::drawBackgroundDirect() {
    _display->fillRect(_x, _y, _w, _h, Theme::BG_DARK);

    uint16_t topColor = Theme::dimColor(Theme::BG_PANEL, 128);
    _display->fillRect(_x, _y, _w, _h / 3, topColor);

    uint16_t midColor = Theme::dimColor(Theme::BG_PANEL, 160);
    _display->fillRect(_x, _y + (_h / 3), _w, _h / 3, midColor);

    uint16_t botColor = Theme::dimColor(Theme::BG_PANEL, 192);
    _display->fillRect(_x, _y + ((_h * 2) / 3), _w, _h - ((_h * 2) / 3), botColor);

    uint16_t borderColor = _highlighted ? _color : Theme::dimColor(_color, 120);
    uint16_t glowColor = Theme::dimColor(_color, 40);

    _display->drawRect(_x + 1, _y + 1, _w - 2, _h - 2, glowColor);
    _display->drawRect(_x, _y, _w, _h, borderColor);

    if (_highlighted) {
        _display->drawRect(_x + 2, _y + 2, _w - 4, _h - 4, Theme::dimColor(_color, 200));
    }
}

void GaugeWidget::drawBarDirect() {
    int barY = _y + (_h - 24);
    int barH = 16;
    int barW = _w - 20;
    int barX = _x + 10;

    float pct = (_maxValue > 0) ? ((float)_value / (float)_maxValue) : 0.0f;
    int fillW = (int)(barW * pct);

    _display->fillRect(barX, barY, barW, barH, Theme::BG_PANEL);
    _display->drawRect(barX, barY, barW, barH, Theme::dimColor(_color, 60));

    if (fillW > 0) {
        uint16_t barColor = _highlighted ? _color : Theme::dimColor(_color, 180);
        _display->fillRect(barX, barY, fillW, barH, barColor);

        if (_highlighted) {
            _display->drawFastHLine(barX, barY, fillW, Theme::dimColor(barColor, 250));
            _display->drawFastHLine(barX, barY + barH - 1, fillW, Theme::dimColor(barColor, 100));
        }
    }
}

void GaugeWidget::drawValueDirect() {
    int cx = _x + (_w / 2);
    int cy = _y + (_h / 2) - 10;

    _display->setTextDatum(textdatum_t::middle_center);
    _display->setFont(&fonts::Font7);
    _display->setTextSize(1);
    _display->setTextColor(_highlighted ? Theme::TEXT_BRIGHT : Theme::dimColor(Theme::TEXT_BRIGHT, 200));

    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%d", _value);
    _display->drawString(valueStr, cx, cy);
}

void GaugeWidget::drawTitleDirect() {
    _display->setTextDatum(textdatum_t::top_left);
    _display->setFont(&fonts::Font2);
    _display->setTextSize(2);

    _display->setTextColor(Theme::dimColor(_color, 60));
    _display->drawString(_title, _x + 9, _y + 9);

    _display->setTextColor(_highlighted ? _color : Theme::dimColor(_color, 200));
    _display->drawString(_title, _x + 8, _y + 8);
}
