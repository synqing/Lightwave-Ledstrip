#include "StripEffects.h"

// ============== BASIC EFFECTS ==============

void solidColor() {
    // TEST: Re-enable orchestrator with double-brightness fix
    uint8_t brightness = getEmotionalBrightness(255);
    CRGB color = getOrchestratedColor(128, brightness);
    fill_solid(strip1, HardwareConfig::STRIP_LENGTH, color);
    fill_solid(strip2, HardwareConfig::STRIP_LENGTH, color);
}

void pulseEffect() {
    // TEMP FIX: Use legacy palette like Strip BPM (which works)
    uint8_t baseBrightness = beatsin8(30, 50, 255);
    uint8_t emotionalBrightness = getEmotionalBrightness(baseBrightness);
    CRGB color = ColorFromPalette(currentPalette, gHue, emotionalBrightness);
    fill_solid(strip1, HardwareConfig::STRIP_LENGTH, color);
    fill_solid(strip2, HardwareConfig::STRIP_LENGTH, color);
}

void confetti() {
    // CENTER ORIGIN CONFETTI - ALL effects MUST originate from CENTER LEDs 79/80
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 10);
    
    // ORCHESTRATOR INTEGRATION: Emotional intensity controls spawn rate
    uint8_t emotionalSpawnRate = 40 + (40 * colorOrchestrator.getEmotionalIntensity());
    
    // Spawn confetti ONLY at center LEDs 79/80 (MANDATORY CENTER ORIGIN)
    if (random8() < emotionalSpawnRate) {
        int centerPos = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80 only
        // Use orchestrated color with emotional modulation
        uint8_t brightness = getEmotionalBrightness(255);
        CRGB emotionalColor = getOrchestratedColor(gHue + random8(64), brightness);
        leds[centerPos] += emotionalColor;
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
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 10);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 10);
    
    // Emotional intensity controls spawn rate
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    uint8_t spawnRate = 40 + (emotionalIntensity * 40); // 40-80 based on emotion
    
    // Spawn new confetti at CENTER (LEDs 79/80)
    if (random8() < spawnRate) {
        int centerPos = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        CRGB color = getOrchestratedColor(gHue + random8(64), getEmotionalBrightness(255));
        strip1[centerPos] += color;
        strip2[centerPos] += color;
    }
    
    // Move existing confetti outward from center with fading
    for (int i = HardwareConfig::STRIP_CENTER_POINT - 1; i >= 0; i--) {
        if (strip1[i+1]) {
            strip1[i] = strip1[i+1];
            strip1[i].fadeToBlackBy(30);
            strip2[i] = strip2[i+1];
            strip2[i].fadeToBlackBy(30);
        }
    }
    for (int i = HardwareConfig::STRIP_CENTER_POINT + 1; i < HardwareConfig::STRIP_LENGTH; i++) {
        if (strip1[i-1]) {
            strip1[i] = strip1[i-1];
            strip1[i].fadeToBlackBy(30);
            strip2[i] = strip2[i-1];
            strip2[i].fadeToBlackBy(30);
        }
    }
}

void sinelon() {
    // CENTER ORIGIN SINELON - Oscillates outward from center LEDs 79/80
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Oscillate from center outward (0 to STRIP_HALF_LENGTH)
    int distFromCenter = beatsin16(13, 0, HardwareConfig::STRIP_HALF_LENGTH);
    
    // Set both sides of center
    int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
    int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
    
    uint8_t emotionalBrightness = getEmotionalBrightness(192);
    
    if (pos1 < HardwareConfig::STRIP_LENGTH) {
        // TEMP FIX: Use legacy palette like Strip BPM (which works)
        CRGB color1 = ColorFromPalette(currentPalette, gHue, emotionalBrightness);
        strip1[pos1] += color1;
        strip2[pos1] += color1;
    }
    if (pos2 >= 0) {
        // TEMP FIX: Use legacy palette like Strip BPM (which works)  
        CRGB color2 = ColorFromPalette(currentPalette, gHue + 128, emotionalBrightness);
        strip1[pos2] += color2;
        strip2[pos2] += color2;
    }
}

