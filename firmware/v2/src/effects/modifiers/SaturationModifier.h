/**
 * @file SaturationModifier.h
 * @brief Color intensity adjustment modifier
 *
 * SaturationModifier adjusts the color saturation of all LEDs,
 * allowing effects to be desaturated (grayscale) or vibrance-boosted.
 *
 * Modes:
 * - ABSOLUTE: Set saturation to fixed value (0-255)
 * - RELATIVE: Add/subtract from current saturation (-128 to +127)
 * - VIBRANCE: Boost low-saturation colors more than high-sat (smart boost)
 *
 * Usage:
 *   SaturationModifier sat(SatMode::VIBRANCE, 64);
 *   modifierStack.add(&sat, ctx);
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
 * @brief Saturation mode
 */
enum class SatMode : uint8_t {
    ABSOLUTE = 0,   // Set saturation to fixed value (0-255)
    RELATIVE,       // Add/subtract from current (-128 to +127)
    VIBRANCE        // Boost low-saturation colors more
};

/**
 * @brief Color saturation adjustment modifier
 */
class SaturationModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Saturation mode
     * @param saturation Saturation value (meaning depends on mode)
     * @param preserveLuminance Keep brightness constant when desaturating
     */
    explicit SaturationModifier(
        SatMode mode = SatMode::ABSOLUTE,
        int16_t saturation = 200,
        bool preserveLuminance = true
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
     * @brief Set saturation mode
     */
    void setMode(SatMode mode);

    /**
     * @brief Set saturation value
     * @param sat 0-255 for ABSOLUTE, -128 to +127 for RELATIVE, 0-255 for VIBRANCE boost
     */
    void setSaturation(int16_t sat);

    /**
     * @brief Set luminance preservation
     */
    void setPreserveLuminance(bool preserve);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    SatMode m_mode;
    int16_t m_saturation;      // Saturation value (mode-dependent interpretation)
    bool m_preserveLuminance;  // Keep brightness when desaturating
    bool m_enabled;
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
