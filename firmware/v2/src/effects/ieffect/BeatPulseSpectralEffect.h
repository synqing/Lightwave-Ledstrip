/**
 * @file BeatPulseSpectralEffect.h
 * @brief Beat Pulse (Spectral) - Three frequency-driven rings
 *
 * Three concentric rings driven by frequency bands:
 *   - Bass ring: outer zone (edges), slow decay
 *   - Mid ring: middle zone, medium decay
 *   - Treble ring: inner zone (centre), fast decay
 *
 * Each ring's intensity is driven by its frequency band amplitude (not beat).
 * The Parity decay (pow(0.94, dt*60)) is used for smoothing each band.
 * This creates a continuously reactive spectral visualisation that maps
 * the frequency spectrum to spatial position on the strip.
 *
 * Core maths:
 *  1. Bass: smooth += (raw - smooth) * factor; ring at dist01 = 0.75
 *  2. Mid: ring at dist01 = 0.45
 *  3. Treble: ring at dist01 = 0.15
 *  4. Each ring: tent profile, width proportional to band energy
 *  5. intensity = max(bass_hit, mid_hit, treble_hit)
 *  6. brightness = 0.08 + intensity * 0.92
 *  7. White mix on treble only (0.35) for sparkle
 *  8. On beat: brief global boost (1.3x, clamped)
 *
 * Effect ID: 117
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseSpectralEffect final : public plugins::IEffect {
public:
    BeatPulseSpectralEffect() = default;
    ~BeatPulseSpectralEffect() override = default;

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
