#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <M5GFX.h>
#include <cmath>
#include "../config/Config.h"

// ============================================================================
// Display Configuration (Tab5: 1280x720 native)
// ============================================================================

#define UI_WIDTH   1280
#define UI_HEIGHT  720
#define TOP_HEIGHT 360
#define BOT_HEIGHT 360

// ============================================================================
// Cyberpunk Palette (RGB565)
// ============================================================================

namespace UIColor {
    constexpr uint16_t BG_DARK     = 0x0000;  // #000000
    constexpr uint16_t BG_PANEL    = 0x0842;  // #080810
    constexpr uint16_t HEADER_ACC  = 0x07F1;  // #00ff88 (Neon Green)
    constexpr uint16_t TEXT_WHITE  = 0xFFFF;
    constexpr uint16_t TEXT_DIM    = 0x8C71;

    // Status colors
    constexpr uint16_t STATUS_OK   = 0x07E0;  // Green
    constexpr uint16_t STATUS_WARN = 0xFFE0;  // Yellow
    constexpr uint16_t STATUS_ERR  = 0xF800;  // Red
    constexpr uint16_t STATUS_CONN = 0x07FF;  // Cyan (connecting)
}

// Per-parameter colors (matches K1 styling)
// Unit A (0-7): Core parameters - bright neon palette
// Unit B (8-15): Zone parameters - color-coded per zone
static constexpr uint16_t PARAM_COLORS[16] = {
    // Unit A (0-7): Core parameters
    0xF810,  // 0: Effect     - hot pink    #ff0080
    0xFFE0,  // 1: Brightness - yellow      #ffff00
    0x07FF,  // 2: Palette    - cyan        #00ffff
    0xFA20,  // 3: Speed      - orange      #ff4400
    0xF81F,  // 4: Intensity  - magenta     #ff00ff
    0x07F1,  // 5: Saturation - green       #00ff88
    0x901F,  // 6: Complexity - purple      #8800ff
    0x047F,  // 7: Variation  - blue        #0088ff
    // Unit B (8-15): Zone parameters with distinct zone colors
    // Zone 0: Cyan variants (cool blue-green)
    0x07FF,  // 8:  Z0 Effect     - cyan        #00ffff
    0x05DF,  // 9:  Z0 Brightness - light cyan  #00bbff
    // Zone 1: Orange variants (warm)
    0xFD20,  // 10: Z1 Effect     - orange      #ff6600
    0xFBE0,  // 11: Z1 Brightness - gold        #ff9900
    // Zone 2: Green variants (nature)
    0x07E0,  // 12: Z2 Effect     - lime green  #00ff00
    0x47E0,  // 13: Z2 Brightness - spring      #44ff00
    // Zone 3: Purple/Violet variants (cool)
    0xA01F,  // 14: Z3 Effect     - purple      #9900ff
    0xC01F,  // 15: Z3 Brightness - violet      #cc00ff
};

// Parameter names from Config.h
static constexpr const char* PARAM_NAMES[16] = {
    ParamName::EFFECT, ParamName::BRIGHTNESS, ParamName::PALETTE, ParamName::SPEED,
    ParamName::INTENSITY, ParamName::SATURATION, ParamName::COMPLEXITY, ParamName::VARIATION,
    ParamName::ZONE0_EFFECT, ParamName::ZONE0_BRIGHTNESS,
    ParamName::ZONE1_EFFECT, ParamName::ZONE1_BRIGHTNESS,
    ParamName::ZONE2_EFFECT, ParamName::ZONE2_BRIGHTNESS,
    ParamName::ZONE3_EFFECT, ParamName::ZONE3_BRIGHTNESS
};

// ============================================================================
// Helpers
// ============================================================================

uint16_t dimColor(uint16_t color, float factor);
uint16_t lerpColor(uint16_t c1, uint16_t c2, float t);

// ============================================================================
// Widgets
// ============================================================================

// Connection state for status display
struct ConnectionStatus {
    bool wifiConnected = false;
    bool wsConnected = false;
    bool unitAOnline = false;
    bool unitBOnline = false;
};

class SystemMonitorWidget {
public:
    SystemMonitorWidget(M5GFX* gfx);
    ~SystemMonitorWidget();

    void init();
    void update(); // Animation loop
    void updateStats(uint32_t freeHeap, uint32_t freePsram, const char* uptime);
    void updateConnection(const ConnectionStatus& status);

private:
    M5GFX* _gfx;
    M5Canvas _sprite;

    // Waveform
    static const int WAVE_POINTS = 64; // Optimized for performance
    float _waveData[WAVE_POINTS];
    float _waveOffset;

    // Stats
    uint32_t _freeHeap = 0;
    uint32_t _freePsram = 0;
    char _uptime[16] = "00:00:00";

    // Connection status
    ConnectionStatus _connStatus;

    // State for optimization
    uint32_t _lastDraw = 0;

    void drawHeader(); // Drawn once
    void drawWaveform();
    void drawStats();
};

class EncoderWidget {
public:
    EncoderWidget(M5GFX* gfx, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t index);
    ~EncoderWidget();

    void setValue(int32_t value);
    void update(bool force = false);
    void push();

    bool isDirty() const { return _dirty; }

private:
    M5GFX* _gfx;
    M5Canvas _sprite;
    
    int32_t _x, _y, _w, _h;
    uint8_t _index;
    int32_t _value = 0;
    
    char _title[32];
    uint16_t _color;
    bool _dirty = true;

    void drawRadialGauge();
    void drawScanlines();
};

// ============================================================================
// Main UI Controller
// ============================================================================

class DisplayUI {
public:
    DisplayUI(M5GFX& display);
    ~DisplayUI();

    void begin();
    void update(uint8_t index, int32_t value);
    void loop();

    // Connection state update (called from main loop)
    void setConnectionState(bool wifiOk, bool wsOk, bool unitA, bool unitB);

    // Get widget for external access (e.g., touch handler)
    EncoderWidget* getWidget(uint8_t index) {
        return (index < 16) ? _widgets[index] : nullptr;
    }

private:
    M5GFX& _display;
    SystemMonitorWidget* _topMonitor;
    EncoderWidget* _widgets[16];
    uint32_t _lastStatUpdate = 0;
    ConnectionStatus _connStatus;

    void updateSystemStats();
};

#endif // DISPLAY_UI_H