/**
 * @file CoreEffects.cpp
 * @brief Core v2 effects implementation with CENTER PAIR compliance
 *
 * Ported from v1 with proper CENTER ORIGIN patterns.
 * All effects use RenderContext for Actor-based rendering.
 */

#include "CoreEffects.h"
#include "LGPInterferenceEffects.h"
#include "LGPGeometricEffects.h"
#include "LGPAdvancedEffects.h"
#include "LGPOrganicEffects.h"
#include "LGPQuantumEffects.h"
#include "LGPColorMixingEffects.h"
#include "LGPNovelPhysicsEffects.h"
#include "LGPChromaticEffects.h"
// Legacy effects disabled - see plugins/legacy/ when ready to integrate
// #include "../plugins/legacy/LegacyEffectRegistration.h"
#include "utils/FastLEDOptim.h"
#include "../core/narrative/NarrativeEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== Static State for Stateful Effects ====================
// Note: In v2, effect state should eventually move to EffectContext
// For now, we use static variables as in v1

static byte fireHeat[STRIP_LENGTH];
static uint32_t waveOffset = 0;
static uint32_t plasmaTime = 0;

// ==================== FIRE ====================

void effectFire(RenderContext& ctx) {
    // CENTER ORIGIN FIRE - Sparks ignite at center 79/80 and spread outward
    // Narrative integration: spark frequency modulated by tension
    using namespace lightwaveos::narrative;

    // Cool down every cell
    for (int i = 0; i < STRIP_LENGTH; i++) {
        fireHeat[i] = qsub8(fireHeat[i], random8(0, ((55 * 10) / STRIP_LENGTH) + 2));
    }

    // Heat diffuses
    for (int k = 1; k < STRIP_LENGTH - 1; k++) {
        fireHeat[k] = (fireHeat[k - 1] + fireHeat[k] + fireHeat[k + 1]) / 3;
    }

    // Ignite new sparks at CENTER PAIR (79/80)
    // Apply narrative tension to spark frequency (opt-in, backward compatible)
    float narrativeTension = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeTension = NARRATIVE.getTension();  // 0.0-1.0
    }
    uint8_t sparkChance = (uint8_t)((80 + ctx.speed) * (0.5f + narrativeTension * 0.5f));
    if (random8() < sparkChance) {
        int center = CENTER_LEFT + random8(2);  // 79 or 80
        fireHeat[center] = qadd8(fireHeat[center], random8(160, 255));
    }

    // Map heat to LEDs with CENTER PAIR pattern
    for (int i = 0; i < STRIP_LENGTH; i++) {
        CRGB color = HeatColor(fireHeat[i]);

        // Strip 1
        ctx.leds[i] = color;

        // Strip 2 (mirrored)
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

// ==================== OCEAN ====================

void effectOcean(RenderContext& ctx) {
    // CENTER ORIGIN OCEAN - Waves emanate from center 79/80
    // Narrative integration: wave intensity modulated by narrative intensity
    using namespace lightwaveos::narrative;
    
    static uint32_t waterOffset = 0;
    waterOffset += ctx.speed / 2;

    if (waterOffset > 65535) waterOffset = waterOffset % 65536;
    
    // Get narrative intensity for wave modulation (opt-in, backward compatible)
    float narrativeIntensity = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeIntensity = NARRATIVE.getIntensity();
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from CENTER PAIR
        float distFromCenter = abs((float)i - CENTER_LEFT);

        // Create wave motion from center outward
        uint8_t wave1 = sin8((uint16_t)(distFromCenter * 10) + waterOffset);
        uint8_t wave2 = sin8((uint16_t)(distFromCenter * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Apply narrative intensity modulation to wave amplitude
        combinedWave = (uint8_t)(combinedWave * narrativeIntensity);

        // Ocean colors from deep blue to cyan
        uint8_t hue = 160 + (combinedWave >> 3);
        uint8_t brightness = 100 + (uint8_t)((combinedWave >> 1) * narrativeIntensity);
        uint8_t saturation = 255 - (combinedWave >> 2);

        CRGB color = CHSV(hue, saturation, brightness);

        // Both strips
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

// ==================== PLASMA ====================

void effectPlasma(RenderContext& ctx) {
    // CENTER ORIGIN PLASMA - Plasma field from center
    // Narrative integration: speed modulated by tempo multiplier
    using namespace utils;
    using namespace lightwaveos::narrative;
    
    // Apply narrative tempo multiplier to speed (opt-in, backward compatible)
    float narrativeSpeed = ctx.speed;
    if (NARRATIVE.isEnabled()) {
        narrativeSpeed *= NARRATIVE.getTempoMultiplier();
    }
    
    plasmaTime += (uint16_t)narrativeSpeed;
    if (plasmaTime > 65535) plasmaTime = plasmaTime % 65536;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Use utility function for center-based sin16 calculations
        float v1 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 8.0f, plasmaTime);
        float v2 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 5.0f, (uint16_t)(-plasmaTime * 0.75f));
        float v3 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 3.0f, (uint16_t)(plasmaTime * 0.5f));

        uint8_t paletteIndex = (uint8_t)((v1 + v2 + v3) * 10.0f + 15.0f) + ctx.hue;
        uint8_t brightness = (uint8_t)((v1 + v2 + 2.0f) * 63.75f);

        CRGB color = ColorFromPalette(*ctx.palette, paletteIndex, brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

// ==================== CONFETTI ====================

void effectConfetti(RenderContext& ctx) {
    // CENTER ORIGIN CONFETTI - Sparks spawn at center 79/80 and fade outward

    // Fade all LEDs
    fadeToBlackBy(ctx.leds, ctx.numLeds, 10);

    // Spawn new confetti at CENTER PAIR
    if (random8() < 80) {
        int centerPos = CENTER_LEFT + random8(2);  // 79 or 80
        CRGB color = ColorFromPalette(*ctx.palette, ctx.hue + random8(64), 255);

        ctx.leds[centerPos] += color;
        ctx.leds[centerPos + STRIP_LENGTH] += color;
    }

    // Move confetti outward from center with fading
    // Left side propagation
    for (int i = CENTER_LEFT - 1; i >= 0; i--) {
        if (ctx.leds[i + 1]) {
            ctx.leds[i] = ctx.leds[i + 1];
            ctx.leds[i].fadeToBlackBy(25);

            // Mirror to strip 2
            ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
        }
    }

    // Right side propagation
    for (int i = CENTER_RIGHT + 1; i < STRIP_LENGTH; i++) {
        if (ctx.leds[i - 1]) {
            ctx.leds[i] = ctx.leds[i - 1];
            ctx.leds[i].fadeToBlackBy(25);

            // Mirror to strip 2
            ctx.leds[i + STRIP_LENGTH] = ctx.leds[i];
        }
    }
}

// ==================== SINELON ====================

void effectSinelon(RenderContext& ctx) {
    // CENTER ORIGIN SINELON - Oscillates outward from center
    using namespace utils;
    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    // Oscillate from center outward using utility function
    int distFromCenter = fastled_beatsin16(13, 0, HALF_LENGTH);

    // Set both sides of center
    int pos1 = CENTER_RIGHT + distFromCenter;  // Right side
    int pos2 = CENTER_LEFT - distFromCenter;   // Left side

    CRGB color1 = ColorFromPalette(*ctx.palette, ctx.hue, 192);
    CRGB color2 = ColorFromPalette(*ctx.palette, ctx.hue + 128, 192);

    if (pos1 < STRIP_LENGTH) {
        ctx.leds[pos1] += color1;
        ctx.leds[pos1 + STRIP_LENGTH] += color1;
    }
    if (pos2 >= 0) {
        ctx.leds[pos2] += color2;
        ctx.leds[pos2 + STRIP_LENGTH] += color2;
    }
}

// ==================== JUGGLE ====================

void effectJuggle(RenderContext& ctx) {
    // CENTER ORIGIN JUGGLE - Multiple dots oscillate from center
    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    uint8_t dothue = ctx.hue;
    for (int i = 0; i < 8; i++) {
        // Oscillate from center outward
        int distFromCenter = beatsin16(i + 7, 0, HALF_LENGTH);

        int pos1 = CENTER_RIGHT + distFromCenter;
        int pos2 = CENTER_LEFT - distFromCenter;

        CRGB color = CHSV(dothue, 200, 255);

        if (pos1 < STRIP_LENGTH) {
            ctx.leds[pos1] |= color;
            ctx.leds[pos1 + STRIP_LENGTH] |= color;
        }
        if (pos2 >= 0) {
            ctx.leds[pos2] |= color;
            ctx.leds[pos2 + STRIP_LENGTH] |= color;
        }

        dothue += 32;
    }
}

// ==================== BPM ====================

void effectBPM(RenderContext& ctx) {
    // CENTER ORIGIN BPM - Pulses emanate from center
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - CENTER_LEFT);

        // Intensity decreases with distance from center
        uint8_t intensity = beat - (uint8_t)(distFromCenter * 3);
        intensity = max(intensity, (uint8_t)32);

        uint8_t colorIndex = ctx.hue + (uint8_t)(distFromCenter * 2);
        CRGB color = ColorFromPalette(*ctx.palette, colorIndex, intensity);

        ctx.leds[i] = color;
        ctx.leds[i + STRIP_LENGTH] = color;
    }
}

// ==================== WAVE ====================

void effectWave(RenderContext& ctx) {
    // CENTER ORIGIN WAVE - Waves propagate from center
    waveOffset += ctx.speed;
    if (waveOffset > 65535) waveOffset = waveOffset % 65536;

    // Gentle fade
    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - CENTER_LEFT);

        // Wave propagates outward from center
        uint8_t brightness = sin8((uint16_t)(distFromCenter * 15) + (waveOffset >> 4));
        uint8_t colorIndex = (uint8_t)(distFromCenter * 8) + (waveOffset >> 6);

        CRGB color = ColorFromPalette(*ctx.palette, colorIndex, brightness);

        ctx.leds[i] = color;
        ctx.leds[i + STRIP_LENGTH] = color;
    }
}

