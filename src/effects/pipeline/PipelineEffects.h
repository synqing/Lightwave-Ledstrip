#ifndef PIPELINE_EFFECTS_H
#define PIPELINE_EFFECTS_H

#include "../../core/FxEngine.h"

// Forward declarations for pipeline effect functions
void displayPipelineGradient();
void displayPipelineFibonacci();
void displayPipelineAudio();
void displayPipelineMatrix();
void displayPipelineReaction();

// Collection of pipeline effects
class PipelineEffects {
public:
    static void registerAll(FxEngine& engine) {
        // Register pipeline effects as legacy functions for now
        // TODO: Convert these to proper object-based effects
        engine.addEffect("Pipeline Gradient", displayPipelineGradient, 128, 10, 20);
        engine.addEffect("Pipeline Fibonacci", displayPipelineFibonacci, 140, 15, 25);
        engine.addEffect("Pipeline Audio", displayPipelineAudio, 150, 20, 15);
        engine.addEffect("Pipeline Matrix", displayPipelineMatrix, 120, 25, 10);
        engine.addEffect("Pipeline Reaction", displayPipelineReaction, 130, 5, 30);
    }
};

// Stub implementations for now
inline void displayPipelineGradient() {
    // TODO: Implement pipeline gradient
    static uint8_t hue = 0;
    hue++;
    fill_rainbow(leds, HardwareConfig::NUM_LEDS, hue, 7);
}

inline void displayPipelineFibonacci() {
    // TODO: Implement pipeline fibonacci
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    leds[beatsin16(30, 0, HardwareConfig::NUM_LEDS - 1)] = CHSV(beatsin8(20), 255, 255);
}

inline void displayPipelineAudio() {
    // TODO: Implement pipeline audio visualization
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 40);
    for (int i = 0; i < 8; i++) {
        uint8_t brightness = beatsin8(10 + i * 2, 0, 255);
        uint16_t pos = (HardwareConfig::NUM_LEDS * i) / 8;
        leds[pos] = CHSV(i * 32, 255, brightness);
    }
}

inline void displayPipelineMatrix() {
    // TODO: Implement pipeline matrix rain
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 50);
    if (random8() < 80) {
        leds[0] = CHSV(96 + random8(32), 255 - random8(50), 255);
    }
    for (int i = HardwareConfig::NUM_LEDS - 1; i > 0; i--) {
        leds[i] = leds[i - 1];
    }
}

inline void displayPipelineReaction() {
    // TODO: Implement pipeline reaction-diffusion
    static uint8_t heat[HardwareConfig::NUM_LEDS];
    
    // Cool down
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        heat[i] = qsub8(heat[i], random8(0, 10));
    }
    
    // Heat from bottom
    if (random8() < 120) {
        heat[random16(HardwareConfig::NUM_LEDS)] = qadd8(heat[random16(HardwareConfig::NUM_LEDS)], 160);
    }
    
    // Convert heat to LED colors
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        leds[i] = HeatColor(heat[i]);
    }
}

#endif // PIPELINE_EFFECTS_H