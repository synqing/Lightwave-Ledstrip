#ifndef EFFECT_BASE_H
#define EFFECT_BASE_H

#include <FastLED.h>
#include "../config/hardware_config.h"

// Forward declarations
extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t angles[HardwareConfig::NUM_LEDS];
extern uint8_t radii[HardwareConfig::NUM_LEDS];
extern CRGBPalette16 currentPalette;
extern uint8_t currentPaletteIndex;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern uint8_t brightnessVal;

// Base class for all effects
class EffectBase {
protected:
    const char* name;
    uint8_t defaultBrightness;
    uint8_t defaultSpeed;
    uint8_t defaultFade;
    
    // Effect state
    uint32_t lastUpdateTime;
    uint16_t frameCounter;
    
public:
    EffectBase(const char* effectName, 
               uint8_t brightness = 128, 
               uint8_t speed = 10, 
               uint8_t fade = 20) :
        name(effectName),
        defaultBrightness(brightness),
        defaultSpeed(speed),
        defaultFade(fade),
        lastUpdateTime(0),
        frameCounter(0) {}
    
    virtual ~EffectBase() {}
    
    // Pure virtual - must be implemented by derived classes
    virtual void render() = 0;
    
    // Optional overrides
    virtual void init() {
        // Called when effect is activated
        lastUpdateTime = millis();
        frameCounter = 0;
    }
    
    virtual void cleanup() {
        // Called when effect is deactivated
    }
    
    virtual void updateParameters(uint8_t param1, uint8_t param2, uint8_t param3) {
        // Update effect-specific parameters
    }
    
    // Getters
    const char* getName() const { return name; }
    uint8_t getDefaultBrightness() const { return defaultBrightness; }
    uint8_t getDefaultSpeed() const { return defaultSpeed; }
    uint8_t getDefaultFade() const { return defaultFade; }
    
    // Helper functions available to all effects
    protected:
    
    // Get time delta since last update
    uint32_t getDeltaTime() {
        uint32_t now = millis();
        uint32_t delta = now - lastUpdateTime;
        lastUpdateTime = now;
        return delta;
    }
    
    // Increment and return frame counter
    uint16_t getFrameCount() {
        return ++frameCounter;
    }
    
    // Map a value to LED position using angle/radius
    uint8_t mapToPosition(uint16_t ledIndex, uint8_t value, bool useRadius = true) {
        if (ledIndex >= HardwareConfig::NUM_LEDS) return 0;
        
        uint8_t angle = angles[ledIndex];
        uint8_t radius = radii[ledIndex];
        
        if (useRadius) {
            return ((uint16_t)value * radius) >> 8;
        } else {
            return angle;
        }
    }
};

// Function pointer for legacy effects
typedef void (*LegacyEffectFunction)();

// Wrapper to adapt legacy effects to the new system
class LegacyEffectWrapper : public EffectBase {
private:
    LegacyEffectFunction function;
    
public:
    LegacyEffectWrapper(const char* name, 
                       LegacyEffectFunction func,
                       uint8_t brightness = 128,
                       uint8_t speed = 10,
                       uint8_t fade = 20) :
        EffectBase(name, brightness, speed, fade),
        function(func) {}
    
    void render() override {
        if (function) {
            function();
        }
    }
};

#endif // EFFECT_BASE_H