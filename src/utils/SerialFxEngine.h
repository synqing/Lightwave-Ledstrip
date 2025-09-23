#ifndef SERIAL_FX_ENGINE_H
#define SERIAL_FX_ENGINE_H

#include <stdint.h>
#include "../config/features.h"

#if FEATURE_SERIAL_MENU

class SerialFxEngine {
public:
    void nextEffect(uint8_t transitionType = 0, uint16_t duration = 800);
    void prevEffect(uint8_t transitionType = 0, uint16_t duration = 800);
    void setEffect(uint8_t index, uint8_t transitionType = 0, uint16_t duration = 800);

    const char* getCurrentEffectName() const;
    uint8_t getCurrentEffectIndex() const;
    uint8_t getNumEffects() const;

    float getApproximateFPS() const;
    bool getIsTransitioning() const;
    float getTransitionProgress() const;
};

extern SerialFxEngine fxEngine;

#endif // FEATURE_SERIAL_MENU

#endif // SERIAL_FX_ENGINE_H
