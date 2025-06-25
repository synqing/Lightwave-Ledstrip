#ifndef BASIC_EFFECTS_H
#define BASIC_EFFECTS_H

#include "../../core/FxEngine.h"

// Forward declarations of effect functions
void gradientEffect();
void fibonacciEffect();
void waveEffect();
void pulseEffect();
void kaleidoscopeEffect();

// Collection of basic effects
class BasicEffects {
public:
    static void registerAll(FxEngine& engine) {
        // Register function-based effects
        engine.addEffect("Gradient", gradientEffect, 128, 10, 20);
        engine.addEffect("Fibonacci", fibonacciEffect, 128, 15, 15);
        engine.addEffect("Wave", waveEffect, 128, 20, 25);
        engine.addEffect("Pulse", pulseEffect, 128, 30, 10);
        engine.addEffect("Kaleidoscope", kaleidoscopeEffect, 128, 25, 20);
    }
};

#endif // BASIC_EFFECTS_H