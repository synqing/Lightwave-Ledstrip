#ifndef COLOR_ENGINE_H
#define COLOR_ENGINE_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../config/features.h"

#if FEATURE_COLOR_ENGINE

// ColorEngine - Advanced color manipulation for visual enhancements
// Week 1-2: Skeleton implementation (empty methods)
// Week 3: Full implementation with cross-palette blending, diffusion, temporal rotation

class ColorEngine {
public:
    // Singleton access pattern
    static ColorEngine& getInstance() {
        static ColorEngine instance;
        return instance;
    }

    // ====== CORE COLOR RETRIEVAL ======
    // Enhanced ColorFromPalette with all features applied
    CRGB getColor(uint8_t paletteIndex, uint8_t brightness = 255,
                  uint16_t ledIndex = 0, TBlendType blendType = LINEARBLEND);

    // ====== CROSS-PALETTE BLENDING ======
    void enableCrossBlend(bool enable);
    void setBlendPalettes(const CRGBPalette16& pal1,
                          const CRGBPalette16& pal2,
                          const CRGBPalette16* pal3 = nullptr);
    void setBlendFactors(uint8_t pal1Amount, uint8_t pal2Amount,
                         uint8_t pal3Amount = 0);
    bool isCrossBlendEnabled() const { return m_crossBlendEnabled; }
    uint8_t getCrossBlendPalette1() const { return m_crossBlendPaletteId1; }
    uint8_t getCrossBlendPalette2() const { return m_crossBlendPaletteId2; }
    int getCrossBlendPalette3() const { return m_crossBlendPaletteId3; } // -1 if unset
    uint8_t getBlendFactor1() const { return m_blendFactor1; }
    uint8_t getBlendFactor2() const { return m_blendFactor2; }
    uint8_t getBlendFactor3() const { return m_blendFactor3; }

    // Set palettes by ID using the master palette table.
    void setCrossBlendPalettes(uint8_t palette1Id, uint8_t palette2Id, int palette3Id = -1);

    // ====== TEMPORAL PALETTE ROTATION ======
    void enableTemporalRotation(bool enable);
    void setRotationSpeed(float degreesPerFrame);
    float getRotationPhase() const { return m_rotationPhase; }
    bool isTemporalRotationEnabled() const { return m_rotationEnabled; }
    float getRotationSpeed() const { return m_rotationSpeed; }

    // ====== COLOR DIFFUSION ======
    void enableDiffusion(bool enable);
    void setDiffusionAmount(uint8_t amount);
    void applyDiffusion(CRGB* buffer, uint16_t ledCount);
    void applyDiffusionToStrips();

    // ====== FRAME UPDATE ======
    void update();

    // ====== UTILITY ======
    void reset();
    void setEnabled(bool enable) { m_enabled = enable; }
    bool isEnabled() const { return m_enabled; }
    bool isActive() const { return m_active; }
    bool isDiffusionEnabled() const { return m_diffusionEnabled; }
    uint8_t getDiffusionAmount() const { return m_diffusionAmount; }

private:
    // Private constructor (singleton pattern)
    ColorEngine();
    ColorEngine(const ColorEngine&) = delete;
    ColorEngine& operator=(const ColorEngine&) = delete;

    // ====== INTERNAL STATE ======
    bool m_enabled;
    bool m_active;

    // Cross-palette blending state
    bool m_crossBlendEnabled;
    CRGBPalette16 m_blendPalette1;
    CRGBPalette16 m_blendPalette2;
    CRGBPalette16 m_blendPalette3;
    uint8_t m_blendFactor1;
    uint8_t m_blendFactor2;
    uint8_t m_blendFactor3;
    uint8_t m_crossBlendPaletteId1;
    uint8_t m_crossBlendPaletteId2;
    int m_crossBlendPaletteId3; // -1 if unset

    // Temporal rotation state
    bool m_rotationEnabled;
    float m_rotationSpeed;
    float m_rotationPhase;

    // Diffusion state
    bool m_diffusionEnabled;
    uint8_t m_diffusionAmount;

    // ====== INTERNAL HELPERS ======
    CRGB blendPalettes(uint8_t paletteIndex, uint8_t brightness);
    void updateRotationPhase();
};

#endif // FEATURE_COLOR_ENGINE
#endif // COLOR_ENGINE_H
