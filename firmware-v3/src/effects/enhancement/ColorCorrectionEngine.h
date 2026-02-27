// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ColorCorrectionEngine.h
 * @brief Comprehensive color correction system for LED rendering
 *
 * ColorCorrectionEngine provides:
 * - Dual-mode correction (HSV saturation boost / RGB white reduction)
 * - Auto-exposure with BT.601 perceptual luminance
 * - Brown/warm color guardrail (LC_SelfContained pattern)
 * - LUT-based gamma correction
 * - NVS persistence for settings
 *
 * Usage:
 * @code
 * auto& engine = ColorCorrectionEngine::getInstance();
 * engine.setMode(CorrectionMode::RGB);  // or HSV, BOTH, OFF
 *
 * // At palette load:
 * engine.correctPalette(palette, paletteFlags);
 *
 * // Post-render (in render loop):
 * engine.processBuffer(leds, ledCount);
 * @endcode
 */

#pragma once

#include <FastLED.h>
#include <Preferences.h>
#include "../../palettes/Palettes_Master.h"

namespace lightwaveos {
namespace enhancement {

// Import palette flags from palettes namespace
using lightwaveos::palettes::PAL_WHITE_HEAVY;
using lightwaveos::palettes::PAL_EXCLUDED;

/**
 * @brief Correction mode for WHITE_HEAVY palettes
 */
enum class CorrectionMode : uint8_t {
    OFF = 0,    ///< No correction applied
    HSV = 1,    ///< HSV saturation boost (enforce minimum saturation)
    RGB = 2,    ///< RGB white reduction (LC-style, reduce white component)
    BOTH = 3    ///< Both HSV and RGB layered together
};

/**
 * @brief Configuration struct for color correction parameters
 *
 * Follows LC_SelfContained's rt_* variable pattern for runtime control.
 */
struct ColorCorrectionConfig {
    // === Mode Selection ===
    CorrectionMode mode = CorrectionMode::BOTH;  ///< Default: BOTH modes (HSV + RGB)

    // === HSV Mode Parameters ===
    uint8_t hsvMinSaturation = 120;  ///< 0-255, colors below this get boosted

    // === RGB Mode Parameters ===
    uint8_t rgbWhiteThreshold = 150;  ///< minRGB value to consider "whitish"
    uint8_t rgbTargetMin = 100;       ///< Target minimum RGB after correction

    // === Auto-Exposure Parameters ===
    bool autoExposureEnabled = false;
    uint8_t autoExposureTarget = 110;  ///< Target average luma (BT.601)

    // === Gamma Correction ===
    bool gammaEnabled = true;
    float gammaValue = 2.2f;  ///< Standard gamma (1.0-3.0)

    // === Brown Guardrail (LC_SelfContained pattern) ===
    bool brownGuardrailEnabled = false;
    uint8_t maxGreenPercentOfRed = 28;  ///< Max G as % of R for browns
    uint8_t maxBluePercentOfRed = 8;    ///< Max B as % of R for browns

    // === V-Clamping (White Accumulation Prevention) ===
    bool vClampEnabled = true;          ///< Enable brightness V-clamping
    uint8_t maxBrightness = 200;        ///< Max brightness (0-255, conservative 200)
    uint8_t saturationBoostAmount = 25; ///< Saturation boost after V-clamp (0-255)
};

/**
 * @class ColorCorrectionEngine
 * @brief Singleton for comprehensive color correction
 *
 * Integrates at two points in the render pipeline:
 * 1. Palette load time - Corrects WHITE_HEAVY palettes
 * 2. Post-render - Applies auto-exposure, guardrails, gamma
 *
 * Thread Safety: Call only from render thread (Core 1).
 */
class ColorCorrectionEngine {
public:
    /**
     * @brief Get singleton instance
     */
    static ColorCorrectionEngine& getInstance();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set full configuration
     */
    void setConfig(const ColorCorrectionConfig& config);

    /**
     * @brief Get mutable reference to configuration
     */
    ColorCorrectionConfig& getConfig();

    /**
     * @brief Get read-only configuration
     */
    const ColorCorrectionConfig& getConfig() const;

    // ========================================================================
    // MODE CONTROL (Shortcuts)
    // ========================================================================

    /**
     * @brief Set correction mode
     * @param mode OFF, HSV, RGB, or BOTH
     */
    void setMode(CorrectionMode mode);

    /**
     * @brief Get current correction mode
     */
    CorrectionMode getMode() const;

    // ========================================================================
    // PALETTE CORRECTION (At Load Time)
    // ========================================================================

    /**
     * @brief Apply correction to a palette based on its flags
     * @param palette Palette to correct (modified in place)
     * @param paletteFlags Palette metadata flags (PAL_WHITE_HEAVY, etc.)
     *
     * Only applies correction if PAL_WHITE_HEAVY flag is set and mode != OFF.
     */
    void correctPalette(CRGBPalette16& palette, uint8_t paletteFlags);

