// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ColorEngine.h
 * @brief Advanced color manipulation for visual enhancements
 *
 * ColorEngine provides:
 * - Cross-palette blending (blend 2-3 palettes with weighted factors)
 * - Temporal palette rotation (auto-shifting hue over time)
 * - Color diffusion (Gaussian blur for smoother gradients)
 *
 * Ported from LightwaveOS v1 to v2 architecture with namespace isolation.
 *
 * Usage:
 * @code
 * auto& colorEngine = lightwaveos::enhancement::ColorEngine::getInstance();
 * colorEngine.enableCrossBlend(true);
 * colorEngine.setBlendPalettes(HeatColors_p, OceanColors_p);
 * colorEngine.setBlendFactors(200, 55);  // 80% heat, 20% ocean
 *
 * // In render loop:
 * colorEngine.update();
 * CRGB color = colorEngine.getColor(index, brightness);
 * @endcode
 */

#pragma once

#include <FastLED.h>
#include "../../config/features.h"

// Default feature flag if not defined
#ifndef FEATURE_COLOR_ENGINE
#define FEATURE_COLOR_ENGINE 1
#endif

#if FEATURE_COLOR_ENGINE

namespace lightwaveos {
namespace enhancement {

/**
 * @class ColorEngine
 * @brief Singleton providing advanced color manipulation
 *
 * Thread Safety: NOT thread-safe. Call only from Core 1 (render loop).
 * Memory: ~100 bytes static, no heap allocation in hot path.
 */
class ColorEngine {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the singleton ColorEngine
     */
    static ColorEngine& getInstance() {
        static ColorEngine instance;
        return instance;
    }

    // ========================================================================
    // CORE COLOR RETRIEVAL
    // ========================================================================

    /**
     * @brief Get color with all enhancements applied
     * @param paletteIndex Position in palette (0-255)
     * @param brightness Brightness scaling (0-255)
     * @param ledIndex LED position for spatial effects (optional)
     * @param blendType FastLED blend type (LINEARBLEND or NOBLEND)
     * @return Enhanced color
     *
     * This method applies temporal rotation and cross-palette blending
     * if enabled, otherwise falls back to the current global palette.
     */
    CRGB getColor(uint8_t paletteIndex, uint8_t brightness = 255,
                  uint16_t ledIndex = 0, TBlendType blendType = LINEARBLEND);

    /**
     * @brief Apply blend between source and target color
     * @param source Starting color
     * @param target Ending color
     * @param amount Blend amount (0=source, 255=target)
     * @return Blended color
     */
    static CRGB applyBlend(const CRGB& source, const CRGB& target, uint8_t amount);

    // ========================================================================
    // CROSS-PALETTE BLENDING
    // ========================================================================

    /**
     * @brief Enable/disable cross-palette blending
     * @param enable True to enable blending multiple palettes
     */
    void enableCrossBlend(bool enable);

    /**
     * @brief Set palettes for cross-blending
     * @param pal1 Primary palette (required)
     * @param pal2 Secondary palette (required)
     * @param pal3 Tertiary palette (optional, pass nullptr to skip)
     */
    void setBlendPalettes(const CRGBPalette16& pal1,
                          const CRGBPalette16& pal2,
                          const CRGBPalette16* pal3 = nullptr);

    /**
     * @brief Set blend weights for each palette
     * @param pal1Amount Weight for palette 1 (0-255)
     * @param pal2Amount Weight for palette 2 (0-255)
     * @param pal3Amount Weight for palette 3 (0-255)
     *
     * Weights are normalized internally so they don't need to sum to 255.
     */
    void setBlendFactors(uint8_t pal1Amount, uint8_t pal2Amount,
                         uint8_t pal3Amount = 0);

    // ========================================================================
    // TEMPORAL PALETTE ROTATION
    // ========================================================================

    /**
     * @brief Enable/disable temporal rotation
     * @param enable True to enable auto-shifting palette index
     */
    void enableTemporalRotation(bool enable);

