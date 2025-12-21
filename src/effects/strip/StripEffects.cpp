#include "StripEffects.h"
#include "../../utils/TrigLookup.h"
#include "../../config/features.h"

#if FEATURE_NARRATIVE_ENGINE
#include "../../core/NarrativeEngine.h"
#endif

// ============== BASIC EFFECTS ==============

void solidColor() {
    // Solid blue effect for APA102 strip
    fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CRGB::Blue);
}

void pulseEffect() {
    uint8_t brightness = beatsin8(30, 50, 255);
    fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CHSV(160, 255, brightness));
}

void confetti() {
    // CENTER ORIGIN CONFETTI - ALL effects MUST originate from CENTER LEDs 79/80
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 10);
    
    // Spawn confetti ONLY at center LEDs 79/80 (MANDATORY CENTER ORIGIN)
    if (random8() < 80) {
        int centerPos = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80 only
        // Use palette colors instead of rainbow (gHue is FORBIDDEN)
        uint8_t paletteIndex = random8();
        leds[centerPos] += ColorFromPalette(currentPalette, paletteIndex, 255);
    }
    
    // Move confetti outward from center with fading
    for (int i = HardwareConfig::STRIP_CENTER_POINT - 1; i >= 0; i--) {
        if (leds[i+1]) {
            leds[i] = leds[i+1];
            leds[i].fadeToBlackBy(25);
        }
    }
    for (int i = HardwareConfig::STRIP_CENTER_POINT + 2; i < HardwareConfig::NUM_LEDS; i++) {
        if (leds[i-1]) {
            leds[i] = leds[i-1];
            leds[i].fadeToBlackBy(25);
        }
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN CONFETTI
void stripConfetti() {
    // CENTER ORIGIN CONFETTI - Sparks spawn at center LEDs 79/80 and fade as they move outward
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 10);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 10);
    
    // Spawn new confetti at CENTER (LEDs 79/80)
    if (random8() < 80) {
        int centerPos = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        // Use palette colors instead of rainbow (gHue is FORBIDDEN)
        uint8_t paletteIndex = random8();
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        strip1[centerPos] += color;
        if (centerPos < HardwareConfig::STRIP2_LED_COUNT) {
            strip2[centerPos] += color;
        }
    }
    
    // Move existing confetti outward from center with fading
    for (int i = HardwareConfig::STRIP_CENTER_POINT - 1; i >= 0; i--) {
        if (strip1[i+1]) {
            strip1[i] = strip1[i+1];
            strip1[i].fadeToBlackBy(30);
            if (i < HardwareConfig::STRIP2_LED_COUNT && (i+1) < HardwareConfig::STRIP2_LED_COUNT) {
                strip2[i] = strip2[i+1];
                strip2[i].fadeToBlackBy(30);
            }
        }
    }
    for (int i = HardwareConfig::STRIP_CENTER_POINT + 1; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (strip1[i-1]) {
            strip1[i] = strip1[i-1];
            strip1[i].fadeToBlackBy(30);
            if (i < HardwareConfig::STRIP2_LED_COUNT && (i-1) < HardwareConfig::STRIP2_LED_COUNT) {
                strip2[i] = strip2[i-1];
                strip2[i].fadeToBlackBy(30);
            }
        }
    }
}

void sinelon() {
    // CENTER ORIGIN SINELON - Oscillates outward from center LEDs 79/80
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);
    
    // Oscillate from center outward (0 to STRIP_HALF_LENGTH)
    int distFromCenter = beatsin16(13, 0, HardwareConfig::STRIP_HALF_LENGTH);
    
    // Set both sides of center
    int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
    int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
    
    // Use palette colors instead of rainbow (gHue is FORBIDDEN)
    static uint8_t baseIndex = 0;
    baseIndex++; // Slow color drift within palette

    if (pos1 < HardwareConfig::STRIP_LENGTH) {
        CRGB color = ColorFromPalette(currentPalette, baseIndex, 192);
        strip1[pos1] += color;
        strip2[pos1] += color;
    }
    if (pos2 >= 0) {
        CRGB color = ColorFromPalette(currentPalette, baseIndex + 128, 192);
        strip1[pos2] += color;
        strip2[pos2] += color;
    }
}

