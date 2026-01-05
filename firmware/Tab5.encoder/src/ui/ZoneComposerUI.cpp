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
        // Promote pending dirty to dirty (enables re-entry redraw)
        if (_pendingDirty) {
            _dirty = true;
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

    // Zone info display (read-only)
    drawZoneInfo(40, CONTROLS_Y, 1200, 180);

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

    // Prefer actual zone count from server, fallback to editing count
    uint8_t displayCount = _zoneCount > 0 ? _zoneCount : _editingZoneCount;
    
    // Guard: nothing to display
    if (displayCount == 0) {
        _display.setTextColor(Theme::TEXT_DIM);
        _display.setTextDatum(textdatum_t::top_left);
        _display.drawString("Waiting for zone data...", x, y);
        return;
    }
    
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
    _display.drawRect(x, y, w, h, zoneColor565);

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

    // Zone info (read-only display)
    int infoX = x + 300;
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::middle_left);
    
    // Effect name
    const char* effectText = (zone.effectName[0] != '\0') ? zone.effectName : lookupEffectName(zone.effectId);
    char effectBuf[48];
    if (!effectText) {
        snprintf(effectBuf, sizeof(effectBuf), "Effect #%u", zone.effectId);
        effectText = effectBuf;
    }
    _display.drawString(effectText, infoX, y + h / 2);
    
    // Palette name
    int paletteX = infoX + 200;
    const char* paletteText = (zone.paletteName[0] != '\0') ? zone.paletteName : lookupPaletteName(zone.paletteId);
    char paletteBuf[48];
    if (!paletteText) {
        snprintf(paletteBuf, sizeof(paletteBuf), "Palette #%u", zone.paletteId);
        paletteText = paletteBuf;
    }
    _display.drawString(paletteText, paletteX, y + h / 2);
    
    // Blend mode
    int blendX = paletteX + 200;
    const char* blendText = (zone.blendModeName[0] != '\0') ? zone.blendModeName : nullptr;
    char blendBuf[32];
    if (!blendText) {
        snprintf(blendBuf, sizeof(blendBuf), "Blend #%u", zone.blendMode);
        blendText = blendBuf;
    }
    _display.drawString(blendText, blendX, y + h / 2);
}

void ZoneComposerUI::drawZoneInfo(int x, int y, int w, int h) {
    // Zone count display
    _display.setFont(&fonts::Font2);
    _display.setTextSize(1);
    _display.setTextColor(Theme::TEXT_DIM);
    _display.setTextDatum(textdatum_t::top_left);
    _display.drawString("Zones:", x, y);
    
    char countStr[16];
    snprintf(countStr, sizeof(countStr), "%u", _zoneCount);
    _display.setTextColor(Theme::TEXT_BRIGHT);
    _display.drawString(countStr, x + 80, y);
    
    // Zone layout info
    _display.setTextColor(Theme::TEXT_DIM);
    _display.drawString("Layout: Centre-out", x + 200, y);
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



void ZoneComposerUI::handleTouch(int16_t x, int16_t y) {
    // Display-only: no touch interaction
    (void)x;
    (void)y;
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
        markDirty();
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
