#include "BlendingEngine.h"

#if FEATURE_BLENDING_ENGINE

// Week 1-2: Skeleton implementation with empty methods
// Week 5: Full implementation

// ====== CONSTRUCTOR ======
BlendingEngine::BlendingEngine()
    : m_active(false)
    , m_dualStripMode(DUAL_INDEPENDENT)
    , m_dualStripPhaseOffset(0)
    , m_zoneInteractionEnabled(false)
{
    clearBuffers();
}

// ====== BLEND MODES ======
CRGB BlendingEngine::blendPixels(CRGB base, CRGB blend, BlendMode mode, uint8_t alpha) {
    if (alpha == 0) return base;
    if (alpha < 255) {
        blend.nscale8(alpha);
    }

    switch (mode) {
        case BLEND_ADDITIVE:
            return base + blend;

        case BLEND_MULTIPLY:
            return CRGB(
                scale8(base.r, blend.r),
                scale8(base.g, blend.g),
                scale8(base.b, blend.b)
            );

        case BLEND_SCREEN:
            return CRGB(
                255 - scale8(255 - base.r, 255 - blend.r),
                255 - scale8(255 - base.g, 255 - blend.g),
                255 - scale8(255 - base.b, 255 - blend.b)
            );

        case BLEND_OVERLAY:
            // High contrast mix: multiply if base < 128, screen if base > 128
            return CRGB(
                (base.r < 128) ? scale8(base.r * 2, blend.r) : 255 - scale8(255 - base.r, 255 - blend.r),
                (base.g < 128) ? scale8(base.g * 2, blend.g) : 255 - scale8(255 - base.g, 255 - blend.g),
                (base.b < 128) ? scale8(base.b * 2, blend.b) : 255 - scale8(255 - base.b, 255 - blend.b)
            );
            
        case BLEND_LIGHTEN_ONLY:
            return CRGB(
                max(base.r, blend.r),
                max(base.g, blend.g),
                max(base.b, blend.b)
            );
            
        case BLEND_DARKEN_ONLY:
            return CRGB(
                min(base.r, blend.r),
                min(base.g, blend.g),
                min(base.b, blend.b)
            );

        case BLEND_ALPHA:
            // Standard alpha blending: blend * alpha + base * (1-alpha)
            // (Assumes alpha is already applied to blend via nscale8 above)
            return blend + base.nscale8(255 - alpha);

        case BLEND_OVERWRITE:
        default:
            return blend;
    }
}

// ====== DUAL-STRIP COORDINATION ======
void BlendingEngine::setDualStripMode(DualStripMode mode, float phaseOffset) {
    m_dualStripMode = mode;
    m_dualStripPhaseOffset = phaseOffset;
}

void BlendingEngine::applyDualStripCoordination(CRGB* strip1, CRGB* strip2, uint16_t length) {
    // Week 1-2: Stub - no coordination applied
    // Week 5: Implement dual-strip modes
    if (m_dualStripMode == DUAL_INDEPENDENT) {
        return; // No coordination needed
    }
}

// ====== ZONE INTERACTION ======
void BlendingEngine::enableZoneInteraction(bool enable) {
    m_zoneInteractionEnabled = enable;
}

void BlendingEngine::updateZoneInteractions(float deltaTime) {
    // Week 1-2: Stub
    // Week 5: Implement zone interaction physics
}

// ====== BUFFER MANAGEMENT ======
void BlendingEngine::clearBuffers() {
    fill_solid(m_compositeStrip1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
    fill_solid(m_compositeStrip2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
}

#endif // FEATURE_BLENDING_ENGINE
