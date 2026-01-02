// ============================================================================
// DisplayUI - Cyberpunk Dashboard for Tab5.encoder
// ============================================================================
// Sprite-based UI with animated system monitor and radial encoder gauges.
// Memory: ~1.8MB PSRAM for sprites (921KB top + 57KB×16 widgets)
// ============================================================================

#include "DisplayUI.h"
#include <esp_system.h>

// ============================================================================
// RGB565 Color Helpers
// ============================================================================

uint16_t dimColor(uint16_t color, float factor) {
    if (factor <= 0.0f) return 0;
    if (factor >= 1.0f) return color;
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    return ((uint8_t)(r * factor) << 11) | ((uint8_t)(g * factor) << 5) | (uint8_t)(b * factor);
}

uint16_t lerpColor(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;

    uint8_t r1 = (c1 >> 11) & 0x1F; uint8_t g1 = (c1 >> 5) & 0x3F; uint8_t b1 = c1 & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F; uint8_t g2 = (c2 >> 5) & 0x3F; uint8_t b2 = c2 & 0x1F;

    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;

    return (r << 11) | (g << 5) | b;
}

// ============================================================================
// System Monitor Widget
// ============================================================================

SystemMonitorWidget::SystemMonitorWidget(M5GFX* gfx)
    : _gfx(gfx), _sprite(gfx), _waveOffset(0) {

    // Explicitly use PSRAM for this large 921KB sprite
    _sprite.setPsram(true);
    _sprite.setColorDepth(16);
    _sprite.createSprite(UI_WIDTH, TOP_HEIGHT);

    for(int i=0; i<WAVE_POINTS; i++) _waveData[i] = 0;
}

SystemMonitorWidget::~SystemMonitorWidget() {
    _sprite.deleteSprite();
}

void SystemMonitorWidget::init() {
    _sprite.fillSprite(UIColor::BG_DARK);
    drawHeader();
    _sprite.pushSprite(0, 0);
}

void SystemMonitorWidget::drawHeader() {
    // Header background
    _sprite.fillRect(0, 0, UI_WIDTH, 40, UIColor::BG_PANEL);

    // Bottom border with glow
    _sprite.drawFastHLine(0, 39, UI_WIDTH, UIColor::HEADER_ACC);
    _sprite.drawFastHLine(0, 38, UI_WIDTH, dimColor(UIColor::HEADER_ACC, 0.4f));

    // Title
    _sprite.setTextDatum(textdatum_t::middle_left);
    _sprite.setFont(&fonts::Font4);

    // Shadow
    _sprite.setTextColor(dimColor(UIColor::HEADER_ACC, 0.3f));
    _sprite.drawString("LIGHTWAVEOS // TAB5 CONTROLLER", 22, 20);

    // Main Text
    _sprite.setTextColor(UIColor::HEADER_ACC);
    _sprite.drawString("LIGHTWAVEOS // TAB5 CONTROLLER", 20, 20);
}

void SystemMonitorWidget::updateStats(uint32_t freeHeap, uint32_t freePsram, const char* uptime) {
    _freeHeap = freeHeap;
    _freePsram = freePsram;
    strncpy(_uptime, uptime, 15);
}

void SystemMonitorWidget::updateConnection(const ConnectionStatus& status) {
    _connStatus = status;
}

void SystemMonitorWidget::update() {
    _sprite.startWrite();

    // Only clear the dynamic area below the header (y=40+)
    _sprite.fillRect(0, 40, UI_WIDTH, TOP_HEIGHT - 40, UIColor::BG_DARK);

    drawWaveform();
    drawStats();

    _sprite.endWrite();
    _sprite.pushSprite(0, 0);
}

void SystemMonitorWidget::drawWaveform() {
    _waveOffset += 0.15f;
    int cy = (TOP_HEIGHT / 2) + 20;
    float stepX = (float)UI_WIDTH / (float)(WAVE_POINTS - 1);

    // Calculate waveform physics
    for(int i=0; i<WAVE_POINTS; i++) {
        float x = i * 0.3f + _waveOffset;
        _waveData[i] = (sin(x) * 40.0f) + (sin(x * 2.5f) * 20.0f) + (cos(x * 5.7f) * 10.0f);
    }

    // Draw lines with gradient: Cyan (0x07FF) to Pink (0xF81F)
    for(int i=0; i<WAVE_POINTS-1; i++) {
        int x1 = (int)(i * stepX);
        int x2 = (int)((i+1) * stepX);
        int y1 = cy + (int)_waveData[i];
        int y2 = cy + (int)_waveData[i+1];

        float t = (float)i / (float)WAVE_POINTS;
        uint16_t col = lerpColor(0x07FF, 0xF81F, t);

        // Thick line with glow
        _sprite.drawLine(x1, y1, x2, y2, col);
        _sprite.drawLine(x1, y1+1, x2, y2+1, col);
        _sprite.drawFastVLine(x1, y1+2, 10, dimColor(col, 0.2f));
    }
}

