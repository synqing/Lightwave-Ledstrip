#include "EnhancedStripEffects.h"

#if FEATURE_ENHANCEMENT_ENGINES

#include "../engines/ColorEngine.h"
#include "../engines/MotionEngine.h"
#include "../../config/features.h"
#include "../../core/EffectTypes.h"

// External references
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern uint8_t paletteSpeed;
extern VisualParams visualParams;
extern CRGBPalette16 currentPalette;
// NOTE: gHue is NOT used - rainbow cycling is forbidden per CLAUDE.md

// ============== FIRE ENHANCED ==============
// Dual-palette fire (HeatColors + LavaColors) for deeper, richer tones
void fireEnhanced() {
    static byte heat[HardwareConfig::STRIP_LENGTH];
    ColorEngine& colorEngine = ColorEngine::getInstance();

    // Configure dual-palette blending (HeatColors + LavaColors)
    static bool initialized = false;
    if (!initialized) {
        colorEngine.enableCrossBlend(true);
        colorEngine.setBlendPalettes(HeatColors_p, LavaColors_p);
        colorEngine.setBlendFactors(180, 75, 0);  // 70% heat, 30% lava
        initialized = true;
    }

    // Cool down every cell
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / HardwareConfig::STRIP_LENGTH) + 2));
    }

    // Heat diffuses from center outward
    for(int k = 1; k < HardwareConfig::STRIP_LENGTH - 1; k++) {
        heat[k] = (heat[k - 1] + heat[k] + heat[k + 1]) / 3;
    }

    // Ignite new sparks at CENTER (79/80)
    float intensityNorm = visualParams.getIntensityNorm();
    if (intensityNorm < 0.1f) intensityNorm = 0.1f;
    uint8_t sparkChance = 120 * intensityNorm;
    if(random8() < sparkChance) {
        int center = HardwareConfig::STRIP_CENTER_POINT + random8(2);
        uint8_t heatAmount = 160 + (95 * intensityNorm);
        heat[center] = qadd8(heat[center], random8(160, heatAmount));
    }

    // Map heat to colors using ColorEngine for dual-palette blending
    for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
        uint8_t scaledHeat = heat[j] * visualParams.getIntensityNorm();

        // Use ColorEngine for blended HeatColors + LavaColors (full brightness, then scale)
        CRGB color = colorEngine.getColor(scaledHeat, 255);

        // Apply brightness scaling AFTER getting saturated color
        color.nscale8(scaledHeat);

        // Apply saturation control
        if (visualParams.getSaturationNorm() < 1.0f) {
            CHSV hsv = rgb2hsv_approximate(color);
            hsv.sat = visualParams.getSaturationNorm() * 255;
            color = hsv;
        }

        strip1[j] = color;
        strip2[j] = color;
    }

    colorEngine.update();
}

// ============== OCEAN ENHANCED ==============
// Triple-palette ocean (deep blue / mid cyan / surface shimmer) for layered depth
void stripOceanEnhanced() {
    static uint32_t waterOffset = 0;
    ColorEngine& colorEngine = ColorEngine::getInstance();

    // Configure triple-palette blending (deep / mid / surface)
    static bool initialized = false;
    if (!initialized) {
        // Create custom ocean palettes
        static CRGBPalette16 deepOcean = CRGBPalette16(
            CRGB::Black, CRGB::MidnightBlue, CRGB::DarkBlue, CRGB::Navy
        );
        static CRGBPalette16 midOcean = CRGBPalette16(
            CRGB::DarkBlue, CRGB::Blue, CRGB::DodgerBlue, CRGB::DeepSkyBlue
        );
        static CRGBPalette16 surfaceOcean = CRGBPalette16(
            CRGB::DeepSkyBlue, CRGB::Cyan, CRGB::Aqua, CRGB::LightCyan
        );

        colorEngine.enableCrossBlend(true);
        colorEngine.setBlendPalettes(deepOcean, midOcean, &surfaceOcean);
        colorEngine.setBlendFactors(100, 100, 55);  // Balanced blend
        initialized = true;
    }

    waterOffset += paletteSpeed / 2;
    if (waterOffset > 65535) {
        waterOffset = waterOffset % 65536;
    }

    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate distance from CENTER
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);

        // Create wave-like motion from center outward
        uint8_t wave1 = sin8((distFromCenter * 10) + waterOffset);
        uint8_t wave2 = sin8((distFromCenter * 7) - waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;

        // Use ColorEngine for triple-palette depth layering (full brightness, then scale)
        CRGB color = colorEngine.getColor(combinedWave, 255);

        // Apply brightness scaling AFTER getting saturated color
        uint8_t brightness = 100 + (combinedWave >> 1);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }

    colorEngine.update();
}

