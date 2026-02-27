/**
 * @file BPMEffect.h
 * @brief BPM - Dual-layer beat-synced effect with traveling waves and expanding rings
 *
 * Effect ID: 6
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * v2 REWRITE - Fixes fundamental design flaw (no wave propagation)
 *
 * Dual-Layer Architecture:
 * - LAYER 1: Background traveling sine wave from center (continuous)
 * - LAYER 2: Beat-triggered expanding rings (on each beat)
 *
 * Audio Integration (v8 pattern):
 * - heavy_bands → Spring → wave speed modulation
 * - beatStrength → ring intensity
 * - tempoConfidence → ring expansion rate
 * - Palette-based colors (NO chromagram - causes muddy colors)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../effects/enhancement/SmoothingEngine.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class BPMEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_BPM;

    BPMEffect();
    ~BPMEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // v2: Wave phase accumulator (was missing - caused static gradient!)
    float m_phase = 0.0f;

    // v2: Spring for smooth speed modulation (v8 pattern)
    enhancement::Spring m_speedSpring;

    // v2: Ring buffer for beat-triggered expanding rings
    static constexpr int MAX_RINGS = 4;
    float m_ringRadius[MAX_RINGS] = {0};
    float m_ringIntensity[MAX_RINGS] = {0};
    uint8_t m_nextRing = 0;

    // Tempo lock hysteresis
    bool m_tempoLocked = false;
    
    // EMA smoothing for heavyEnergy (prevents pops)
    float m_heavyEnergySmooth = 0.0f;
    bool m_heavyEnergySmoothInitialized = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