// ==================== RIPPLE ====================

void effectRipple(RenderContext& ctx) {
    // CENTER ORIGIN RIPPLE - Ripples expand from center
    static struct {
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    } ripples[5] = {};

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    // Spawn new ripples at center
    if (random8() < 30) {
        for (int i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].radius = 0;
                ripples[i].speed = 0.5f + (random8() / 255.0f) * 2.0f;
                ripples[i].hue = random8();
                ripples[i].active = true;
                break;
            }
        }
    }

    // Update and render ripples
    for (int r = 0; r < 5; r++) {
        if (!ripples[r].active) continue;

        ripples[r].radius += ripples[r].speed * (ctx.speed / 10.0f);

        if (ripples[r].radius > HALF_LENGTH) {
            ripples[r].active = false;
            continue;
        }

        // Draw ripple
        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - CENTER_LEFT);
            float wavePos = distFromCenter - ripples[r].radius;

            if (abs(wavePos) < 3.0f) {
                uint8_t brightness = 255 - (uint8_t)(abs(wavePos) * 85);
                brightness = (brightness * (uint8_t)(HALF_LENGTH - ripples[r].radius)) / HALF_LENGTH;

                CRGB color = ColorFromPalette(*ctx.palette,
                                              ripples[r].hue + (uint8_t)distFromCenter,
                                              brightness);
                ctx.leds[i] += color;
                ctx.leds[i + STRIP_LENGTH] += color;
            }
        }
    }
}

