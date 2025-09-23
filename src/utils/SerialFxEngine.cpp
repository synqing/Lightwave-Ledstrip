#include "SerialFxEngine.h"

#if FEATURE_SERIAL_MENU

#include <Arduino.h>
#include "../effects.h"
#include "../config/hardware_config.h"
#include "../effects/transitions/TransitionEngine.h"
#include "../core/EffectTypes.h"
#if FEATURE_PERFORMANCE_MONITOR
#include "../hardware/PerformanceMonitor.h"
#endif

// Externals from main.cpp
extern uint8_t currentEffect;
extern uint16_t fps;
extern TransitionEngine transitionEngine;
extern void startAdvancedTransition(uint8_t newEffect);
extern Effect effects[];
extern const uint8_t NUM_EFFECTS;

#if FEATURE_PERFORMANCE_MONITOR
extern PerformanceMonitor perfMon;
#endif

void SerialFxEngine::nextEffect(uint8_t /*transitionType*/, uint16_t /*duration*/) {
    if (NUM_EFFECTS == 0) return;
    uint8_t next = (currentEffect + 1) % NUM_EFFECTS;
    startAdvancedTransition(next);
}

void SerialFxEngine::prevEffect(uint8_t /*transitionType*/, uint16_t /*duration*/) {
    if (NUM_EFFECTS == 0) return;
    uint8_t prev = (currentEffect == 0) ? NUM_EFFECTS - 1 : currentEffect - 1;
    startAdvancedTransition(prev);
}

void SerialFxEngine::setEffect(uint8_t index, uint8_t /*transitionType*/, uint16_t /*duration*/) {
    if (index >= NUM_EFFECTS) return;
    if (index == currentEffect) return;
    startAdvancedTransition(index);
}

const char* SerialFxEngine::getCurrentEffectName() const {
    if (currentEffect >= NUM_EFFECTS) return "Unknown";
    return effects[currentEffect].name;
}

uint8_t SerialFxEngine::getCurrentEffectIndex() const {
    return currentEffect;
}

uint8_t SerialFxEngine::getNumEffects() const {
    return NUM_EFFECTS;
}

float SerialFxEngine::getApproximateFPS() const {
#if FEATURE_PERFORMANCE_MONITOR
    return perfMon.getCurrentFPS();
#else
    return fps;
#endif
}

bool SerialFxEngine::getIsTransitioning() const {
    return transitionEngine.isActive();
}

float SerialFxEngine::getTransitionProgress() const {
    return transitionEngine.isActive() ? transitionEngine.getProgress() : 0.0f;
}

SerialFxEngine fxEngine;

#endif // FEATURE_SERIAL_MENU
