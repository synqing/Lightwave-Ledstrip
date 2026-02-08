/**
 * @file ChromaticInterferenceEffect.cpp
 * @brief LGP Chromatic Interference implementation
 */

#include "ChromaticInterferenceEffect.h"
#include "../CoreEffects.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

ChromaticInterferenceEffect::ChromaticInterferenceEffect()
    : m_interferencePhase(0.0f)
{
}

bool ChromaticInterferenceEffect::init(plugins::EffectContext& ctx) {
    m_interferencePhase = 0.0f;
    return true;
}

CRGB ChromaticInterferenceEffect::chromaticDispersionPalette(float position,
                                                             float aberration,
                                                             float phase,
                                                             float intensity,
                                                             const plugins::PaletteRef& palette,
                                                             uint8_t baseHue) const {
    if (!palette.isValid()) {
        // Fallback to simple RGB dispersion if no palette
        float distFromCenter = (float)centerPairDistance((uint16_t)position);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;
        if (normalizedDist < 0.0f) normalizedDist = 0.0f;
        if (normalizedDist > 1.0f) normalizedDist = 1.0f;
        
        // Simplified RGB dispersion
        float redFocus = 0.5f + 0.5f * sinf((normalizedDist - 0.04f * aberration) * PI + phase);
        float greenFocus = 0.5f + 0.5f * sinf(normalizedDist * PI + phase);
        float blueFocus = 0.5f + 0.5f * sinf((normalizedDist + 0.05f * aberration) * PI + phase);
        
        uint8_t r = (uint8_t)(128 + 127 * redFocus);
        uint8_t g = (uint8_t)(128 + 127 * greenFocus);
        uint8_t b = (uint8_t)(128 + 127 * blueFocus);
        
        CRGB out(r, g, b);
        out.nscale8_video((uint8_t)(constrain(intensity * 255.0f, 0.0f, 255.0f)));
        return out;
    }

    // Normalised distance from centre (0..1)
    float distFromCenter = (float)centerPairDistance((uint16_t)position);
    float normalizedDist = distFromCenter / (float)HALF_LENGTH;
    if (normalizedDist < 0.0f) normalizedDist = 0.0f;
    if (normalizedDist > 1.0f) normalizedDist = 1.0f;

    // Use palette as a "spectrum" source: distance selects along palette,
    // phase scrolls it slowly to keep the effect alive.
    uint8_t phaseScroll = (uint8_t)((phase * 255.0f) / TWO_PI);
    uint8_t idx = baseHue + (uint8_t)(normalizedDist * 255.0f) + phaseScroll;

    // Chromatic separation in palette space: stronger separation as aberration increases.
    // Keep this deliberately larger than the physically tiny Δn mapping so it remains visible.
    int16_t sep = (int16_t)(8 + (aberration * 24.0f));   // ~8..80

    CRGB cR = palette.getColor((uint8_t)(idx - sep), 255);
    CRGB cG = palette.getColor(idx, 255);
    CRGB cB = palette.getColor((uint8_t)(idx + sep), 255);

    // Dispersion modulation weights (0..1) derived from the original channel foci.
    // We recompute a simplified version here to avoid depending on internal offsets.
    float aberr = aberration;
    if (aberr < 0.0f) aberr = 0.0f;
    if (aberr > 3.0f) aberr = 3.0f;

    // Use slightly exaggerated offsets so the per-channel weights differ enough to matter.
    float redOffset  = -0.04f * aberr;
    float blueOffset = +0.05f * aberr;

    float redFocus   = 0.5f + 0.5f * sinf((normalizedDist + redOffset) * PI + phase);
    float greenFocus = 0.5f + 0.5f * sinf((normalizedDist) * PI + phase);
    float blueFocus  = 0.5f + 0.5f * sinf((normalizedDist + blueOffset) * PI + phase);

    float wSum = redFocus + greenFocus + blueFocus;
    if (wSum < 0.001f) wSum = 1.0f;

    // Weighted blend of the three palette samples
    float rF = (cR.r * redFocus + cG.r * greenFocus + cB.r * blueFocus) / wSum;
    float gF = (cR.g * redFocus + cG.g * greenFocus + cB.g * blueFocus) / wSum;
    float bF = (cR.b * redFocus + cG.b * greenFocus + cB.b * blueFocus) / wSum;

    CRGB out((uint8_t)constrain((int)rF, 0, 255),
             (uint8_t)constrain((int)gF, 0, 255),
             (uint8_t)constrain((int)bF, 0, 255));

    // Apply intensity scaling (0..1)
    out.nscale8_video((uint8_t)(constrain(intensity * 255.0f, 0.0f, 255.0f)));
    return out;
}