void juggle() {
    // CENTER ORIGIN JUGGLE - ALL effects MUST originate from CENTER LEDs 79/80
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        // Oscillate from center outward (MANDATORY CENTER ORIGIN)
        int distFromCenter = beatsin16(i+7, 0, HardwareConfig::STRIP_HALF_LENGTH);
        
        // Set positions on both sides of center (79/80)
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        CRGB color = CHSV(dothue, 200, 255);
        
        if (pos1 < HardwareConfig::NUM_LEDS) {
            leds[pos1] |= color;
        }
        if (pos2 >= 0) {
            leds[pos2] |= color;
        }
        
        dothue += 32;
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN JUGGLE
void stripJuggle() {
    // CENTER ORIGIN JUGGLE - Multiple dots oscillate outward from center LEDs 79/80
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);
    
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        // Oscillate from center outward (0 to STRIP_HALF_LENGTH)
        int distFromCenter = beatsin16(i+7, 0, HardwareConfig::STRIP_HALF_LENGTH);
        
        // Set positions on both sides of center
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        CRGB color = CHSV(dothue, 200, 255);
        
        if (pos1 < HardwareConfig::STRIP_LENGTH) {
            strip1[pos1] |= color;
            strip2[pos1] |= color;
        }
        if (pos2 >= 0) {
            strip1[pos2] |= color;
            strip2[pos2] |= color;
        }
        
        dothue += 32;
    }
}

void bpm() {
    // CENTER ORIGIN BPM - ALL effects MUST originate from CENTER LEDs 79/80
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    
    // Calculate distance from center for each LED (MANDATORY CENTER ORIGIN)
    for(int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Intensity decreases with distance from center
        uint8_t intensity = beat - (distFromCenter * 3);
        intensity = max(intensity, (uint8_t)32); // Minimum brightness

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2);

        // Get color at full brightness, then scale - preserves saturation
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(intensity);
        leds[i] = color;
    }
}

// ============== ADVANCED WAVE EFFECTS ==============

void waveEffect() {
    // === NARRATIVE ENGINE: Query global dramatic timing ===
#if FEATURE_NARRATIVE_ENGINE
    float narrativeIntensity = NarrativeEngine::getInstance().getIntensity();
#else
    float narrativeIntensity = 1.0f;  // Full brightness when engine disabled
#endif
    
    // CENTER ORIGIN WAVES - Start from center LEDs 79/80 and propagate outward
    static uint32_t wavePosition = 0;  // Changed to uint32_t to prevent overflow
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, fadeAmount);
    
    uint16_t waveSpeed = map(paletteSpeed, 1, 50, 100, 10);
    wavePosition += waveSpeed;
    
    // Prevent overflow by wrapping
    if (wavePosition > 65535) {
        wavePosition = wavePosition % 65536;
    }
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Wave propagates outward from center
        uint8_t brightness = sin8((distFromCenter * 15) + (wavePosition >> 4));
        
        // === NARRATIVE MODULATION: Scale brightness by cycle intensity ===
        // Minimum 20% brightness during REST, full range during BUILD→HOLD→RELEASE
        brightness = (uint8_t)(brightness * (0.2f + 0.8f * narrativeIntensity));
        
        uint8_t colorIndex = (distFromCenter * 8) + (wavePosition >> 6);
        
        CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

