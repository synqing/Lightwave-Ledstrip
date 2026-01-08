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

void ZoneComposerUI::begin(lv_obj_t* parent) {
    markDirty();
    _lastRenderTime = 0;

    // Initialize editing segments with default 3-zone layout
    generateZoneSegments(3);

    // Validate presets at boot (back-test against v2 firmware expectations)
    validatePresets();

    // Phase 1: Initialize LVGL styles
    initStyles();

    // Phase 2: Create LVGL widgets if parent provided
    if (parent) {
        createInteractiveUI(parent);
        Serial.println("[ZoneComposer] LVGL interactive UI created");
    }

    Serial.println("[ZoneComposer] Interactive UI initialized");
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

// ============================================================================
// Phase 1: State Management Implementation
// ============================================================================

void ZoneComposerUI::initStyles() {
    // Selected parameter style (blue accent border + bg tint)
    lv_style_init(&_styleSelected);
    lv_style_set_border_color(&_styleSelected, lv_color_hex(Theme::ACCENT));
    lv_style_set_border_width(&_styleSelected, 3);
    lv_style_set_bg_color(&_styleSelected, lv_color_hex(0x1A237E));  // Dark blue tint
    lv_style_set_bg_opa(&_styleSelected, LV_OPA_30);

    // Highlighted parameter style (lighter border)
    lv_style_init(&_styleHighlighted);
    lv_style_set_border_color(&_styleHighlighted, lv_color_hex(0x64B5F6));  // Light blue
    lv_style_set_border_width(&_styleHighlighted, 2);
    lv_style_set_bg_opa(&_styleHighlighted, LV_OPA_20);

    // Normal style
    lv_style_init(&_styleNormal);
    lv_style_set_border_width(&_styleNormal, 1);
    lv_style_set_border_color(&_styleNormal, lv_color_hex(0x424242));  // Dark gray
    lv_style_set_bg_opa(&_styleNormal, LV_OPA_TRANSP);

    Serial.println("[ZoneComposer] LVGL styles initialized");
}

void ZoneComposerUI::selectParameter(uint8_t zoneIndex, ZoneParameterMode mode) {
    if (zoneIndex >= 4) return;

    ZoneSelection newSelection;
    newSelection.type = SelectionType::ZONE_PARAMETER;
    newSelection.zoneIndex = zoneIndex;
    newSelection.mode = mode;

    // Toggle if same parameter clicked again
    if (_currentSelection == newSelection) {
        clearSelection();
        Serial.printf("[ZoneComposer] Deselected Zone %u %s\n",
                     zoneIndex, (int)mode == 0 ? "Effect" :
                                (int)mode == 1 ? "Palette" :
                                (int)mode == 2 ? "Speed" : "Brightness");
        return;
    }

    // Update selection
    clearSelection();  // Remove old highlighting
    _currentSelection = newSelection;
    applySelectionHighlight();

    Serial.printf("[ZoneComposer] Selected Zone %u %s (Mode: %d)\n",
                 zoneIndex, (int)mode == 0 ? "Effect" :
                           (int)mode == 1 ? "Palette" :
                           (int)mode == 2 ? "Speed" : "Brightness",
                 (int)mode);
}

void ZoneComposerUI::selectZoneCount() {
    clearSelection();
    _currentSelection.type = SelectionType::ZONE_COUNT;
    _currentSelection.zoneIndex = 0;  // Encoder 0 controls zone count

    applySelectionHighlight();
    Serial.println("[ZoneComposer] Selected Zone Count (Encoder 0)");
}

void ZoneComposerUI::selectPreset() {
    clearSelection();
    _currentSelection.type = SelectionType::PRESET;
    _currentSelection.zoneIndex = 0;  // Encoder 0 controls preset (updated from plan)

    applySelectionHighlight();
    Serial.println("[ZoneComposer] Selected Preset (Encoder 0)");
}

void ZoneComposerUI::setActiveMode(ZoneParameterMode mode) {
    _activeMode = mode;

    // Update mode button highlighting (when widgets are created in Phase 3)
    for (int i = 0; i < 4; i++) {
        if (_modeButtons[i]) {
            if (i == (int)mode) {
                lv_obj_add_style(_modeButtons[i], &_styleSelected, 0);
            } else {
                lv_obj_remove_style(_modeButtons[i], &_styleSelected, 0);
            }
        }
    }

    // If a zone parameter is selected, switch its mode
    if (_currentSelection.type == SelectionType::ZONE_PARAMETER) {
        _currentSelection.mode = mode;
        applySelectionHighlight();
    }

    Serial.printf("[ZoneComposer] Active mode changed to: %s\n",
                 (int)mode == 0 ? "Effect" :
                 (int)mode == 1 ? "Palette" :
                 (int)mode == 2 ? "Speed" : "Brightness");
}

void ZoneComposerUI::clearSelection() {
    // Remove all highlighting from zone parameter widgets
    for (int i = 0; i < 4; i++) {
        if (_zoneEffectLabels[i]) {
            lv_obj_remove_style(_zoneEffectLabels[i], &_styleSelected, 0);
        }
        if (_zonePaletteLabels[i]) {
            lv_obj_remove_style(_zonePaletteLabels[i], &_styleSelected, 0);
        }
        if (_zoneSpeedLabels[i]) {
            lv_obj_remove_style(_zoneSpeedLabels[i], &_styleSelected, 0);
        }
        if (_zoneBrightnessLabels[i]) {
            lv_obj_remove_style(_zoneBrightnessLabels[i], &_styleSelected, 0);
        }
    }

    // Remove highlighting from zone count and preset rows
    if (_zoneCountRow) {
        lv_obj_remove_style(_zoneCountRow, &_styleSelected, 0);
    }
    if (_presetRow) {
        lv_obj_remove_style(_presetRow, &_styleSelected, 0);
    }

    _currentSelection.type = SelectionType::NONE;
}

void ZoneComposerUI::applySelectionHighlight() {
    switch (_currentSelection.type) {
        case SelectionType::ZONE_PARAMETER: {
            uint8_t zone = _currentSelection.zoneIndex;
            lv_obj_t* target = getParameterWidget(zone, _currentSelection.mode);

            if (target) {
                lv_obj_add_style(target, &_styleSelected, 0);
            }
            break;
        }

        case SelectionType::ZONE_COUNT:
            if (_zoneCountRow) {
                lv_obj_add_style(_zoneCountRow, &_styleSelected, 0);
            }
            break;

        case SelectionType::PRESET:
            if (_presetRow) {
                lv_obj_add_style(_presetRow, &_styleSelected, 0);
            }
            break;

        case SelectionType::NONE:
            break;
    }
}

lv_obj_t* ZoneComposerUI::getParameterWidget(uint8_t zoneIndex, ZoneParameterMode mode) {
    if (zoneIndex >= 4) return nullptr;

    switch (mode) {
        case ZoneParameterMode::EFFECT:
            return _zoneEffectLabels[zoneIndex];
        case ZoneParameterMode::PALETTE:
            return _zonePaletteLabels[zoneIndex];
        case ZoneParameterMode::SPEED:
            return _zoneSpeedLabels[zoneIndex];
        case ZoneParameterMode::BRIGHTNESS:
            return _zoneBrightnessLabels[zoneIndex];
        default:
            return nullptr;
    }
}

void ZoneComposerUI::handleEncoderChange(uint8_t encoderIndex, int32_t delta) {
    // Route based on current selection
    switch (_currentSelection.type) {
        case SelectionType::ZONE_PARAMETER:
            // Encoder N adjusts Zone N parameter
            if (encoderIndex == _currentSelection.zoneIndex) {
                adjustZoneParameter(encoderIndex, delta);
            } else if (encoderIndex < 4) {
                // Fallback: encoder N always controls zone N
                adjustZoneParameter(encoderIndex, delta);
            }
            break;

        case SelectionType::ZONE_COUNT:
            if (encoderIndex == 0) {
                adjustZoneCount(delta);
            }
            break;

        case SelectionType::PRESET:
            if (encoderIndex == 0) {
                adjustPreset(delta);
            }
            break;

        case SelectionType::NONE:
            // Fallback: Encoder N adjusts Zone N with current mode
            if (encoderIndex < 4) {
                adjustZoneParameter(encoderIndex, delta);
            }
            break;
    }
}

void ZoneComposerUI::adjustZoneParameter(uint8_t zoneIndex, int32_t delta) {
    if (zoneIndex >= 4) return;

    switch (_activeMode) {
        case ZoneParameterMode::EFFECT: {
            // TODO: Get max effect ID from effect list
            constexpr int MAX_EFFECT_ID = 75;  // Placeholder
            int newVal = _zoneEffects[zoneIndex] + delta;
            if (newVal < 0) newVal = MAX_EFFECT_ID;
            if (newVal > MAX_EFFECT_ID) newVal = 0;
            _zoneEffects[zoneIndex] = newVal;

            updateEffectLabel(zoneIndex);

            // Send WebSocket command to v2 firmware
            if (_wsClient && _wsClient->isConnected()) {
                _wsClient->sendZoneEffect(zoneIndex, _zoneEffects[zoneIndex]);
            }

            Serial.printf("[ZoneComposer] Zone %u Effect → %u\n", zoneIndex, _zoneEffects[zoneIndex]);
            break;
        }

        case ZoneParameterMode::PALETTE: {
            // TODO: Get max palette ID from palette list
            constexpr int MAX_PALETTE_ID = 23;  // Placeholder
            int newVal = _zonePalettes[zoneIndex] + delta;
            if (newVal < 0) newVal = MAX_PALETTE_ID;
            if (newVal > MAX_PALETTE_ID) newVal = 0;
            _zonePalettes[zoneIndex] = newVal;

            updatePaletteLabel(zoneIndex);

            // Send WebSocket command to v2 firmware
            if (_wsClient && _wsClient->isConnected()) {
                _wsClient->sendZonePalette(zoneIndex, _zonePalettes[zoneIndex]);
            }

            Serial.printf("[ZoneComposer] Zone %u Palette → %u\n", zoneIndex, _zonePalettes[zoneIndex]);
            break;
        }

        case ZoneParameterMode::SPEED: {
            int newVal = _zoneSpeeds[zoneIndex] + delta;
            if (newVal < 1) newVal = 1;
            if (newVal > 50) newVal = 50;
            _zoneSpeeds[zoneIndex] = newVal;

            updateSpeedLabel(zoneIndex);

            // Send WebSocket command to v2 firmware
            if (_wsClient && _wsClient->isConnected()) {
                _wsClient->sendZoneSpeed(zoneIndex, _zoneSpeeds[zoneIndex]);
            }

            Serial.printf("[ZoneComposer] Zone %u Speed → %u\n", zoneIndex, _zoneSpeeds[zoneIndex]);
            break;
        }

        case ZoneParameterMode::BRIGHTNESS: {
            int newVal = _zoneBrightness[zoneIndex] + (delta * 5);  // Faster adjustment
            if (newVal < 0) newVal = 0;
            if (newVal > 255) newVal = 255;
            _zoneBrightness[zoneIndex] = newVal;

            updateBrightnessLabel(zoneIndex);

            // Send WebSocket command to v2 firmware
            if (_wsClient && _wsClient->isConnected()) {
                _wsClient->sendZoneBrightness(zoneIndex, _zoneBrightness[zoneIndex]);
            }

            Serial.printf("[ZoneComposer] Zone %u Brightness → %u\n", zoneIndex, _zoneBrightness[zoneIndex]);
            break;
        }

        default:
            break;
    }
}

void ZoneComposerUI::adjustZoneCount(int32_t delta) {
    int newCount = _zoneCount + delta;
    if (newCount < 1) newCount = 4;  // Wrap to 4
    if (newCount > 4) newCount = 1;  // Wrap to 1
    _zoneCount = newCount;

    // Generate new zone layout
    generateZoneSegments(_zoneCount);

    // Send WebSocket command to v2 firmware
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZonesSetLayout(_editingSegments, _editingZoneCount);
    }

    updateZoneCountLabel();
    Serial.printf("[ZoneComposer] Zone Count → %u\n", _zoneCount);
}