    // ========================================================================
    // BUFFER CORRECTION (Post-Render)
    // ========================================================================

    /**
     * @brief Apply full post-render correction pipeline to LED buffer
     * @param buffer LED buffer to process
     * @param count Number of LEDs
     *
     * Pipeline order: Auto-Exposure -> White/Brown Guardrail -> Gamma
     */
    void processBuffer(CRGB* buffer, uint16_t count);

    // ========================================================================
    // INDIVIDUAL CORRECTION STAGES (for testing/debugging)
    // ========================================================================

    /**
     * @brief Apply auto-exposure (BT.601 luminance-based)
     *
     * Calculates average perceptual brightness and scales down if above target.
     * Never boosts, only reduces to prevent blown-out frames.
     */
    void applyAutoExposure(CRGB* buffer, uint16_t count);

    /**
     * @brief Apply white guardrail to desaturated colors
     *
     * Based on mode:
     * - HSV: Boost saturation of low-saturation pixels
     * - RGB: Reduce white component (minimum RGB channel)
     */
    void applyWhiteGuardrail(CRGB* buffer, uint16_t count);

    /**
     * @brief Apply brown guardrail (LC_SelfContained pattern)
     *
     * Clamps green and blue channels relative to red for brownish colors,
     * preventing muddy/oversaturated warm tones.
     */
    void applyBrownGuardrail(CRGB* buffer, uint16_t count);

    /**
     * @brief Apply gamma correction using LUT
     */
    void applyGamma(CRGB* buffer, uint16_t count);

    /**
     * @brief Apply brightness V-clamping to prevent white saturation
     *
     * Clamps max(R,G,B) to maxV using hue-preserving proportional scaling.
     * Applied BEFORE white guardrail in pipeline.
     *
     * @param buffer LED buffer to process
     * @param count Number of LEDs
     * @param maxV Maximum brightness value (200 recommended)
     */
    void applyBrightnessClamp(CRGB* buffer, uint16_t count, uint8_t maxV);

    /**
     * @brief Apply saturation boost to maintain chromaticity after V-clamping
     *
     * Boosts saturation by fixed amount using rgb2hsv_approximate().
     * Applied AFTER V-clamping to restore color intensity.
     *
     * @param buffer LED buffer to process
     * @param count Number of LEDs
     * @param boostAmount Saturation increase (0-255)
     */
    void applySaturationBoost(CRGB* buffer, uint16_t count, uint8_t boostAmount);

    // ========================================================================
    // STATIC PALETTE CORRECTION METHODS
    // ========================================================================

    /**
     * @brief Apply HSV saturation boost to palette
     * @param palette Palette to modify
     * @param minSat Minimum saturation to enforce (0-255)
     */
    static void applyHSVSaturationBoost(CRGBPalette16& palette, uint8_t minSat);

    /**
     * @brief Apply RGB white curation to palette (LC-style)
     * @param palette Palette to modify
     * @param threshold MinRGB value to consider "whitish"
     * @param target Target minimum RGB after correction
     */
    static void applyRGBWhiteCuration(CRGBPalette16& palette,
                                       uint8_t threshold,
                                       uint8_t target);

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    /**
     * @brief Save current configuration to NVS
     */
    void saveToNVS();

    /**
     * @brief Load configuration from NVS
     */
    void loadFromNVS();

private:
    ColorCorrectionEngine();
    ColorCorrectionEngine(const ColorCorrectionEngine&) = delete;
    ColorCorrectionEngine& operator=(const ColorCorrectionEngine&) = delete;

    ColorCorrectionConfig m_config;

    // ========================================================================
    // LOOKUP TABLES
    // ========================================================================

    static uint8_t s_gammaLUT[256];       ///< Gamma correction table
    static uint8_t s_srgbLinearLUT[256];  ///< sRGB to linear conversion
    static bool s_lutsInitialized;

    /**
     * @brief Initialize LUTs (called once on first use)
     */
    void initLUTs();

    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================

    /**
     * @brief Check if color is "whitish" (low saturation, high value)
     */
    static bool isWhitish(const CRGB& c, uint8_t threshold);

    /**
     * @brief Check if color is "brownish" (R > G >= B)
     *
     * LC_SelfContained pattern: `(c.r > c.g) && (c.g >= c.b)`
     */
    static bool isBrownish(const CRGB& c);

    /**
     * @brief Calculate perceptual luminance using BT.601 coefficients
     * @return Luminance value 0-255
     *
     * Formula: Y = (77*R + 150*G + 29*B) >> 8
     */
    static uint8_t calculateLuma(const CRGB& c);
};

} // namespace enhancement
} // namespace lightwaveos
