/**
 * @file StrobeModifier.h
 * @brief Rhythmic pulsing modifier (beat-synced strobe)
 *
 * StrobeModifier creates rhythmic pulsing/flashing effects by
 * periodically dimming the LED buffer. Can sync to beat or run
 * at a fixed rate.
 *
 * Modes:
 * - BEAT_SYNC: Flash on beat boundaries (full beat cycle)
 * - SUBDIVISION: Flash on 1/4, 1/8, 1/16 note divisions
 * - MANUAL_RATE: Fixed Hz strobe rate (1-30 Hz)
 *
 * Usage:
 *   StrobeModifier strobe(StrobeMode::SUBDIVISION, 4, 0.3f);
 *   modifierStack.add(&strobe, ctx);
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
 * @brief Strobe mode
 */
enum class StrobeMode : uint8_t {
    BEAT_SYNC = 0,      // Flash on beats (full beat phase)
    SUBDIVISION,        // Flash on 1/4, 1/8, 1/16 notes
    MANUAL_RATE         // Fixed Hz strobe
};

/**
 * @brief Rhythmic pulsing modifier
 */
class StrobeModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Strobe mode
     * @param subdivision Note subdivision (1=quarter, 2=eighth, 4=sixteenth, etc.)
     * @param dutyCycle On/off ratio (0.0-1.0, lower = shorter flashes)
     * @param intensity Blackout depth (0.0-1.0, 1.0 = full black)
     * @param rateHz Flash rate in Hz for MANUAL_RATE mode (1-30)
     */
    explicit StrobeModifier(
        StrobeMode mode = StrobeMode::BEAT_SYNC,
        uint8_t subdivision = 1,
        float dutyCycle = 0.3f,
        float intensity = 1.0f,
        float rateHz = 4.0f
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
     * @brief Set strobe mode
     */
    void setMode(StrobeMode mode);

    /**
     * @brief Set note subdivision (1-16)
     */
    void setSubdivision(uint8_t subdivision);

    /**
     * @brief Set duty cycle (0.0-1.0)
     */
    void setDutyCycle(float dutyCycle);

    /**
     * @brief Set blackout intensity (0.0-1.0)
     */
    void setIntensity(float intensity);

    /**
     * @brief Set strobe rate in Hz for MANUAL_RATE (1-30)
     */
    void setRateHz(float rateHz);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    StrobeMode m_mode;
    uint8_t m_subdivision;     // 1 (quarter), 2 (eighth), 4 (sixteenth), etc.
    float m_dutyCycle;         // 0.0-1.0 (on time / total period)
    float m_intensity;         // 0.0-1.0 (blackout depth)
    float m_rateHz;            // Flash rate for MANUAL_RATE mode
    bool m_enabled;

    // Calculate current phase (0.0-1.0) based on mode
    float calculatePhase(const plugins::EffectContext& ctx) const;
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