void ZoneComposerUI::adjustPreset(int32_t delta) {
    const char* presets[] = {"Unified", "Dual Split", "Triple Rings", "Quad Active", "Heartbeat Focus"};
    const int presetCount = 5;

    int newIndex = _currentPresetIndex + delta;
    if (newIndex < 0) newIndex = presetCount - 1;  // Wrap to end
    if (newIndex >= presetCount) newIndex = 0;     // Wrap to start

    _currentPresetIndex = newIndex;
    _presetName = presets[_currentPresetIndex];

    // Load the preset zone layout
    loadPreset(_currentPresetIndex);

    // Send WebSocket command to v2 firmware
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZonesSetLayout(_editingSegments, _editingZoneCount);
    }

    updatePresetLabel();
    Serial.printf("[ZoneComposer] Preset → %s (%u zones)\n", _presetName, _editingZoneCount);
}

// Label update helpers (Phase 1 stubs - will be fully implemented in Phase 2)
void ZoneComposerUI::updateEffectLabel(uint8_t zoneIndex) {
    if (zoneIndex >= 4 || !_zoneEffectLabels[zoneIndex]) return;
    // TODO: Update LVGL label text in Phase 2
    Serial.printf("[ZoneComposer] updateEffectLabel(%u) - stub\n", zoneIndex);
}

