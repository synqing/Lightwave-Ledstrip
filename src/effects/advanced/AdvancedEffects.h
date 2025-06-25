#ifndef ADVANCED_EFFECTS_H
#define ADVANCED_EFFECTS_H

#include "../../core/FxEngine.h"

// Forward declarations of advanced effect functions
void hdrEffect();
void supersampledEffect();
void timeAlphaEffect();
void fxWaveRippleEffect();
void fxWaveInterferenceEffect();
void fxWaveOrbitalEffect();

// Collection of advanced effects
class AdvancedEffects {
public:
    static void registerAll(FxEngine& engine) {
        // Register advanced function-based effects
        engine.addEffect("HDR", hdrEffect, 128, 15, 20);
        engine.addEffect("Supersampled", supersampledEffect, 128, 10, 15);
        engine.addEffect("Time Alpha", timeAlphaEffect, 128, 20, 25);
        
        // Register FxWave effects
        engine.addEffect("Wave Ripple", fxWaveRippleEffect, 128, 25, 20);
        engine.addEffect("Wave Interference", fxWaveInterferenceEffect, 128, 20, 15);
        engine.addEffect("Wave Orbital", fxWaveOrbitalEffect, 128, 30, 25);
    }
};

#endif // ADVANCED_EFFECTS_H