void rippleEffect() {
    // CENTER ORIGIN RIPPLES - Always start from CENTER LEDs 79/80 and move outward
    static struct {
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    } ripples[5];
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, fadeAmount);
    
    // Spawn new ripples at CENTER ONLY - frequency controlled by complexity
    uint8_t spawnChance = 30 * visualParams.getComplexityNorm();
    if (random8() < spawnChance) {
        for (uint8_t i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].radius = 0;
                ripples[i].speed = (0.5f + (random8() / 255.0f) * 2.0f) * visualParams.getIntensityNorm();
                ripples[i].hue = random8();
                ripples[i].active = true;
                break;
            }
        }
    }
    
    // Update and render CENTER ORIGIN ripples
    for (uint8_t r = 0; r < 5; r++) {
        if (!ripples[r].active) continue;
        
        ripples[r].radius += ripples[r].speed * (paletteSpeed / 10.0f);
        
        if (ripples[r].radius > HardwareConfig::STRIP_HALF_LENGTH) {
            ripples[r].active = false;
            continue;
        }
        
        // Draw ripple moving outward from center
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            float wavePos = distFromCenter - ripples[r].radius;
            
            if (abs(wavePos) < 3.0f) {
                uint8_t brightness = 255 - (abs(wavePos) * 85);
                brightness = (brightness * (HardwareConfig::STRIP_HALF_LENGTH - ripples[r].radius)) / HardwareConfig::STRIP_HALF_LENGTH;
                brightness = brightness * visualParams.getIntensityNorm();
                
                // Get color at full brightness, then scale - preserves saturation
                CRGB color = ColorFromPalette(currentPalette, ripples[r].hue + distFromCenter, 255);
                color.nscale8(brightness);
                strip1[i] += color;
                strip2[i] += color;
            }
        }
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN INTERFERENCE
void stripInterference() {
    // CENTER ORIGIN INTERFERENCE - Waves emanate from center LEDs 79/80 and interfere
    static float wave1Phase = 0;
    static float wave2Phase = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, fadeAmount);
    
    wave1Phase += paletteSpeed / 20.0f;
    wave2Phase -= paletteSpeed / 30.0f;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Two waves emanating from center with different frequencies
        // Using fast lookup tables instead of native sin()
        float wave1 = TrigLookup::sinf_lookup(normalizedDist * PI * 4 + wave1Phase) * 127 + 128;
        float wave2 = TrigLookup::sinf_lookup(normalizedDist * PI * 6 + wave2Phase) * 127 + 128;

        // Interference pattern from CENTER ORIGIN
        float interference = (wave1 + wave2) / 2.0f;
        uint8_t brightness = interference;

        // Hue varies with distance from center
        uint8_t hue = (uint8_t)(wave1Phase * 20) + (distFromCenter * 8);

        CRGB color = ColorFromPalette(currentPalette, hue, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN BPM
void stripBPM() {
    // CENTER ORIGIN BPM - Pulses emanate from center LEDs 79/80
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t colorIndex = (uint8_t)(distFromCenter * 2);
        uint8_t brightness = beat + (uint8_t)(distFromCenter * 3);

        // Get color at full brightness, then scale - preserves saturation
        CRGB color = ColorFromPalette(currentPalette, colorIndex, 255);
        color.nscale8(brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN PLASMA
void stripPlasma() {
    // CENTER ORIGIN PLASMA - Plasma field generated from center LEDs 79/80
    static uint32_t time = 0;
    time += paletteSpeed;
    
    // Prevent overflow
    if (time > 65535) {
        time = time % 65536;
    }
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Plasma calculations from center outward (using fast lookup)
        float v1 = TrigLookup::sinf_lookup(normalizedDist * 8.0f + time / 100.0f);
        float v2 = TrigLookup::sinf_lookup(normalizedDist * 5.0f - time / 150.0f);
        float v3 = TrigLookup::sinf_lookup(normalizedDist * 3.0f + time / 200.0f);

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)((v1 + v2 + v3) * 10.0f + 15.0f) + (uint8_t)(distFromCenter * 2);
        uint8_t brightness = (uint8_t)((v1 + v2) * 63.75f + 191.25f);

        // Get color at full brightness, then scale - preserves saturation
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============== MATHEMATICAL PATTERNS ==============

void plasma() {
    // CENTER ORIGIN PLASMA - Uses distance from center for symmetric pattern
    static uint32_t time = 0;
    time += paletteSpeed;

    // Prevent overflow
    if (time > 65535) {
        time = time % 65536;
    }

    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        // CENTER ORIGIN: Use distance from center instead of linear position
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);

        // Plasma waves radiate from center (using fast lookup)
        float v1 = TrigLookup::sinf_lookup(distFromCenter / 8.0f + time / 100.0f);
        float v2 = TrigLookup::sinf_lookup(distFromCenter / 5.0f - time / 150.0f);
        float v3 = TrigLookup::sinf_lookup(distFromCenter / 3.0f + time / 200.0f);

        // Use palette colors instead of rainbow (gHue is FORBIDDEN)
        uint8_t paletteIndex = (uint8_t)((v1 + v2 + v3) * 42.5f + 127.5f) + (uint8_t)(distFromCenter * 2);
        uint8_t brightness = (uint8_t)((v1 + v2) * 63.75f + 191.25f);

        // Get color at full brightness, then scale - preserves saturation
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);
        leds[i] = color;
    }
}

