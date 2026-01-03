// ============================================================================
// ZoneComposerUI Implementation
// ============================================================================

#include "ZoneComposerUI.h"
#include "widgets/UIHeader.h"
#include "../input/ButtonHandler.h"
#include "../network/WebSocketClient.h"
#include "../utils/NameLookup.h"
#include "Theme.h"
#include <Arduino.h>
#include <cstdio>
#include "../zones/ZoneDefinition.h"

ZoneComposerUI::ZoneComposerUI(M5GFX& display)
    : _display(display)
{
    // Initialize zone states with defaults
    for (uint8_t i = 0; i < 4; i++) {
        _zones[i] = ZoneState();
        _zones[i].ledStart = i * 40;  // Placeholder LED ranges
        _zones[i].ledEnd = (i + 1) * 40 - 1;
    }
}

ZoneComposerUI::~ZoneComposerUI() {
    // Cleanup handled by display
}

void ZoneComposerUI::begin() {
    markDirty();
    _lastRenderTime = 0;
    
    // Initialize editing segments with default 3-zone layout
    generateZoneSegments(3);
    _zoneCountSelect = 3;
    
    // Validate presets at boot (back-test against v2 firmware expectations)
    validatePresets();
}

void ZoneComposerUI::validatePresets() {
    // Test all presets to ensure they match v2 firmware expectations
    for (int8_t presetId = 0; presetId <= 4; presetId++) {
        loadPreset(presetId);
        uint8_t count = _editingZoneCount;
        if (!validateLayout(_editingSegments, count)) {
            Serial.printf("[ZoneComposer] WARNING: Preset %d failed validation!\n", presetId);
        } else {
            Serial.printf("[ZoneComposer] Preset %d validated OK (%u zones)\n", presetId, count);
        }
    }
    
    // Restore default
    generateZoneSegments(3);
}

void ZoneComposerUI::loop() {
    uint32_t now = millis();
    if (now - _lastRenderTime >= FRAME_INTERVAL_MS) {
        // Only set dirty if pending and enough time has passed
        if (_pendingDirty) {
            markDirty();
            _pendingDirty = false;
        }
        
        if (_dirty) {
            render();
            _dirty = false;
        }
        _lastRenderTime = now;
    }
}

void ZoneComposerUI::updateZone(uint8_t zoneId, const ZoneState& state) {
    if (zoneId >= 4) return;
    
    _zones[zoneId] = state;
    
    // Update LED range from segments if available
    if (zoneId < _zoneCount) {
        const zones::ZoneSegment& seg = _segments[zoneId];
        _zones[zoneId].ledStart = seg.s1LeftStart;
        _zones[zoneId].ledEnd = seg.s1RightEnd;
    }
    
    markDirty();
}

void ZoneComposerUI::updateSegments(const zones::ZoneSegment* segments, uint8_t count) {
    if (!segments || count == 0 || count > zones::MAX_ZONES) {
        return;
    }
    
    _zoneCount = count;
    for (uint8_t i = 0; i < count; i++) {
        _segments[i] = segments[i];
        _editingSegments[i] = segments[i];  // Also update editing copy
        
        // Update LED ranges in zone states
        if (i < 4) {
            _zones[i].ledStart = segments[i].s1LeftStart;
            _zones[i].ledEnd = segments[i].s1RightEnd;
        }
    }
    _editingZoneCount = count;
    _zoneCountSelect = count;
    
    markDirty();
}

void ZoneComposerUI::render() {
    _display.startWrite();

    // Clear screen (header will be rendered separately by DisplayUI)
    _display.fillScreen(Theme::BG_DARK);

    // LED strip visualization (y offset accounts for header)
    drawLedStripVisualiser(40, LED_STRIP_Y, 1200, 80);

    // Zone controls (y offset accounts for header)
    drawZoneList(40, ZONE_LIST_Y, 1200, 320);

    // Editor controls - NO APPLY BUTTON (y offset accounts for header)
    drawEditorControls(40, CONTROLS_Y, 1200, 180);

    // Draw dropdown overlay if open
    if (_openDropdown != DropdownType::NONE) {
        drawDropdown();
    }

    _display.endWrite();
}

