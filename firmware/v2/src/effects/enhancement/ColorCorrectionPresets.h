/**
 * @file ColorCorrectionPresets.h
 * @brief Built-in presets for ColorCorrectionEngine
 *
 * Provides 4 curated presets for common use cases:
 * - OFF: All corrections disabled
 * - SUBTLE: Minimal processing, RGB mode only
 * - BALANCED: Default recommended settings
 * - AGGRESSIVE: Maximum correction for challenging palettes
 */

#pragma once

#include "ColorCorrectionEngine.h"

namespace lightwaveos {
namespace enhancement {

/**
 * @brief Preset identifiers for quick configuration
 */
enum class ColorCorrectionPreset : uint8_t {
    OFF = 0,        ///< All corrections disabled
    SUBTLE = 1,     ///< Light touch - RGB only, no auto-exposure
    BALANCED = 2,   ///< Recommended default settings
    AGGRESSIVE = 3  ///< Maximum correction for problem palettes
};

/**
 * @brief Get preset name as string
 */
inline const char* getPresetName(ColorCorrectionPreset preset) {
    switch (preset) {
        case ColorCorrectionPreset::OFF:        return "Off";
        case ColorCorrectionPreset::SUBTLE:     return "Subtle";
        case ColorCorrectionPreset::BALANCED:   return "Balanced";
        case ColorCorrectionPreset::AGGRESSIVE: return "Aggressive";
        default:                                return "Unknown";
    }
}

/**
 * @brief Get total number of presets
 */
inline uint8_t getPresetCount() { return 4; }

/**
 * @brief Get configuration for a preset
 * @param preset The preset to retrieve
 * @return Configuration struct for the preset
 */
inline ColorCorrectionConfig getPresetConfig(ColorCorrectionPreset preset) {
    ColorCorrectionConfig config;

    switch (preset) {
        case ColorCorrectionPreset::OFF:
            // All corrections disabled
            config.mode = CorrectionMode::OFF;
            config.autoExposureEnabled = false;
            config.gammaEnabled = false;
            config.brownGuardrailEnabled = false;
            config.vClampEnabled = false;
            config.ditheringEnabled = false;
            config.spectralCorrectionEnabled = false;
            config.laceEnabled = false;
            break;

        case ColorCorrectionPreset::SUBTLE:
            // Light touch - RGB mode, no auto-exposure, minimal processing
            config.mode = CorrectionMode::RGB;
            config.autoExposureEnabled = false;
            config.autoExposureTarget = 120;  // Higher target if enabled
            config.gammaEnabled = true;
            config.gammaValue = 2.0f;         // Lower gamma
            config.brownGuardrailEnabled = false;
            config.vClampEnabled = true;
            config.maxBrightness = 220;       // Allow more brightness
            config.saturationBoostAmount = 15;
            config.ditheringEnabled = true;
            config.spectralCorrectionEnabled = true;
            config.laceEnabled = false;
            // HSV params (not used when mode=RGB)
            config.hsvMinSaturation = 100;
            // RGB params
            config.rgbWhiteThreshold = 170;   // Higher threshold = less correction
            config.rgbTargetMin = 120;
            break;

        case ColorCorrectionPreset::BALANCED:
            // Default recommended settings - already the struct defaults
            // Explicitly set for clarity
            config.mode = CorrectionMode::BOTH;
            config.hsvMinSaturation = 120;
            config.rgbWhiteThreshold = 150;
            config.rgbTargetMin = 100;
            config.autoExposureEnabled = true;
            config.autoExposureTarget = 110;
            config.gammaEnabled = true;
            config.gammaValue = 2.2f;
            config.brownGuardrailEnabled = true;
            config.maxGreenPercentOfRed = 28;
            config.maxBluePercentOfRed = 8;
            config.vClampEnabled = true;
            config.maxBrightness = 200;
            config.saturationBoostAmount = 25;
            config.ditheringEnabled = true;
            config.spectralCorrectionEnabled = true;
            config.laceEnabled = false;
            break;

        case ColorCorrectionPreset::AGGRESSIVE:
            // Maximum correction for problem palettes
            config.mode = CorrectionMode::BOTH;
            config.hsvMinSaturation = 150;    // Higher min saturation
            config.rgbWhiteThreshold = 120;   // Lower threshold = more correction
            config.rgbTargetMin = 80;         // More aggressive white reduction
            config.autoExposureEnabled = true;
            config.autoExposureTarget = 90;   // Lower target = more dimming
            config.gammaEnabled = true;
            config.gammaValue = 2.4f;         // Higher gamma
            config.brownGuardrailEnabled = true;
            config.maxGreenPercentOfRed = 22; // Tighter brown control
            config.maxBluePercentOfRed = 5;
            config.vClampEnabled = true;
            config.maxBrightness = 180;       // More aggressive clamping
            config.saturationBoostAmount = 35;
            config.ditheringEnabled = true;
            config.spectralCorrectionEnabled = true;
            config.laceEnabled = true;        // Enable LACE for detail
            config.laceWindowSize = 5;
            config.laceStrength = 60;         // Higher contrast boost
            break;
    }

    return config;
}

/**
 * @brief Apply a preset to the ColorCorrectionEngine
 * @param preset The preset to apply
 * @param saveToNVS If true, persist the preset to flash
 */
inline void applyPreset(ColorCorrectionPreset preset, bool saveToNVS = false) {
    auto& engine = ColorCorrectionEngine::getInstance();
    engine.setConfig(getPresetConfig(preset));
    if (saveToNVS) {
        engine.saveToNVS();
    }
}

/**
 * @brief Detect which preset best matches current configuration
 * @return The matching preset, or BALANCED if no exact match
 *
 * Compares key settings to identify if current config matches a preset.
 * Useful for UI preset selector highlighting.
 */
inline ColorCorrectionPreset detectCurrentPreset() {
    const auto& config = ColorCorrectionEngine::getInstance().getConfig();

    // Check OFF first (easiest to match)
    if (config.mode == CorrectionMode::OFF &&
        !config.autoExposureEnabled &&
        !config.gammaEnabled &&
        !config.brownGuardrailEnabled) {
        return ColorCorrectionPreset::OFF;
    }

    // Check SUBTLE (RGB mode, no auto-exposure, no brown guardrail)
    if (config.mode == CorrectionMode::RGB &&
        !config.autoExposureEnabled &&
        !config.brownGuardrailEnabled) {
        return ColorCorrectionPreset::SUBTLE;
    }

    // Check AGGRESSIVE (lower targets, higher saturation, LACE enabled)
    if (config.mode == CorrectionMode::BOTH &&
        config.autoExposureTarget <= 95 &&
        config.hsvMinSaturation >= 140 &&
        config.laceEnabled) {
        return ColorCorrectionPreset::AGGRESSIVE;
    }

    // Default to BALANCED
    return ColorCorrectionPreset::BALANCED;
}

} // namespace enhancement
} // namespace lightwaveos