// ============== NATURE-INSPIRED EFFECTS ==============

void fire() {
    // CENTER ORIGIN FIRE - Starts from center LEDs 79/80 and spreads outward
    static byte heat[HardwareConfig::STRIP_LENGTH];
    
    // Cool down every cell a little
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / HardwareConfig::STRIP_LENGTH) + 2));
    }
    
    // Heat diffuses from center outward
    for(int k = 1; k < HardwareConfig::STRIP_LENGTH - 1; k++) {
        heat[k] = (heat[k - 1] + heat[k] + heat[k + 1]) / 3;
    }
    
    // Ignite new sparks at CENTER (79/80) - intensity controls spark frequency and heat
    float intensityNorm = visualParams.getIntensityNorm();
    if (intensityNorm < 0.1f) intensityNorm = 0.1f; // Ensure minimum activity
    uint8_t sparkChance = 120 * intensityNorm;
    if(random8() < sparkChance) {
        int center = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        uint8_t heatAmount = 160 + (95 * intensityNorm);
        heat[center] = qadd8(heat[center], random8(160, heatAmount));
    }
    
    // Map heat to both strips with CENTER ORIGIN
    for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
        // Scale heat by intensity - HeatColor already returns vibrant colors
        uint8_t scaledHeat = heat[j] * visualParams.getIntensityNorm();
        CRGB color = HeatColor(scaledHeat);

        strip1[j] = color;

        // Only write to strip2 within its bounds (40 LEDs)
        if (j < HardwareConfig::STRIP2_LED_COUNT) {
            strip2[j] = color;
        }
    }
}

