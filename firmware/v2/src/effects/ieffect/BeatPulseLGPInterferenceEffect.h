/**
 * @file BeatPulseLGPInterferenceEffect.h
 * @brief Beat Pulse with LGP optical interference exploitation
 *
 * VISUAL IDENTITY:
 * Expanding ring from centre with DUAL-STRIP PHASE CONTROL. Exploits the
 * LGP's optical interference properties by driving Strip 1 and Strip 2
 * with configurable phase relationships.
 *
 * LGP INTERFERENCE MODES:
 * - IN_PHASE (0°):     Both strips identical - uniform brightness
 * - QUADRATURE (90°):  Strip 2 leads by π/2 - circular/rotating appearance
 * - ANTI_PHASE (180°): Strip 2 inverted - standing wave nodes visible
 *
 * SPATIAL FREQUENCY:
 * Controls the number of standing wave "boxes" visible on the LGP.
 * Higher frequency = more nodes = finer interference pattern.
 *
 * HTML PARITY CORE:
 * Uses same timing spine as Stack/Shockwave for consistent beat response.
 *
 * Effect ID: 120
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

/**
 * @brief Phase relationship between Strip 1 and Strip 2
 */
enum class LGPPhaseMode : uint8_t {
    IN_PHASE = 0,      ///< Both strips identical (0° offset)
    QUADRATURE = 1,    ///< Strip 2 leads by 90° (π/2)
    ANTI_PHASE = 2     ///< Strip 2 inverted (180° / π offset)
};

class BeatPulseLGPInterferenceEffect final : public plugins::IEffect {
public:
    BeatPulseLGPInterferenceEffect() = default;
    ~BeatPulseLGPInterferenceEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_beatIntensity = 0.0f;      // Amplitude-driven ring position (HTML parity)
    uint32_t m_lastBeatTimeMs = 0;     // Fallback metronome tracking
    float m_fallbackBpm = 128.0f;      // Fallback metronome BPM

    // LGP interference parameters
    LGPPhaseMode m_phaseMode = LGPPhaseMode::ANTI_PHASE;
    float m_spatialFreq = 4.0f;        // Boxes per half-strip (2-12 typical)
    float m_motionPhase = 0.0f;        // Slow phase drift for standing wave animation
};

} // namespace lightwaveos::effects::ieffect
