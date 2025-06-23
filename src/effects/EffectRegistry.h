#ifndef EFFECT_REGISTRY_H
#define EFFECT_REGISTRY_H

#include "../core/FxEngine.h"
#include "../config/features.h"

// Effect categories
#if FEATURE_BASIC_EFFECTS
#include "basic/BasicEffects.h"
#endif

#if FEATURE_ADVANCED_EFFECTS
#include "advanced/AdvancedEffects.h"
#endif

#if FEATURE_PIPELINE_EFFECTS
#include "pipeline/PipelineEffects.h"
#endif

class EffectRegistry {
public:
    static void registerAllEffects(FxEngine& engine) {
        Serial.println(F("[INFO] Registering effect categories..."));
        
        #if FEATURE_BASIC_EFFECTS
        registerBasicEffects(engine);
        #endif
        
        #if FEATURE_ADVANCED_EFFECTS
        registerAdvancedEffects(engine);
        #endif
        
        #if FEATURE_PIPELINE_EFFECTS
        registerPipelineEffects(engine);
        #endif
    }
    
private:
    #if FEATURE_BASIC_EFFECTS
    static void registerBasicEffects(FxEngine& engine) {
        // Register basic effects
        BasicEffects::registerAll(engine);
        Serial.println(F("[INFO] Basic effects registered"));
    }
    #endif
    
    #if FEATURE_ADVANCED_EFFECTS
    static void registerAdvancedEffects(FxEngine& engine) {
        // Register advanced effects
        AdvancedEffects::registerAll(engine);
        Serial.println(F("[INFO] Advanced effects registered"));
    }
    #endif
    
    #if FEATURE_PIPELINE_EFFECTS
    static void registerPipelineEffects(FxEngine& engine) {
        // Register pipeline effects
        PipelineEffects::registerAll(engine);
        Serial.println(F("[INFO] Pipeline effects registered"));
    }
    #endif
};

#endif // EFFECT_REGISTRY_H