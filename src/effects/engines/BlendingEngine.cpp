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
    // Week 1-2: Only implement OVERWRITE
    // Week 5: Implement all 8+ blend modes
    switch (mode) {
        case BLEND_ADDITIVE:
            // Week 5: Implement additive blending
            return base;

        case BLEND_MULTIPLY:
            // Week 5: Implement multiply blending
            return base;

        case BLEND_SCREEN:
            // Week 5: Implement screen blending
            return base;

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
