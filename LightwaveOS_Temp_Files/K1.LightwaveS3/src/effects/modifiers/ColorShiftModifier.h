/**
 * @file ColorShiftModifier.h
 * @brief Palette rotation modifier (hue offset)
 *
 * ColorShiftModifier rotates the hue of all LEDs by a fixed or dynamic
 * offset. This creates color shifting effects without modifying the
 * base effect's logic.
 *
 * Modes:
 * - FIXED: Static hue offset
 * - AUTO_ROTATE: Continuously rotating hue
 * - AUDIO_CHROMA: Driven by audio chromagram
 * - BEAT_PULSE: Hue pulses on beats
 *
 * Usage:
 *   ColorShiftModifier colorShift(ColorShiftMode::AUTO_ROTATE, 128);
 *   modifierStack.add(&colorShift, ctx);
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "IEffectModifier.h"

namespace lightwaveos {
namespace effects {
namespace modifiers {

/**
 * @brief Color shift mode
 */
enum class ColorShiftMode : uint8_t {
    FIXED = 0,          // Static hue offset
    AUTO_ROTATE,        // Continuously rotating hue
    AUDIO_CHROMA,       // Driven by audio chromagram (requires FEATURE_AUDIO_SYNC)
    BEAT_PULSE          // Hue pulses on beats (requires FEATURE_AUDIO_SYNC)
};

/**
 * @brief Hue rotation modifier
 */
class ColorShiftModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Color shift mode
     * @param hueOffset Initial hue offset (0-255)
     * @param rotationSpeed Rotation speed in hue units/second (for AUTO_ROTATE)
     */
    explicit ColorShiftModifier(
        ColorShiftMode mode = ColorShiftMode::FIXED,
        uint8_t hueOffset = 0,
        float rotationSpeed = 30.0f
    );

    //--------------------------------------------------------------------------
    // IEffectModifier Implementation
    //--------------------------------------------------------------------------

    bool init(const plugins::EffectContext& ctx) override;
    void apply(plugins::EffectContext& ctx) override;
    void unapply() override;
    const ModifierMetadata& getMetadata() const override;

    //--------------------------------------------------------------------------
    // Parameters
    //--------------------------------------------------------------------------

    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

    /**
     * @brief Set color shift mode
     */
    void setMode(ColorShiftMode mode);

    /**
     * @brief Set hue offset (0-255)
     */
    void setHueOffset(uint8_t offset);

    /**
     * @brief Set rotation speed (hue units/second)
     */
    void setRotationSpeed(float speed);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    ColorShiftMode m_mode;
    uint8_t m_hueOffset;       // Current hue offset (0-255)
    float m_rotationSpeed;     // Hue units per second
    float m_accumulatedHue;    // For AUTO_ROTATE mode
    bool m_enabled;

    // Calculate current hue offset based on mode
    uint8_t calculateOffset(const plugins::EffectContext& ctx);
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