    /**
     * @brief Set rotation speed
     * @param degreesPerFrame Rotation speed in degrees per frame
     *
     * At 120 FPS, 1.0 degree/frame = 120 degrees/second = 3 full rotations/second
     */
    void setRotationSpeed(float degreesPerFrame);

    /**
     * @brief Get current rotation phase
     * @return Current phase (0.0 to 360.0)
     */
    float getRotationPhase() const { return m_rotationPhase; }

    // ========================================================================
    // COLOR DIFFUSION
    // ========================================================================

    /**
     * @brief Enable/disable color diffusion (blur)
     * @param enable True to enable diffusion
     */
    void enableDiffusion(bool enable);

    /**
     * @brief Set diffusion strength
     * @param amount Blur intensity (0-255, higher = more blur)
     */
    void setDiffusionAmount(uint8_t amount);

    /**
     * @brief Apply diffusion to LED buffer
     * @param buffer LED buffer to blur
     * @param ledCount Number of LEDs in buffer
     *
     * Uses FastLED's blur1d() for Gaussian blur. Safe to call
     * every frame - returns immediately if diffusion disabled.
     */
    void applyDiffusion(CRGB* buffer, uint16_t ledCount);

    // ========================================================================
    // FRAME UPDATE
    // ========================================================================

    /**
     * @brief Update engine state (call once per frame)
     *
     * Updates rotation phase and internal state. Call this in the
     * render loop before using getColor().
     */
    void update();

    // ========================================================================
    // UTILITY
    // ========================================================================

    /**
     * @brief Reset all settings to defaults
     */
    void reset();

    /**
     * @brief Check if any enhancement is active
     * @return True if any feature is enabled
     */
    bool isActive() const { return m_active; }

    /**
     * @brief Check if cross-blend is enabled
     */
    bool isCrossBlendEnabled() const { return m_crossBlendEnabled; }

    /**
     * @brief Check if temporal rotation is enabled
     */
    bool isRotationEnabled() const { return m_rotationEnabled; }

    /**
     * @brief Check if diffusion is enabled
     */
    bool isDiffusionEnabled() const { return m_diffusionEnabled; }

    /**
     * @brief Get blend factor for palette 1
     */
    uint8_t getBlendFactor1() const { return m_blendFactor1; }

    /**
     * @brief Get blend factor for palette 2
     */
    uint8_t getBlendFactor2() const { return m_blendFactor2; }

    /**
     * @brief Get blend factor for palette 3
     */
    uint8_t getBlendFactor3() const { return m_blendFactor3; }

    /**
     * @brief Get rotation speed in degrees per frame
     */
    float getRotationSpeed() const { return m_rotationSpeed; }

    /**
     * @brief Get diffusion amount (0-255)
     */
    uint8_t getDiffusionAmount() const { return m_diffusionAmount; }

private:
    // Private constructor (singleton pattern)
    ColorEngine();
    ColorEngine(const ColorEngine&) = delete;
    ColorEngine& operator=(const ColorEngine&) = delete;

    // ========================================================================
    // INTERNAL STATE
    // ========================================================================

    bool m_active;

    // Cross-palette blending state
    bool m_crossBlendEnabled;
    CRGBPalette16 m_blendPalette1;
    CRGBPalette16 m_blendPalette2;
    CRGBPalette16 m_blendPalette3;
    uint8_t m_blendFactor1;
    uint8_t m_blendFactor2;
    uint8_t m_blendFactor3;

    // Temporal rotation state
    bool m_rotationEnabled;
    float m_rotationSpeed;
    float m_rotationPhase;

    // Diffusion state
    bool m_diffusionEnabled;
    uint8_t m_diffusionAmount;

    // ========================================================================
    // INTERNAL HELPERS
    // ========================================================================

    /**
     * @brief Blend colors from multiple palettes
     */
    CRGB blendPalettes(uint8_t paletteIndex, uint8_t brightness);

    /**
     * @brief Update rotation phase
     */
    void updateRotationPhase();
};

} // namespace enhancement
} // namespace lightwaveos

#endif // FEATURE_COLOR_ENGINE
