// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// ActionRowWidget - Touch action buttons row
// ============================================================================
// Renders the third row of touch buttons for colour correction controls.
// ============================================================================

#ifdef SIMULATOR_BUILD
    #include "../M5GFX_Mock.h"
#else
    #include <M5GFX.h>
#endif

#include "../Theme.h"

class ActionRowWidget {
public:
    ActionRowWidget(M5GFX* display, int x, int y, int w, int h);
    ~ActionRowWidget();

    void setGamma(float value, bool enabled);
    void setColourMode(uint8_t mode);
    void setAutoExposure(bool enabled);
    void setBrownGuardrail(bool enabled);
    void markDirty();
    void render();

private:
    M5GFX* _display = nullptr;
    LGFX_Sprite _sprite;
    bool _spriteOk = false;
    bool _dirty = true;

    int _x = 0;
    int _y = 0;
    int _w = 0;
    int _h = 0;

    bool _gammaEnabled = true;
    float _gammaValue = 2.2f;
    uint8_t _colourMode = 2;  // 0=OFF, 1=HSV, 2=RGB, 3=BOTH
    bool _autoExposureEnabled = false;
    bool _brownGuardrailEnabled = false;

    void drawButton(int index, const char* label, const char* value, uint16_t accent, bool active);
    static const char* colourModeLabel(uint8_t mode);
};
