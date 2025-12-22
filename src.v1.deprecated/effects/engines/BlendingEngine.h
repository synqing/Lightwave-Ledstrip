#ifndef BLENDING_ENGINE_H
#define BLENDING_ENGINE_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../config/features.h"

#if FEATURE_BLENDING_ENGINE

// BlendingEngine - Advanced zone/layer compositing for visual enhancements
// Week 1-2: Skeleton implementation (empty methods)
// Week 5: Full implementation with blend modes, layer ordering, zone interactions

// ====== BLEND MODES ======
enum BlendMode {
    BLEND_OVERWRITE,         // Default: replace (current behavior)
    BLEND_ADDITIVE,          // Add colors (light accumulation)
    BLEND_MULTIPLY,          // Multiply color channels (shadow/filter)
    BLEND_SCREEN,            // Inverse multiply (lighten)
    BLEND_OVERLAY,           // Combination of multiply and screen
    BLEND_ALPHA,             // Traditional alpha blending
    BLEND_LIGHTEN_ONLY,      // Take brighter pixel
    BLEND_DARKEN_ONLY,       // Take darker pixel
    BLEND_MODE_COUNT
};

// ====== DUAL-STRIP COORDINATION ======
enum DualStripMode {
    DUAL_INDEPENDENT,        // Strips run independently
    DUAL_SYNCHRONIZED,       // Both strips identical
    DUAL_PHASE_LOCKED,       // Same effect, phase-shifted
    DUAL_ANTI_PHASE,         // 180° phase offset
    DUAL_INTERFERENCE        // Wave interference patterns
};

// ====== BLENDING ENGINE ======
class BlendingEngine {
public:
    // Singleton access pattern
    static BlendingEngine& getInstance() {
        static BlendingEngine instance;
        return instance;
    }

    // ====== BLEND MODES ======
    static CRGB blendPixels(CRGB base, CRGB blend, BlendMode mode, uint8_t alpha = 255);

    // ====== DUAL-STRIP COORDINATION ======
    void setDualStripMode(DualStripMode mode, float phaseOffset = 0);
    void applyDualStripCoordination(CRGB* strip1, CRGB* strip2, uint16_t length);

    // ====== ZONE INTERACTION ======
    void enableZoneInteraction(bool enable);
    void updateZoneInteractions(float deltaTime);

    // ====== BUFFER MANAGEMENT ======
    void clearBuffers();
    CRGB* getCompositeStrip1() { return m_compositeStrip1; }
    CRGB* getCompositeStrip2() { return m_compositeStrip2; }

    // ====== UTILITY ======
    bool isActive() const { return m_active; }

private:
    // Private constructor (singleton pattern)
    BlendingEngine();
    BlendingEngine(const BlendingEngine&) = delete;
    BlendingEngine& operator=(const BlendingEngine&) = delete;

    // ====== INTERNAL STATE ======
    bool m_active;

    // Dual-strip coordination
    DualStripMode m_dualStripMode;
    float m_dualStripPhaseOffset;

    // Zone interaction
    bool m_zoneInteractionEnabled;

    // Temporary buffers for multi-layer compositing
    CRGB m_compositeStrip1[HardwareConfig::STRIP_LENGTH];
    CRGB m_compositeStrip2[HardwareConfig::STRIP_LENGTH];
};

#endif // FEATURE_BLENDING_ENGINE
#endif // BLENDING_ENGINE_H