void ZoneComposerUI::updatePaletteLabel(uint8_t zoneIndex) {
    if (zoneIndex >= 4 || !_zonePaletteLabels[zoneIndex]) return;
    // TODO: Update LVGL label text in Phase 2
    Serial.printf("[ZoneComposer] updatePaletteLabel(%u) - stub\n", zoneIndex);
}

void ZoneComposerUI::updateSpeedLabel(uint8_t zoneIndex) {
    if (zoneIndex >= 4 || !_zoneSpeedLabels[zoneIndex]) return;
    // TODO: Update LVGL label text in Phase 2
    Serial.printf("[ZoneComposer] updateSpeedLabel(%u) - stub\n", zoneIndex);
}

void ZoneComposerUI::updateBrightnessLabel(uint8_t zoneIndex) {
    if (zoneIndex >= 4 || !_zoneBrightnessLabels[zoneIndex]) return;
    // TODO: Update LVGL label text in Phase 2
    Serial.printf("[ZoneComposer] updateBrightnessLabel(%u) - stub\n", zoneIndex);
}

void ZoneComposerUI::updateZoneCountLabel() {
    if (!_zoneCountValueLabel) return;
    // TODO: Update LVGL label text in Phase 2
    Serial.printf("[ZoneComposer] updateZoneCountLabel() - stub\n");
}

