/**
 * @file LGPExperimentalAudioPack.h
 * @brief 10 experimental audio-reactive LGP effects (centre-origin, dual-strip)
 *
 * Pack IDs: 152-161
 *
 * Design constraints:
 * - Centre-origin rendering only (LED 79/80 origin).
 * - No heap allocation in render().
 * - Audio-coupled timing uses raw dt via AudioReactivePolicy.
 * - Palette-driven colour (no forced rainbow sweep logic).
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFluxRiftEffect final : public plugins::IEffect {
public:
    LGPFluxRiftEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_fluxEnv = 0.0f;
    float m_beatPulse = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPBeatPrismEffect final : public plugins::IEffect {
public:
    LGPBeatPrismEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_prism = 0.0f;
    float m_beatPulse = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPHarmonicTideEffect final : public plugins::IEffect {
public:
    LGPHarmonicTideEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_harmonic = 0.0f;
    float m_rootSmooth = 0.0f;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPBassQuakeEffect final : public plugins::IEffect {
public:
    LGPBassQuakeEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_bassEnv = 0.0f;
    float m_impact = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPTrebleNetEffect final : public plugins::IEffect {
public:
    LGPTrebleNetEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_trebleEnv = 0.0f;
    float m_shimmer = 0.0f;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPRhythmicGateEffect final : public plugins::IEffect {
public:
    LGPRhythmicGateEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_gate = 0.0f;
    float m_pulse = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPSpectralKnotEffect final : public plugins::IEffect {
public:
    LGPSpectralKnotEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_knot = 0.0f;
    float m_rotation = 0.0f;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPSaliencyBloomEffect final : public plugins::IEffect {
public:
    LGPSaliencyBloomEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_saliency = 0.0f;
    float m_bloom = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPTransientLatticeEffect final : public plugins::IEffect {
public:
    LGPTransientLatticeEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_transient = 0.0f;
    float m_memory = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

class LGPWaveletMirrorEffect final : public plugins::IEffect {
public:
    LGPWaveletMirrorEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_phase = 0.0f;
    float m_waveEnv = 0.0f;
    float m_beatTrail = 0.0f;
    uint32_t m_lastBeatMs = 0;
    float m_hue = 24.0f;
    float m_audioPresence = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
