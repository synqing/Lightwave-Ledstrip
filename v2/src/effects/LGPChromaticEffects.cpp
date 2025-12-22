/**
 * @file LGPChromaticEffects.cpp
 * @brief Physics-accurate chromatic dispersion effects using Cauchy equation
 *
 * Implements chromatic dispersion based on Cauchy equation: n(λ) = A + B/λ² + C/λ⁴
 * Reference: b1. LGP_OPTICAL_PHYSICS_REFERENCE.md lines 380-450
 *
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPChromaticEffects.h"
#include "CoreEffects.h"
#include "../core/actors/RendererActor.h"
#include "utils/FastLEDOptim.h"
#include <FastLED.h>
#include <math.h>

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ============================================================================
// Constants
// ============================================================================

// STRIP_LENGTH and other constants are defined in global headers
// PI is defined in Arduino.h

// Animation state (persistent across frames)
static float lensPhase = 0.0f;
static float pulsePhase = 0.0f;
static float interferencePhase = 0.0f;

// ============================================================================
// Physics-Accurate Chromatic Dispersion (Cauchy Equation)
// ============================================================================

static CRGB chromaticDispersion(float position, float aberration, float phase, float intensity) {
    // Normalize position from centre (0.0 = centre, 1.0 = edge)
    // Use standardized center distance calculation
    float distFromCenter = (float)centerPairDistance((uint16_t)position);
    float normalizedDist = distFromCenter / (float)HALF_LENGTH;
    
    // Cauchy dispersion (b1 reference):
    // n(λ) = n0 + B/(λ² - C)
    //
    // NOTE: b1 provides B and C in μm²; we therefore compute using λ in μm.
    const float n0 = 1.49f;       // Base refractive index (acrylic/PMMA)
    const float B = 0.0045f;      // μm² (approx)
    const float C = 0.0001f;      // μm² (approx)
    
    // Wavelengths in micrometers (μm)
    const float lambda_red = 0.70f;    // 700nm
    const float lambda_green = 0.55f;  // 550nm (reference)
    const float lambda_blue = 0.45f;   // 450nm
    
    // n(λ) = n0 + B/(λ² - C)
    const float n_red = n0 + (B / ((lambda_red * lambda_red) - C));
    const float n_green = n0 + (B / ((lambda_green * lambda_green) - C));
    const float n_blue = n0 + (B / ((lambda_blue * lambda_blue) - C));
    
    // Δn relative to green; used to scale the phase offset.
    // Typical acrylic dispersion gives Δn ~ 0.005 across visible spectrum.
    const float deltaR = (n_green - n_red);    // positive
    const float deltaB = (n_blue - n_green);   // positive
    
    // Map small Δn to a perceptible phase offset.
    // 0.005 * 20 ≈ 0.1, matching b1's ±0.1 * aberration rule-of-thumb.
    const float DISPERSION_SCALE = 20.0f;
    
    // Clamp aberration to avoid rainbow-like cycling
    if (aberration < 0.0f) aberration = 0.0f;
    if (aberration > 3.0f) aberration = 3.0f;
    
    const float redOffset = -0.1f * aberration * deltaR * DISPERSION_SCALE;   // red leads
    const float blueOffset = +0.1f * aberration * deltaB * DISPERSION_SCALE;  // blue lags
    
    // Generate phase-separated sine waves for each channel
    float redFocus = sinf((normalizedDist + redOffset) * PI + phase);
    float greenFocus = sinf(normalizedDist * PI + phase);
    float blueFocus = sinf((normalizedDist + blueOffset) * PI + phase);
    
    // Convert to RGB values (128 = neutral, ±127 = full swing)
    uint8_t r = (uint8_t)constrain((int)(128 + 127 * redFocus), 0, 255);
    uint8_t g = (uint8_t)constrain((int)(128 + 127 * greenFocus), 0, 255);
    uint8_t b = (uint8_t)constrain((int)(128 + 127 * blueFocus), 0, 255);
    
    // Apply intensity scaling
    r = (uint8_t)(r * intensity);
    g = (uint8_t)(g * intensity);
    b = (uint8_t)(b * intensity);
    
    return CRGB(r, g, b);
}

/**
 * @brief Palette-aware chromatic dispersion.
 *
 * The original physics-based RGB method can be very near-neutral (subtle dispersion),
 * which the post-render white guardrail may classify as "whitish" under CC modes.
 *
 * This variant uses the active palette as a spectral source so palette selection
 * meaningfully affects these effects, while still using the dispersion phases
 * as the modulation mechanism.
 */