void ZoneComposerUI::updatePresetLabel() {
    if (!_presetValueLabel) return;
    // TODO: Update LVGL label text in Phase 2
    Serial.printf("[ZoneComposer] updatePresetLabel() - stub\n");
}

// ============================================================================
// Phase 2: Widget Creation Implementation
// ============================================================================

// Theme constants matching DisplayUI.cpp
static constexpr uint32_t TAB5_COLOR_BG_PAGE = 0x0A0A0B;
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_BASE = 0x121214;
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_ELEVATED = 0x1A1A1C;
static constexpr uint32_t TAB5_COLOR_BORDER_BASE = 0x2A2A2E;
static constexpr uint32_t TAB5_COLOR_FG_PRIMARY = 0xFFFFFF;
static constexpr uint32_t TAB5_COLOR_FG_SECONDARY = 0x9CA3AF;
static constexpr uint32_t TAB5_COLOR_BRAND_PRIMARY = 0xFFC700;

// Helper function matching DisplayUI's make_card()
static lv_obj_t* make_zone_card(lv_obj_t* parent, bool elevated) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card,
                              lv_color_hex(elevated ? TAB5_COLOR_BG_SURFACE_ELEVATED
                                                    : TAB5_COLOR_BG_SURFACE_BASE),
                              LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void ZoneComposerUI::createInteractiveUI(lv_obj_t* parent) {
    Serial.println("[ZoneComposer] Creating LVGL Zone Composer UI");

    // Note: This creates the INTERACTIVE zone parameter controls (touch + encoder).
    // The LED visualization is still rendered via M5GFX in render().

    // Main container (not needed, parent is already the zone screen)
    // Just create components directly on parent

    // Zone count selector row
    _zoneCountRow = createZoneCountRow(parent);

    // Preset selector row
    _presetRow = createPresetRow(parent);

    // Zone parameter grid (4 zones × 4 parameters each)
    createZoneParameterGrid(parent);

    // Mode selector + back button
    createModeSelector(parent);

    Serial.println("[ZoneComposer] LVGL interactive UI created");
}

lv_obj_t* ZoneComposerUI::createZoneCountRow(lv_obj_t* parent) {
    // Create simple label (no complex card needed)
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, "Zone Count: 1");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label, zoneCountTouchCb, LV_EVENT_CLICKED, this);
    _zoneCountValueLabel = label;
    return label;
}

lv_obj_t* ZoneComposerUI::createPresetRow(lv_obj_t* parent) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, "Preset: Unified");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label, presetTouchCb, LV_EVENT_CLICKED, this);
    _presetValueLabel = label;
    return label;
}

void ZoneComposerUI::createZoneParameterGrid(lv_obj_t* parent) {
    // Create 4 zone parameter rows
    for (uint8_t i = 0; i < 4; i++) {
        createZoneParamRow(parent, i);
    }
}