void ocean() {
    // CENTER ORIGIN: Ocean waves emanate from center
    static uint32_t waterOffset = 0;
    waterOffset += paletteSpeed / 2;

    // Prevent overflow
    if (waterOffset > 65535) {
        waterOffset = waterOffset % 65536;
    }

    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        // Calculate distance from center for symmetry
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);

        // Create wave-like motion from center
        uint8_t wave1 = sin8((distFromCenter * 10) + waterOffset);
        uint8_t wave2 = sin8((distFromCenter * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;

        // Ocean colors from deep blue to cyan
        uint8_t hue = 160 + (combinedWave >> 3);  // Blue range
        uint8_t brightness = 100 + (combinedWave >> 1);
        uint8_t saturation = 255 - (combinedWave >> 2);

        leds[i] = CHSV(hue, saturation, brightness);
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN OCEAN
void stripOcean() {
    // CENTER ORIGIN OCEAN - Waves emanate from center LEDs 79/80
    static uint32_t waterOffset = 0;
    waterOffset += paletteSpeed / 2;
    
    // Prevent overflow
    if (waterOffset > 65535) {
        waterOffset = waterOffset % 65536;
    }
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Create wave-like motion from center outward
        uint8_t wave1 = sin8((distFromCenter * 10) + waterOffset);
        uint8_t wave2 = sin8((distFromCenter * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Ocean colors from deep blue to cyan
        uint8_t hue = 160 + (combinedWave >> 3);  // Blue range
        uint8_t brightness = 100 + (combinedWave >> 1);
        uint8_t saturation = 255 - (combinedWave >> 2);
        
        CRGB color = CHSV(hue, saturation, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============== NEW CENTER ORIGIN EFFECTS ==============

// HEARTBEAT - Pulses emanate from center like a beating heart
void heartbeatEffect() {
    static float phase = 0;
    static float lastBeat = 0;

    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);

    // Heartbeat rhythm: lub-dub... lub-dub... (using fast lookup)
    float beatPattern = TrigLookup::sinf_lookup(phase) + TrigLookup::sinf_lookup(phase * 2.1f) * 0.4f;
    
    if (beatPattern > 1.8f && phase - lastBeat > 2.0f) {
        lastBeat = phase;
        // Generate pulse from CENTER
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
            
            // Position-based palette index (no gHue - rainbow cycling forbidden)
            uint8_t paletteIndex = (uint8_t)(distFromCenter * 2);
            uint8_t brightness = 255 * (1.0f - normalizedDist);

            // Get color at full brightness, then scale - preserves saturation
            CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
            color.nscale8(brightness);

            strip1[i] += color;
            strip2[i] += color;
        }
    }
    
    phase += paletteSpeed / 200.0f;
}

// BREATHING - Smooth expansion and contraction from center
void breathingEffect() {
    static float breathPhase = 0;

    // Smooth breathing curve (using fast lookup)
    float breath = (TrigLookup::sinf_lookup(breathPhase) + 1.0f) / 2.0f;
    float radius = breath * HardwareConfig::STRIP_HALF_LENGTH;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 15);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 15);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        if (distFromCenter <= radius) {
            float intensity = 1.0f - (distFromCenter / radius) * 0.5f;
            uint8_t brightness = 255 * intensity * breath;

            // Position-based palette index (no gHue - rainbow cycling forbidden)
            uint8_t paletteIndex = (uint8_t)(distFromCenter * 3);

            // Get color at full brightness, then scale - preserves saturation
            CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
            color.nscale8(brightness);
            strip1[i] = color;
            strip2[i] = color;
        }
    }
    
    breathPhase += paletteSpeed / 100.0f;
}

// SHOCKWAVE - Explosive rings emanate from center
void shockwaveEffect() {
    static float shockwaves[5] = {-1, -1, -1, -1, -1};
    static uint8_t waveHues[5] = {0};
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 25);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 25);
    
    // Spawn new shockwave from CENTER - complexity controls frequency
    // Use random palette index instead of gHue (rainbow cycling forbidden)
    static uint8_t paletteOffset = 0;
    paletteOffset += 7;  // Slowly rotate through palette

    uint8_t spawnChance = 20 * visualParams.getComplexityNorm();
    if (random8() < spawnChance) {
        for (int w = 0; w < 5; w++) {
            if (shockwaves[w] < 0) {
                shockwaves[w] = 0;
                waveHues[w] = paletteOffset + random8(64);  // No gHue
                break;
            }
        }
    }
    
    // Update and render shockwaves
    for (int w = 0; w < 5; w++) {
        if (shockwaves[w] >= 0) {
            float t = shockwaves[w] / (float)HardwareConfig::STRIP_HALF_LENGTH;
            float speedScale = 1.0f - 0.7f * Easing::ease(t, EASE_OUT_QUAD);
            shockwaves[w] += (paletteSpeed / 20.0f) * visualParams.getIntensityNorm() * speedScale;
            
            if (shockwaves[w] > HardwareConfig::STRIP_HALF_LENGTH) {
                shockwaves[w] = -1;
                continue;
            }
            
            // Render shockwave ring
            for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
                float ringDist = abs(distFromCenter - shockwaves[w]);
                
                // Ring thickness controlled by complexity
                float ringThickness = 3.0f + (3.0f * visualParams.getComplexityNorm());
                if (ringDist < ringThickness) {
                    float intensity = 1.0f - (ringDist / ringThickness);
                    float decay = 1.0f - Easing::ease(t, EASE_OUT_CUBIC);
                    uint8_t brightness = 255 * intensity * decay;
                    brightness = brightness * visualParams.getIntensityNorm();
                    
                    // Get color at full brightness, then scale - preserves saturation
                    CRGB color = ColorFromPalette(currentPalette, waveHues[w], 255);
                    color.nscale8(brightness);
                    strip1[i] += color;
                    strip2[i] += color;
                }
            }
        }
    }
}

// VORTEX - Spiral patterns emanating from center
void vortexEffect() {
    static float vortexAngle = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Spiral calculation (using fast lookup)
        float spiralOffset = normalizedDist * 8.0f + vortexAngle;
        float intensity = TrigLookup::sinf_lookup(spiralOffset) * 0.5f + 0.5f;
        intensity *= (1.0f - normalizedDist * 0.5f); // Fade towards edges

        uint8_t brightness = 255 * intensity;

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 5 + vortexAngle * 20);

        // Get color at full brightness, then scale - preserves saturation
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        // Opposite spiral direction for strip2
        if (i < HardwareConfig::STRIP_CENTER_POINT) {
            strip1[i] = color;
            strip2[HardwareConfig::STRIP_LENGTH - 1 - i] = color;
        } else {
            strip1[i] = color;
            strip2[HardwareConfig::STRIP_LENGTH - 1 - i] = color;
        }
    }
    
    vortexAngle += paletteSpeed / 50.0f;
}