void SystemMonitorWidget::drawStats() {
    int leftX = 30;
    int startY = 80;
    int lh = 35;

    _sprite.setFont(&fonts::Font2);
    _sprite.setTextDatum(textdatum_t::top_left);

    char buf[64];

    // Left side: Memory stats
    _sprite.setTextColor(UIColor::TEXT_DIM);
    _sprite.drawString("HEAP FREE:", leftX, startY);
    _sprite.setTextColor(UIColor::HEADER_ACC);
    sprintf(buf, "%u KB", _freeHeap / 1024);
    _sprite.drawString(buf, leftX + 110, startY);

    _sprite.setTextColor(UIColor::TEXT_DIM);
    _sprite.drawString("PSRAM FREE:", leftX, startY + lh);
    _sprite.setTextColor(UIColor::HEADER_ACC);
    sprintf(buf, "%.1f MB", _freePsram / (1024.0f * 1024.0f));
    _sprite.drawString(buf, leftX + 110, startY + lh);

    _sprite.setTextColor(UIColor::TEXT_DIM);
    _sprite.drawString("UPTIME:", leftX, startY + lh*2);
    _sprite.setTextColor(UIColor::HEADER_ACC);
    _sprite.drawString(_uptime, leftX + 110, startY + lh*2);

    // Right side: Connection status
    _sprite.setTextDatum(textdatum_t::top_right);
    int rightX = UI_WIDTH - 30;

    // WiFi status
    uint16_t wifiColor = _connStatus.wifiConnected ? UIColor::STATUS_OK : UIColor::STATUS_ERR;
    _sprite.setTextColor(wifiColor);
    sprintf(buf, "WIFI: [%s]", _connStatus.wifiConnected ? "CONNECTED" : "OFFLINE");
    _sprite.drawString(buf, rightX, startY);

    // WebSocket status
    uint16_t wsColor = _connStatus.wsConnected ? UIColor::STATUS_OK :
                       (_connStatus.wifiConnected ? UIColor::STATUS_CONN : UIColor::STATUS_ERR);
    _sprite.setTextColor(wsColor);
    sprintf(buf, "WS: [%s]", _connStatus.wsConnected ? "ACTIVE" :
                            (_connStatus.wifiConnected ? "CONNECTING" : "OFFLINE"));
    _sprite.drawString(buf, rightX, startY + lh);

    // I2C Unit status
    _sprite.setTextColor(_connStatus.unitAOnline ? UIColor::STATUS_OK : UIColor::STATUS_ERR);
    sprintf(buf, "UNIT 0x42: [%s]", _connStatus.unitAOnline ? "ONLINE" : "OFFLINE");
    _sprite.drawString(buf, rightX, startY + lh*2);

    _sprite.setTextColor(_connStatus.unitBOnline ? UIColor::STATUS_OK : UIColor::STATUS_ERR);
    sprintf(buf, "UNIT 0x41: [%s]", _connStatus.unitBOnline ? "ONLINE" : "OFFLINE");
    _sprite.drawString(buf, rightX, startY + lh*3);
}

// ============================================================================
// Encoder Widget
// ============================================================================

EncoderWidget::EncoderWidget(M5GFX* gfx, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t index)
    : _gfx(gfx), _sprite(gfx), _x(x), _y(y), _w(w), _h(h), _index(index) {

    _sprite.setColorDepth(16);
    _sprite.createSprite(_w, _h);

    strncpy(_title, PARAM_NAMES[index], 31);
    _color = PARAM_COLORS[index];
}

EncoderWidget::~EncoderWidget() {
    _sprite.deleteSprite();
}

void EncoderWidget::setValue(int32_t value) {
    if (_value != value) {
        _value = value;
        _dirty = true;
    }
}