// ==================== HEARTBEAT ====================

void effectHeartbeat(RenderContext& ctx) {
    // CENTER ORIGIN HEARTBEAT - Pulses like a heart from center
    // Uses a "lub-dub" pattern: two quick beats then pause

    static uint32_t lastBeatTime = 0;
    static uint8_t beatState = 0;  // 0=waiting, 1=first beat, 2=second beat
    static float pulseRadius = 0;

    uint32_t now = millis();

    // Heartbeat timing (lub-dub pattern)
    // First beat, short pause, second beat, long pause
    const uint32_t BEAT1_DELAY = 0;
    const uint32_t BEAT2_DELAY = 200;   // 200ms after first beat
    const uint32_t CYCLE_TIME = 800;    // Full cycle ~75 BPM

    uint32_t cyclePos = (now - lastBeatTime);

    // Trigger beats
    if (cyclePos >= CYCLE_TIME) {
        lastBeatTime = now;
        beatState = 1;
        pulseRadius = 0;
    } else if (cyclePos >= BEAT2_DELAY && beatState == 1) {
        beatState = 2;
        pulseRadius = 0;
    }

    // Fade existing
    fadeToBlackBy(ctx.leds, ctx.numLeds, 25);

    // Expand pulse from center
    if (beatState > 0 && pulseRadius < HALF_LENGTH) {
        // Draw expanding ring
        for (int dist = 0; dist < HALF_LENGTH; dist++) {
            float delta = fabs(dist - pulseRadius);

            if (delta < 8) {
                float intensity = 1.0f - (delta / 8.0f);
                // Fade as it expands
                intensity *= 1.0f - (pulseRadius / HALF_LENGTH) * 0.7f;

                uint8_t brightness = (uint8_t)(255 * intensity);
                CRGB color = ColorFromPalette(*ctx.palette,
                                              ctx.hue + (uint8_t)(dist * 2),
                                              brightness);

                // Strip 1: center pair
                uint16_t left1 = CENTER_LEFT - dist;
                uint16_t right1 = CENTER_RIGHT + dist;

                if (left1 < STRIP_LENGTH) ctx.leds[left1] += color;
                if (right1 < STRIP_LENGTH) ctx.leds[right1] += color;

                // Strip 2: center pair
                uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
                uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;

                if (left2 < ctx.numLeds) ctx.leds[left2] += color;
                if (right2 < ctx.numLeds) ctx.leds[right2] += color;
            }
        }

        // Expand pulse outward
        pulseRadius += ctx.speed / 8.0f;
    }
}