// COLLISION - Particles shoot from edges to center and explode
void collisionEffect() {
    static bool exploding = false;
    static float approachT = 0.0f;
    static uint32_t holdStartMs = 0;
    static float explosionT = 0.0f;
    static uint8_t collisionPaletteBase = 0;  // Palette base for colors (no gHue - rainbow forbidden)

    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 30);

    if (!exploding) {
        float step = (0.01f + (paletteSpeed / 255.0f) * 0.05f) * (0.5f + visualParams.getIntensityNorm());
        approachT = Easing::clamp01(approachT + step);

        float eased = Easing::ease(approachT, EASE_IN_OUT_CUBIC);
        float particle1Pos = eased * HardwareConfig::STRIP_CENTER_POINT;
        float particle2Pos = (HardwareConfig::STRIP_LENGTH - 1) - eased * (HardwareConfig::STRIP_LENGTH - 1 - HardwareConfig::STRIP_CENTER_POINT);

        // Draw particles - use palette-based colors
        for (int trail = 0; trail < 10; trail++) {
            int pos1 = (int)particle1Pos - trail;
            int pos2 = (int)particle2Pos + trail;

            if (pos1 >= 0 && pos1 < HardwareConfig::STRIP_LENGTH) {
                uint8_t brightness = 255 - (trail * 25);
                // Get colors at full brightness, then scale
                CRGB c1 = ColorFromPalette(currentPalette, collisionPaletteBase, 255);
                CRGB c2 = ColorFromPalette(currentPalette, collisionPaletteBase + 128, 255);
                c1.nscale8(brightness);
                c2.nscale8(brightness);
                strip1[pos1] = c1;
                strip2[pos1] = c2;
            }

            if (pos2 >= 0 && pos2 < HardwareConfig::STRIP_LENGTH) {
                uint8_t brightness = 255 - (trail * 25);
                CRGB c1 = ColorFromPalette(currentPalette, collisionPaletteBase + 128, 255);
                CRGB c2 = ColorFromPalette(currentPalette, collisionPaletteBase, 255);
                c1.nscale8(brightness);
                c2.nscale8(brightness);
                strip1[pos2] = c1;
                strip2[pos2] = c2;
            }
        }
        
        uint32_t holdDurationMs = 80 + (uint32_t)(400.0f * visualParams.getComplexityNorm());
        if (approachT >= 1.0f) {
            if (holdStartMs == 0) holdStartMs = millis();
            uint32_t heldMs = millis() - holdStartMs;
            float holdT = holdDurationMs > 0 ? (float)heldMs / (float)holdDurationMs : 1.0f;
            float beat = Easing::arc(holdT, 0.3f, 0.4f, 0.3f, EASE_OUT_CUBIC, EASE_IN_CUBIC);

            for (int i = HardwareConfig::STRIP_CENTER_POINT - 2; i <= HardwareConfig::STRIP_CENTER_POINT + 2; i++) {
                if (i >= 0 && i < HardwareConfig::STRIP_LENGTH) {
                    uint8_t b = (uint8_t)(255.0f * beat);
                    // Position-based palette index (no gHue - rainbow cycling forbidden)
                    uint8_t paletteIdx = collisionPaletteBase + (uint8_t)((i - HardwareConfig::STRIP_CENTER_POINT) * 10);
                    CRGB c = ColorFromPalette(currentPalette, paletteIdx, 255);
                    c.nscale8(b);
                    strip1[i] += c;
                    strip2[i] += c;
                }
            }

            if (holdT >= 1.0f) {
                exploding = true;
                explosionT = 0.0f;
                holdStartMs = 0;
                approachT = 0.0f;
            }
        }
    } else {
        // Explosion expanding from center
        float step = (0.015f + (paletteSpeed / 255.0f) * 0.06f) * (0.6f + visualParams.getIntensityNorm());
        explosionT = Easing::clamp01(explosionT + step);
        float explosionRadius = (HardwareConfig::STRIP_HALF_LENGTH + 10.0f) * Easing::ease(explosionT, EASE_OUT_CUBIC);
        float envelope = 1.0f - Easing::ease(explosionT, EASE_OUT_QUAD);
        
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            
            if (distFromCenter <= explosionRadius && distFromCenter >= explosionRadius - 10) {
                float intensity = 1.0f - ((distFromCenter - (explosionRadius - 10)) / 10.0f);
                uint8_t brightness = 255 * intensity * envelope;

                // Position-based palette index (no gHue - rainbow cycling forbidden)
                uint8_t paletteIdx = collisionPaletteBase + random8(64);
                CRGB color = ColorFromPalette(currentPalette, paletteIdx, 255);
                color.nscale8(brightness);
                strip1[i] += color;
                strip2[i] += color;
            }
        }
        
        // Reset when explosion reaches edges
        if (explosionT >= 1.0f) {
            exploding = false;
            approachT = 0.0f;
            holdStartMs = 0;
            explosionT = 0.0f;
        }
    }
}

