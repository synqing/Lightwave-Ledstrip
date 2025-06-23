#ifndef LIGHT_GUIDE_EFFECTS_H
#define LIGHT_GUIDE_EFFECTS_H

#include "config/features.h"

#if LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

// Include all light guide effect headers
#include "LightGuideBase.h"
#include "StandingWaveEffect.h"
#include "PlasmaFieldEffect.h"
#include "VolumetricDisplayEffect.h"

// Light Guide Effect Registry
namespace LightGuideEffects {
    
    // Effect instances (allocated on heap to save stack space)
    extern StandingWaveEffect* standingWave;
    extern PlasmaFieldEffect* plasmaField;
    extern VolumetricDisplayEffect* volumetricDisplay;
    
    // Effect management
    void initializeLightGuideEffects();
    void cleanupLightGuideEffects();
    LightGuideEffectBase* getLightGuideEffect(uint8_t index);
    uint8_t getLightGuideEffectCount();
    const char* getLightGuideEffectName(uint8_t index);
    
    // Parameter control for encoder integration
    void setGlobalInterferenceStrength(float strength);
    void setGlobalPhaseOffset(float offset);
    void setGlobalPropagationSpeed(float speed);
    void setGlobalEdgeBalance(float balance);
    void setGlobalSyncMode(LightGuideSyncMode mode);
    
    // Current effect management
    extern uint8_t currentLightGuideEffect;
    extern LightGuideEffectBase* currentEffect;
    
    void selectLightGuideEffect(uint8_t index);
    void updateCurrentEffect();
}

// Implementation
namespace LightGuideEffects {
    
    // Effect instances
    StandingWaveEffect* standingWave = nullptr;
    PlasmaFieldEffect* plasmaField = nullptr;
    VolumetricDisplayEffect* volumetricDisplay = nullptr;
    
    // Current effect tracking
    uint8_t currentLightGuideEffect = 0;
    LightGuideEffectBase* currentEffect = nullptr;
    
    void initializeLightGuideEffects() {
        // Allocate effects on heap to conserve stack space
        standingWave = new StandingWaveEffect();
        plasmaField = new PlasmaFieldEffect();
        volumetricDisplay = new VolumetricDisplayEffect();
        
        // Set initial effect
        currentEffect = standingWave;
        currentLightGuideEffect = 0;
        
        Serial.println("Light Guide Effects initialized");
        Serial.print("Available effects: ");
        Serial.println(getLightGuideEffectCount());
    }
    
    void cleanupLightGuideEffects() {
        delete standingWave;
        delete plasmaField;
        delete volumetricDisplay;
        
        standingWave = nullptr;
        plasmaField = nullptr;
        volumetricDisplay = nullptr;
        currentEffect = nullptr;
        
        Serial.println("Light Guide Effects cleaned up");
    }
    
    LightGuideEffectBase* getLightGuideEffect(uint8_t index) {
        switch (index) {
            case 0: return standingWave;
            case 1: return plasmaField;
            case 2: return volumetricDisplay;
            default: return standingWave;
        }
    }
    
    uint8_t getLightGuideEffectCount() {
        return 3;  // Update when adding new effects
    }
    
    const char* getLightGuideEffectName(uint8_t index) {
        LightGuideEffectBase* effect = getLightGuideEffect(index);
        return effect ? effect->getName() : "Unknown";
    }
    
    void selectLightGuideEffect(uint8_t index) {
        if (index < getLightGuideEffectCount()) {
            currentLightGuideEffect = index;
            currentEffect = getLightGuideEffect(index);
            
            Serial.print("Selected light guide effect: ");
            Serial.println(getLightGuideEffectName(index));
        }
    }
    
    void updateCurrentEffect() {
        if (currentEffect) {
            currentEffect->render();
        }
    }
    
    // Global parameter control
    void setGlobalInterferenceStrength(float strength) {
        if (standingWave) standingWave->setInterferenceStrength(strength);
        if (plasmaField) plasmaField->setInterferenceStrength(strength);
        if (volumetricDisplay) volumetricDisplay->setInterferenceStrength(strength);
    }
    
    void setGlobalPhaseOffset(float offset) {
        if (standingWave) standingWave->setPhaseOffset(offset);
        if (plasmaField) plasmaField->setPhaseOffset(offset);
        if (volumetricDisplay) volumetricDisplay->setPhaseOffset(offset);
    }
    
    void setGlobalPropagationSpeed(float speed) {
        if (standingWave) standingWave->setPropagationSpeed(speed);
        if (plasmaField) plasmaField->setPropagationSpeed(speed);
        if (volumetricDisplay) volumetricDisplay->setPropagationSpeed(speed);
    }
    
