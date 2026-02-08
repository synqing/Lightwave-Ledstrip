/**
 * @file BeatPulseSpectralPulseEffect.h
 * @brief Beat Pulse (Spectral Pulse) - Stationary zones pulsing by frequency band
 *
 * Unlike Spectral (which uses travelling rings), Spectral Pulse divides the
 * half-strip into three fixed zones that pulse in brightness according to
 * their respective frequency bands:
 *   - Inner zone (0.0–0.33): treble → fast, snappy response
 *   - Middle zone (0.33–0.66): mid → moderate response
 *   - Outer zone (0.66–1.0): bass → slow, heavy response
 *
 * No ring movement at all — zones glow and dim with the music.
 * The zone boundaries use soft crossfades (not hard edges) to avoid
 * visible "seams" on the physical strip.
 *
 * Core maths:
 *  1. Read bass/mid/treble, smooth with dt-correct exponential
 *  2. Map dist01 to zone weight using soft crossfade (width 0.08)
 *  3. intensity = trebleWeight * smoothTreble + midWeight * smoothMid
 *                 + bassWeight * smoothBass
 *  4. On beat: brief 0.25 boost to all zones
 *  5. brightness = 0.10 + intensity * 0.90
 *  6. White mix = intensity * 0.20
 *
 * Effect ID: 118
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseSpectralPulseEffect final : public plugins::IEffect {
public:
    BeatPulseSpectralPulseEffect() = default;
    ~BeatPulseSpectralPulseEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_smoothBass = 0.0f;
    float m_smoothMid = 0.0f;
    float m_smoothTreble = 0.0f;
    float m_beatBoost = 0.0f;
    float m_fallbackBpm = 128.0f;
    uint32_t m_lastFallbackBeatMs = 0;
    float m_fallbackPhase = 0.0f;
};

} // namespace lightwaveos::effects::ieffect