// GRAVITY WELL - Everything gets pulled toward center
void gravityWellEffect() {
    static struct GravityParticle {
        float position;
        float velocity;
        uint8_t hue;
        bool active;
    } particles[20];
    
    static bool initialized = false;
    if (!initialized) {
        for (int p = 0; p < 20; p++) {
            particles[p].position = random16(HardwareConfig::STRIP_LENGTH);
            particles[p].velocity = 0;
            particles[p].hue = random8();
            particles[p].active = true;
        }
        initialized = true;
    }
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);
    
    // Update particles with gravity toward center
    for (int p = 0; p < 20; p++) {
        if (particles[p].active) {
            float distFromCenter = particles[p].position - HardwareConfig::STRIP_CENTER_POINT;
            float gravity = -distFromCenter * 0.01f * paletteSpeed / 10.0f;
            
            particles[p].velocity += gravity;
            particles[p].velocity *= 0.95f; // Damping
            particles[p].position += particles[p].velocity;
            
            // Respawn at edges if particle reaches center
            if (abs(particles[p].position - HardwareConfig::STRIP_CENTER_POINT) < 2) {
                particles[p].position = random8(2) ? 0 : HardwareConfig::STRIP_LENGTH - 1;
                particles[p].velocity = 0;
                particles[p].hue = random8();
            }
            
            // Draw particle with motion blur
            int pos = particles[p].position;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                strip1[pos] += ColorFromPalette(currentPalette, particles[p].hue, 255);
                strip2[pos] += ColorFromPalette(currentPalette, particles[p].hue + 64, 255);
                
                // Motion blur
                for (int blur = 1; blur < 4; blur++) {
                    int blurPos = pos - (particles[p].velocity > 0 ? blur : -blur);
                    if (blurPos >= 0 && blurPos < HardwareConfig::STRIP_LENGTH) {
                        uint8_t brightness = 255 / (blur + 1);
                        strip1[blurPos] += ColorFromPalette(currentPalette, particles[p].hue, brightness);
                        strip2[blurPos] += ColorFromPalette(currentPalette, particles[p].hue + 64, brightness);
                    }
                }
            }
        }
    }
}

// ============== EFFECT REGISTRATION ==============

#include "../basic/BasicEffects.h"