    void setGlobalEdgeBalance(float balance) {
        if (standingWave) standingWave->setEdgeBalance(balance);
        if (plasmaField) plasmaField->setEdgeBalance(balance);
        if (volumetricDisplay) volumetricDisplay->setEdgeBalance(balance);
    }
    
    void setGlobalSyncMode(LightGuideSyncMode mode) {
        if (standingWave) standingWave->setSyncMode(mode);
        if (plasmaField) plasmaField->setSyncMode(mode);
        if (volumetricDisplay) volumetricDisplay->setSyncMode(mode);
    }
}

// Light Guide Effect Integration Macros
#define LIGHT_GUIDE_EFFECT_COUNT LightGuideEffects::getLightGuideEffectCount()
#define CURRENT_LIGHT_GUIDE_EFFECT LightGuideEffects::currentEffect
#define LIGHT_GUIDE_EFFECT_NAME(index) LightGuideEffects::getLightGuideEffectName(index)

// Encoder integration helper functions
inline void handleLightGuideEncoderChange(uint8_t encoder, int32_t value) {
    switch (encoder) {
        case 0: // Effect selection
            {
                uint8_t newIndex = LightGuideEffects::currentLightGuideEffect;
                if (value > 0) {
                    newIndex = (newIndex + 1) % LIGHT_GUIDE_EFFECT_COUNT;
                } else if (value < 0) {
                    newIndex = (newIndex == 0) ? LIGHT_GUIDE_EFFECT_COUNT - 1 : newIndex - 1;
                }
                LightGuideEffects::selectLightGuideEffect(newIndex);
            }
            break;
            
        case 1: // Interference strength
            if (CURRENT_LIGHT_GUIDE_EFFECT) {
                float current = CURRENT_LIGHT_GUIDE_EFFECT->getInterferenceStrength();
                float newValue = constrain(current + value * 0.1f, 0.0f, 2.0f);
                CURRENT_LIGHT_GUIDE_EFFECT->setInterferenceStrength(newValue);
            }
            break;
            
        case 2: // Phase offset
            if (CURRENT_LIGHT_GUIDE_EFFECT) {
                float current = CURRENT_LIGHT_GUIDE_EFFECT->getPhaseOffset();
                float newValue = fmod(current + value * 0.1f, LightGuide::TWO_PI_F);
                CURRENT_LIGHT_GUIDE_EFFECT->setPhaseOffset(newValue);
            }
            break;
            
        case 3: // Propagation speed
            if (CURRENT_LIGHT_GUIDE_EFFECT) {
                float current = CURRENT_LIGHT_GUIDE_EFFECT->getPropagationSpeed();
                float newValue = constrain(current + value * 0.1f, 0.1f, 5.0f);
                CURRENT_LIGHT_GUIDE_EFFECT->setPropagationSpeed(newValue);
            }
            break;
            
        case 4: // Edge balance
            if (CURRENT_LIGHT_GUIDE_EFFECT) {
                float current = CURRENT_LIGHT_GUIDE_EFFECT->getEdgeBalance();
                float newValue = constrain(current + value * 0.05f, 0.0f, 1.0f);
                CURRENT_LIGHT_GUIDE_EFFECT->setEdgeBalance(newValue);
            }
            break;
            
        case 5: // Sync mode
            if (CURRENT_LIGHT_GUIDE_EFFECT) {
                int currentMode = (int)CURRENT_LIGHT_GUIDE_EFFECT->getSyncMode();
                int newMode = currentMode + value;
                if (newMode < 0) newMode = 5;  // Wrap to last mode
                if (newMode > 5) newMode = 0;  // Wrap to first mode
                CURRENT_LIGHT_GUIDE_EFFECT->setSyncMode((LightGuideSyncMode)newMode);
            }
            break;
            
        case 6: // Effect-specific parameter 1
            // TODO: Handle effect-specific parameters
            // handleEffectSpecificParameter1(value);
            break;
            
        case 7: // Effect-specific parameter 2
            // TODO: Handle effect-specific parameters  
            // handleEffectSpecificParameter2(value);
            break;
    }
}