static CRGB chromaticDispersionPalette(float position,
                                       float aberration,
                                       float phase,
                                       float intensity,
                                       const CRGBPalette16* palette,
                                       uint8_t baseHue)
{
    if (palette == nullptr) {
        return chromaticDispersion(position, aberration, phase, intensity);
    }

    // Normalised distance from centre (0..1)
    float distFromCenter = (float)centerPairDistance((uint16_t)position);
    float normalizedDist = distFromCenter / (float)HALF_LENGTH;
    if (normalizedDist < 0.0f) normalizedDist = 0.0f;
    if (normalizedDist > 1.0f) normalizedDist = 1.0f;

    // Use palette as a "spectrum" source: distance selects along palette,
    // phase scrolls it slowly to keep the effect alive.
    uint8_t phaseScroll = (uint8_t)((phase * 255.0f) / (2.0f * PI));
    uint8_t idx = baseHue + (uint8_t)(normalizedDist * 255.0f) + phaseScroll;

    // Chromatic separation in palette space: stronger separation as aberration increases.
    // Keep this deliberately larger than the physically tiny Δn mapping so it remains visible.
    int16_t sep = (int16_t)(8 + (aberration * 24.0f));   // ~8..80

    CRGB cR = ColorFromPalette(*palette, (uint8_t)(idx - sep), 255);
    CRGB cG = ColorFromPalette(*palette, (uint8_t)(idx),       255);
    CRGB cB = ColorFromPalette(*palette, (uint8_t)(idx + sep), 255);

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

// ============================================================================
// Effect 1: Chromatic Lens
// ============================================================================

void effectChromaticLens(RenderContext& ctx) {
    // Static aberration, lens position controlled by speed encoder
    float intensity = ctx.brightness / 255.0f;
    // Aberration strength from complexity parameter (b1: complexity_norm * 3)
    float aberration = (ctx.complexity / 255.0f) * 3.0f;
    
    // Lens position controlled by speed (phase animation)
    lensPhase += ctx.speed * 0.01f;
    if (lensPhase > 2.0f * PI) lensPhase -= 2.0f * PI;
    
    for (int i = 0; i < STRIP_LENGTH && i < ctx.numLeds; i++) {
        CRGB color = chromaticDispersionPalette((float)i, aberration, lensPhase, intensity, ctx.palette, ctx.hue);
        ctx.leds[i] = color;
    }
    
    // Mirror to second strip if present
    if (ctx.numLeds >= STRIP_LENGTH * 2) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int mirrorIdx = STRIP_LENGTH * 2 - 1 - i;
            if (mirrorIdx < ctx.numLeds) {
                // Invert color channels for visual variety
                CRGB color = chromaticDispersionPalette((float)i, aberration, lensPhase + PI, intensity, ctx.palette, ctx.hue);
                ctx.leds[mirrorIdx] = CRGB(color.b, color.g, color.r);
            }
        }
    }
}

// ============================================================================
// Effect 2: Chromatic Pulse
// ============================================================================

void effectChromaticPulse(RenderContext& ctx) {
    // Aberration sweeps from centre outward, intensity pulse
    float baseIntensity = ctx.brightness / 255.0f;
    float baseAberration = (ctx.complexity / 255.0f) * 3.0f;
    
    // Pulse phase for expansion/contraction
    pulsePhase += ctx.speed * 0.015f;
    if (pulsePhase > 2.0f * PI) pulsePhase -= 2.0f * PI;
    
    // Pulsing aberration (expands and contracts)
    float aberration = baseAberration * (0.5f + 0.5f * sinf(pulsePhase));
    
    // Pulsing intensity
    float intensity = baseIntensity * (0.7f + 0.3f * sinf(pulsePhase * 1.5f));
    
    // Phase offset for animation
    float phase = pulsePhase * 0.5f;
    
    for (int i = 0; i < STRIP_LENGTH && i < ctx.numLeds; i++) {
        CRGB color = chromaticDispersionPalette((float)i, aberration, phase, intensity, ctx.palette, ctx.hue);
        ctx.leds[i] = color;
    }
    
    // Mirror to second strip if present
    if (ctx.numLeds >= STRIP_LENGTH * 2) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int mirrorIdx = STRIP_LENGTH * 2 - 1 - i;
            if (mirrorIdx < ctx.numLeds) {
                CRGB color = chromaticDispersionPalette((float)i, aberration, phase + PI * 0.5f, intensity, ctx.palette, ctx.hue);
                ctx.leds[mirrorIdx] = color;
            }
        }
    }
}