void ZoneComposerUI::drawLedStripVisualiser(int x, int y, int w, int h) {
    // Use editing segments for visualization
    uint8_t visZoneCount = _editingZoneCount > 0 ? _editingZoneCount : _zoneCount;
    const zones::ZoneSegment* visSegments = _editingZoneCount > 0 ? _editingSegments : _segments;
    
    // Title (Font4 size 1 = 32px)
    _display.setFont(&fonts::Font4);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.setTextDatum(textdatum_t::top_center);
    _display.drawString("LED STRIP VISUALIZATION", _display.width() / 2, y - 40);
    
    // Labels above strips (Font2 size 1 = 18px)
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::top_left);
    _display.drawString("Left (0-79)", x, y - 20);
    _display.setTextDatum(textdatum_t::top_right);
    _display.drawString("Right (80-159)", x + w, y - 20);
    
    // Calculate dimensions for mirrored layout
    int stripH = 60;  // Taller for visibility
    int ledW = (w - 20) / 160;  // 160 total LEDs
    if (ledW < 2) ledW = 2;
    
    int centreX = x + w / 2;
    int gap = 8;  // Wider centre gap
    
    // Draw LEFT strip (0-79) - REVERSED so LED 79 is at centre
    for (int i = 0; i < 80; i++) {
        uint8_t ledIdx = 79 - i;  // Reverse: LED 79 at centre
        int ledX = centreX - gap/2 - (i+1) * ledW;
        
        uint8_t zoneId = 255;
        for (uint8_t z = 0; z < visZoneCount; z++) {
            if (ledIdx >= visSegments[z].s1LeftStart && ledIdx <= visSegments[z].s1LeftEnd) {
                zoneId = z;
                break;
            }
        }
        uint16_t color = (zoneId < zones::MAX_ZONES) ? 
            rgb888To565(getZoneColor(zoneId)) : Theme::BG_PANEL;
        
        _display.fillRect(ledX, y, ledW-1, stripH, color);
        
        // Highlight centre LED 79
        if (ledIdx == zones::CENTER_LEFT) {
            _display.drawRect(ledX, y, ledW-1, stripH, Theme::TEXT_BRIGHT);
        }
    }
    
    // Draw RIGHT strip (80-159) - NORMAL order from centre
    for (int i = 0; i < 80; i++) {
        uint8_t ledIdx = 80 + i;  // LED 80 at centre
        int ledX = centreX + gap/2 + i * ledW;
        
        uint8_t zoneId = 255;
        for (uint8_t z = 0; z < visZoneCount; z++) {
            if (ledIdx >= visSegments[z].s1RightStart && ledIdx <= visSegments[z].s1RightEnd) {
                zoneId = z;
                break;
            }
        }
        uint16_t color = (zoneId < zones::MAX_ZONES) ? 
            rgb888To565(getZoneColor(zoneId)) : Theme::BG_PANEL;
        
        _display.fillRect(ledX, y, ledW-1, stripH, color);
        
        // Highlight centre LED 80
        if (ledIdx == zones::CENTER_RIGHT) {
            _display.drawRect(ledX, y, ledW-1, stripH, Theme::TEXT_BRIGHT);
        }
    }
    
    // Draw centre divider with label
    _display.fillRect(centreX - gap/2, y, gap, stripH, Theme::ACCENT);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextDatum(textdatum_t::top_center);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.drawString("Centre pair: LEDs 79 (left) / 80 (right)", centreX, y + stripH + 8);
}

void ZoneComposerUI::drawZoneList(int x, int y, int w, int h) {
    // Section label (Font2 size 1 = 18px)
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::top_left);
    _display.drawString("Zone Controls", x, y - 25);

    // Draw zone rows (up to _editingZoneCount zones)
    uint8_t displayCount = _editingZoneCount > 0 ? _editingZoneCount : _zoneCount;
    int rowH = h / displayCount;
    if (rowH < 40) rowH = 40;

    for (uint8_t i = 0; i < displayCount && i < zones::MAX_ZONES; i++) {
        int rowY = y + i * rowH;
        drawZoneRow(i, x, rowY, w, rowH - 4);
    }
}

void ZoneComposerUI::drawZoneRow(uint8_t zoneId, int x, int y, int w, int h) {
    if (zoneId >= zones::MAX_ZONES) return;

    const ZoneState& zone = _zones[zoneId];
    uint32_t zoneColor = getZoneColor(zoneId);
    uint16_t zoneColor565 = rgb888To565(zoneColor);
    
    // Use editing segments if available
    bool useEditing = (zoneId < _editingZoneCount);
    const zones::ZoneSegment* seg = useEditing ? &_editingSegments[zoneId] : 
                                      (zoneId < _zoneCount ? &_segments[zoneId] : nullptr);

    // Background panel
    _display.fillRect(x, y, w, h, Theme::BG_PANEL);
    uint16_t borderColor = (_selectedZone == zoneId) ? Theme::TEXT_BRIGHT : zoneColor565;
    _display.drawRect(x, y, w, h, borderColor);

    // Zone header
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.setTextDatum(textdatum_t::middle_left);
    char zoneTitle[16];
    snprintf(zoneTitle, sizeof(zoneTitle), "Zone %u", zoneId);
    _display.drawString(zoneTitle, x + 10, y + h / 2);

    // LED range
    char ledRange[32];
    if (seg) {
        snprintf(ledRange, sizeof(ledRange), "LED %u-%u / %u-%u", 
                 seg->s1LeftStart, seg->s1LeftEnd, seg->s1RightStart, seg->s1RightEnd);
    } else {
        snprintf(ledRange, sizeof(ledRange), "LED %u-%u", zone.ledStart, zone.ledEnd);
    }
    _display.setTextColor(Theme::TEXT_DIM);
    _display.drawString(ledRange, x + 100, y + h / 2);

    // Dropdowns
    int dropdownW = 150;
    int dropdownH = 30;
    int dropdownY = y + (h - dropdownH) / 2;
    int dropdownSpacing = 160;

    // Effect dropdown
    int effectX = x + 300;
    bool effectHighlight = (_openDropdown == DropdownType::EFFECT && _openDropdownZone == zoneId);
    _display.fillRect(effectX, dropdownY, dropdownW, dropdownH, effectHighlight ? Theme::ACCENT : Theme::BG_DARK);
    _display.drawRect(effectX, dropdownY, dropdownW, dropdownH, Theme::ACCENT);
    _display.setFont(&fonts::Font2);
    _display.setTextSize(2);  // 24px for effect/palette names
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.setTextDatum(textdatum_t::middle_left);
    
    // Effect name with fallback lookup
    const char* effectText = zone.effectName;
    if (!effectText) {
        effectText = lookupEffectName(zone.effectId);
    }
    char effectBuf[48];
    if (!effectText) {
        snprintf(effectBuf, sizeof(effectBuf), "Effect #%u", zone.effectId);
        effectText = effectBuf;
    }
    _display.drawString(effectText, effectX + 15, y + h / 2);

    // Palette dropdown
    int paletteX = x + 300 + dropdownSpacing;
    bool paletteHighlight = (_openDropdown == DropdownType::PALETTE && _openDropdownZone == zoneId);
    _display.fillRect(paletteX, dropdownY, dropdownW, dropdownH, paletteHighlight ? Theme::ACCENT : Theme::BG_DARK);
    _display.drawRect(paletteX, dropdownY, dropdownW, dropdownH, Theme::ACCENT);
    
    // Palette name with fallback lookup
    const char* paletteText = zone.paletteName;
    if (!paletteText) {
        paletteText = lookupPaletteName(zone.paletteId);
    }
    char paletteBuf[48];
    if (!paletteText) {
        snprintf(paletteBuf, sizeof(paletteBuf), "Palette #%u", zone.paletteId);
        paletteText = paletteBuf;
    }
    _display.drawString(paletteText, paletteX + 15, y + h / 2);

    // Blend dropdown
    int blendX = x + 300 + dropdownSpacing * 2;
    bool blendHighlight = (_openDropdown == DropdownType::BLEND && _openDropdownZone == zoneId);
    _display.fillRect(blendX, dropdownY, dropdownW, dropdownH, blendHighlight ? Theme::ACCENT : Theme::BG_DARK);
    _display.drawRect(blendX, dropdownY, dropdownW, dropdownH, Theme::ACCENT);
    const char* blendText = zone.blendModeName ? zone.blendModeName : "Blend";
    char blendBuf[32];
    if (!zone.blendModeName) {
        snprintf(blendBuf, sizeof(blendBuf), "Blend #%u", zone.blendMode);
        blendText = blendBuf;
    }
    _display.drawString(blendText, blendX + 10, y + h / 2);
}