// ============== LGP HOLOGRAPHIC ENHANCED ==============
// Adds color diffusion for smoother, shimmer effect
void lgpHolographicEnhanced() {
    ColorEngine& colorEngine = ColorEngine::getInstance();

    // Enable color diffusion for smoother shimmer
    static bool initialized = false;
    if (!initialized) {
        colorEngine.enableDiffusion(true);
        colorEngine.setDiffusionAmount(80);  // Moderate diffusion for smooth shimmer
        initialized = true;
    }

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    static float phase1 = 0, phase2 = 0, phase3 = 0;
    phase1 += speed * 0.02f;
    phase2 += speed * 0.03f;
    phase3 += speed * 0.05f;

    int numLayers = 2 + (complexity * 3);  // 2-5 layers

    // Render holographic layers
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float dist = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalized = dist / HardwareConfig::STRIP_HALF_LENGTH;

        float layerSum = 0;

        // Layer 1 - Slow, wide pattern
        layerSum += sin(dist * 0.05f + phase1) * (numLayers >= 1 ? 1.0f : 0);

        // Layer 2 - Medium pattern
        layerSum += sin(dist * 0.1f + phase2) * (numLayers >= 2 ? 0.7f : 0);

        // Layer 3 - Fast, narrow pattern
        layerSum += sin(dist * 0.2f + phase3) * (numLayers >= 3 ? 0.5f : 0);

        // Layer 4 - Very fast shimmer
        layerSum += sin(dist * 0.4f - phase1 * 2) * (numLayers >= 4 ? 0.3f : 0);

        // Layer 5 - Ultra-fast sparkle
        layerSum += sin(dist * 0.8f + phase2 * 3) * (numLayers >= 5 ? 0.2f : 0);

        // Normalize and scale
        layerSum = (layerSum + (float)numLayers) / (2.0f * numLayers);

        uint8_t brightness = (uint8_t)(layerSum * 255 * intensity);

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(normalized * 255 + phase1 * 10);

        // Get color at full brightness, then scale
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }

    // Apply diffusion AFTER rendering for smooth shimmer effect
    colorEngine.applyDiffusionToStrips();
    colorEngine.update();
}

// ============== SHOCKWAVE ENHANCED ==============
// Uses MotionEngine for momentum-based decay
void shockwaveEnhanced() {
    MotionEngine& motionEngine = MotionEngine::getInstance();
    static bool initialized = false;
    static uint8_t paletteBase = 0;  // Position-based color base (no gHue)

    if (!initialized) {
        motionEngine.enable();
        motionEngine.getMomentumEngine().reset();
        initialized = true;
    }

    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 25);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 25);

    // Spawn new shockwave with momentum physics
    uint8_t spawnChance = 20 * visualParams.getComplexityNorm();
    if (random8() < spawnChance) {
        float velocity = (paletteSpeed / 20.0f) * visualParams.getIntensityNorm();

        // Position-based color from palette (no gHue - rainbow cycling forbidden)
        uint8_t colorIndex = paletteBase + random8(64);
        CRGB color = ColorFromPalette(currentPalette, colorIndex, 255);

        motionEngine.getMomentumEngine().addParticle(0.5f, velocity, 1.0f, color);
        paletteBase += 17;  // Advance palette position for variety
    }

    // Update and render particles with momentum
    motionEngine.update();
    MomentumEngine& momentum = motionEngine.getMomentumEngine();

    for (uint8_t p = 0; p < 32; p++) {
        Particle* particle = momentum.getParticle(p);
        if (!particle || !particle->active) continue;

        // Convert normalized position (0-1) to LED index from center
        float radius = abs(particle->position - 0.5f) * 2.0f;  // 0-1 range
        float ledRadius = radius * HardwareConfig::STRIP_HALF_LENGTH;

        // Render shockwave ring
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
            float diff = abs(distFromCenter - ledRadius);

            if (diff < 5) {
                uint8_t brightness = 255 * (1.0f - (diff / 5.0f)) * particle->velocity;

                // Scale particle color by brightness (already saturated)
                CRGB scaledColor = particle->color;
                scaledColor.nscale8(brightness);

                strip1[i] += scaledColor;
                strip2[i] += scaledColor;
            }
        }
    }
}

