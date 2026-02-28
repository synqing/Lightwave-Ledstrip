/**
 * @file ColorCorrectionEngine.cpp
 * @brief Implementation of comprehensive color correction system
 *
 * Implements dual-mode correction (HSV saturation boost / RGB white reduction),
 * auto-exposure with BT.601 perceptual luminance, brown guardrail, and gamma correction.
 *
 * Pipeline order (from LC_SelfContained):
 * Effect → Auto-Exposure → White/Brown Guardrail → Gamma → show()
 */

#include "ColorCorrectionEngine.h"
#include <algorithm>
#include <esp_log.h>

static const char* TAG = "ColorCorrection";

namespace lightwaveos {
namespace enhancement {

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

uint8_t ColorCorrectionEngine::s_gammaLUT[256] = {0};
uint8_t ColorCorrectionEngine::s_srgbLinearLUT[256] = {0};
bool ColorCorrectionEngine::s_lutsInitialized = false;

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

ColorCorrectionEngine& ColorCorrectionEngine::getInstance() {
    static ColorCorrectionEngine instance;
    return instance;
}

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ColorCorrectionEngine::ColorCorrectionEngine() {
    initLUTs();
    loadFromNVS();
}

// ============================================================================
// LUT INITIALIZATION
// ============================================================================

void ColorCorrectionEngine::initLUTs() {
    if (s_lutsInitialized) return;

    // Generate gamma 2.2 correction LUT
    for (uint16_t i = 0; i < 256; ++i) {
        float normalized = (float)i / 255.0f;
        float gammaCorrected = powf(normalized, m_config.gammaValue);
        s_gammaLUT[i] = (uint8_t)(gammaCorrected * 255.0f + 0.5f);
    }

    // Generate sRGB to linear conversion LUT
    for (uint16_t i = 0; i < 256; ++i) {
        float srgb = (float)i / 255.0f;
        float linear;
        if (srgb <= 0.04045f) {
            linear = srgb / 12.92f;
        } else {
            linear = powf((srgb + 0.055f) / 1.055f, 2.4f);
        }
        s_srgbLinearLUT[i] = (uint8_t)(linear * 255.0f + 0.5f);
    }

    s_lutsInitialized = true;
    ESP_LOGI(TAG, "LUTs initialized (gamma=%.1f)", m_config.gammaValue);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ColorCorrectionEngine::setConfig(const ColorCorrectionConfig& config) {
    m_config = config;

    // Regenerate gamma LUT if gamma value changed
    if (config.gammaEnabled) {
        for (uint16_t i = 0; i < 256; ++i) {
            float normalized = (float)i / 255.0f;
            s_gammaLUT[i] = (uint8_t)(powf(normalized, m_config.gammaValue) * 255.0f + 0.5f);
        }
    }
}

ColorCorrectionConfig& ColorCorrectionEngine::getConfig() {
    return m_config;
}

const ColorCorrectionConfig& ColorCorrectionEngine::getConfig() const {
    return m_config;
}

// ============================================================================
// MODE CONTROL
// ============================================================================

void ColorCorrectionEngine::setMode(CorrectionMode mode) {
    m_config.mode = mode;
    ESP_LOGI(TAG, "Mode set to: %d", (int)mode);
}

CorrectionMode ColorCorrectionEngine::getMode() const {
    return m_config.mode;
}

// ============================================================================
// PALETTE CORRECTION (At Load Time)
// ============================================================================

void ColorCorrectionEngine::correctPalette(CRGBPalette16& palette, uint8_t paletteFlags) {
    // Skip if mode is OFF or palette doesn't have WHITE_HEAVY flag
    if (m_config.mode == CorrectionMode::OFF) return;
    if (!(paletteFlags & PAL_WHITE_HEAVY)) return;

    ESP_LOGD(TAG, "Correcting WHITE_HEAVY palette (mode=%d)", (int)m_config.mode);

    if (m_config.mode == CorrectionMode::HSV || m_config.mode == CorrectionMode::BOTH) {
        applyHSVSaturationBoost(palette, m_config.hsvMinSaturation);
    }

    if (m_config.mode == CorrectionMode::RGB || m_config.mode == CorrectionMode::BOTH) {
        applyRGBWhiteCuration(palette, m_config.rgbWhiteThreshold, m_config.rgbTargetMin);
    }
}

// ============================================================================
// HSV SATURATION BOOST (Static)
// ============================================================================

void ColorCorrectionEngine::applyHSVSaturationBoost(CRGBPalette16& palette, uint8_t minSat) {
    for (int i = 0; i < 16; i++) {
        CHSV hsv = rgb2hsv_approximate(palette[i]);

        // Only boost if below minimum saturation and not too dark
        if (hsv.s < minSat && hsv.v > 64) {
            hsv.s = minSat;
            palette[i] = CRGB(hsv);  // Convert back using CHSV constructor
        }
    }
}

// ============================================================================
// RGB WHITE CURATION (Static, LC-Style)
// ============================================================================

void ColorCorrectionEngine::applyRGBWhiteCuration(CRGBPalette16& palette,
                                                   uint8_t threshold,
                                                   uint8_t target) {
    for (int i = 0; i < 16; i++) {
        CRGB& c = palette[i];

        // Find min and max RGB components
        uint8_t minVal = std::min(c.r, std::min(c.g, c.b));
        uint8_t maxVal = std::max(c.r, std::max(c.g, c.b));

        // Check if "whitish" (high minimum, low spread between min/max)
        if (minVal > threshold && (maxVal - minVal) < 40) {
            if (minVal > target) {
                uint8_t diff = minVal - target;
                // Reduce all channels proportionally to remove white component
                c.r = (c.r > diff) ? c.r - diff : 0;
                c.g = (c.g > diff) ? c.g - diff : 0;
                c.b = (c.b > diff) ? c.b - diff : 0;
            }
        }
    }
}

// ============================================================================
// BUFFER CORRECTION (Post-Render Pipeline)
// ============================================================================

void ColorCorrectionEngine::processBuffer(CRGB* buffer, uint16_t count) {
    // Pipeline order (updated for white accumulation prevention):
    // 1. Auto-Exposure (if enabled)
    // 2. V-Clamping (NEW - prevents white accumulation from qadd8)
    // 3. Saturation Boost (NEW - restores chromaticity after V-clamp)
    // 4. White Guardrail (if mode != OFF)
    // 5. Brown Guardrail (if enabled)
    // 6. Gamma Correction (if enabled)

    if (m_config.autoExposureEnabled) {
        applyAutoExposure(buffer, count);
    }

    // V-Clamping: Cap brightness to prevent white saturation
    if (m_config.vClampEnabled) {
        applyBrightnessClamp(buffer, count, m_config.maxBrightness);
    }

    // Saturation Boost: Enhance chromaticity across all effects.
    // Runs unconditionally (not gated on maxBrightness) — the +25 S bump
    // is an intentional part of the visual pipeline, not just V-clamp recovery.
    if (m_config.vClampEnabled && m_config.saturationBoostAmount > 0) {
        applySaturationBoost(buffer, count, m_config.saturationBoostAmount);
    }

    if (m_config.mode != CorrectionMode::OFF) {
        applyWhiteGuardrail(buffer, count);
    }

    if (m_config.brownGuardrailEnabled) {
        applyBrownGuardrail(buffer, count);
    }

    if (m_config.gammaEnabled) {
        applyGamma(buffer, count);
    }
}

// ============================================================================
// AUTO-EXPOSURE (BT.601 Luminance-Based)
// ============================================================================

void ColorCorrectionEngine::applyAutoExposure(CRGB* buffer, uint16_t count) {
    if (!m_config.autoExposureEnabled || count == 0) return;

    // Calculate average perceptual luminance using BT.601 coefficients
    // Optimization: Sample every 4th LED to save 75% iteration overhead
    // Visual difference is imperceptible since we're computing an average
    constexpr uint16_t SAMPLE_STRIDE = 4;
    uint32_t sumLuma = 0;
    uint16_t sampleCount = 0;
    for (uint16_t i = 0; i < count; i += SAMPLE_STRIDE) {
        sumLuma += calculateLuma(buffer[i]);
        sampleCount++;
    }
    uint8_t avgLuma = (sampleCount > 0) ? (sumLuma / sampleCount) : 0;

    // Only downscale if above target (never boost to prevent blown-out frames)
    if (avgLuma > m_config.autoExposureTarget && avgLuma > 0) {
        uint8_t factor = (uint16_t)m_config.autoExposureTarget * 255 / avgLuma;

        // Use FastLED's optimized scaling
        nscale8_video(buffer, count, factor);
    }
}

// ============================================================================
// WHITE GUARDRAIL (Per-Pixel)
// ============================================================================

void ColorCorrectionEngine::applyWhiteGuardrail(CRGB* buffer, uint16_t count) {
    if (m_config.mode == CorrectionMode::OFF) return;

    for (uint16_t i = 0; i < count; i++) {
        CRGB& c = buffer[i];

        // Check if whitish (low saturation, high value)
        if (!isWhitish(c, m_config.rgbWhiteThreshold)) continue;

        if (m_config.mode == CorrectionMode::HSV || m_config.mode == CorrectionMode::BOTH) {
            // HSV: Boost saturation of desaturated pixels
            CHSV hsv = rgb2hsv_approximate(c);
            if (hsv.s < m_config.hsvMinSaturation && hsv.v > 64) {
                hsv.s = m_config.hsvMinSaturation;
                c = CRGB(hsv);
            }
        }

        if (m_config.mode == CorrectionMode::RGB || m_config.mode == CorrectionMode::BOTH) {
            // RGB: Reduce white component
            uint8_t minVal = std::min(c.r, std::min(c.g, c.b));
            if (minVal > m_config.rgbTargetMin) {
                uint8_t reduction = minVal - m_config.rgbTargetMin;
                c.r = (c.r > reduction) ? c.r - reduction : 0;
                c.g = (c.g > reduction) ? c.g - reduction : 0;
                c.b = (c.b > reduction) ? c.b - reduction : 0;
            }
        }
    }
}

// ============================================================================
// BROWN GUARDRAIL (LC_SelfContained Pattern)
// ============================================================================

void ColorCorrectionEngine::applyBrownGuardrail(CRGB* buffer, uint16_t count) {
    if (!m_config.brownGuardrailEnabled) return;

    for (uint16_t i = 0; i < count; i++) {
        CRGB& c = buffer[i];

        // Only apply to brownish colors: R > G >= B
        if (!isBrownish(c)) continue;

        // Clamp green and blue relative to red
        uint8_t maxG = (uint16_t)c.r * m_config.maxGreenPercentOfRed / 100;
        uint8_t maxB = (uint16_t)c.r * m_config.maxBluePercentOfRed / 100;

        if (c.g > maxG) c.g = maxG;
        if (c.b > maxB) c.b = maxB;
    }
}

// ============================================================================
// GAMMA CORRECTION (LUT-Based)
// ============================================================================

void ColorCorrectionEngine::applyGamma(CRGB* buffer, uint16_t count) {
    if (!m_config.gammaEnabled) return;

    for (uint16_t i = 0; i < count; i++) {
        buffer[i].r = s_gammaLUT[buffer[i].r];
        buffer[i].g = s_gammaLUT[buffer[i].g];
        buffer[i].b = s_gammaLUT[buffer[i].b];
    }
}

// ============================================================================
// BRIGHTNESS V-CLAMPING (White Accumulation Prevention)
// ============================================================================

void ColorCorrectionEngine::applyBrightnessClamp(CRGB* buffer, uint16_t count, uint8_t maxV) {
    if (maxV >= 255) return;  // No clamping needed

    for (uint16_t i = 0; i < count; i++) {
        CRGB& c = buffer[i];

        // Find maximum channel (brightness proxy)
        uint8_t maxChannel = std::max(c.r, std::max(c.g, c.b));

        // Only clamp if above threshold
        if (maxChannel > maxV) {
            // Hue-preserving proportional scaling
            // Scale factor: (maxV * 256) / maxChannel (fixed-point 8.8)
            uint16_t scaleFactor = ((uint16_t)maxV << 8) / maxChannel;

            // Apply proportional scaling to all channels (preserves hue ratio)
            c.r = (uint8_t)(((uint16_t)c.r * scaleFactor) >> 8);
            c.g = (uint8_t)(((uint16_t)c.g * scaleFactor) >> 8);
            c.b = (uint8_t)(((uint16_t)c.b * scaleFactor) >> 8);
        }
    }
}

// ============================================================================
// POST-CLAMP SATURATION BOOST (Maintains Chromaticity)
// ============================================================================

void ColorCorrectionEngine::applySaturationBoost(CRGB* buffer, uint16_t count, uint8_t boostAmount) {
    if (boostAmount == 0) return;

    for (uint16_t i = 0; i < count; i++) {
        CRGB& c = buffer[i];

        // Skip very dark pixels (avoid divide-by-zero in rgb2hsv_approximate)
        uint8_t maxChannel = std::max(c.r, std::max(c.g, c.b));
        if (maxChannel < 16) continue;

        // Convert to HSV, boost saturation, convert back
        CHSV hsv = rgb2hsv_approximate(c);
        hsv.s = qadd8(hsv.s, boostAmount);  // Saturating add prevents overflow
        hsv2rgb_spectrum(hsv, c);
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool ColorCorrectionEngine::isWhitish(const CRGB& c, uint8_t threshold) {
    uint8_t minVal = std::min(c.r, std::min(c.g, c.b));
    uint8_t maxVal = std::max(c.r, std::max(c.g, c.b));

    // Whitish: high minimum value and low spread (desaturated)
    return (minVal > threshold) && (maxVal - minVal) < 40;
}

bool ColorCorrectionEngine::isBrownish(const CRGB& c) {
    // LC_SelfContained pattern: (c.r > c.g) && (c.g >= c.b)
    return (c.r > c.g) && (c.g >= c.b);
}

uint8_t ColorCorrectionEngine::calculateLuma(const CRGB& c) {
    // ITU-R BT.601 luminance coefficients: 0.299R + 0.587G + 0.114B
    // Scaled: (77*R + 150*G + 29*B) >> 8
    return (77 * c.r + 150 * c.g + 29 * c.b) >> 8;
}

// ============================================================================
// NVS PERSISTENCE
// ============================================================================

void ColorCorrectionEngine::saveToNVS() {
    Preferences prefs;
    if (prefs.begin("colorCorr", false)) {
        prefs.putUChar("mode", (uint8_t)m_config.mode);
        prefs.putUChar("hsvMinSat", m_config.hsvMinSaturation);
        prefs.putUChar("rgbThresh", m_config.rgbWhiteThreshold);
        prefs.putUChar("rgbTarget", m_config.rgbTargetMin);
        prefs.putBool("aeEnabled", m_config.autoExposureEnabled);
        prefs.putUChar("aeTarget", m_config.autoExposureTarget);
        prefs.putBool("gammaEn", m_config.gammaEnabled);
        prefs.putFloat("gammaVal", m_config.gammaValue);
        prefs.putBool("brownEn", m_config.brownGuardrailEnabled);
        prefs.putUChar("brownG", m_config.maxGreenPercentOfRed);
        prefs.putUChar("brownB", m_config.maxBluePercentOfRed);
        // V-Clamping settings (white accumulation prevention)
        prefs.putBool("vClampEn", m_config.vClampEnabled);
        prefs.putUChar("maxBright", m_config.maxBrightness);
        prefs.putUChar("satBoost", m_config.saturationBoostAmount);
        prefs.end();
        ESP_LOGI(TAG, "Settings saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for write");
    }
}

void ColorCorrectionEngine::loadFromNVS() {
    Preferences prefs;
    if (prefs.begin("colorCorr", true)) {
        m_config.mode = (CorrectionMode)prefs.getUChar("mode", (uint8_t)CorrectionMode::RGB);
        m_config.hsvMinSaturation = prefs.getUChar("hsvMinSat", 120);
        m_config.rgbWhiteThreshold = prefs.getUChar("rgbThresh", 150);
        m_config.rgbTargetMin = prefs.getUChar("rgbTarget", 100);
        m_config.autoExposureEnabled = prefs.getBool("aeEnabled", false);
        m_config.autoExposureTarget = prefs.getUChar("aeTarget", 110);
        m_config.gammaEnabled = prefs.getBool("gammaEn", true);
        m_config.gammaValue = prefs.getFloat("gammaVal", 2.2f);
        m_config.brownGuardrailEnabled = prefs.getBool("brownEn", false);
        m_config.maxGreenPercentOfRed = prefs.getUChar("brownG", 28);
        m_config.maxBluePercentOfRed = prefs.getUChar("brownB", 8);
        // V-Clamping settings (white accumulation prevention)
        m_config.vClampEnabled = prefs.getBool("vClampEn", true);
        m_config.maxBrightness = prefs.getUChar("maxBright", 255);
        m_config.saturationBoostAmount = prefs.getUChar("satBoost", 25);
        prefs.end();
        ESP_LOGI(TAG, "Settings loaded from NVS (mode=%d, vClamp=%d)",
                 (int)m_config.mode, m_config.vClampEnabled);
    } else {
        ESP_LOGW(TAG, "NVS not found, using defaults");
    }
}

} // namespace enhancement
} // namespace lightwaveos
