/**
 * @file IntensityModifier.h
 * @brief Brightness envelope modifier (audio-reactive)
 *
 * IntensityModifier scales LED brightness based on audio signals or
 * time-based envelopes. It multiplies each LED's RGB values by a
 * scaling factor derived from:
 * - Audio RMS energy
 * - Beat phase
 * - Custom envelopes
 *
 * This creates pulsing, breathing, or beat-synced intensity effects
 * without modifying the base effect's logic.
 *
 * Usage:
 *   IntensityModifier intensity(IntensitySource::AUDIO_RMS);
 *   modifierStack.add(&intensity, ctx);
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
 * @brief Intensity modulation source
 */
enum class IntensitySource : uint8_t {
    CONSTANT = 0,       // Fixed scaling factor
    AUDIO_RMS,          // Modulated by RMS energy
    AUDIO_BEAT_PHASE,   // Pulsing on beat phase
    SINE_WAVE,          // Time-based sine wave
    TRIANGLE_WAVE       // Time-based triangle wave
};

/**
 * @brief Brightness envelope modifier
 */
class IntensityModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param source Intensity modulation source
     * @param baseIntensity Base intensity (0.0 - 1.0)
     * @param depth Modulation depth (0.0 - 1.0)
     */
    explicit IntensityModifier(
        IntensitySource source = IntensitySource::CONSTANT,
        float baseIntensity = 1.0f,
        float depth = 0.5f
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
     * @brief Set intensity source
     */
    void setSource(IntensitySource source);

    /**
     * @brief Set base intensity (0.0 - 1.0)
     */
    void setBaseIntensity(float intensity);

    /**
     * @brief Set modulation depth (0.0 - 1.0)
     */
    void setDepth(float depth);

    /**
     * @brief Set sine wave frequency (Hz) for SINE_WAVE mode
     */
    void setFrequency(float hz);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    IntensitySource m_source;
    float m_baseIntensity;  // 0.0 - 1.0
    float m_depth;          // 0.0 - 1.0 (modulation amount)
    float m_frequency;      // Hz (for wave modes)
    bool m_enabled;

    // Calculate scaling factor based on source
    float calculateScaling(const plugins::EffectContext& ctx);
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
