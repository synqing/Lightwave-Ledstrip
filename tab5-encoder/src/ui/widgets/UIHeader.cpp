// ============================================================================
// UIHeader - Standard header implementation
// ============================================================================

#include "UIHeader.h"

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
#else
    #include <M5Unified.h>
#endif

#include <cstdio>
#include <cstring>

UIHeader::UIHeader(M5GFX* display)
    : _display(display), _sprite(display)
{
    _sprite.setColorDepth(16);
    _sprite.setPsram(true);
    _spriteOk = _sprite.createSprite(Theme::SCREEN_W, Theme::STATUS_BAR_H);
}

UIHeader::~UIHeader() {
    _sprite.deleteSprite();
}

void UIHeader::setConnection(const DeviceConnState& state) {
    if (_conn.wifi != state.wifi || _conn.ws != state.ws ||
        _conn.encA != state.encA || _conn.encB != state.encB) {
        _conn = state;
        _dirty = true;
    }
}

void UIHeader::setPower(int8_t batteryPercent, bool isCharging, float voltage) {
    if (_batteryPercent != batteryPercent || _isCharging != isCharging || 
        (_voltage != voltage && voltage >= 0.0f)) {
        _batteryPercent = batteryPercent;
        _isCharging = isCharging;
        _voltage = voltage;
        _dirty = true;
    }
}

void UIHeader::render() {
    if (!_dirty) return;

    if (!_display) return;

    if (_spriteOk) {
        _sprite.startWrite();
        _sprite.fillSprite(Theme::BG_DARK);

        // Bottom border
        _sprite.drawFastHLine(0, Theme::STATUS_BAR_H - 1, Theme::SCREEN_W, Theme::ACCENT);
        _sprite.drawFastHLine(0, Theme::STATUS_BAR_H - 2, Theme::SCREEN_W, Theme::dimColor(Theme::ACCENT, 60));

        drawTitle();
        drawConnectionStatus();
        drawPowerBar();

        _sprite.endWrite();
        _sprite.pushSprite(0, 0);
    } else {
        _display->startWrite();
        _display->fillRect(0, 0, Theme::SCREEN_W, Theme::STATUS_BAR_H, Theme::BG_DARK);
        _display->drawFastHLine(0, Theme::STATUS_BAR_H - 1, Theme::SCREEN_W, Theme::ACCENT);
        _display->drawFastHLine(0, Theme::STATUS_BAR_H - 2, Theme::SCREEN_W, Theme::dimColor(Theme::ACCENT, 60));

        _display->setTextDatum(textdatum_t::middle_left);
        _display->setFont(&fonts::FreeSansBold18pt7b);
        _display->setTextColor(Theme::ACCENT);
        _display->drawString("LIGHTWAVEOS", 20, 40);

        _display->setFont(&fonts::FreeSans12pt7b);
        _display->setTextColor(Theme::TEXT_DIM);
        _display->drawString("// TAB5 CONTROLLER", 280, 40);

        _display->endWrite();
    }
    _dirty = false;
}

void UIHeader::drawTitle() {
    _sprite.setTextDatum(textdatum_t::middle_left);
    _sprite.setFont(&fonts::FreeSansBold18pt7b);
    _sprite.setTextColor(Theme::ACCENT);
    _sprite.drawString("LIGHTWAVEOS", 20, 40);

    _sprite.setFont(&fonts::FreeSans12pt7b);
    _sprite.setTextColor(Theme::TEXT_DIM);
    _sprite.drawString("// TAB5 CONTROLLER", 280, 40);
}

