#ifndef ADVANCED_EFFECTS_H
#define ADVANCED_EFFECTS_H

#include "../../core/FxEngine.h"
#include "HDREffect.h"
#include "SupersampledEffect.h"
#include "TimeAlphaEffect.h"
#include "FxWaveEffects.h"

// Collection of advanced effects
class AdvancedEffects {
public:
    static void registerAll(FxEngine& engine) {
        // Register advanced object-based effects
        engine.addEffect(new HDREffect());
        engine.addEffect(new SupersampledEffect());
        engine.addEffect(new TimeAlphaEffect());
        
        // Register FxWave effects
        engine.addEffect(new FxWaveRippleEffect());
        engine.addEffect(new FxWaveInterferenceEffect());
        engine.addEffect(new FxWaveOrbitalEffect());
    }
};

#endif // ADVANCED_EFFECTS_H