// ============== COLLISION ENHANCED ==============
// Uses PhaseController for independent phase per particle
void collisionEnhanced() {
    MotionEngine& motionEngine = MotionEngine::getInstance();
    static bool initialized = false;
    static uint8_t paletteBase = 0;  // Position-based color base (no gHue)

    if (!initialized) {
        motionEngine.enable();
        motionEngine.getMomentumEngine().reset();
        initialized = true;
    }

    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 40);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 40);

    // Spawn collision particles with palette-based colors
    if (random8() < 30) {
        float pos = random(100) / 100.0f;
        float vel = (random(100) - 50) / 500.0f;

        // Position-based color from palette (no gHue - rainbow cycling forbidden)
        uint8_t colorIndex = paletteBase + random8(64);
        CRGB color = ColorFromPalette(currentPalette, colorIndex, 255);

        motionEngine.getMomentumEngine().addParticle(pos, vel, 1.0f, color);
        paletteBase += 23;  // Advance palette position for variety
    }

    motionEngine.update();
    MomentumEngine& momentum = motionEngine.getMomentumEngine();

    // Render particles with independent phase
    for (uint8_t p = 0; p < 32; p++) {
        Particle* particle = momentum.getParticle(p);
        if (!particle || !particle->active) continue;

        int ledPos = particle->position * HardwareConfig::STRIP_LENGTH;
        if (ledPos >= 0 && ledPos < HardwareConfig::STRIP_LENGTH) {
            strip1[ledPos] += particle->color;
            strip2[ledPos] += particle->color;

            // Trail effect
            if (ledPos > 0) {
                CRGB trailColor = particle->color;
                trailColor.nscale8(128);
                strip1[ledPos - 1] += trailColor;
                strip2[ledPos - 1] += trailColor;
            }
            if (ledPos < HardwareConfig::STRIP_LENGTH - 1) {
                CRGB trailColor = particle->color;
                trailColor.nscale8(128);
                strip1[ledPos + 1] += trailColor;
                strip2[ledPos + 1] += trailColor;
            }
        }
    }
}

// ============== LGP WAVE COLLISION ENHANCED ==============
// Uses PhaseController for phase-shifted interference
void lgpWaveCollisionEnhanced() {
    MotionEngine& motionEngine = MotionEngine::getInstance();
    static float wave1Pos = 0;
    static float wave2Pos = HardwareConfig::STRIP_LENGTH;
    static float wave1Vel = 2.0f;
    static float wave2Vel = -2.0f;
    static bool initialized = false;
    static uint8_t paletteBase = 0;  // Position-based color base (no gHue)

    if (!initialized) {
        motionEngine.enable();
        motionEngine.getPhaseController().enableAutoRotate(10.0f);  // 10 deg/sec
        initialized = true;
    }

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();

    // Update wave positions
    wave1Pos += wave1Vel * speed;
    wave2Pos += wave2Vel * speed;

    // Bounce at edges
    if (wave1Pos < 0 || wave1Pos > HardwareConfig::STRIP_LENGTH) {
        wave1Vel = -wave1Vel;
        wave1Pos = constrain(wave1Pos, 0, HardwareConfig::STRIP_LENGTH);
    }
    if (wave2Pos < 0 || wave2Pos > HardwareConfig::STRIP_LENGTH) {
        wave2Vel = -wave2Vel;
        wave2Pos = constrain(wave2Pos, 0, HardwareConfig::STRIP_LENGTH);
    }

    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 30);

    // Get phase from MotionEngine
    motionEngine.update();
    float phase = motionEngine.getPhaseController().getStripPhaseRadians();

    // Slowly evolve palette base for color variation (no gHue)
    static uint32_t lastPhaseUpdate = 0;
    if (millis() - lastPhaseUpdate > 100) {
        paletteBase += 1;
        lastPhaseUpdate = millis();
    }

    // Render with phase-shifted interference
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float dist1 = abs(i - wave1Pos);
        float dist2 = abs(i - wave2Pos);
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);

        // Wave packets with phase shift
        float packet1 = exp(-dist1 * 0.05f) * cos(dist1 * 0.5f + phase);
        float packet2 = exp(-dist2 * 0.05f) * cos(dist2 * 0.5f - phase);

        float interference = packet1 + packet2;
        uint8_t brightness = 128 + (127 * interference * intensity);

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = paletteBase + (uint8_t)(distFromCenter * 2) + (uint8_t)(phase * 40);

        // Get color at full brightness, then scale
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255);
        color.nscale8(brightness);

        strip1[i] = color;
        strip2[i] = color;
    }
}

#endif // FEATURE_ENHANCEMENT_ENGINES
