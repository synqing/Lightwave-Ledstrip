#ifndef BASIC_EFFECTS_H
#define BASIC_EFFECTS_H

#include "../../core/FxEngine.h"
#include "GradientEffect.h"
#include "FibonacciEffect.h"
#include "WaveEffect.h"
#include "PulseEffect.h"
#include "KaleidoscopeEffect.h"

// Collection of basic effects
class BasicEffects {
public:
    static void registerAll(FxEngine& engine) {
        // Register object-based effects
        engine.addEffect(new GradientEffect());
        engine.addEffect(new FibonacciEffect());
        engine.addEffect(new WaveEffect());
        engine.addEffect(new PulseEffect());
        engine.addEffect(new KaleidoscopeEffect());
    }
};

#endif // BASIC_EFFECTS_H