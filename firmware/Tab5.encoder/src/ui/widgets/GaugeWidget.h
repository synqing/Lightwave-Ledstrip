#pragma once
// ============================================================================
// GaugeWidget - Radial encoder gauge display
// ============================================================================

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
#else
    #include <M5GFX.h>
#endif
#include "../Theme.h"

class GaugeWidget {
public:
    GaugeWidget(M5GFX* display, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t index);
    ~GaugeWidget();

    void setValue(int32_t value);
    void setMaxValue(uint8_t max);
    void setHighlight(bool active);
    void markDirty() { _dirty = true; }
    void render();

private:
    M5GFX* _display;
    M5Canvas _sprite;
    bool _spriteOk = false;

    int32_t _x, _y, _w, _h;
    uint8_t _index;

    int32_t _value = 0;
    uint8_t _maxValue = 255;  // Default to 255, updated from ParameterMap
    bool _highlighted = false;
    bool _dirty = true;

    uint16_t _color;
    const char* _title;

    void drawBackground();
    void drawBar();
    void drawValue();
    void drawTitle();

    void drawBackgroundDirect();
    void drawBarDirect();
    void drawValueDirect();
    void drawTitleDirect();
};