// Effect-specific parameter handlers
inline void handleEffectSpecificParameter1(int32_t value) {
    if (!CURRENT_LIGHT_GUIDE_EFFECT) return;
    
    uint8_t effectIndex = LightGuideEffects::currentLightGuideEffect;
    
    switch (effectIndex) {
        case 0: // Standing Wave Effect
            {
                StandingWaveEffect* effect = static_cast<StandingWaveEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                float current = effect->getWaveFrequency();
                float newValue = constrain(current + value * 0.1f, 0.5f, 10.0f);
                effect->setWaveFrequency(newValue);
            }
            break;
            
        case 1: // Plasma Field Effect
            {
                PlasmaFieldEffect* effect = static_cast<PlasmaFieldEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                float current = effect->getFieldAnimationSpeed();
                float newValue = constrain(current + value * 0.1f, 0.1f, 3.0f);
                effect->setFieldAnimationSpeed(newValue);
            }
            break;
            
        case 2: // Volumetric Display Effect
            {
                VolumetricDisplayEffect* effect = static_cast<VolumetricDisplayEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                uint8_t current = effect->getMovementPattern();
                uint8_t newValue = (current + value + 4) % 4;  // Wrap around 0-3
                effect->setMovementPattern(newValue);
            }
            break;
    }
}

inline void handleEffectSpecificParameter2(int32_t value) {
    if (!CURRENT_LIGHT_GUIDE_EFFECT) return;
    
    uint8_t effectIndex = LightGuideEffects::currentLightGuideEffect;
    
    switch (effectIndex) {
        case 0: // Standing Wave Effect
            {
                StandingWaveEffect* effect = static_cast<StandingWaveEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                float current = effect->getFrequencyOffset();
                float newValue = constrain(current + value * 0.01f, 0.001f, 1.0f);
                effect->setFrequencyOffset(newValue);
            }
            break;
            
        case 1: // Plasma Field Effect
            {
                PlasmaFieldEffect* effect = static_cast<PlasmaFieldEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                float current = effect->getChargeSeparation();
                float newValue = constrain(current + value * 0.1f, 0.1f, 2.0f);
                effect->setChargeSeparation(newValue);
            }
            break;
            
        case 2: // Volumetric Display Effect
            {
                VolumetricDisplayEffect* effect = static_cast<VolumetricDisplayEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                float current = effect->getPatternSpeed();
                float newValue = constrain(current + value * 0.1f, 0.1f, 3.0f);
                effect->setPatternSpeed(newValue);
            }
            break;
    }
}

// Serial menu integration for light guide effects
inline void printLightGuideEffectStatus() {
    if (!CURRENT_LIGHT_GUIDE_EFFECT) return;
    
    Serial.println("\n=== Light Guide Effect Status ===");
    Serial.print("Current Effect: ");
    Serial.println(CURRENT_LIGHT_GUIDE_EFFECT->getName());
    Serial.print("Interference Strength: ");
    Serial.println(CURRENT_LIGHT_GUIDE_EFFECT->getInterferenceStrength());
    Serial.print("Phase Offset: ");
    Serial.println(CURRENT_LIGHT_GUIDE_EFFECT->getPhaseOffset());
    Serial.print("Propagation Speed: ");
    Serial.println(CURRENT_LIGHT_GUIDE_EFFECT->getPropagationSpeed());
    Serial.print("Edge Balance: ");
    Serial.println(CURRENT_LIGHT_GUIDE_EFFECT->getEdgeBalance());
    Serial.print("Sync Mode: ");
    Serial.println((int)CURRENT_LIGHT_GUIDE_EFFECT->getSyncMode());
    
    // Effect-specific status
    uint8_t effectIndex = LightGuideEffects::currentLightGuideEffect;
    switch (effectIndex) {
        case 0: // Standing Wave
            {
                StandingWaveEffect* effect = static_cast<StandingWaveEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                Serial.print("Wave Frequency: ");
                Serial.println(effect->getWaveFrequency());
                Serial.print("Frequency Offset: ");
                Serial.println(effect->getFrequencyOffset());
                Serial.print("Wave Count: ");
                Serial.println(effect->getWaveCount());
            }
            break;
            
        case 1: // Plasma Field
            {
                PlasmaFieldEffect* effect = static_cast<PlasmaFieldEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                Serial.print("Field Animation Speed: ");
                Serial.println(effect->getFieldAnimationSpeed());
                Serial.print("Charge Separation: ");
                Serial.println(effect->getChargeSeparation());
                Serial.print("Active Particles: ");
                Serial.println(effect->getActiveParticleCount());
            }
            break;
            
        case 2: // Volumetric Display
            {
                VolumetricDisplayEffect* effect = static_cast<VolumetricDisplayEffect*>(CURRENT_LIGHT_GUIDE_EFFECT);
                Serial.print("Movement Pattern: ");
                Serial.println(effect->getMovementPattern());
                Serial.print("Pattern Speed: ");
                Serial.println(effect->getPatternSpeed());
                Serial.print("Active Objects: ");
                Serial.println(effect->getActiveObjectCount());
            }
            break;
    }
    Serial.println("===============================");
}

#endif // LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

#endif // LIGHT_GUIDE_EFFECTS_H