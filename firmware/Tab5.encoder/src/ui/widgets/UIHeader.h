#pragma once
// ============================================================================
// UIHeader - Standard header for all Tab5 UI screens
// ============================================================================
// Consistent header with title, connection status, and power bar
// ============================================================================

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
#else
    #include <M5GFX.h>
#endif
#include "../Theme.h"

struct DeviceConnState {
    bool wifi = false;
    bool ws = false;
    bool encA = false;
    bool encB = false;
};

class UIHeader {
public:
    UIHeader(M5GFX* display);
    ~UIHeader();

    void setConnection(const DeviceConnState& state);
    void setPower(int8_t batteryPercent, bool isCharging, float voltage = -1.0f);
    void markDirty() { _dirty = true; }
    void render();

private:
    M5GFX* _display;
    M5Canvas _sprite;
    bool _spriteOk = false;

    DeviceConnState _conn;
    int8_t _batteryPercent = -1;  // -1 = unknown/not set
    bool _isCharging = false;
    float _voltage = -1.0f;  // -1.0 = unknown/not set (in volts)

    bool _dirty = true;

    void drawTitle();
    void drawConnectionStatus();
    void drawPowerBar();
};