void UIHeader::drawConnectionStatus() {
    int x = 580;
    int y = 20;
    int spacing = 35;

    _sprite.setFont(&fonts::FreeSansBold9pt7b);
    _sprite.setTextDatum(textdatum_t::top_left);

    // WiFi
    uint16_t wifiCol = _conn.wifi ? Theme::STATUS_OK : Theme::STATUS_ERR;
    _sprite.setTextColor(wifiCol);
    _sprite.drawString(_conn.wifi ? "WIFI OK" : "WIFI --", x, y);

    // WebSocket
    uint16_t wsCol = _conn.ws ? Theme::STATUS_OK :
                     (_conn.wifi ? Theme::STATUS_CONN : Theme::STATUS_ERR);
    _sprite.setTextColor(wsCol);
    _sprite.drawString(_conn.ws ? "WS OK" : "WS --", x + 120, y);

    // Encoder A
    uint16_t encACol = _conn.encA ? Theme::STATUS_OK : Theme::STATUS_ERR;
    _sprite.setTextColor(encACol);
    _sprite.drawString(_conn.encA ? "ENC-A OK" : "ENC-A --", x, y + spacing);

    // Encoder B
    uint16_t encBCol = _conn.encB ? Theme::STATUS_OK : Theme::STATUS_ERR;
    _sprite.setTextColor(encBCol);
    _sprite.drawString(_conn.encB ? "ENC-B OK" : "ENC-B --", x + 120, y + spacing);
}

void UIHeader::drawPowerBar() {
    // Power bar positioned on the right side of header
    // Layout: [Voltage] [Percentage%] [Bar] [CHG]
    int barX = 1080;  // Moved left to make room for voltage
    int barY = 25;
    int barW = 100;   // Slightly narrower bar
    int barH = 20;
    
    // Battery percentage text (right of voltage, left of bar)
    int percentX = barX - 5;
    int percentY = barY;
    
    // Voltage text (leftmost)
    int voltageX = percentX - 60;
    int voltageY = barY;
    
    // Small font for voltage
    _sprite.setFont(&fonts::FreeSans9pt7b);
    _sprite.setTextDatum(textdatum_t::top_right);
    
    // Draw voltage if available
    if (_voltage >= 0.0f) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1fV", _voltage);
        _sprite.setTextColor(Theme::TEXT_DIM);
        _sprite.drawString(buf, voltageX, voltageY);
    }
    
    // Medium font for percentage
    _sprite.setFont(&fonts::FreeSans12pt7b);
    _sprite.setTextDatum(textdatum_t::top_right);
    
    // Draw battery percentage
    if (_batteryPercent >= 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", _batteryPercent);
        // Low battery warning: flash red text if < 20%
        uint16_t percentColor = (_batteryPercent < 20 && !_isCharging) ? 
                                 Theme::STATUS_ERR : Theme::TEXT_BRIGHT;
        _sprite.setTextColor(percentColor);
        _sprite.drawString(buf, percentX, percentY);
    } else {
        _sprite.setTextColor(Theme::TEXT_DIM);
        _sprite.drawString("---", percentX, percentY);
    }
    
    // Draw battery bar background (outline)
    uint16_t outlineColor = Theme::TEXT_DIM;
    // Low battery warning: red outline if < 20% and not charging
    if (_batteryPercent >= 0 && _batteryPercent < 20 && !_isCharging) {
        outlineColor = Theme::STATUS_ERR;
    }
    _sprite.drawRect(barX, barY, barW, barH, outlineColor);
    
    // Draw battery bar fill
    if (_batteryPercent >= 0) {
        int fillW = (barW - 2) * _batteryPercent / 100;
        if (fillW > 0) {
            // Choose color based on battery level
            uint16_t barColor;
            if (_batteryPercent > 50) {
                barColor = Theme::STATUS_OK;  // Green
            } else if (_batteryPercent > 20) {
                barColor = Theme::STATUS_CONN;  // Orange
            } else {
                barColor = Theme::STATUS_ERR;  // Red (low battery)
            }
            
            _sprite.fillRect(barX + 1, barY + 1, fillW, barH - 2, barColor);
        }
    }
    
    // Draw charging indicator if charging
    if (_isCharging) {
        int iconX = barX + barW + 5;
        int iconY = barY;
        
        _sprite.setFont(&fonts::FreeSansBold9pt7b);
        _sprite.setTextDatum(textdatum_t::top_left);
        _sprite.setTextColor(Theme::ACCENT);
        _sprite.drawString("CHG", iconX, iconY);  // Charging indicator
    }
}