void EncoderWidget::update(bool force) {
    if (!_dirty && !force) return;

    _sprite.startWrite();

    // Background
    _sprite.fillSprite(UIColor::BG_PANEL);

    // Dimmed border
    _sprite.drawRect(0, 0, _w, _h, dimColor(_color, 0.2f));

    // Radial gauge
    drawRadialGauge();

    // Title at bottom
    _sprite.setTextDatum(textdatum_t::bottom_center);
    _sprite.setFont(&fonts::Font0);

    // Text glow
    _sprite.setTextColor(dimColor(_color, 0.5f));
    _sprite.drawString(_title, _w/2 + 1, _h - 5 + 1);

    // Text main
    _sprite.setTextColor(_color);
    _sprite.drawString(_title, _w/2, _h - 5);

    // Scanlines overlay
    drawScanlines();

    _sprite.endWrite();
    _dirty = false;
}

void EncoderWidget::drawRadialGauge() {
    int cx = _w / 2;
    int cy = _h / 2 - 10;
    int r = 70;

    // Angles: 144 to 396 degrees
    int startAng = 144;
    int endAng = 396;

    // Background track
    _sprite.drawArc(cx, cy, r, r-6, startAng, endAng, dimColor(_color, 0.15f));

    // Active value
    int valClamped = (_value > 255) ? 255 : (_value < 0 ? 0 : _value);
    float pct = (float)valClamped / 255.0f;
    int currAng = startAng + (int)(pct * (endAng - startAng));

    // Glow pass
    _sprite.drawArc(cx, cy, r+2, r-8, startAng, currAng, dimColor(_color, 0.4f));

    // Core pass
    _sprite.drawArc(cx, cy, r, r-6, startAng, currAng, _color);

    // Large digit
    _sprite.setTextDatum(textdatum_t::middle_center);
    _sprite.setFont(&fonts::Font7);
    _sprite.setTextColor(UIColor::TEXT_WHITE);
    _sprite.drawNumber(valClamped, cx, cy);

    // Denominator
    _sprite.setFont(&fonts::Font0);
    _sprite.setTextColor(UIColor::TEXT_DIM);
    _sprite.drawString("/ 255", cx, cy + 28);
}

void EncoderWidget::drawScanlines() {
    // Very faint lines every 3rd pixel for CRT effect
    for(int i=0; i<_h; i+=3) {
        _sprite.drawFastHLine(0, i, _w, 0x0000);
    }
}

void EncoderWidget::push() {
    _sprite.pushSprite(_x, _y);
}

// ============================================================================
// DisplayUI Controller
// ============================================================================

DisplayUI::DisplayUI(M5GFX& display) : _display(display) {
    _topMonitor = new SystemMonitorWidget(&display);

    int cellW = UI_WIDTH / 8;   // 160
    int cellH = BOT_HEIGHT / 2; // 180

    for(int i=0; i<16; i++) {
        int r = i / 8;
        int c = i % 8;
        int x = c * cellW;
        int y = TOP_HEIGHT + (r * cellH);

        _widgets[i] = new EncoderWidget(&display, x, y, cellW, cellH, i);
    }
}

DisplayUI::~DisplayUI() {
    delete _topMonitor;
    for(int i=0; i<16; i++) {
        delete _widgets[i];
    }
}

void DisplayUI::begin() {
    _display.fillScreen(UIColor::BG_DARK);

    _topMonitor->init();

    for(int i=0; i<16; i++) {
        _widgets[i]->update(true);
        _widgets[i]->push();
    }
}

void DisplayUI::update(uint8_t index, int32_t value) {
    if (index >= 16) return;

    _widgets[index]->setValue(value);
    _widgets[index]->update();
    _widgets[index]->push();
}

void DisplayUI::setConnectionState(bool wifiOk, bool wsOk, bool unitA, bool unitB) {
    _connStatus.wifiConnected = wifiOk;
    _connStatus.wsConnected = wsOk;
    _connStatus.unitAOnline = unitA;
    _connStatus.unitBOnline = unitB;
    _topMonitor->updateConnection(_connStatus);
}

void DisplayUI::loop() {
    updateSystemStats();
    _topMonitor->update();
}

void DisplayUI::updateSystemStats() {
    // Throttle to 2Hz (every 500ms)
    if (millis() - _lastStatUpdate < 500) {
        return;
    }
    _lastStatUpdate = millis();

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t freePsram = ESP.getFreePsram();

    unsigned long s = millis() / 1000;
    char up[16];
    sprintf(up, "%02lu:%02lu:%02lu", s/3600, (s/60)%60, s%60);

    _topMonitor->updateStats(freeHeap, freePsram, up);
}