// ==================== INTERFERENCE ====================

void effectInterference(RenderContext& ctx) {
    // CENTER ORIGIN INTERFERENCE - Two waves from center create patterns
    static float wave1Phase = 0;
    static float wave2Phase = 0;

    wave1Phase += ctx.speed / 20.0f;
    wave2Phase -= ctx.speed / 30.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - CENTER_LEFT);
        float normalizedDist = distFromCenter / HALF_LENGTH;

        // Two waves emanating from center
        float wave1 = sin(normalizedDist * PI * 4 + wave1Phase) * 127 + 128;
        float wave2 = sin(normalizedDist * PI * 6 + wave2Phase) * 127 + 128;

        // Interference pattern
        uint8_t brightness = (uint8_t)((wave1 + wave2) / 2.0f);
        uint8_t hue = (uint8_t)(wave1Phase * 20) + (uint8_t)(distFromCenter * 8);

        CRGB color = ColorFromPalette(*ctx.palette, ctx.hue + hue, brightness);

        ctx.leds[i] = color;
        ctx.leds[i + STRIP_LENGTH] = color;
    }
}

// ==================== BREATHING ====================

void effectBreathing(RenderContext& ctx) {
    // CENTER ORIGIN BREATHING - Smooth expansion/contraction from center
    static float breathPhase = 0;

    float breath = (sin(breathPhase) + 1.0f) / 2.0f;
    float radius = breath * HALF_LENGTH;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 15);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - CENTER_LEFT);

        if (distFromCenter <= radius) {
            float intensity = 1.0f - (distFromCenter / radius) * 0.5f;
            uint8_t brightness = (uint8_t)(255 * intensity * breath);

            CRGB color = ColorFromPalette(*ctx.palette, ctx.hue + (uint8_t)distFromCenter, brightness);

            ctx.leds[i] = color;
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }

    breathPhase += ctx.speed / 100.0f;
}

// ==================== PULSE (from main.cpp) ====================

