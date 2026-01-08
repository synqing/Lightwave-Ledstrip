/**
 * @file GlitchModifier.h
 * @brief Controlled chaos modifier (beat-synced)
 *
 * GlitchModifier adds controlled glitch effects to LED output. It can:
 * - Randomly flip pixels
 * - Create RGB channel shifts
 * - Add noise bursts
 * - Trigger on beats (audio-reactive)
 *
 * Modes:
 * - PIXEL_FLIP: Random pixel color inversions
 * - CHANNEL_SHIFT: RGB channel displacement
 * - NOISE_BURST: Random brightness noise
 * - BEAT_SYNC: Glitch triggers on beats (requires FEATURE_AUDIO_SYNC)
 *
 * Usage:
 *   GlitchModifier glitch(GlitchMode::BEAT_SYNC, 0.1f);
 *   modifierStack.add(&glitch, ctx);
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
 * @brief Glitch mode
 */
enum class GlitchMode : uint8_t {
    PIXEL_FLIP = 0,     // Random pixel color inversions
    CHANNEL_SHIFT,      // RGB channel displacement
    NOISE_BURST,        // Random brightness noise
    BEAT_SYNC           // Glitch on beats (audio-reactive)
};

/**
 * @brief Controlled chaos modifier
 */
class GlitchModifier : public IEffectModifier {
public:
    /**
     * @brief Constructor
     * @param mode Glitch mode
     * @param intensity Glitch intensity (0.0 - 1.0)
     */
    explicit GlitchModifier(
        GlitchMode mode = GlitchMode::PIXEL_FLIP,
        float intensity = 0.1f
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
     * @brief Set glitch mode
     */
    void setMode(GlitchMode mode);

    /**
     * @brief Set glitch intensity (0.0 - 1.0)
     */
    void setIntensity(float intensity);

    /**
     * @brief Set channel shift offset (pixels)
     */
    void setChannelShift(int8_t shift);

    //--------------------------------------------------------------------------
    // Enable/Disable
    //--------------------------------------------------------------------------

    bool isEnabled() const override { return m_enabled; }
    void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
    GlitchMode m_mode;
    float m_intensity;          // 0.0 - 1.0
    int8_t m_channelShift;      // Pixel offset for channel shift
    bool m_enabled;

    // RNG state for deterministic glitches
    uint32_t m_seed;

    // Glitch trigger state (for BEAT_SYNC)
    bool m_wasOnBeat;

    // Simple pseudo-random number generator (fast, deterministic)
    uint32_t random();

    // Apply glitch effects
    void applyPixelFlip(plugins::EffectContext& ctx);
    void applyChannelShift(plugins::EffectContext& ctx);
    void applyNoiseBurst(plugins::EffectContext& ctx);
    void applyBeatSync(plugins::EffectContext& ctx);
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