void juggle() {
    // CENTER ORIGIN JUGGLE - ALL effects MUST originate from CENTER LEDs 79/80
    fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, 20);
    
    uint8_t dothue = 0;
    uint8_t emotionalBrightness = getEmotionalBrightness(255);
    
    for(int i = 0; i < 8; i++) {
        // Oscillate from center outward (MANDATORY CENTER ORIGIN)
        int distFromCenter = beatsin16(i+7, 0, HardwareConfig::STRIP_HALF_LENGTH);
        
        // Set positions on both sides of center (79/80)
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        CRGB color = getOrchestratedColor(dothue, emotionalBrightness);
        
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
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    uint8_t dothue = 0;
    uint8_t emotionalBrightness = getEmotionalBrightness(255);
    
    for(int i = 0; i < 8; i++) {
        // Oscillate from center outward (0 to STRIP_HALF_LENGTH)
        int distFromCenter = beatsin16(i+7, 0, HardwareConfig::STRIP_HALF_LENGTH);
        
        // Set positions on both sides of center
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        CRGB color = getOrchestratedColor(dothue, emotionalBrightness);
        
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
    // CENTER ORIGIN BPM - Optimized with pre-calculated distances
    uint8_t BeatsPerMinute = 62;
    
    // Emotional intensity affects the beat strength
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    uint8_t baseBeat = 64 + (emotionalIntensity * 64); // 64-128 based on emotion
    uint8_t beat = beatsin8(BeatsPerMinute, baseBeat, 255);
    
    // Use pre-calculated distance lookup
    extern uint8_t distanceFromCenter[];
    
    // Process in strips for better cache locality
    for(int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        uint8_t dist = distanceFromCenter[i];
        
        // Intensity decreases with distance - use integer math
        uint16_t intensity = beat - ((dist * 3) >> 2);  // Approximate * 0.75
        intensity = max(intensity, (uint16_t)32);
        
        // Use orchestrated color with emotional brightness
        uint8_t colorIndex = gHue + (dist >> 1);
        CRGB baseColor = getOrchestratedColorFast(colorIndex, 255);
        leds[i] = baseColor.scale8(intensity);
    }
}

// ============== ADVANCED WAVE EFFECTS ==============

void waveEffect() {
    // CENTER ORIGIN WAVES - Optimized with lookup table
    static uint16_t wavePosition = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    // Emotional intensity affects wave speed
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    // FIXED: Correct speed mapping - low speed = slow movement
    uint16_t baseSpeed = map(paletteSpeed, 1, 50, 10, 200);  // 1=slow, 50=fast
    uint16_t waveSpeed = baseSpeed + (emotionalIntensity * baseSpeed * 0.5f); // Up to 1.5x speed
    wavePosition += waveSpeed;
    
    // Use pre-calculated distance lookup
    extern uint8_t distanceFromCenter[];
    
    // Process both strips with optimized calculations
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t dist = distanceFromCenter[i];
        
        // Wave propagates outward - use wave LUT
        extern uint8_t wavePatternLUT[256];
        
        uint8_t waveIdx = ((dist * 15) >> 3) + (wavePosition >> 4);
        uint8_t brightness = wavePatternLUT[waveIdx];
        uint8_t colorIndex = (dist << 3) + (wavePosition >> 6);  // dist * 8
        
        // Use orchestrated color with emotional brightness
        uint8_t emotionalBrightness = getEmotionalBrightness(brightness);
        CRGB color = getOrchestratedColorFast(colorIndex, emotionalBrightness);
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
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    // ORCHESTRATOR INTEGRATION: Emotional state controls ripple behavior
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    
    // Spawn new ripples at CENTER ONLY - emotional intensity and complexity control frequency
    uint8_t baseSpawnChance = 20 + (20 * emotionalIntensity);  // Emotional component
    uint8_t complexityBonus = 15 * visualParams.getComplexityNorm();  // Complexity component
    uint8_t spawnChance = baseSpawnChance + complexityBonus;
    
    if (random8() < spawnChance) {
        for (uint8_t i = 0; i < 5; i++) {
            if (!ripples[i].active) {
                ripples[i].radius = 0;
                // Speed influenced by both visual params and emotional intensity
                float combinedIntensity = (visualParams.getIntensityNorm() + emotionalIntensity) / 2.0f;
                ripples[i].speed = (0.5f + (random8() / 255.0f) * 2.0f) * combinedIntensity;
                ripples[i].hue = random8();
                ripples[i].active = true;
                break;
            }
        }
    }
    
    // Update and render CENTER ORIGIN ripples
    for (uint8_t r = 0; r < 5; r++) {
        if (!ripples[r].active) continue;
        
        ripples[r].radius += ripples[r].speed * (paletteSpeed / 50.0f);  // FIXED: Slower ripple expansion
        
        if (ripples[r].radius > HardwareConfig::STRIP_HALF_LENGTH) {
            ripples[r].active = false;
            continue;
        }
        
        // Draw ripple moving outward from center - optimized
        extern uint8_t distanceFromCenter[];
        
        // Convert radius to integer for faster comparison
        uint16_t intRadius = ripples[r].radius;
        uint16_t radiusLow = (intRadius > 3) ? intRadius - 3 : 0;
        uint16_t radiusHigh = intRadius + 3;
        
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            uint8_t dist = distanceFromCenter[i];
            
            // Quick bounds check
            if (dist >= radiusLow && dist <= radiusHigh) {
                int16_t wavePos = dist - intRadius;
                uint8_t absDiff = abs(wavePos);
                
                if (absDiff < 3) {
                    uint8_t brightness = 255 - (absDiff * 85);
                    brightness = (brightness * (HardwareConfig::STRIP_HALF_LENGTH - intRadius)) / HardwareConfig::STRIP_HALF_LENGTH;
                    brightness = scale8(brightness, visualParams.intensity);
                    
                    // ORCHESTRATOR INTEGRATION: Use orchestrated colors for cinematic ripples
                    uint8_t colorIndex = ripples[r].hue + dist;
                    CRGB color = getOrchestratedColorFast(colorIndex, brightness);
                    
                    // Apply decay based on radius (using existing ripple decay LUT)
                    extern uint8_t rippleDecayLUT[80];
                    if (intRadius < 80) {
                        color.nscale8(rippleDecayLUT[intRadius]);
                    }
                    
                    // Apply saturation control (preserve visual params control)
                    color = blend(CRGB::White, color, visualParams.saturation);
                    
                    // Apply additional emotional brightness modulation
                    uint8_t emotionalMod = getEmotionalBrightness(255);
                    color.nscale8(emotionalMod);
                    
                    strip1[i] += color;
                    strip2[i] += color;
                }
            }
        }
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN INTERFERENCE
void stripInterference() {
    // CENTER ORIGIN INTERFERENCE - Waves emanate from center LEDs 79/80 and interfere
    static float wave1Phase = 0;
    static float wave2Phase = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    // Emotional intensity affects wave speed
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    float speedMultiplier = 1.0f + (emotionalIntensity * 0.8f); // 1.0x to 1.8x speed
    
    wave1Phase += (paletteSpeed / 20.0f) * speedMultiplier;
    wave2Phase -= (paletteSpeed / 30.0f) * speedMultiplier;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Two waves emanating from center with different frequencies
        float wave1 = sin(normalizedDist * PI * 4 + wave1Phase) * 127 + 128;
        float wave2 = sin(normalizedDist * PI * 6 + wave2Phase) * 127 + 128;
        
        // Interference pattern from CENTER ORIGIN
        float interference = (wave1 + wave2) / 2.0f;
        uint8_t brightness = interference;
        
        // Hue varies with distance from center
        uint8_t hue = (uint8_t)(wave1Phase * 20) + (distFromCenter * 8);
        
        // Use orchestrated color with emotional brightness
        uint8_t emotionalBrightness = getEmotionalBrightness(brightness);
        CRGB color = getOrchestratedColorFast(hue, emotionalBrightness);
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
        uint8_t colorIndex = gHue + (distFromCenter * 2);
        uint8_t brightness = beat - gHue + (distFromCenter * 10);
        
        CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN PLASMA
void stripPlasma() {
    // CENTER ORIGIN PLASMA - Plasma field generated from center LEDs 79/80
    static uint16_t time = 0;
    time += paletteSpeed;
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Plasma calculations from center outward
        float v1 = sin(normalizedDist * 8.0f + time / 100.0f);
        float v2 = sin(normalizedDist * 5.0f - time / 150.0f);
        float v3 = sin(normalizedDist * 3.0f + time / 200.0f);
        
        uint8_t hue = (uint8_t)((v1 + v2 + v3) * 42.5f + 127.5f) + gHue;
        uint8_t brightness = (uint8_t)((v1 + v2) * 63.75f + 191.25f);
        
        CRGB color = CHSV(hue, 255, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============== MATHEMATICAL PATTERNS ==============

void plasma() {
    static uint16_t time = 0;
    time += paletteSpeed;
    
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float v1 = sin((float)i / 8.0f + time / 100.0f);
        float v2 = sin((float)i / 5.0f - time / 150.0f);
        float v3 = sin((float)i / 3.0f + time / 200.0f);
        
        uint8_t hue = (uint8_t)((v1 + v2 + v3) * 42.5f + 127.5f) + gHue;
        uint8_t brightness = (uint8_t)((v1 + v2) * 63.75f + 191.25f);
        
        leds[i] = CHSV(hue, 255, brightness);
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
    
    // ORCHESTRATOR INTEGRATION: Combine visual params with emotional intensity
    float combinedIntensity = (visualParams.getIntensityNorm() + colorOrchestrator.getEmotionalIntensity()) / 2.0f;
    
    // Ignite new sparks at CENTER (79/80) - emotional intensity controls spark frequency and heat
    uint8_t sparkChance = 120 * combinedIntensity;
    if(random8() < sparkChance) {
        int center = HardwareConfig::STRIP_CENTER_POINT + random8(2); // 79 or 80
        uint8_t heatAmount = 160 + (95 * combinedIntensity);
        heat[center] = qadd8(heat[center], random8(160, heatAmount));
    }
    
    // Map heat to both strips with CENTER ORIGIN using orchestrated colors
    for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
        // Scale heat by intensity
        uint8_t scaledHeat = heat[j] * visualParams.getIntensityNorm();
        
        // Use orchestrated color instead of HeatColor() - maps heat to palette index
        // Lower heat = lower palette indices (cooler colors), higher heat = higher indices (warmer colors)
        uint8_t paletteIndex = map(scaledHeat, 0, 255, 0, 240); // Leave some headroom
        uint8_t brightness = scaledHeat; // Heat directly controls brightness
        
        // Get orchestrated color with emotional modulation
        CRGB color = getOrchestratedColor(paletteIndex, getEmotionalBrightness(brightness));
        
        // Apply saturation control (desaturate towards white)
        color = blend(CRGB::White, color, visualParams.saturation);
        
        strip1[j] = color;
        strip2[j] = color;
    }
}

void ocean() {
    static uint16_t waterOffset = 0;
    
    // Emotional intensity affects wave speed
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    uint16_t waveSpeed = (paletteSpeed / 2) + (emotionalIntensity * paletteSpeed / 4);
    waterOffset += waveSpeed;
    
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        // Create wave-like motion
        uint8_t wave1 = sin8((i * 10) + waterOffset);
        uint8_t wave2 = sin8((i * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Use orchestrated color with wave modulation
        uint8_t paletteIndex = 160 + (combinedWave >> 3);  // Blue range mapped to palette
        uint8_t brightness = 100 + (combinedWave >> 1);
        
        leds[i] = getOrchestratedColor(paletteIndex, getEmotionalBrightness(brightness));
    }
}

// NEW STRIP-SPECIFIC EFFECT - CENTER ORIGIN OCEAN
void stripOcean() {
    // CENTER ORIGIN OCEAN - Waves emanate from center LEDs 79/80
    static uint16_t waterOffset = 0;
    
    // Emotional intensity affects wave speed
    float emotionalIntensity = colorOrchestrator.getEmotionalIntensity();
    uint16_t waveSpeed = (paletteSpeed / 2) + (emotionalIntensity * paletteSpeed / 4);
    waterOffset += waveSpeed;
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER (79/80)
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Create wave-like motion from center outward
        uint8_t wave1 = sin8((distFromCenter * 10) + waterOffset);
        uint8_t wave2 = sin8((distFromCenter * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Use orchestrated color with wave modulation
        uint8_t paletteIndex = 160 + (combinedWave >> 3);  // Blue range mapped to palette
        uint8_t brightness = 100 + (combinedWave >> 1);
        
        CRGB color = getOrchestratedColor(paletteIndex, getEmotionalBrightness(brightness));
        strip1[i] = color;
        strip2[i] = color;
    }
}

// ============== NEW CENTER ORIGIN EFFECTS ==============

// HEARTBEAT - Pulses emanate from center like a beating heart
void heartbeatEffect() {
    static float phase = 0;
    static float lastBeat = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Heartbeat rhythm: lub-dub... lub-dub...
    float beatPattern = sin(phase) + sin(phase * 2.1f) * 0.4f;
    
    if (beatPattern > 1.8f && phase - lastBeat > 2.0f) {
        lastBeat = phase;
        // Generate pulse from CENTER
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
            
            // Pulse intensity decreases with distance
            uint8_t brightness = 255 * (1.0f - normalizedDist);
            uint8_t paletteIndex = gHue + normalizedDist * 50;
            CRGB color = getOrchestratedColor(paletteIndex, getEmotionalBrightness(brightness));
            
            strip1[i] += color;
            strip2[i] += color;
        }
    }
    
    phase += paletteSpeed / 200.0f;
}

// BREATHING - Smooth expansion and contraction from center
void breathingEffect() {
    static float breathPhase = 0;
    
    // Smooth breathing curve
    float breath = (sin(breathPhase) + 1.0f) / 2.0f;
    float radius = breath * HardwareConfig::STRIP_HALF_LENGTH;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 15);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 15);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        
        if (distFromCenter <= radius) {
            float intensity = 1.0f - (distFromCenter / radius) * 0.5f;
            uint8_t brightness = 255 * intensity * breath;
            
            uint8_t paletteIndex = gHue + distFromCenter * 3;
            CRGB color = getOrchestratedColor(paletteIndex, getEmotionalBrightness(brightness));
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
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 25);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 25);
    
    // Spawn new shockwave from CENTER - complexity controls frequency
    uint8_t spawnChance = 20 * visualParams.getComplexityNorm();
    if (random8() < spawnChance) {
        for (int w = 0; w < 5; w++) {
            if (shockwaves[w] < 0) {
                shockwaves[w] = 0;
                waveHues[w] = gHue + random8(64);
                break;
            }
        }
    }
    
    // Update and render shockwaves
    for (int w = 0; w < 5; w++) {
        if (shockwaves[w] >= 0) {
            shockwaves[w] += (paletteSpeed / 20.0f) * visualParams.getIntensityNorm();
            
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
                    uint8_t brightness = 255 * intensity * (1.0f - shockwaves[w] / HardwareConfig::STRIP_HALF_LENGTH);
                    brightness = brightness * visualParams.getIntensityNorm();
                    
                    CRGB color = ColorFromPalette(currentPalette, waveHues[w], brightness);
                    // Apply saturation control
                    color = blend(CRGB::White, color, visualParams.saturation);
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
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Spiral calculation
        float spiralOffset = normalizedDist * 8.0f + vortexAngle;
        float intensity = sin(spiralOffset) * 0.5f + 0.5f;
        intensity *= (1.0f - normalizedDist * 0.5f); // Fade towards edges
        
        uint8_t brightness = 255 * intensity;
        uint8_t hue = gHue + distFromCenter * 5 + vortexAngle * 20;
        
        CRGB color = ColorFromPalette(currentPalette, hue, brightness);
        
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
    static float particle1Pos = 0;
    static float particle2Pos = HardwareConfig::STRIP_LENGTH - 1;
    static bool exploding = false;
    static float explosionRadius = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 30);
    
    if (!exploding) {
        // Move particles toward center - FIXED: Much slower speed scaling
        float speedMultiplier = paletteSpeed / 100.0f;  // 0.01 to 0.5 range
        particle1Pos += speedMultiplier;
        particle2Pos -= speedMultiplier;
        
        // Draw particles
        for (int trail = 0; trail < 10; trail++) {
            int pos1 = particle1Pos - trail;
            int pos2 = particle2Pos + trail;
            
            if (pos1 >= 0 && pos1 < HardwareConfig::STRIP_LENGTH) {
                uint8_t brightness = 255 - (trail * 25);
                strip1[pos1] = ColorFromPalette(currentPalette, gHue, brightness);
                strip2[pos1] = ColorFromPalette(currentPalette, gHue + 128, brightness);
            }
            
            if (pos2 >= 0 && pos2 < HardwareConfig::STRIP_LENGTH) {
                uint8_t brightness = 255 - (trail * 25);
                strip1[pos2] = ColorFromPalette(currentPalette, gHue + 128, brightness);
                strip2[pos2] = ColorFromPalette(currentPalette, gHue, brightness);
            }
        }
        
        // Check for collision at center
        if (particle1Pos >= HardwareConfig::STRIP_CENTER_POINT - 5 && 
            particle2Pos <= HardwareConfig::STRIP_CENTER_POINT + 5) {
            exploding = true;
            explosionRadius = 0;
        }
    } else {
        // Explosion expanding from center - FIXED: Consistent speed scaling
        explosionRadius += paletteSpeed / 50.0f;  // Much slower expansion
        
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            
            if (distFromCenter <= explosionRadius && distFromCenter >= explosionRadius - 10) {
                float intensity = 1.0f - ((distFromCenter - (explosionRadius - 10)) / 10.0f);
                uint8_t brightness = 255 * intensity;
                
                CRGB color = ColorFromPalette(currentPalette, gHue + random8(64), brightness);
                strip1[i] += color;
                strip2[i] += color;
            }
        }
        
        // Reset when explosion reaches edges
        if (explosionRadius > HardwareConfig::STRIP_HALF_LENGTH + 10) {
            exploding = false;
            particle1Pos = 0;
            particle2Pos = HardwareConfig::STRIP_LENGTH - 1;
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
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Update particles with gravity toward center
    for (int p = 0; p < 20; p++) {
        if (particles[p].active) {
            float distFromCenter = particles[p].position - HardwareConfig::STRIP_CENTER_POINT;
            // FIXED: Much weaker gravity for controllable speed
            float gravity = -distFromCenter * 0.001f * paletteSpeed / 10.0f;
            
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