void StripEffects::registerAll(FxEngine& engine) {
    // Register basic strip effects
    engine.addEffect("Solid Color", solidColor);
    engine.addEffect("Pulse", pulseEffect);
    engine.addEffect("Confetti", stripConfetti);
    engine.addEffect("Sinelon", sinelon);
    engine.addEffect("Juggle", stripJuggle);
    engine.addEffect("BPM", stripBPM);
    
    // Register wave effects
    engine.addEffect("Wave", waveEffect);
    engine.addEffect("Ripple", rippleEffect);
    engine.addEffect("Interference", stripInterference);
    engine.addEffect("Plasma", stripPlasma);
    
    // Register nature effects
    engine.addEffect("Fire", fire);
    engine.addEffect("Ocean", stripOcean);
    
    // Register center-origin effects
    engine.addEffect("Heartbeat", heartbeatEffect);
    engine.addEffect("Breathing", breathingEffect);
    engine.addEffect("Shockwave", shockwaveEffect);
    engine.addEffect("Vortex", vortexEffect);
    engine.addEffect("Collision", collisionEffect);
    engine.addEffect("Gravity Well", gravityWellEffect);
    
    // Register LGP interference effects
    engine.addEffect("LGP Box Wave", lgpBoxWave);
    engine.addEffect("LGP Holographic", lgpHolographic);
    engine.addEffect("LGP Modal Resonance", lgpModalResonance);
    engine.addEffect("LGP Interference Scanner", lgpInterferenceScanner);
    engine.addEffect("LGP Wave Collision", lgpWaveCollision);
    
    // Register LGP geometric effects
    engine.addEffect("LGP Diamond Lattice", lgpDiamondLattice);
    engine.addEffect("LGP Hexagonal Grid", lgpHexagonalGrid);
    engine.addEffect("LGP Spiral Vortex", lgpSpiralVortex);
    engine.addEffect("LGP Sierpinski", lgpSierpinskiTriangles);
    engine.addEffect("LGP Chevron Waves", lgpChevronWaves);
    engine.addEffect("LGP Concentric Rings", lgpConcentricRings);
    engine.addEffect("LGP Star Burst", lgpStarBurst);
    engine.addEffect("LGP Mesh Network", lgpMeshNetwork);
    
    // Register LGP advanced effects
    engine.addEffect("LGP Moiré Curtains", lgpMoireCurtains);
    engine.addEffect("LGP Radial Ripple", lgpRadialRipple);
    engine.addEffect("LGP Holographic Vortex", lgpHolographicVortex);
    engine.addEffect("LGP Evanescent Drift", lgpEvanescentDrift);
    engine.addEffect("LGP Chromatic Shear", lgpChromaticShear);
    engine.addEffect("LGP Modal Cavity", lgpModalCavity);
    engine.addEffect("LGP Fresnel Zones", lgpFresnelZones);
    engine.addEffect("LGP Photonic Crystal", lgpPhotonicCrystal);
    
    // Register LGP organic effects
    engine.addEffect("LGP Aurora Borealis", lgpAuroraBorealis);
    engine.addEffect("LGP Bioluminescent", lgpBioluminescentWaves);
    engine.addEffect("LGP Plasma Membrane", lgpPlasmaMembrane);
    engine.addEffect("LGP Neural Network", lgpNeuralNetwork);
    engine.addEffect("LGP Crystal Growth", lgpCrystallineGrowth);
    engine.addEffect("LGP Fluid Dynamics", lgpFluidDynamics);
    
    // Register LGP color mixing effects
    engine.addEffect("LGP Color Temperature", lgpColorTemperature);
    engine.addEffect("LGP RGB Prism", lgpRGBPrism);
    engine.addEffect("LGP Complementary Mix", lgpComplementaryMixing);
    engine.addEffect("LGP Additive/Subtractive", lgpAdditiveSubtractive);
    engine.addEffect("LGP Quantum Colors", lgpQuantumColors);
    engine.addEffect("LGP Doppler Shift", lgpDopplerShift);
    engine.addEffect("LGP Chromatic Aberration", lgpChromaticAberration);
    engine.addEffect("LGP HSV Cylinder", lgpHSVCylinder);
    engine.addEffect("LGP Perceptual Blend", lgpPerceptualBlend);
    engine.addEffect("LGP Metameric Colors", lgpMetamericColors);
    engine.addEffect("LGP Color Accelerator", lgpColorAccelerator);
    engine.addEffect("LGP DNA Helix", lgpDNAHelix);
    engine.addEffect("LGP Phase Transition", lgpPhaseTransition);
    
#if FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC
    // Register LGP audio-reactive effects
    engine.addEffect("LGP Frequency Collision", lgpFrequencyCollision);
    engine.addEffect("LGP Beat Interference", lgpBeatInterference);
    engine.addEffect("LGP Spectral Morphing", lgpSpectralMorphing);
    engine.addEffect("LGP Audio Quantum", lgpAudioQuantumCollapse);
    engine.addEffect("LGP Rhythm Waves", lgpRhythmWaves);
    engine.addEffect("LGP Envelope Interference", lgpEnvelopeInterference);
    engine.addEffect("LGP Kick Shockwave", lgpKickShockwave);
    engine.addEffect("LGP FFT Color Map", lgpFFTColorMap);
    engine.addEffect("LGP Harmonic Resonance", lgpHarmonicResonance);
    engine.addEffect("LGP Stereo Phase", lgpStereoPhasePattern);
#endif
}
