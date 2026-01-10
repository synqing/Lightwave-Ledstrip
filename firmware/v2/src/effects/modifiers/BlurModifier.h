/**
 * @file BlurModifier.h
 * @brief Spatial smoothing modifier (box/gaussian blur)
 *
 * BlurModifier applies spatial smoothing to the LED buffer,
 * creating softer edges and smoother transitions between LEDs.
 * Useful for reducing harsh edges on 70+ effects.
 *
 * Modes:
 * - BOX: Fast 3-tap average (kernel = [1,1,1]/3)
 * - GAUSSIAN: Weighted 5-tap (kernel = [1,4,6,4,1]/16)
 * - MOTION: Directional blur (placeholder for future)
 *
 * Usage:
 *   BlurModifier blur(BlurMode::BOX, 2, 0.8f);
 *   modifierStack.add(&blur, ctx);
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
 * @brief Blur mode
 */
enum class BlurMode : uint8_t {
    BOX = 0,        // Fast 3-tap average
    GAUSSIAN,       // Weighted 5-tap (1-4-6-4-1)
    MOTION          // Directional blur (future)
};

/**
 * @brief Spatial blur modifier
 */
class BlurModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Blur mode
     * @param radius Blur radius (1-5 pixels)
     * @param strength Blend factor (0.0-1.0, 1.0 = full blur)
     */
    explicit BlurModifier(
        BlurMode mode = BlurMode::BOX,
        uint8_t radius = 2,
        float strength = 0.8f
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
     * @brief Set blur mode
     */
    void setMode(BlurMode mode);

    /**
     * @brief Set blur radius (1-5)
     */
    void setRadius(uint8_t radius);

    /**
     * @brief Set blend strength (0.0-1.0)
     */
    void setStrength(float strength);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    BlurMode m_mode;
    uint8_t m_radius;          // 1-5 pixels
    float m_strength;          // 0.0-1.0 blend factor
    bool m_enabled;

    // Temp buffer for in-place blur (320 LEDs max)
    static constexpr uint16_t MAX_LEDS = 320;
    CRGB m_tempBuffer[MAX_LEDS];

    // Apply box blur
    void applyBoxBlur(plugins::EffectContext& ctx);

    // Apply gaussian blur
    void applyGaussianBlur(plugins::EffectContext& ctx);
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