void ZoneComposerUI::drawEditorControls(int x, int y, int w, int h) {
    // Zone count selector
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::top_left);
    _display.drawString("Zones:", x, y);
    
    char countStr[16];
    snprintf(countStr, sizeof(countStr), "%u", _zoneCountSelect);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.drawString(countStr, x + 80, y);
    
    // Zone count steppers
    int stepperW = 40;
    int stepperH = 30;
    int stepperY = y + 20;
    
    // Decrease zone count
    _display.fillRect(x + 120, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 120, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.setTextDatum(textdatum_t::middle_center);
    _display.drawString("-", x + 120 + stepperW / 2, stepperY + stepperH / 2);
    
    // Increase zone count
    _display.fillRect(x + 170, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 170, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.drawString("+", x + 170 + stepperW / 2, stepperY + stepperH / 2);

    // Preset selector
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::top_left);
    _display.drawString("Preset:", x + 250, y);
    
    const char* presetName = "Custom";
    if (_presetSelect >= 0 && _presetSelect <= 4) {
        const char* presetNames[] = {"Unified", "Dual Split", "Triple Rings", "Quad Active", "Heartbeat Focus"};
        presetName = presetNames[_presetSelect];
    }
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.drawString(presetName, x + 330, y);
    
    // Preset steppers
    _display.fillRect(x + 500, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 500, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.setTextDatum(textdatum_t::middle_center);
    _display.drawString("-", x + 500 + stepperW / 2, stepperY + stepperH / 2);
    
    _display.fillRect(x + 550, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 550, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.drawString("+", x + 550 + stepperW / 2, stepperY + stepperH / 2);

    // Draw stepper controls for selected zone
    if (_selectedZone < _editingZoneCount) {
        drawStepperControls(x, y + 60, w, 80);
    }
}

uint32_t ZoneComposerUI::getZoneColor(uint8_t zoneId) const {
    if (zoneId >= 4) return ZONE_COLORS[0];
    return ZONE_COLORS[zoneId];
}

uint16_t ZoneComposerUI::rgb888To565(uint32_t rgb888) {
    uint8_t r = (rgb888 >> 16) & 0xFF;
    uint8_t g = (rgb888 >> 8) & 0xFF;
    uint8_t b = rgb888 & 0xFF;
    
    // Convert to RGB565
    r = r >> 3;
    g = g >> 2;
    b = b >> 3;
    
    return (r << 11) | (g << 5) | b;
}

uint8_t ZoneComposerUI::getZoneForLed(uint8_t ledIndex, bool isRight) {
    for (uint8_t i = 0; i < _zoneCount; i++) {
        const zones::ZoneSegment& seg = _segments[i];
        if (isRight) {
            if (ledIndex >= seg.s1RightStart && ledIndex <= seg.s1RightEnd) {
                return seg.zoneId;
            }
        } else {
            if (ledIndex >= seg.s1LeftStart && ledIndex <= seg.s1LeftEnd) {
                return seg.zoneId;
            }
        }
    }
    return 255;  // No zone assigned
}

int8_t ZoneComposerUI::hitTestLedStrip(int16_t x, int16_t y) {
    // Check if touch is within LED strip area
    if (y < LED_STRIP_Y || y > LED_STRIP_Y + LED_STRIP_H) {
        return -1;
    }
    
    int stripW = (_display.width() - 40) / 2;
    int gap = 4;
    
    // Left strip (0-79)
    if (x >= 20 && x < 20 + stripW) {
        int ledW = stripW / 80;
        int ledIndex = 79 - ((x - 20) / ledW);
        if (ledIndex >= 0 && ledIndex <= 79) {
            return ledIndex;  // Return left LED index
        }
    }
    
    // Right strip (80-159)
    if (x >= 20 + stripW + gap && x < 20 + stripW + gap + stripW) {
        int rightLedW = stripW / 80;
        int ledIndex = 80 + ((x - 20 - stripW - gap) / rightLedW);
        if (ledIndex >= 80 && ledIndex <= zones::MAX_LED) {
            return ledIndex;  // Return right LED index
        }
    }
    
    return -1;
}

void ZoneComposerUI::handleTouch(int16_t x, int16_t y) {
    // #region agent log
    {
        // HZ1: Determine whether touch events are being processed with inconsistent zone counts / dropdown state.
        char buf[360];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HZ1\",\"location\":\"Tab5.encoder/src/ui/ZoneComposerUI.cpp:handleTouch\",\"message\":\"touch.enter\",\"data\":{\"x\":%d,\"y\":%d,\"openDd\":%u,\"openDdZone\":%u,\"zoneCount\":%u,\"editingZoneCount\":%u,\"selectedZone\":%u,\"ddX\":%d,\"ddY\":%d,\"ddW\":%d,\"ddH\":%d,\"ddScroll\":%d},\"timestamp\":%lu}",
            x, y,
            static_cast<unsigned>(_openDropdown),
            static_cast<unsigned>(_openDropdownZone),
            static_cast<unsigned>(_zoneCount),
            static_cast<unsigned>(_editingZoneCount),
            static_cast<unsigned>(_selectedZone),
            static_cast<int>(_dropdownX), static_cast<int>(_dropdownY),
            static_cast<int>(_dropdownW), static_cast<int>(_dropdownH),
            static_cast<int>(_dropdownScroll),
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    // If dropdown is open, check if touch is in dropdown or closes it
    if (_openDropdown != DropdownType::NONE) {
        if (x >= _dropdownX && x < _dropdownX + _dropdownW &&
            y >= _dropdownY && y < _dropdownY + _dropdownH) {
            // Touch inside dropdown - handle selection
            int itemH = 30;
            int itemIndex = (y - _dropdownY + _dropdownScroll) / itemH;
            handleDropdownSelection(_openDropdown, _openDropdownZone, itemIndex);
            closeDropdown();
        } else {
            // Touch outside - close dropdown
            closeDropdown();
        }
        return;
    }

    // Check LED strip touch
    int8_t ledIndex = hitTestLedStrip(x, y);
    if (ledIndex >= 0) {
        bool isRight = (ledIndex >= 80);
        uint8_t zoneId = 255;
        for (uint8_t z = 0; z < _editingZoneCount; z++) {
            if (isRight) {
                if (ledIndex >= _editingSegments[z].s1RightStart && ledIndex <= _editingSegments[z].s1RightEnd) {
                    zoneId = z;
                    break;
                }
            } else {
                if (ledIndex >= _editingSegments[z].s1LeftStart && ledIndex <= _editingSegments[z].s1LeftEnd) {
                    zoneId = z;
                    break;
                }
            }
        }
        if (zoneId < zones::MAX_ZONES) {
            _selectedZone = zoneId;
            markDirty();
        }
    }
    
    // Check dropdown touches in zone rows
    int8_t dropdownHit = hitTestDropdown(x, y);
    if (dropdownHit >= 0) {
        // Dropdown was tapped - will be handled by hitTestDropdown setting state
        markDirty();
    }
    
    // Check stepper controls
    if (y >= CONTROLS_Y + 60 && y < CONTROLS_Y + 140) {
        // Zone count steppers
        if (x >= 20 + 120 && x < 20 + 160 && y >= CONTROLS_Y + 20 && y < CONTROLS_Y + 50) {
            // Decrease zone count
            if (_zoneCountSelect > 1) {
                _zoneCountSelect--;
                generateZoneSegments(_zoneCountSelect);
                _presetSelect = -1;  // Clear preset
            }
            markDirty();
        } else if (x >= 20 + 170 && x < 20 + 210 && y >= CONTROLS_Y + 20 && y < CONTROLS_Y + 50) {
            // Increase zone count
            if (_zoneCountSelect < 4) {
                _zoneCountSelect++;
                generateZoneSegments(_zoneCountSelect);
                _presetSelect = -1;  // Clear preset
            }
            markDirty();
        }
        
        // Preset steppers
        if (x >= 20 + 500 && x < 20 + 540 && y >= CONTROLS_Y + 20 && y < CONTROLS_Y + 50) {
            // Decrease preset
            if (_presetSelect > -1) {
                _presetSelect--;
                if (_presetSelect >= 0) {
                    loadPreset(_presetSelect);
                } else {
                    generateZoneSegments(_zoneCountSelect);
                }
            }
            markDirty();
        } else if (x >= 20 + 550 && x < 20 + 590 && y >= CONTROLS_Y + 20 && y < CONTROLS_Y + 50) {
            // Increase preset
            if (_presetSelect < 4) {
                _presetSelect++;
                loadPreset(_presetSelect);
            }
            markDirty();
        }
        
        // Zone boundary steppers (if zone selected)
        if (_selectedZone < _editingZoneCount) {
            int stepperY = CONTROLS_Y + 60;
            // Left Start steppers
            if (x >= 20 + 130 && x < 20 + 180 && y >= stepperY && y < stepperY + 30) {
                // Decrease left start (not implemented - would require adjusting previous zone)
            } else if (x >= 20 + 190 && x < 20 + 240 && y >= stepperY && y < stepperY + 30) {
                // Increase left start (not implemented)
            }
            // Left End steppers
            else if (x >= 20 + 410 && x < 20 + 460 && y >= stepperY && y < stepperY + 30) {
                adjustZoneBoundary(_selectedZone, false);
            } else if (x >= 20 + 470 && x < 20 + 520 && y >= stepperY && y < stepperY + 30) {
                adjustZoneBoundary(_selectedZone, true);
            }
        }
    }
}

int8_t ZoneComposerUI::hitTestDropdown(int16_t x, int16_t y) {
    // Check if touch is in zone list area
    if (y < ZONE_LIST_Y || y > ZONE_LIST_Y + 200) {
        return -1;
    }

    // Use the same "display count" concept as rendering (editing takes precedence).
    const uint8_t displayCount = (_editingZoneCount > 0) ? _editingZoneCount : _zoneCount;

    // #region agent log
    {
        // HZ2: Confirm whether _zoneCount==0 (division-by-zero risk) or mismatch with _editingZoneCount.
        char buf[260];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HZ2\",\"location\":\"Tab5.encoder/src/ui/ZoneComposerUI.cpp:hitTestDropdown\",\"message\":\"dropdown.hittest.enter\",\"data\":{\"x\":%d,\"y\":%d,\"zoneCount\":%u,\"editingZoneCount\":%u,\"displayCount\":%u},\"timestamp\":%lu}",
            x, y,
            static_cast<unsigned>(_zoneCount),
            static_cast<unsigned>(_editingZoneCount),
            static_cast<unsigned>(displayCount),
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    if (displayCount == 0) {
        // Nothing to hit-test yet (prevents division by zero and bogus row math).
        return -1;
    }

    int rowH = 200 / displayCount;
    if (rowH < 40) rowH = 40;
    
    // Determine which zone row
    int rowIndex = (y - ZONE_LIST_Y) / rowH;
    if (rowIndex < 0 || rowIndex >= displayCount) {
        return -1;
    }
    
    uint8_t zoneId = rowIndex;
    int dropdownW = 150;
    int dropdownSpacing = 160;
    int baseX = 20 + 300;
    
    // Check Effect dropdown
    if (x >= baseX && x < baseX + dropdownW) {
        int dropdownH = 30;
        int rowY = ZONE_LIST_Y + rowIndex * rowH;
        int dropdownY = rowY + (rowH - dropdownH) / 2;
        if (y >= dropdownY && y < dropdownY + dropdownH) {
            openDropdown(DropdownType::EFFECT, zoneId, baseX, dropdownY, dropdownW, 200);
            return 0;
        }
    }
    
    // Check Palette dropdown
    int paletteX = baseX + dropdownSpacing;
    if (x >= paletteX && x < paletteX + dropdownW) {
        int dropdownH = 30;
        int rowY = ZONE_LIST_Y + rowIndex * rowH;
        int dropdownY = rowY + (rowH - dropdownH) / 2;
        if (y >= dropdownY && y < dropdownY + dropdownH) {
            openDropdown(DropdownType::PALETTE, zoneId, paletteX, dropdownY, dropdownW, 200);
            return 1;
        }
    }
    
    // Check Blend dropdown
    int blendX = baseX + dropdownSpacing * 2;
    if (x >= blendX && x < blendX + dropdownW) {
        int dropdownH = 30;
        int rowY = ZONE_LIST_Y + rowIndex * rowH;
        int dropdownY = rowY + (rowH - dropdownH) / 2;
        if (y >= dropdownY && y < dropdownY + dropdownH) {
            openDropdown(DropdownType::BLEND, zoneId, blendX, dropdownY, dropdownW, 200);
            return 2;
        }
    }
    
    return -1;
}

void ZoneComposerUI::openDropdown(DropdownType type, uint8_t zoneId, int16_t x, int16_t y, int16_t w, int16_t h) {
    _openDropdown = type;
    _openDropdownZone = zoneId;
    _dropdownX = x;
    _dropdownY = y + 35;  // Position below the button
    _dropdownW = w;
    _dropdownH = h;
    _dropdownScroll = 0;

    // #region agent log
    {
        // HZ3: Prove dropdown open parameters + which zone/type is being opened.
        char buf[260];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HZ3\",\"location\":\"Tab5.encoder/src/ui/ZoneComposerUI.cpp:openDropdown\",\"message\":\"dropdown.open\",\"data\":{\"type\":%u,\"zoneId\":%u,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},\"timestamp\":%lu}",
            static_cast<unsigned>(type),
            static_cast<unsigned>(zoneId),
            static_cast<int>(_dropdownX), static_cast<int>(_dropdownY),
            static_cast<int>(_dropdownW), static_cast<int>(_dropdownH),
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    markDirty();
}

void ZoneComposerUI::closeDropdown() {
    _openDropdown = DropdownType::NONE;
    _openDropdownZone = 255;
    _dropdownScroll = 0;
    markDirty();
}

void ZoneComposerUI::drawDropdown() {
    if (_openDropdown == DropdownType::NONE) return;
    
    // Draw dropdown background
    _display.fillRect(_dropdownX, _dropdownY, _dropdownW, _dropdownH, Theme::BG_PANEL);
    _display.drawRect(_dropdownX, _dropdownY, _dropdownW, _dropdownH, Theme::ACCENT);
    
    // Draw dropdown list
    drawDropdownList(_openDropdown, _dropdownX, _dropdownY, _dropdownW, _dropdownH);
}

void ZoneComposerUI::drawDropdownList(DropdownType type, int16_t x, int16_t y, int16_t w, int16_t h) {
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    int itemH = 30;
    int maxItems = h / itemH;
    
    if (type == DropdownType::EFFECT) {
        // Draw effect list (simplified - show IDs 0-95)
        for (int i = 0; i < maxItems && i < 96; i++) {
            int itemY = y + i * itemH;
            uint8_t effectId = i;
            
            // Highlight current selection
            if (effectId == _zones[_openDropdownZone].effectId) {
                _display.fillRect(x, itemY, w, itemH, Theme::ACCENT);
            }
            
            _display.setTextColor(Theme::TEXT_BRIGHT);
            _display.setTextDatum(textdatum_t::middle_left);
            char buf[32];
            snprintf(buf, sizeof(buf), "Effect #%u", effectId);
            _display.drawString(buf, x + 10, itemY + itemH / 2);
        }
    } else if (type == DropdownType::PALETTE) {
        // Draw palette list (0-63)
        for (int i = 0; i < maxItems && i < 64; i++) {
            int itemY = y + i * itemH;
            uint8_t paletteId = i;
            
            if (paletteId == _zones[_openDropdownZone].paletteId) {
                _display.fillRect(x, itemY, w, itemH, Theme::ACCENT);
            }
            
            _display.setTextColor(Theme::TEXT_BRIGHT);
            _display.setTextDatum(textdatum_t::middle_left);
            char buf[32];
            snprintf(buf, sizeof(buf), "Palette #%u", paletteId);
            _display.drawString(buf, x + 10, itemY + itemH / 2);
        }
    } else if (type == DropdownType::BLEND) {
        // Draw blend mode list (0-7)
        const char* blendNames[] = {
            "OVERWRITE", "ADDITIVE", "ALPHA", "MULTIPLY",
            "SCREEN", "SUBTRACT", "DIFFERENCE", "EXCLUSION"
        };
        
        for (int i = 0; i < 8 && i < maxItems; i++) {
            int itemY = y + i * itemH;
            uint8_t blendMode = i;
            
            if (blendMode == _zones[_openDropdownZone].blendMode) {
                _display.fillRect(x, itemY, w, itemH, Theme::ACCENT);
            }
            
            _display.setTextColor(Theme::TEXT_BRIGHT);
            _display.setTextDatum(textdatum_t::middle_left);
            _display.drawString(blendNames[i], x + 10, itemY + itemH / 2);
        }
    }
}

void ZoneComposerUI::handleDropdownSelection(DropdownType type, uint8_t zoneId, int itemIndex) {
    // #region agent log
    {
        // HZ4: Confirm whether selections are ignored due to missing ws client or invalid indices.
        char buf[260];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HZ4\",\"location\":\"Tab5.encoder/src/ui/ZoneComposerUI.cpp:handleDropdownSelection\",\"message\":\"dropdown.select\",\"data\":{\"type\":%u,\"zoneId\":%u,\"itemIndex\":%d,\"ws\":%s},\"timestamp\":%lu}",
            static_cast<unsigned>(type),
            static_cast<unsigned>(zoneId),
            itemIndex,
            _wsClient ? "true" : "false",
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    if (zoneId >= zones::MAX_ZONES) return;
    
    if (type == DropdownType::EFFECT) {
        if (itemIndex >= 0 && itemIndex < 96) {
            // Update local state immediately so UI reflects selection even if WS is slow/unavailable.
            _zones[zoneId].effectId = static_cast<uint8_t>(itemIndex);
            _zones[zoneId].effectName = nullptr;  // Force lookup fallback on next render
            if (_wsClient) _wsClient->sendZoneEffect(zoneId, itemIndex);
            markDirty();
        }
    } else if (type == DropdownType::PALETTE) {
        if (itemIndex >= 0 && itemIndex < 64) {
            _zones[zoneId].paletteId = static_cast<uint8_t>(itemIndex);
            _zones[zoneId].paletteName = nullptr;  // Force lookup fallback on next render
            if (_wsClient) _wsClient->sendZonePalette(zoneId, itemIndex);
            markDirty();
        }
    } else if (type == DropdownType::BLEND) {
        if (itemIndex >= 0 && itemIndex < 8) {
            _zones[zoneId].blendMode = static_cast<uint8_t>(itemIndex);
            _zones[zoneId].blendModeName = nullptr;  // Force string fallback on next render
            if (_wsClient) _wsClient->sendZoneBlend(zoneId, itemIndex);
            markDirty();
        }
    }
}

void ZoneComposerUI::generateZoneSegments(uint8_t zoneCount) {
    if (zoneCount < 1 || zoneCount > zones::MAX_ZONES) {
        return;
    }
    
    const uint8_t LEDsPerSide = 80;
    const uint8_t centerLeft = zones::CENTER_LEFT;
    const uint8_t centerRight = zones::CENTER_RIGHT;
    
    // Distribute LEDs evenly across zones, centre-out
    const uint8_t ledsPerZone = LEDsPerSide / zoneCount;
    const uint8_t remainder = LEDsPerSide % zoneCount;
    
    // Build zones from centre outward
    uint8_t leftEnd = centerLeft;
    uint8_t rightStart = centerRight;
    
    for (uint8_t i = 0; i < zoneCount; i++) {
        // Calculate zone size (give remainder to outermost zones)
        const uint8_t zoneSize = ledsPerZone + (i >= zoneCount - remainder ? 1 : 0);
        
        // Left segment (descending from centre)
        const uint8_t leftStart = leftEnd - zoneSize + 1;
        
        // Right segment (ascending from centre)
        const uint8_t rightEnd = rightStart + zoneSize - 1;
        
        _editingSegments[i].zoneId = i;
        _editingSegments[i].s1LeftStart = leftStart;
        _editingSegments[i].s1LeftEnd = leftEnd;
        _editingSegments[i].s1RightStart = rightStart;
        _editingSegments[i].s1RightEnd = rightEnd;
        _editingSegments[i].totalLeds = zoneSize * 2;
        
        // Move outward for next zone
        leftEnd = leftStart - 1;
        rightStart = rightEnd + 1;
    }
    
    // Reverse to get centre-out order (zone 0 = innermost)
    // Actually, we built them outer-in, so reverse
    for (uint8_t i = 0; i < zoneCount / 2; i++) {
        zones::ZoneSegment temp = _editingSegments[i];
        _editingSegments[i] = _editingSegments[zoneCount - 1 - i];
        _editingSegments[zoneCount - 1 - i] = temp;
        _editingSegments[i].zoneId = i;
        _editingSegments[zoneCount - 1 - i].zoneId = zoneCount - 1 - i;
    }
    
    // Re-assign zone IDs to match order
    for (uint8_t i = 0; i < zoneCount; i++) {
        _editingSegments[i].zoneId = i;
    }
    
    _editingZoneCount = zoneCount;
    markDirty();
}

void ZoneComposerUI::loadPreset(int8_t presetId) {
    if (presetId < 0 || presetId > 4) {
        _presetSelect = -1;
        return;
    }
    
    // Preset definitions (matching webapp and v2 firmware)
    static const zones::ZoneSegment PRESET_0[3] = {  // Unified
        {0, 65, 79, 80, 94, 30},
        {1, 20, 64, 95, 139, 90},
        {2, 0, 19, 140, 159, 40}
    };
    static const zones::ZoneSegment PRESET_1[3] = {  // Dual Split (same as Unified)
        {0, 65, 79, 80, 94, 30},
        {1, 20, 64, 95, 139, 90},
        {2, 0, 19, 140, 159, 40}
    };
    static const zones::ZoneSegment PRESET_2[3] = {  // Triple Rings (same as Unified)
        {0, 65, 79, 80, 94, 30},
        {1, 20, 64, 95, 139, 90},
        {2, 0, 19, 140, 159, 40}
    };
    static const zones::ZoneSegment PRESET_3[4] = {  // Quad Active
        {0, 60, 79, 80, 99, 40},
        {1, 40, 59, 100, 119, 40},
        {2, 20, 39, 120, 139, 40},
        {3, 0, 19, 140, 159, 40}
    };
    static const zones::ZoneSegment PRESET_4[3] = {  // Heartbeat Focus (same as Unified)
        {0, 65, 79, 80, 94, 30},
        {1, 20, 64, 95, 139, 90},
        {2, 0, 19, 140, 159, 40}
    };
    
    const zones::ZoneSegment* preset = nullptr;
    uint8_t count = 0;
    
    switch (presetId) {
        case 0: preset = PRESET_0; count = 3; break;
        case 1: preset = PRESET_1; count = 3; break;
        case 2: preset = PRESET_2; count = 3; break;
        case 3: preset = PRESET_3; count = 4; break;
        case 4: preset = PRESET_4; count = 3; break;
    }
    
    if (preset) {
        for (uint8_t i = 0; i < count; i++) {
            _editingSegments[i] = preset[i];
        }
        _editingZoneCount = count;
        _zoneCountSelect = count;
        _presetSelect = presetId;
        markDirty();
    }
}

void ZoneComposerUI::drawStepperControls(int x, int y, int w, int h) {
    if (_selectedZone >= _editingZoneCount) return;
    
    const zones::ZoneSegment& seg = _editingSegments[_selectedZone];
    
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::top_left);
    
    char label[32];
    snprintf(label, sizeof(label), "Zone %u:", _selectedZone);
    _display.drawString(label, x, y);
    
    // Left start/end steppers
    int stepperW = 50;
    int stepperH = 30;
    int stepperY = y + 20;
    int stepperSpacing = 80;
    
    // Left Start
    _display.setTextColor(Theme::TEXT_DIM);
    _display.drawString("L Start:", x, stepperY);
    char leftStartStr[8];
    snprintf(leftStartStr, sizeof(leftStartStr), "%u", seg.s1LeftStart);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.drawString(leftStartStr, x + 80, stepperY);
    
    _display.fillRect(x + 130, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 130, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.setTextDatum(textdatum_t::middle_center);
    _display.drawString("-", x + 130 + stepperW / 2, stepperY + stepperH / 2);
    
    _display.fillRect(x + 190, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 190, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.drawString("+", x + 190 + stepperW / 2, stepperY + stepperH / 2);
    
    // Left End
    _display.setTextDatum(textdatum_t::top_left);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.drawString("L End:", x + 280, stepperY);
    char leftEndStr[8];
    snprintf(leftEndStr, sizeof(leftEndStr), "%u", seg.s1LeftEnd);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.drawString(leftEndStr, x + 360, stepperY);
    
    _display.fillRect(x + 410, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 410, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.setTextDatum(textdatum_t::middle_center);
    _display.drawString("-", x + 410 + stepperW / 2, stepperY + stepperH / 2);
    
    _display.fillRect(x + 470, stepperY, stepperW, stepperH, Theme::BG_PANEL);
    _display.drawRect(x + 470, stepperY, stepperW, stepperH, Theme::ACCENT);
    _display.drawString("+", x + 470 + stepperW / 2, stepperY + stepperH / 2);
}

void ZoneComposerUI::adjustZoneBoundary(uint8_t zoneId, bool increase) {
    if (zoneId >= _editingZoneCount) return;
    
    zones::ZoneSegment& seg = _editingSegments[zoneId];
    
    // Adjust left end (which determines right start via symmetry)
    if (increase) {
        if (seg.s1LeftEnd < zones::CENTER_LEFT) {
            seg.s1LeftEnd++;
            // Recalculate right segment to maintain symmetry
            uint8_t leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
            uint8_t leftDist = zones::CENTER_LEFT - seg.s1LeftEnd;
            seg.s1RightStart = zones::CENTER_RIGHT + leftDist;
            seg.s1RightEnd = seg.s1RightStart + leftSize - 1;
            seg.totalLeds = leftSize * 2;
        }
    } else {
        if (seg.s1LeftEnd > seg.s1LeftStart) {
            seg.s1LeftEnd--;
            // Recalculate right segment
            uint8_t leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
            uint8_t leftDist = zones::CENTER_LEFT - seg.s1LeftEnd;
            seg.s1RightStart = zones::CENTER_RIGHT + leftDist;
            seg.s1RightEnd = seg.s1RightStart + leftSize - 1;
            seg.totalLeds = leftSize * 2;
        }
    }
    
    markDirty();
    
    // Send immediate layout update to server
    if (_wsClient) {
        _wsClient->sendZonesSetLayout(_editingSegments, _editingZoneCount);
    }
}

bool ZoneComposerUI::validateLayout(const zones::ZoneSegment* segments, uint8_t count) const {
    if (!segments || count == 0 || count > zones::MAX_ZONES) {
        return false;
    }
    
    // Coverage map: track which LEDs are assigned (per strip, 0-159)
    bool coverage[zones::MAX_LED + 1] = {false};
    
    for (uint8_t i = 0; i < count; i++) {
        const zones::ZoneSegment& seg = segments[i];
        
        // 1. Boundary Range Check
        if (seg.s1LeftStart > seg.s1LeftEnd || seg.s1LeftEnd > zones::CENTER_LEFT) {
            return false;
        }
        if (seg.s1RightStart < zones::CENTER_RIGHT || seg.s1RightStart > seg.s1RightEnd || seg.s1RightEnd > zones::MAX_LED) {
            return false;
        }
        
        // 2. Minimum Zone Size Check
        uint8_t leftSize = seg.s1LeftEnd - seg.s1LeftStart + 1;
        uint8_t rightSize = seg.s1RightEnd - seg.s1RightStart + 1;
        if (leftSize == 0 || rightSize == 0) {
            return false;
        }
        
        // 3. Symmetry Check
        if (leftSize != rightSize) {
            return false;
        }
        
        // Check distance from centre
        uint8_t leftDist = zones::CENTER_LEFT - seg.s1LeftEnd;
        uint8_t rightDist = seg.s1RightStart - zones::CENTER_RIGHT;
        if (leftDist != rightDist) {
            return false;
        }
        
        // 4. Centre Pair Check (at least one zone must include 79 or 80)
        bool includesCentre = (seg.s1LeftEnd >= zones::CENTER_LEFT) || (seg.s1RightStart <= zones::CENTER_RIGHT);
        if (i == 0 && !includesCentre) {
            return false;
        }
        
        // 5. Coverage Check - mark LEDs as used
        for (uint8_t led = seg.s1LeftStart; led <= seg.s1LeftEnd; led++) {
            if (coverage[led]) {
                return false;  // Overlap
            }
            coverage[led] = true;
        }
        for (uint8_t led = seg.s1RightStart; led <= seg.s1RightEnd; led++) {
            if (coverage[led]) {
                return false;  // Overlap
            }
            coverage[led] = true;
        }
    }
    
    // 6. Complete Coverage Check - verify all LEDs 0-159 are covered
    for (uint8_t led = 0; led <= zones::MAX_LED; led++) {
        if (!coverage[led]) {
            return false;
        }
    }
    
    // 7. Ordering Check - zones must be ordered centre-outward
    for (uint8_t i = 0; i < count - 1; i++) {
        const zones::ZoneSegment& seg1 = segments[i];      // Inner zone
        const zones::ZoneSegment& seg2 = segments[i + 1];  // Outer zone
        
        // Left segments: inner zone should end at higher number than outer zone starts
        if (seg1.s1LeftEnd <= seg2.s1LeftStart) {
            return false;
        }
        
        // Right segments: inner zone should start at lower number than outer zone starts
        if (seg1.s1RightStart >= seg2.s1RightStart) {
            return false;
        }
    }
    
    return true;
}
