/**
 * @file SpeedModifier.h
 * @brief Temporal scaling modifier (0.1x - 3.0x speed multiplier)
 *
 * SpeedModifier warps animation time by modifying the speed parameter
 * passed to effects. This creates slow-motion or fast-forward effects.
 *
 * Implementation Note:
 * - Does NOT modify LED buffer directly
 * - Modifies ctx.speed BEFORE effect render (pre-modifier)
 * - This is a special case modifier that hooks earlier in pipeline
 *
 * Usage:
 *   SpeedModifier speed(2.0f);  // 2x faster
 *   modifierStack.add(&speed, ctx);
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
 * @brief Speed multiplier modifier
 *
 * Note: This modifier is a "pre-modifier" - it must be applied BEFORE
 * the effect renders, not after. It modifies ctx.speed temporarily.
 */
class SpeedModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param multiplier Speed multiplier (0.1 - 3.0)
     */
    explicit SpeedModifier(float multiplier = 1.0f);

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
     * @brief Set speed multiplier
     * @param multiplier Speed multiplier (0.1 - 3.0)
     */
    void setMultiplier(float multiplier);

    /**
     * @brief Get current multiplier
     */
    float getMultiplier() const { return m_multiplier; }

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

    /**
     * @brief This is a pre-render modifier
     * @return true - speed must be applied BEFORE effect render
     */
    bool isPreRender() const override { return true; }

private:
    float m_multiplier;      // Speed multiplier (0.1 - 3.0)
    uint8_t m_originalSpeed; // Original speed value (for restoration)
    bool m_enabled;

    static constexpr float MIN_MULTIPLIER = 0.1f;
    static constexpr float MAX_MULTIPLIER = 3.0f;
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
