/**
 * @file TrailModifier.h
 * @brief Temporal persistence modifier (fade trails)
 *
 * TrailModifier creates trailing/ghosting effects by blending
 * current frames with faded previous frames. Replaces 157+
 * hardcoded fadeToBlackBy() calls across effects.
 *
 * Modes:
 * - CONSTANT: Fixed fade rate
 * - BEAT_REACTIVE: Fade rate varies with beat phase
 * - VELOCITY: Fade based on LED change rate (future)
 *
 * Usage:
 *   TrailModifier trail(TrailMode::BEAT_REACTIVE, 20, 5, 50);
 *   modifierStack.add(&trail, ctx);
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
 * @brief Trail mode
 */
enum class TrailMode : uint8_t {
    CONSTANT = 0,       // Fixed fade rate
    BEAT_REACTIVE,      // Fade rate varies with beat
    VELOCITY            // Fade based on LED change rate (future)
};

/**
 * @brief Temporal persistence modifier
 */
class TrailModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Trail mode
     * @param fadeRate Base fade amount (0-255, for fadeToBlackBy)
     * @param minFade Lower bound for reactive modes (0-255)
     * @param maxFade Upper bound for reactive modes (0-255)
     */
    explicit TrailModifier(
        TrailMode mode = TrailMode::CONSTANT,
        uint8_t fadeRate = 20,
        uint8_t minFade = 5,
        uint8_t maxFade = 50
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
     * @brief Set trail mode
     */
    void setMode(TrailMode mode);

    /**
     * @brief Set base fade rate (0-255)
     */
    void setFadeRate(uint8_t rate);

    /**
     * @brief Set min fade for reactive modes (0-255)
     */
    void setMinFade(uint8_t min);

    /**
     * @brief Set max fade for reactive modes (0-255)
     */
    void setMaxFade(uint8_t max);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    TrailMode m_mode;
    uint8_t m_fadeRate;        // Base fade amount (0-255)
    uint8_t m_minFade;         // Lower bound for reactive
    uint8_t m_maxFade;         // Upper bound for reactive
    bool m_enabled;

    // Previous frame history buffer (320 LEDs max)
    static constexpr uint16_t MAX_LEDS = 320;
    CRGB m_previousFrame[MAX_LEDS];
    bool m_hasHistory;

    // Calculate effective fade rate based on mode and context
    uint8_t calculateFadeRate(const plugins::EffectContext& ctx) const;
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