lv_obj_t* ZoneComposerUI::createZoneParamRow(lv_obj_t* parent, uint8_t zoneIndex) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 50);

    // Zone label
    lv_obj_t* zoneLabel = lv_label_create(row);
    char zoneName[16];
    snprintf(zoneName, sizeof(zoneName), "Zone %d", zoneIndex + 1);
    lv_label_set_text(zoneLabel, zoneName);
    lv_obj_set_style_text_color(zoneLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);

    // Effect parameter
    lv_obj_t* effectCard = createClickableParameter(row, "Effect", "Fire", zoneIndex, ZoneParameterMode::EFFECT);
    _zoneEffectLabels[zoneIndex] = effectCard;

    // Palette parameter
    lv_obj_t* paletteCard = createClickableParameter(row, "Palette", "Rainbow", zoneIndex, ZoneParameterMode::PALETTE);
    _zonePaletteLabels[zoneIndex] = paletteCard;

    // Speed parameter
    lv_obj_t* speedCard = createClickableParameter(row, "Speed", "50", zoneIndex, ZoneParameterMode::SPEED);
    _zoneSpeedLabels[zoneIndex] = speedCard;

    // Brightness parameter
    lv_obj_t* brightCard = createClickableParameter(row, "Brightness", "128", zoneIndex, ZoneParameterMode::BRIGHTNESS);
    _zoneBrightnessLabels[zoneIndex] = brightCard;

    return row;
}

lv_obj_t* ZoneComposerUI::createClickableParameter(lv_obj_t* parent,
                                                    const char* label,
                                                    const char* value,
                                                    uint8_t zoneIndex,
                                                    ZoneParameterMode mode) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 100, 40);
    lv_obj_set_style_bg_color(card, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);

    lv_obj_t* valueLabel = lv_label_create(card);
    lv_label_set_text(valueLabel, value);
    lv_obj_set_style_text_color(valueLabel, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_center(valueLabel);

    ParameterMetadata* meta = new ParameterMetadata();
    meta->zoneIndex = zoneIndex;
    meta->mode = mode;
    lv_obj_set_user_data(card, meta);

    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, parameterTouchCb, LV_EVENT_CLICKED, this);

    return card;
}

void ZoneComposerUI::createModeSelector(lv_obj_t* parent) {
    const char* modeNames[] = {"Effect", "Palette", "Speed", "Brightness"};

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_set_size(btn, 80, 40);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, modeNames[i]);
        lv_obj_center(label);

        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, modeButtonCb, LV_EVENT_CLICKED, this);

        _modeButtons[i] = btn;
    }
}

lv_obj_t* ZoneComposerUI::createBackButton(lv_obj_t* parent) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, LV_PCT(100), 50);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);

    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, backButtonCb, LV_EVENT_CLICKED, this);

    return btn;
}

// ============================================================================
// Phase 2: LVGL Event Callbacks
// ============================================================================

void ZoneComposerUI::parameterTouchCb(lv_event_t* e) {
    ZoneComposerUI* ui = (ZoneComposerUI*)lv_event_get_user_data(e);
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    ParameterMetadata* meta = (ParameterMetadata*)lv_obj_get_user_data(target);
    if (!meta) return;
    ui->selectParameter(meta->zoneIndex, meta->mode);
}

void ZoneComposerUI::zoneCountTouchCb(lv_event_t* e) {
    ZoneComposerUI* ui = (ZoneComposerUI*)lv_event_get_user_data(e);
    ui->selectZoneCount();
}

void ZoneComposerUI::presetTouchCb(lv_event_t* e) {
    ZoneComposerUI* ui = (ZoneComposerUI*)lv_event_get_user_data(e);
    ui->selectPreset();
}

void ZoneComposerUI::modeButtonCb(lv_event_t* e) {
    ZoneComposerUI* ui = (ZoneComposerUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int modeIndex = (int)(intptr_t)lv_obj_get_user_data(btn);
    ui->setActiveMode((ZoneParameterMode)modeIndex);
}

void ZoneComposerUI::backButtonCb(lv_event_t* e) {
    ZoneComposerUI* ui = (ZoneComposerUI*)lv_event_get_user_data(e);
    Serial.println("[ZoneComposer] Back button pressed - returning to GLOBAL screen");
    if (ui->_backButtonCallback) {
        ui->_backButtonCallback();
    }
}