void effectPulse(RenderContext& ctx) {
    // CENTER PAIR Pulse - Canonical pattern
    float phase = (ctx.frameCount * ctx.speed / 60.0f);
    float pulsePos = fmod(phase, (float)HALF_LENGTH);

    memset(ctx.leds, 0, ctx.numLeds * sizeof(CRGB));

    for (int dist = 0; dist < HALF_LENGTH; dist++) {
        float delta = fabs(dist - pulsePos);
        float intensity = (delta < 10) ? (1.0f - delta / 10.0f) : 0.0f;

        if (intensity > 0) {
            CRGB color = ColorFromPalette(*ctx.palette, ctx.hue + dist * 3,
                                          (uint8_t)(intensity * 255));

            // Strip 1: center pair
            uint16_t left1 = CENTER_LEFT - dist;
            uint16_t right1 = CENTER_RIGHT + dist;

            if (left1 < STRIP_LENGTH) ctx.leds[left1] = color;
            if (right1 < STRIP_LENGTH) ctx.leds[right1] = color;

            // Strip 2: center pair
            uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
            uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;

            if (left2 < ctx.numLeds) ctx.leds[left2] = color;
            if (right2 < ctx.numLeds) ctx.leds[right2] = color;
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerCoreEffects(RendererActor* renderer) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // Register all core effects
    if (renderer->registerEffect(0, "Fire", effectFire)) count++;
    if (renderer->registerEffect(1, "Ocean", effectOcean)) count++;
    if (renderer->registerEffect(2, "Plasma", effectPlasma)) count++;
    if (renderer->registerEffect(3, "Confetti", effectConfetti)) count++;
    if (renderer->registerEffect(4, "Sinelon", effectSinelon)) count++;
    if (renderer->registerEffect(5, "Juggle", effectJuggle)) count++;
    if (renderer->registerEffect(6, "BPM", effectBPM)) count++;
    if (renderer->registerEffect(7, "Wave", effectWave)) count++;
    if (renderer->registerEffect(8, "Ripple", effectRipple)) count++;
    if (renderer->registerEffect(9, "Heartbeat", effectHeartbeat)) count++;
    if (renderer->registerEffect(10, "Interference", effectInterference)) count++;
    if (renderer->registerEffect(11, "Breathing", effectBreathing)) count++;
    if (renderer->registerEffect(12, "Pulse", effectPulse)) count++;

    return count;
}

uint8_t registerAllEffects(RendererActor* renderer) {
    if (!renderer) return 0;

    uint8_t total = 0;

    // =============== REGISTER ALL NATIVE V2 EFFECTS ===============
    // 68 total effects organized by category
    // Legacy v1 effects are disabled until plugins/legacy is properly integrated

    // Core effects (13) - IDs 0-12
    total += registerCoreEffects(renderer);

    // LGP Interference effects (5) - IDs 13-17
    total += registerLGPInterferenceEffects(renderer, total);

    // LGP Geometric effects (8) - IDs 18-25
    total += registerLGPGeometricEffects(renderer, total);

    // LGP Advanced effects (8) - IDs 26-33
    total += registerLGPAdvancedEffects(renderer, total);

    // LGP Organic effects (6) - IDs 34-39
    total += registerLGPOrganicEffects(renderer, total);

    // LGP Quantum effects (10) - IDs 40-49
    total += registerLGPQuantumEffects(renderer, total);

    // LGP Color Mixing effects (10) - IDs 50-59
    total += registerLGPColorMixingEffects(renderer, total);

    // LGP Novel Physics effects (5) - IDs 60-64
    total += registerLGPNovelPhysicsEffects(renderer, total);

    // LGP Chromatic effects (3) - IDs 65-67 (physics-accurate Cauchy dispersion)
    total += registerLGPChromaticEffects(renderer, total);

    // =============== EFFECT COUNT PARITY VALIDATION ===============
    // Runtime validation: ensure registered count matches expected
    constexpr uint8_t EXPECTED_EFFECT_COUNT = 68;
    if (total != EXPECTED_EFFECT_COUNT) {
        Serial.printf("[WARNING] Effect count mismatch: registered %d, expected %d\n", total, EXPECTED_EFFECT_COUNT);
        Serial.printf("[WARNING] This may indicate missing effect registrations or metadata drift\n");
    } else {
        Serial.printf("[OK] Effect count validated: %d effects registered (matches expected)\n", total);
    }

    return total;
}

} // namespace effects
} // namespace lightwaveos
