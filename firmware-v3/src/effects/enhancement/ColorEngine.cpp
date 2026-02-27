/**
 * @file ColorEngine.cpp
 * @brief Implementation of advanced color manipulation engine
 *
 * Ported from LightwaveOS v1 to v2 architecture.
 */

#include "ColorEngine.h"

#if FEATURE_COLOR_ENGINE

namespace lightwaveos {
namespace enhancement {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ColorEngine::ColorEngine()
    : m_active(false)
    , m_crossBlendEnabled(false)
    , m_blendFactor1(255)
    , m_blendFactor2(0)
    , m_blendFactor3(0)
    , m_rotationEnabled(false)
    , m_rotationSpeed(0.0f)
    , m_rotationPhase(0.0f)
    , m_diffusionEnabled(false)
    , m_diffusionAmount(0)
{
    // Initialize palettes to defaults
    m_blendPalette1 = HeatColors_p;
    m_blendPalette2 = HeatColors_p;
    m_blendPalette3 = HeatColors_p;
}

// ============================================================================
// CORE COLOR RETRIEVAL
// ============================================================================

CRGB ColorEngine::getColor(uint8_t paletteIndex, uint8_t brightness,
                           uint16_t ledIndex, TBlendType blendType) {
    // Suppress unused parameter warning
    (void)ledIndex;

    // Apply temporal rotation if enabled
    uint8_t rotatedIndex = paletteIndex;
    if (m_rotationEnabled) {
        // Convert phase (0-360) to palette index offset (0-255)
        rotatedIndex = paletteIndex + (uint8_t)(m_rotationPhase * 255.0f / 360.0f);
    }

    // Use cross-palette blending if enabled
    if (m_crossBlendEnabled) {
        return blendPalettes(rotatedIndex, brightness);
    }

    // Fallback to current palette (extern from main.cpp)
    extern CRGBPalette16 currentPalette;
    return ColorFromPalette(currentPalette, rotatedIndex, brightness, blendType);
}

CRGB ColorEngine::applyBlend(const CRGB& source, const CRGB& target, uint8_t amount) {
    // Use FastLED's optimized blend function
    return blend(source, target, amount);
}

// ============================================================================
// CROSS-PALETTE BLENDING
// ============================================================================

void ColorEngine::enableCrossBlend(bool enable) {
    m_crossBlendEnabled = enable;
}

void ColorEngine::setBlendPalettes(const CRGBPalette16& pal1,
                                   const CRGBPalette16& pal2,
                                   const CRGBPalette16* pal3) {
    m_blendPalette1 = pal1;
    m_blendPalette2 = pal2;
    if (pal3) {
        m_blendPalette3 = *pal3;
    }
}

void ColorEngine::setBlendFactors(uint8_t pal1Amount, uint8_t pal2Amount,
                                  uint8_t pal3Amount) {
    m_blendFactor1 = pal1Amount;
    m_blendFactor2 = pal2Amount;
    m_blendFactor3 = pal3Amount;
}

CRGB ColorEngine::blendPalettes(uint8_t paletteIndex, uint8_t brightness) {
    // Get colors from each palette
    CRGB color1 = ColorFromPalette(m_blendPalette1, paletteIndex, brightness, LINEARBLEND);
    CRGB color2 = ColorFromPalette(m_blendPalette2, paletteIndex, brightness, LINEARBLEND);
    CRGB color3 = ColorFromPalette(m_blendPalette3, paletteIndex, brightness, LINEARBLEND);

    // Normalize blend factors to ensure they sum to ~255
    uint16_t totalFactor = m_blendFactor1 + m_blendFactor2 + m_blendFactor3;
    if (totalFactor == 0) {
        return color1; // Avoid division by zero
    }

    // Weighted blend using 16-bit math for accuracy
    uint16_t r = ((uint16_t)color1.r * m_blendFactor1 +
                  (uint16_t)color2.r * m_blendFactor2 +
                  (uint16_t)color3.r * m_blendFactor3) / totalFactor;
    uint16_t g = ((uint16_t)color1.g * m_blendFactor1 +
                  (uint16_t)color2.g * m_blendFactor2 +
                  (uint16_t)color3.g * m_blendFactor3) / totalFactor;
    uint16_t b = ((uint16_t)color1.b * m_blendFactor1 +
                  (uint16_t)color2.b * m_blendFactor2 +
                  (uint16_t)color3.b * m_blendFactor3) / totalFactor;

    return CRGB(r, g, b);
}

// ============================================================================
// TEMPORAL ROTATION
// ============================================================================

void ColorEngine::enableTemporalRotation(bool enable) {
    m_rotationEnabled = enable;
}

void ColorEngine::setRotationSpeed(float degreesPerFrame) {
    m_rotationSpeed = degreesPerFrame;
}

void ColorEngine::updateRotationPhase() {
    if (m_rotationEnabled) {
        m_rotationPhase += m_rotationSpeed;
        // Wrap to 0-360 range
        while (m_rotationPhase >= 360.0f) {
            m_rotationPhase -= 360.0f;
        }
        while (m_rotationPhase < 0.0f) {
            m_rotationPhase += 360.0f;
        }
    }
}

// ============================================================================
// COLOR DIFFUSION
// ============================================================================

void ColorEngine::enableDiffusion(bool enable) {
    m_diffusionEnabled = enable;
}

void ColorEngine::setDiffusionAmount(uint8_t amount) {
    m_diffusionAmount = amount;
}

void ColorEngine::applyDiffusion(CRGB* buffer, uint16_t ledCount) {
    if (!m_diffusionEnabled || m_diffusionAmount == 0 || buffer == nullptr) {
        return;
    }

    // Apply FastLED's blur1d() for Gaussian blur
    // diffusionAmount controls blur intensity (0-255)
    // Higher values = more blur (more neighbor color mixing)
    blur1d(buffer, ledCount, m_diffusionAmount);
}

// ============================================================================
// FRAME UPDATE
// ============================================================================

void ColorEngine::update() {
    updateRotationPhase();
    m_active = m_crossBlendEnabled || m_rotationEnabled || m_diffusionEnabled;
}

// ============================================================================
// UTILITY
// ============================================================================

void ColorEngine::reset() {
    m_active = false;
    m_crossBlendEnabled = false;
    m_rotationEnabled = false;
    m_diffusionEnabled = false;
    m_rotationPhase = 0.0f;
    m_diffusionAmount = 0;
    m_blendFactor1 = 255;
    m_blendFactor2 = 0;
    m_blendFactor3 = 0;
    m_rotationSpeed = 0.0f;
}

} // namespace enhancement
} // namespace lightwaveos

#endif // FEATURE_COLOR_ENGINE
