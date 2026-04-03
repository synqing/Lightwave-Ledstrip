/**
 * ZMDisplay — Colour-coded tabbed display for AtomS3 0.85" LCD (128x128).
 *
 * Design 15: Four colour-coded pages with accent bars, navigation dots,
 * and full-screen parameter overlays. Uses BitmapFont for pixel-level
 * text rendering via M5GFX fillRect/drawPixel calls.
 *
 * Pages:
 *   1. Connection (Green)      — WiFi, WebSocket, IP, RSSI
 *   2. Zone Overview (Cyan)    — zone brightness bars + master + LED count
 *   3. Effect & Speed (Amber)  — effect name, speed, zone count
 *   4. EdgeMixer (Violet)      — mode, spread, strength, spatial/temporal
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <M5Unified.h>
#include <cstring>
#include <cstdio>
#include "BitmapFont.h"

namespace zmdisp {
    static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}  // namespace zmdisp

class ZMDisplay {
public:
    // -----------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------

    /// Initialise display. Call after M5.begin().
    void begin() {
        M5.Display.setRotation(0);
        M5.Display.fillScreen(_bgColour);
        _currentPage = 0;
        _dirty = true;
        _overlayActive = false;
        _lastRender = 0;
    }

    /// Called from loop(). Handles page rendering and overlay timeout.
    void update(uint32_t now) {
        // Overlay timeout check
        if (_overlayActive && (now - _overlayStart >= OVERLAY_DURATION_MS)) {
            _overlayActive = false;
            _dirty = true;  // Force page redraw after overlay clears
        }

        // Rate limit: 5Hz max (200ms between redraws)
        if (!_dirty || (now - _lastRender < MIN_RENDER_INTERVAL_MS)) return;
        _dirty = false;
        _lastRender = now;

        if (_overlayActive) {
            renderOverlay();
        } else {
            renderPage(_currentPage);
        }
    }

    /// Cycle to the next page (swipe left).
    void nextPage() {
        _currentPage = (_currentPage + 1) % PAGE_COUNT;
        _overlayActive = false;
        _dirty = true;
    }

    /// Cycle to the previous page (swipe right).
    void prevPage() {
        _currentPage = (_currentPage + PAGE_COUNT - 1) % PAGE_COUNT;
        _overlayActive = false;
        _dirty = true;
    }

    /// Return the active page index (0-based).
    uint8_t getCurrentPage() const { return _currentPage; }

    /// Trigger a full-screen parameter overlay.
    /// pageHint: 0=zone, 1=effect, 2=edgemixer, 3=connection
    void triggerOverlay(const char* paramName, uint16_t value, uint8_t pageHint) {
        if (paramName) {
            strncpy(_overlayParamName, paramName, sizeof(_overlayParamName) - 1);
            _overlayParamName[sizeof(_overlayParamName) - 1] = '\0';
        }
        _overlayValue = value;
        _overlayPageHint = (pageHint < PAGE_COUNT) ? pageHint : 0;
        _overlayStart = millis();
        _overlayActive = true;
        _dirty = true;
    }

    // -----------------------------------------------------------------
    // State setters (just update internal fields + dirty flag)
    // -----------------------------------------------------------------

    void setZoneBrightness(uint8_t zone, uint8_t brightness) {
        if (zone >= MAX_ZONES) return;
        if (_zoneBrightness[zone] == brightness) return;
        _zoneBrightness[zone] = brightness;
        _dirty = true;
    }

    void setMasterBrightness(uint8_t brightness) {
        if (_masterBrightness == brightness) return;
        _masterBrightness = brightness;
        _dirty = true;
    }

    void setSpeed(uint8_t speed) {
        if (_speed == speed) return;
        _speed = speed;
        _dirty = true;
    }

    void setEffectName(const char* name) {
        if (!name) return;
        if (strcmp(_effectName, name) == 0) return;
        strncpy(_effectName, name, sizeof(_effectName) - 1);
        _effectName[sizeof(_effectName) - 1] = '\0';
        _dirty = true;
    }

    void setZoneCount(uint8_t count) {
        if (_zoneCount == count) return;
        _zoneCount = count;
        _dirty = true;
    }

    void setZonesEnabled(bool enabled) {
        if (_zonesEnabled == enabled) return;
        _zonesEnabled = enabled;
        _dirty = true;
    }

    void setWifiState(bool connected, int8_t rssi, const char* ip) {
        bool changed = (_wifiConnected != connected) || (_rssi != rssi);
        if (ip && strcmp(_ipAddr, ip) != 0) changed = true;
        if (!changed) return;

        _wifiConnected = connected;
        _rssi = rssi;
        if (ip) {
            strncpy(_ipAddr, ip, sizeof(_ipAddr) - 1);
            _ipAddr[sizeof(_ipAddr) - 1] = '\0';
        }
        _dirty = true;
    }

    void setWsState(bool connected) {
        if (_wsConnected == connected) return;
        _wsConnected = connected;
        _dirty = true;
    }

    void setEdgeMixer(uint8_t mode, uint8_t spread, uint8_t strength,
                       bool spatial, bool temporal) {
        bool changed = (_emMode != mode) || (_emSpread != spread) ||
                       (_emStrength != strength) || (_emSpatial != spatial) ||
                       (_emTemporal != temporal);
        if (!changed) return;
        _emMode = mode;
        _emSpread = spread;
        _emStrength = strength;
        _emSpatial = spatial;
        _emTemporal = temporal;
        _dirty = true;
    }

private:
    // -----------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------

    static constexpr uint8_t  SCREEN_W = 128;
    static constexpr uint8_t  SCREEN_H = 128;
    static constexpr uint8_t  PAGE_COUNT = 4;
    static constexpr uint8_t  MAX_ZONES = 3;
    static constexpr uint8_t  ACCENT_BAR_H = 4;
    static constexpr uint8_t  NAV_DOT_H = 4;
    static constexpr uint16_t OVERLAY_DURATION_MS = 2000;
    static constexpr uint16_t MIN_RENDER_INTERVAL_MS = 200;

    // Colour palette (RGB565)
    static constexpr uint16_t COL_BG        = zmdisp::rgb565(10, 10, 10);
    static constexpr uint16_t COL_WHITE      = zmdisp::rgb565(255, 255, 255);
    static constexpr uint16_t COL_DIM_GREY   = zmdisp::rgb565(120, 120, 120);
    static constexpr uint16_t COL_DARK_GREY  = zmdisp::rgb565(42, 42, 42);
    static constexpr uint16_t COL_SEP        = zmdisp::rgb565(32, 32, 32);

    // Page accent colours
    static constexpr uint16_t COL_CYAN       = zmdisp::rgb565(0, 188, 212);
    static constexpr uint16_t COL_AMBER      = zmdisp::rgb565(255, 179, 0);
    static constexpr uint16_t COL_VIOLET     = zmdisp::rgb565(171, 71, 188);
    static constexpr uint16_t COL_GREEN      = zmdisp::rgb565(76, 175, 80);

    // Zone colours
    static constexpr uint16_t COL_Z1         = zmdisp::rgb565(255, 68, 68);
    static constexpr uint16_t COL_Z2         = zmdisp::rgb565(68, 255, 68);
    static constexpr uint16_t COL_Z3         = zmdisp::rgb565(68, 136, 255);

    // Status colours
    static constexpr uint16_t COL_RED_DOT    = zmdisp::rgb565(255, 60, 60);
    static constexpr uint16_t COL_GREEN_DOT  = zmdisp::rgb565(60, 255, 60);
    static constexpr uint16_t COL_AMBER_DOT  = zmdisp::rgb565(255, 200, 60);

    // Accent colour per page
    // Page order: Connection, Zones, Effect, EdgeMixer
    static constexpr uint16_t PAGE_ACCENT[PAGE_COUNT] = {
        COL_GREEN, COL_CYAN, COL_AMBER, COL_VIOLET
    };

    static constexpr uint16_t ZONE_COLOUR[MAX_ZONES] = {
        COL_Z1, COL_Z2, COL_Z3
    };

    // Page names for overlay — order matches renderPage switch
    static constexpr const char* PAGE_NAMES[PAGE_COUNT] = {
        "CONNECTION", "ZONES", "EFFECT", "EDGEMIXER"
    };

    // EdgeMixer mode names
    static constexpr const char* EM_MODE_NAMES[7] = {
        "OFF", "BLEND", "MORPH", "SPLASH", "MIRROR", "PULSE", "CASCADE"
    };

    // -----------------------------------------------------------------
    // State
    // -----------------------------------------------------------------

    uint8_t _currentPage = 0;
    bool    _dirty = true;
    uint32_t _lastRender = 0;

    // Zone state
    uint8_t _zoneBrightness[MAX_ZONES] = {};
    uint8_t _masterBrightness = 0;
    uint8_t _speed = 128;
    char    _effectName[24] = "---";
    uint8_t _zoneCount = 1;
    bool    _zonesEnabled = false;

    // Connection state
    bool    _wifiConnected = false;
    bool    _wsConnected = false;
    int8_t  _rssi = -100;
    char    _ipAddr[16] = "---";

    // EdgeMixer state
    uint8_t _emMode = 0;
    uint8_t _emSpread = 0;
    uint8_t _emStrength = 0;
    bool    _emSpatial = false;
    bool    _emTemporal = false;

    // Overlay state
    bool     _overlayActive = false;
    uint32_t _overlayStart = 0;
    char     _overlayParamName[24] = "";
    uint16_t _overlayValue = 0;
    uint8_t  _overlayPageHint = 0;

    // -----------------------------------------------------------------
    // Drawing primitives
    // -----------------------------------------------------------------

    /// Draw a single character from the 5x7 bitmap font.
    void drawChar(int16_t x, int16_t y, char ch, uint16_t colour) {
        const uint8_t* glyph = bmfont::getGlyph(ch);
        if (!glyph) return;
        for (uint8_t row = 0; row < bmfont::CHAR_H; row++) {
            for (uint8_t col = 0; col < bmfont::CHAR_W; col++) {
                if (glyph[row] & (1 << (bmfont::CHAR_W - 1 - col))) {
                    M5.Display.drawPixel(x + col, y + row, colour);
                }
            }
        }
    }

    /// Draw a large character from the 7x10 bitmap font (for overlay values).
    void drawLargeChar(int16_t x, int16_t y, char ch, uint16_t colour) {
        const uint8_t* glyph = bmfont::getLargeGlyph(ch);
        if (!glyph) return;
        for (uint8_t row = 0; row < bmfont::LARGE_H; row++) {
            for (uint8_t col = 0; col < bmfont::LARGE_W; col++) {
                if (glyph[row] & (1 << (bmfont::LARGE_W - 1 - col))) {
                    M5.Display.drawPixel(x + col, y + row, colour);
                }
            }
        }
    }

    /// Draw a null-terminated string with the 5x7 font.
    /// spacing = pixels between characters (includes char width).
    void drawString(int16_t x, int16_t y, const char* str, uint16_t colour,
                    uint8_t spacing = 6) {
        while (*str) {
            drawChar(x, y, *str, colour);
            x += spacing;
            str++;
        }
    }

    /// Draw a null-terminated string with the 7x10 large font.
    void drawLargeString(int16_t x, int16_t y, const char* str, uint16_t colour,
                         uint8_t spacing = 8) {
        while (*str) {
            drawLargeChar(x, y, *str, colour);
            x += spacing;
            str++;
        }
    }

    /// Measure string width in pixels (5x7 font).
    uint16_t stringWidth(const char* str, uint8_t spacing = 6) const {
        uint16_t len = strlen(str);
        return (len > 0) ? (len * spacing - (spacing - bmfont::CHAR_W)) : 0;
    }

    /// Measure string width in pixels (7x10 large font).
    uint16_t largeStringWidth(const char* str, uint8_t spacing = 8) const {
        uint16_t len = strlen(str);
        return (len > 0) ? (len * spacing - (spacing - bmfont::LARGE_W)) : 0;
    }

    /// Draw a string centred horizontally on screen.
    void drawCentred(int16_t y, const char* str, uint16_t colour, uint8_t spacing = 6) {
        int16_t x = (SCREEN_W - (int16_t)stringWidth(str, spacing)) / 2;
        if (x < 0) x = 0;
        drawString(x, y, str, colour, spacing);
    }

    /// Draw a large string centred horizontally on screen.
    void drawLargeCentred(int16_t y, const char* str, uint16_t colour, uint8_t spacing = 8) {
        int16_t x = (SCREEN_W - (int16_t)largeStringWidth(str, spacing)) / 2;
        if (x < 0) x = 0;
        drawLargeString(x, y, str, colour, spacing);
    }

    /// Draw a string right-aligned at (xRight, y).
    void drawRightAligned(int16_t xRight, int16_t y, const char* str,
                          uint16_t colour, uint8_t spacing = 6) {
        int16_t x = xRight - (int16_t)stringWidth(str, spacing);
        if (x < 0) x = 0;
        drawString(x, y, str, colour, spacing);
    }

    /// Draw a horizontal bar (filled portion + unfilled background).
    void drawBar(int16_t x, int16_t y, uint16_t w, uint16_t h,
                 uint8_t value, uint8_t maxVal, uint16_t fillColour) {
        uint16_t fillW = (maxVal > 0) ? ((uint32_t)value * w / maxVal) : 0;
        if (fillW > w) fillW = w;
        if (fillW > 0) {
            M5.Display.fillRect(x, y, fillW, h, fillColour);
        }
        if (fillW < w) {
            M5.Display.fillRect(x + fillW, y, w - fillW, h, COL_DARK_GREY);
        }
    }

    /// Draw a horizontal separator line.
    void drawSeparator(int16_t y) {
        M5.Display.fillRect(4, y, 120, 1, COL_SEP);
    }

    // -----------------------------------------------------------------
    // Common page elements
    // -----------------------------------------------------------------

    /// Draw the accent colour bar at the top of the page.
    void drawAccentBar(uint16_t colour) {
        M5.Display.fillRect(0, 0, SCREEN_W, ACCENT_BAR_H, colour);
    }

    /// Draw navigation dots at the bottom of the screen.
    void drawNavDots(uint8_t activePage) {
        // 4 dots, 6px wide, 4px tall, 8px spacing, centred
        constexpr uint8_t DOT_W = 6;
        constexpr uint8_t DOT_GAP = 8;
        constexpr uint8_t TOTAL_W = PAGE_COUNT * DOT_W + (PAGE_COUNT - 1) * (DOT_GAP - DOT_W);
        // TOTAL_W = 4*6 + 3*2 = 30
        int16_t startX = (SCREEN_W - TOTAL_W) / 2;
        int16_t y = SCREEN_H - NAV_DOT_H;

        // Clear bottom strip
        M5.Display.fillRect(0, y, SCREEN_W, NAV_DOT_H, COL_BG);

        for (uint8_t i = 0; i < PAGE_COUNT; i++) {
            uint16_t col = (i == activePage) ? PAGE_ACCENT[activePage] : COL_DARK_GREY;
            M5.Display.fillRect(startX + i * DOT_GAP, y, DOT_W, NAV_DOT_H, col);
        }
    }

    /// Clear the content area (between accent bar and nav dots).
    void clearContent() {
        M5.Display.fillRect(0, ACCENT_BAR_H, SCREEN_W,
                            SCREEN_H - ACCENT_BAR_H - NAV_DOT_H, COL_BG);
    }

    // -----------------------------------------------------------------
    // Page rendering
    // -----------------------------------------------------------------

    void renderPage(uint8_t page) {
        clearContent();
        drawAccentBar(PAGE_ACCENT[page]);
        drawNavDots(page);

        switch (page) {
            case 0: renderConnectionPage(); break;
            case 1: renderZonePage(); break;
            case 2: renderEffectPage(); break;
            case 3: renderEdgeMixerPage(); break;
        }
    }

    // ---- Page 1: Zone Overview (Cyan) ----
    void renderZonePage() {
        // Header: zone label left, prominent ON/OFF state right
        drawString(4, 8, "ZONES", COL_CYAN);

        if (_zonesEnabled) {
            // Bright green zone count + ON indicator
            char hdr[16];
            snprintf(hdr, sizeof(hdr), "%dZ ON", _zoneCount);
            drawRightAligned(124, 8, hdr, COL_GREEN_DOT);
        } else {
            // Bright red OFF — unmissable inactive state
            drawRightAligned(124, 8, "OFF", COL_RED_DOT);
        }

        // Separator
        drawSeparator(17);

        // Zone rows — show LED count per zone
        // K1 divides 160 LEDs from centre outward (from ZoneDefinition.h):
        //   1Z: Z1=160  |  2Z: Z1=40 Z2=120  |  3Z: Z1=30 Z2=90 Z3=40
        static constexpr uint8_t LED_COUNTS[3][3] = {
            {160,   0,   0},   // 1 zone
            { 40, 120,   0},   // 2 zones
            { 30,  90,  40},   // 3 zones
        };
        uint8_t zci = (_zoneCount >= 1 && _zoneCount <= 3) ? _zoneCount - 1 : 0;

        for (uint8_t z = 0; z < MAX_ZONES; z++) {
            int16_t rowY = 20 + z * 24;
            uint16_t zCol = ZONE_COLOUR[z];
            bool active = (z < _zoneCount);

            // When zones are disabled, dim all bar colours to 25%
            uint16_t barCol  = (_zonesEnabled) ? zCol : dimColour565(zCol);
            uint16_t textCol = (_zonesEnabled) ? COL_WHITE : COL_DIM_GREY;

            // Zone label
            char zlabel[4];
            snprintf(zlabel, sizeof(zlabel), "Z%d", z + 1);
            drawString(4, rowY, zlabel, active ? barCol : COL_DARK_GREY);

            if (active) {
                // LED count
                char lcnt[8];
                snprintf(lcnt, sizeof(lcnt), "%dLED", LED_COUNTS[zci][z]);
                drawString(22, rowY, lcnt, COL_DIM_GREY);

                // Brightness percentage right-aligned
                uint8_t pct = (uint16_t)_zoneBrightness[z] * 100 / 255;
                char vbuf[8];
                snprintf(vbuf, sizeof(vbuf), "%d%%", pct);
                drawRightAligned(124, rowY, vbuf, textCol);

                // Brightness bar — dimmed when zones are off
                drawBar(4, rowY + 9, 120, 5, _zoneBrightness[z], 255, barCol);
            } else {
                drawString(22, rowY, "---", COL_DARK_GREY);
            }

            if (z < MAX_ZONES - 1) drawSeparator(rowY + 17);
        }

        // Master bar at bottom
        drawSeparator(93);
        drawString(4, 97, "MASTER", COL_DIM_GREY);
        char mastPct[8];
        snprintf(mastPct, sizeof(mastPct), "%d%%",
                 (uint16_t)_masterBrightness * 100 / 255);
        drawRightAligned(124, 97, mastPct, COL_WHITE);
        drawBar(4, 107, 120, 6, _masterBrightness, 255, COL_WHITE);

        // LED strip preview: 120px bar showing zone layout from centre out
        drawSeparator(115);
        int16_t stripX = 4;
        int16_t stripY = 118;
        int16_t stripW = 120;
        uint8_t stripH = 4;
        // Draw proportional zone segments (dimmed when zones disabled)
        if (_zoneCount >= 1) {
            // Centre-origin: Z1 in middle, Z2 flanks, Z3 edges
            uint8_t total = 160;
            for (uint8_t z = 0; z < _zoneCount; z++) {
                uint8_t leds = LED_COUNTS[zci][z];
                int16_t segW = (int16_t)leds * stripW / total;
                uint16_t sCol = _zonesEnabled ? ZONE_COLOUR[z]
                                              : dimColour565(ZONE_COLOUR[z]);
                if (z == 0) {
                    // Z1 = centre
                    int16_t cx = stripX + (stripW - segW) / 2;
                    M5.Display.fillRect(cx, stripY, segW, stripH, sCol);
                } else if (z == 1) {
                    // Z2 = flanks
                    int16_t halfW = segW / 2;
                    int16_t z1w = (int16_t)LED_COUNTS[zci][0] * stripW / total;
                    int16_t cx1 = stripX + (stripW - z1w) / 2;
                    M5.Display.fillRect(cx1 - halfW, stripY, halfW, stripH, sCol);
                    M5.Display.fillRect(cx1 + z1w, stripY, halfW, stripH, sCol);
                } else {
                    // Z3 = edges
                    int16_t halfW = segW / 2;
                    M5.Display.fillRect(stripX, stripY, halfW, stripH, sCol);
                    M5.Display.fillRect(stripX + stripW - halfW, stripY, halfW, stripH, sCol);
                }
            }
            // Centre marker
            M5.Display.fillRect(stripX + stripW / 2, stripY - 1, 1, stripH + 2, COL_WHITE);
        }
    }

    // ---- Page 2: Effect & Speed (Amber) ----
    void renderEffectPage() {
        // Header
        drawString(4, 8, "EFFECT", COL_DIM_GREY);

        // Effect name centred — word-wrap if needed
        drawEffectNameWrapped(24, COL_AMBER, 7);

        // Separator
        drawSeparator(42);

        // Speed section
        drawString(4, 46, "SPEED", COL_DIM_GREY);
        char spbuf[8];
        snprintf(spbuf, sizeof(spbuf), "%d", _speed);
        drawRightAligned(124, 46, spbuf, COL_WHITE);
        drawBar(4, 56, 120, 8, _speed, 255, COL_AMBER);

        // Separator
        drawSeparator(68);

        // Zone count section
        drawString(4, 72, "ZONES", COL_DIM_GREY);
        char zcbuf[8];
        snprintf(zcbuf, sizeof(zcbuf), "%d", _zoneCount);
        drawRightAligned(124, 72, zcbuf, COL_WHITE);

        // Colour chips for active zones
        for (uint8_t z = 0; z < _zoneCount && z < MAX_ZONES; z++) {
            M5.Display.fillRect(4 + z * 14, 82, 10, 6, ZONE_COLOUR[z]);
        }

        // Zones enabled indicator
        drawString(4, 92, _zonesEnabled ? "ENABLED" : "OFF",
                   _zonesEnabled ? COL_GREEN : COL_DIM_GREY);

        // Separator
        drawSeparator(102);

        // Brightness summary
        drawString(4, 106, "BRI", COL_DIM_GREY);
        char bribuf[8];
        snprintf(bribuf, sizeof(bribuf), "%d", _masterBrightness);
        drawRightAligned(124, 106, bribuf, COL_WHITE);
    }

    // ---- Page 3: EdgeMixer (Violet) ----
    void renderEdgeMixerPage() {
        // Header
        drawString(4, 8, "EDGEMIXER", COL_DIM_GREY);

        // Mode name centred
        uint8_t modeIdx = (_emMode < 7) ? _emMode : 0;
        drawCentred(20, EM_MODE_NAMES[modeIdx], COL_VIOLET);

        // Separator
        drawSeparator(30);

        // Spread
        drawString(4, 34, "SPREAD", COL_DIM_GREY);
        char sbuf[8];
        snprintf(sbuf, sizeof(sbuf), "%d", _emSpread);
        drawRightAligned(124, 34, sbuf, COL_WHITE);
        drawBar(4, 44, 120, 8, _emSpread, 255, COL_VIOLET);

        // Separator
        drawSeparator(56);

        // Strength
        drawString(4, 60, "STRENGTH", COL_DIM_GREY);
        char stbuf[8];
        snprintf(stbuf, sizeof(stbuf), "%d", _emStrength);
        drawRightAligned(124, 60, stbuf, COL_WHITE);
        drawBar(4, 70, 120, 8, _emStrength, 255, COL_VIOLET);

        // Separator
        drawSeparator(82);

        // Spatial/temporal indicators
        drawString(4, 86, "SPATIAL", _emSpatial ? COL_GREEN_DOT : COL_DIM_GREY);
        drawString(68, 86, _emSpatial ? "ON" : "OFF",
                   _emSpatial ? COL_GREEN_DOT : COL_DIM_GREY);

        drawString(4, 98, "TEMPORAL", _emTemporal ? COL_GREEN_DOT : COL_DIM_GREY);
        drawString(68, 98, _emTemporal ? "ON" : "OFF",
                   _emTemporal ? COL_GREEN_DOT : COL_DIM_GREY);
    }

    // ---- Page 4: Connection (Green) ----
    void renderConnectionPage() {
        // WiFi status
        drawString(4, 8, "WIFI", COL_DIM_GREY);
        // Status dot
        M5.Display.fillRect(40, 10, 4, 4,
                            _wifiConnected ? COL_GREEN_DOT : COL_RED_DOT);
        drawString(48, 8, _wifiConnected ? "CONNECTED" : "OFFLINE",
                   _wifiConnected ? COL_GREEN : COL_RED_DOT);

        // Separator
        drawSeparator(18);

        // WebSocket status
        drawString(4, 22, "WS", COL_DIM_GREY);
        M5.Display.fillRect(20, 24, 4, 4,
                            _wsConnected ? COL_GREEN_DOT : COL_AMBER_DOT);
        drawString(28, 22, _wsConnected ? "CONNECTED" : "WAITING",
                   _wsConnected ? COL_GREEN : COL_AMBER_DOT);

        // Separator
        drawSeparator(32);

        // IP address centred
        drawCentred(38, _ipAddr, COL_WHITE);

        // Separator
        drawSeparator(48);

        // Signal strength bars (4 graduated bars)
        drawString(4, 54, "SIGNAL", COL_DIM_GREY);
        drawSignalBars(80, 54);

        // RSSI quality label
        drawCentred(68, rssiQualityLabel(), rssiQualityColour());

        // RSSI numeric value
        char rssibuf[12];
        snprintf(rssibuf, sizeof(rssibuf), "%ddBm", _rssi);
        drawCentred(80, rssibuf, COL_DIM_GREY);
    }

    // -----------------------------------------------------------------
    // Overlay rendering
    // -----------------------------------------------------------------

    void renderOverlay() {
        // Full screen black
        M5.Display.fillScreen(COL_BG);

        // Accent bar from hinted page
        uint16_t accent = PAGE_ACCENT[_overlayPageHint];
        drawAccentBar(accent);

        // Parameter name centred at y=12
        drawCentred(12, _overlayParamName, COL_DIM_GREY);

        // Large value centred at y=30
        char valBuf[8];
        snprintf(valBuf, sizeof(valBuf), "%d", _overlayValue);
        drawLargeCentred(30, valBuf, COL_WHITE);

        // Progress bar at y=50 (accent-coloured, value/255)
        drawBar(4, 50, 120, 8, (_overlayValue > 255) ? 255 : (uint8_t)_overlayValue,
                255, accent);

        // Percentage text centred at y=64
        uint8_t pct = (uint16_t)((_overlayValue > 255) ? 255 : _overlayValue) * 100 / 255;
        char pctBuf[8];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
        drawCentred(64, pctBuf, COL_WHITE);

        // Current page name centred at y=82
        drawCentred(82, PAGE_NAMES[_overlayPageHint], COL_DIM_GREY);

        // Page dots at bottom
        drawNavDots(_overlayPageHint);
    }

    // -----------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------

    /// Draw effect name with word wrapping at y, max 2 lines.
    void drawEffectNameWrapped(int16_t y, uint16_t colour, uint8_t spacing) {
        const uint16_t maxLineW = SCREEN_W - 8;  // 4px margin each side

        // Try single line first
        if (stringWidth(_effectName, spacing) <= maxLineW) {
            drawCentred(y, _effectName, colour, spacing);
            return;
        }

        // Word-wrap: find break point
        char line1[24];
        char line2[24];
        strncpy(line1, _effectName, sizeof(line1) - 1);
        line1[sizeof(line1) - 1] = '\0';
        line2[0] = '\0';

        // Find last space that fits in line width
        int16_t breakIdx = -1;
        for (int16_t i = (int16_t)strlen(line1) - 1; i > 0; i--) {
            if (line1[i] == ' ') {
                line1[i] = '\0';
                if (stringWidth(line1, spacing) <= maxLineW) {
                    breakIdx = i;
                    break;
                }
                line1[i] = ' ';  // Restore and keep searching
            }
        }

        if (breakIdx > 0) {
            strncpy(line1, _effectName, breakIdx);
            line1[breakIdx] = '\0';
            strncpy(line2, _effectName + breakIdx + 1, sizeof(line2) - 1);
            line2[sizeof(line2) - 1] = '\0';
        } else {
            // No good break point — truncate
            strncpy(line1, _effectName, sizeof(line1) - 1);
            line1[sizeof(line1) - 1] = '\0';
        }

        drawCentred(y, line1, colour, spacing);
        if (line2[0]) {
            drawCentred(y + bmfont::CHAR_H + 2, line2, colour, spacing);
        }
    }

    /// Draw 4 graduated signal strength bars.
    void drawSignalBars(int16_t x, int16_t y) {
        // 4 bars, increasing height: 3, 5, 7, 9 pixels tall
        // RSSI thresholds: >-50 = 4 bars, >-65 = 3, >-75 = 2, >-85 = 1, else 0
        uint8_t bars = 0;
        if (_wifiConnected) {
            if      (_rssi > -50) bars = 4;
            else if (_rssi > -65) bars = 3;
            else if (_rssi > -75) bars = 2;
            else if (_rssi > -85) bars = 1;
        }

        constexpr uint8_t BAR_W = 5;
        constexpr uint8_t BAR_GAP = 3;
        constexpr uint8_t MAX_BAR_H = 10;

        for (uint8_t i = 0; i < 4; i++) {
            uint8_t barH = 3 + i * 2;  // 3, 5, 7, 9
            int16_t barX = x + i * (BAR_W + BAR_GAP);
            int16_t barY = y + (MAX_BAR_H - barH);
            uint16_t col = (i < bars) ? COL_GREEN : COL_DARK_GREY;
            M5.Display.fillRect(barX, barY, BAR_W, barH, col);
        }
    }

    /// Return a quality label for the current RSSI.
    const char* rssiQualityLabel() const {
        if (!_wifiConnected) return "NO SIGNAL";
        if (_rssi > -50) return "EXCELLENT";
        if (_rssi > -65) return "GOOD";
        if (_rssi > -75) return "FAIR";
        if (_rssi > -85) return "WEAK";
        return "VERY WEAK";
    }

    /// Return a colour for the current RSSI quality.
    uint16_t rssiQualityColour() const {
        if (!_wifiConnected) return COL_RED_DOT;
        if (_rssi > -65) return COL_GREEN;
        if (_rssi > -75) return COL_AMBER_DOT;
        return COL_RED_DOT;
    }

    /// Dim an RGB565 colour to roughly 25% brightness.
    static uint16_t dimColour565(uint16_t c) {
        uint8_t r5 = (c >> 11) & 0x1F;
        uint8_t g6 = (c >> 5)  & 0x3F;
        uint8_t b5 =  c        & 0x1F;
        return ((r5 >> 2) << 11) | ((g6 >> 2) << 5) | (b5 >> 2);
    }

    // Background colour (used only in begin)
    static constexpr uint16_t _bgColour = COL_BG;
};