void ChromaticInterferenceEffect::render(plugins::EffectContext& ctx) {
    // Dual-edge injection with dispersion, interference patterns
    float intensity = ctx.brightness / 255.0f;
    // Aberration strength from complexity parameter (b1: complexity_norm * 3)
    float aberration = (ctx.complexity / 255.0f) * 3.0f;
    
    // Interference phase animation
    m_interferencePhase += ctx.speed * PHASE_SPEED;
    if (m_interferencePhase > TWO_PI) m_interferencePhase -= TWO_PI;
    
    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
        float position = (float)i;
        
        // Dual wave sources from edges (constructive/destructive interference)
        float distFromLeft = position / 79.5f;  // Normalized distance from left edge
        float distFromRight = (159.0f - position) / 79.5f; // Normalized distance from right edge
        
        // Wave phases from each edge
        float leftPhase = m_interferencePhase - distFromLeft * TWO_PI;
        float rightPhase = m_interferencePhase - distFromRight * TWO_PI;
        
        // Interference pattern (sum of waves)
        float interference = sinf(leftPhase) + sinf(rightPhase);
        interference = interference / 2.0f; // Normalize to -1..1
        
        // Apply chromatic dispersion with interference modulation
        float phase = m_interferencePhase + interference * INTERFERENCE_MODULATION;
        CRGB color = chromaticDispersionPalette(position, aberration, phase, intensity, ctx.palette, ctx.gHue);
        
        // Modulate intensity based on interference (constructive = brighter, destructive = dimmer)
        float interferenceIntensity = 0.5f + 0.5f * interference;
        color.r = (uint8_t)(color.r * interferenceIntensity);
        color.g = (uint8_t)(color.g * interferenceIntensity);
        color.b = (uint8_t)(color.b * interferenceIntensity);
        
        ctx.leds[i] = color;
    }
    
    // Strip 2: Centre-origin at LED 240 (not edge mirror)
    if (ctx.ledCount >= STRIP_LENGTH * 2) {
        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            uint16_t strip2Idx = STRIP_LENGTH + i;
            if (strip2Idx < ctx.ledCount) {
                float position = (float)i;

                float distFromLeft = position / (float)HALF_LENGTH;
                float distFromRight = ((float)(STRIP_LENGTH - 1) - position) / (float)HALF_LENGTH;

                float leftPhase = m_interferencePhase - distFromLeft * TWO_PI;
                float rightPhase = m_interferencePhase - distFromRight * TWO_PI;

                float interference = sinf(leftPhase) + sinf(rightPhase);
                interference = interference / 2.0f;

                float phase = m_interferencePhase + interference * INTERFERENCE_MODULATION + PI;
                CRGB color = chromaticDispersionPalette(position, aberration, phase, intensity, ctx.palette, ctx.gHue);

                float interferenceIntensity = 0.5f + 0.5f * interference;
                color.r = (uint8_t)(color.r * interferenceIntensity);
                color.g = (uint8_t)(color.g * interferenceIntensity);
                color.b = (uint8_t)(color.b * interferenceIntensity);

                ctx.leds[strip2Idx] = color;
            }
        }
    }
}

void ChromaticInterferenceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ChromaticInterferenceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chromatic Interference",
        "Interfering dispersion patterns",
        plugins::EffectCategory::UNCATEGORIZED,  // ADVANCED_OPTICAL not in enum, using UNCATEGORIZED
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

