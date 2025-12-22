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

constexpr int STRIP_LENGTH = 160;
constexpr int CENTER_LEFT = 79;
constexpr int CENTER_RIGHT = 80;
constexpr int HALF_LENGTH = 80;
// PI is defined in Arduino.h

// Animation state (persistent across frames)
static float lensPhase = 0.0f;
static float pulsePhase = 0.0f;
static float interferencePhase = 0.0f;

// ============================================================================
// Physics-Accurate Chromatic Dispersion (Cauchy Equation)
// ============================================================================

CRGB chromaticDispersion(float position, float aberration, float phase, float intensity) {
    // Normalize position from centre (0.0 = centre, 1.0 = edge)
    float distFromCenter = fabsf(position - 79.5f);
    float normalizedDist = distFromCenter / 79.5f;
    
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
        CRGB color = chromaticDispersion((float)i, aberration, lensPhase, intensity);
        ctx.leds[i] = color;
    }
    
    // Mirror to second strip if present
    if (ctx.numLeds >= STRIP_LENGTH * 2) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int mirrorIdx = STRIP_LENGTH * 2 - 1 - i;
            if (mirrorIdx < ctx.numLeds) {
                // Invert color channels for visual variety
                CRGB color = chromaticDispersion((float)i, aberration, lensPhase + PI, intensity);
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
        CRGB color = chromaticDispersion((float)i, aberration, phase, intensity);
        ctx.leds[i] = color;
    }
    
    // Mirror to second strip if present
    if (ctx.numLeds >= STRIP_LENGTH * 2) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int mirrorIdx = STRIP_LENGTH * 2 - 1 - i;
            if (mirrorIdx < ctx.numLeds) {
                CRGB color = chromaticDispersion((float)i, aberration, phase + PI * 0.5f, intensity);
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
        CRGB color = chromaticDispersion(position, aberration, phase, intensity);
        
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
                
                float distFromLeft = position / 79.5f;
                float distFromRight = (159.0f - position) / 79.5f;
                
                float leftPhase = interferencePhase - distFromLeft * PI * 2.0f;
                float rightPhase = interferencePhase - distFromRight * PI * 2.0f;
                
                float interference = sinf(leftPhase) + sinf(rightPhase);
                interference = interference / 2.0f;
                
                float phase = interferencePhase + interference * 0.5f + PI;
                CRGB color = chromaticDispersion(position, aberration, phase, intensity);
                
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

