#include "ColorEngine.h"

#if FEATURE_COLOR_ENGINE

// Palette extern declarations (avoid multiple definitions from PROGMEM headers)
extern const TProgmemRGBGradientPaletteRef gMasterPalettes[];
extern const uint8_t gMasterPaletteCount;

// Week 1-2: Skeleton implementation with empty methods
// Week 3: Full implementation

// ====== CONSTRUCTOR ======
ColorEngine::ColorEngine()
    : m_enabled(true)
    , m_active(false)
    , m_crossBlendEnabled(false)
    , m_blendFactor1(255)
    , m_blendFactor2(0)
    , m_blendFactor3(0)
    , m_crossBlendPaletteId1(0)
    , m_crossBlendPaletteId2(0)
    , m_crossBlendPaletteId3(-1)
    , m_rotationEnabled(false)
    , m_rotationSpeed(0.0f)
    , m_rotationPhase(0.0f)
    , m_diffusionEnabled(false)
    , m_diffusionAmount(0)
{
    // Initialize palettes to current palette (will be set externally)
    m_blendPalette1 = HeatColors_p;
    m_blendPalette2 = HeatColors_p;
    m_blendPalette3 = HeatColors_p;
}

// ====== CORE COLOR RETRIEVAL ======
CRGB ColorEngine::getColor(uint8_t paletteIndex, uint8_t brightness,
                           uint16_t ledIndex, TBlendType blendType) {
    if (!m_enabled) {
        extern CRGBPalette16 currentPalette;
        CRGB color = ColorFromPalette(currentPalette, paletteIndex, 255, blendType);
        color.nscale8(brightness);
        return color;
    }

    // Apply temporal rotation if enabled
    uint8_t rotatedIndex = paletteIndex;
    if (m_rotationEnabled) {
        rotatedIndex = paletteIndex + (uint8_t)m_rotationPhase;
    }

    // Use cross-palette blending if enabled
    if (m_crossBlendEnabled) {
        return blendPalettes(rotatedIndex, brightness);
    }

    // Fallback to current palette
    // Get color at full brightness first, then scale - preserves saturation
    extern CRGBPalette16 currentPalette;
    CRGB color = ColorFromPalette(currentPalette, rotatedIndex, 255, blendType);
    color.nscale8(brightness);
    return color;
}

// ====== CROSS-PALETTE BLENDING ======
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

void ColorEngine::setCrossBlendPalettes(uint8_t palette1Id, uint8_t palette2Id, int palette3Id) {
    if (palette1Id >= gMasterPaletteCount || palette2Id >= gMasterPaletteCount) {
        return;
    }
    m_crossBlendPaletteId1 = palette1Id;
    m_crossBlendPaletteId2 = palette2Id;
    m_crossBlendPaletteId3 = palette3Id;

    CRGBPalette16 p1(gMasterPalettes[palette1Id]);
    CRGBPalette16 p2(gMasterPalettes[palette2Id]);

    if (palette3Id >= 0 && palette3Id < gMasterPaletteCount) {
        CRGBPalette16 p3(gMasterPalettes[(uint8_t)palette3Id]);
        setBlendPalettes(p1, p2, &p3);
    } else {
        setBlendPalettes(p1, p2, nullptr);
    }
}

void ColorEngine::setBlendFactors(uint8_t pal1Amount, uint8_t pal2Amount,
                                  uint8_t pal3Amount) {
    m_blendFactor1 = pal1Amount;
    m_blendFactor2 = pal2Amount;
    m_blendFactor3 = pal3Amount;
}

CRGB ColorEngine::blendPalettes(uint8_t paletteIndex, uint8_t brightness) {
    // Get colors at FULL brightness first, then scale after blending - preserves saturation
    CRGB color1 = ColorFromPalette(m_blendPalette1, paletteIndex, 255);
    CRGB color2 = ColorFromPalette(m_blendPalette2, paletteIndex, 255);
    CRGB color3 = ColorFromPalette(m_blendPalette3, paletteIndex, 255);

    // Normalize blend factors to ensure they sum to 255
    uint16_t totalFactor = m_blendFactor1 + m_blendFactor2 + m_blendFactor3;
    if (totalFactor == 0) {
        return color1; // Avoid division by zero
    }

    // Weighted blend using 16-bit math for accuracy
    uint16_t r = (color1.r * m_blendFactor1 + color2.r * m_blendFactor2 + color3.r * m_blendFactor3) / totalFactor;
    uint16_t g = (color1.g * m_blendFactor1 + color2.g * m_blendFactor2 + color3.g * m_blendFactor3) / totalFactor;
    uint16_t b = (color1.b * m_blendFactor1 + color2.b * m_blendFactor2 + color3.b * m_blendFactor3) / totalFactor;

    // Apply brightness scaling AFTER blending - preserves color saturation
    CRGB result = CRGB(r, g, b);
    result.nscale8(brightness);
    return result;
}

// ====== TEMPORAL ROTATION ======
void ColorEngine::enableTemporalRotation(bool enable) {
    m_rotationEnabled = enable;
}

void ColorEngine::setRotationSpeed(float degreesPerFrame) {
    m_rotationSpeed = degreesPerFrame;
}

void ColorEngine::updateRotationPhase() {
    // Week 1-2: Stub
    // Week 3: Implement phase rotation
    if (m_rotationEnabled) {
        m_rotationPhase += m_rotationSpeed;
        if (m_rotationPhase >= 360.0f) {
            m_rotationPhase -= 360.0f;
        }
    }
}

// ====== COLOR DIFFUSION ======
void ColorEngine::enableDiffusion(bool enable) {
    m_diffusionEnabled = enable;
}

void ColorEngine::setDiffusionAmount(uint8_t amount) {
    m_diffusionAmount = amount;
}

void ColorEngine::applyDiffusion(CRGB* buffer, uint16_t ledCount) {
    if (!m_diffusionEnabled || m_diffusionAmount == 0) {
        return;
    }

    // Apply FastLED's blur1d() for Gaussian blur
    // diffusionAmount controls blur intensity (0-255)
    // Higher values = more blur
    blur1d(buffer, ledCount, m_diffusionAmount);
}

void ColorEngine::applyDiffusionToStrips() {
    if (!m_diffusionEnabled || m_diffusionAmount == 0) {
        return;
    }

    // Apply diffusion to both strips
    extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
    extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];

    applyDiffusion(strip1, HardwareConfig::STRIP1_LED_COUNT);
    applyDiffusion(strip2, HardwareConfig::STRIP2_LED_COUNT);
}

// ====== FRAME UPDATE ======
void ColorEngine::update() {
    // Week 1-2: Minimal update
    // Week 3: Update rotation phase, blend animation, etc.
    updateRotationPhase();
    m_active = m_enabled && (m_crossBlendEnabled || m_rotationEnabled || m_diffusionEnabled);
}

// ====== UTILITY ======
void ColorEngine::reset() {
    m_enabled = true;
    m_active = false;
    m_crossBlendEnabled = false;
    m_rotationEnabled = false;
    m_diffusionEnabled = false;
    m_rotationPhase = 0.0f;
    m_diffusionAmount = 0;
    m_crossBlendPaletteId1 = 0;
    m_crossBlendPaletteId2 = 0;
    m_crossBlendPaletteId3 = -1;
    m_blendFactor1 = 255;
    m_blendFactor2 = 0;
    m_blendFactor3 = 0;
}

#endif // FEATURE_COLOR_ENGINE