// ============================================================================
// Effect 3: Chromatic Interference
// ============================================================================

void effectChromaticInterference(RenderContext& ctx) {
    // Dual-edge injection with dispersion, interference patterns
    float intensity = ctx.brightness / 255.0f;
    // Aberration strength from complexity parameter (b1: complexity_norm * 3)
    float aberration = (ctx.complexity / 255.0f) * 3.0f;
    
    // Interference phase animation
    interferencePhase += ctx.speed * 0.012f;
    if (interferencePhase > 2.0f * PI) interferencePhase -= 2.0f * PI;
    
    for (int i = 0; i < STRIP_LENGTH && i < ctx.numLeds; i++) {
        float position = (float)i;
        
        // Dual wave sources from edges (constructive/destructive interference)
        float distFromLeft = position / 79.5f;  // Normalized distance from left edge
        float distFromRight = (159.0f - position) / 79.5f; // Normalized distance from right edge
        
        // Wave phases from each edge
        float leftPhase = interferencePhase - distFromLeft * PI * 2.0f;
        float rightPhase = interferencePhase - distFromRight * PI * 2.0f;
        
        // Interference pattern (sum of waves)
        float interference = sinf(leftPhase) + sinf(rightPhase);
        interference = interference / 2.0f; // Normalize to -1..1
        
        // Apply chromatic dispersion with interference modulation
        float phase = interferencePhase + interference * 0.5f;
        CRGB color = chromaticDispersionPalette(position, aberration, phase, intensity, ctx.palette, ctx.hue);
        
        // Modulate intensity based on interference (constructive = brighter, destructive = dimmer)
        float interferenceIntensity = 0.5f + 0.5f * interference;
        color.r = (uint8_t)(color.r * interferenceIntensity);
        color.g = (uint8_t)(color.g * interferenceIntensity);
        color.b = (uint8_t)(color.b * interferenceIntensity);
        
        ctx.leds[i] = color;
    }
    
    // Mirror to second strip if present
    if (ctx.numLeds >= STRIP_LENGTH * 2) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int mirrorIdx = STRIP_LENGTH * 2 - 1 - i;
            if (mirrorIdx < ctx.numLeds) {
                float position = (float)i;
                
                float distFromLeft = position / (float)HALF_LENGTH;
                float distFromRight = ((float)(STRIP_LENGTH - 1) - position) / (float)HALF_LENGTH;
                
                float leftPhase = interferencePhase - distFromLeft * PI * 2.0f;
                float rightPhase = interferencePhase - distFromRight * PI * 2.0f;
                
                float interference = sinf(leftPhase) + sinf(rightPhase);
                interference = interference / 2.0f;
                
                float phase = interferencePhase + interference * 0.5f + PI;
                CRGB color = chromaticDispersionPalette(position, aberration, phase, intensity, ctx.palette, ctx.hue);
                
                float interferenceIntensity = 0.5f + 0.5f * interference;
                color.r = (uint8_t)(color.r * interferenceIntensity);
                color.g = (uint8_t)(color.g * interferenceIntensity);
                color.b = (uint8_t)(color.b * interferenceIntensity);
                
                ctx.leds[mirrorIdx] = color;
            }
        }
    }
}

// ============================================================================
// Effect Registration
// ============================================================================

uint8_t registerLGPChromaticEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;
    
    uint8_t count = 0;
    
    if (renderer->registerEffect(startId + count, "LGP Chromatic Lens", effectChromaticLens)) count++;
    if (renderer->registerEffect(startId + count, "LGP Chromatic Pulse", effectChromaticPulse)) count++;
    if (renderer->registerEffect(startId + count, "LGP Chromatic Interference", effectChromaticInterference)) count++;
    
    return count;
}

} // namespace effects
} // namespace lightwaveos

