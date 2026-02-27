// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPPerlinInterferenceWeaveEffect.h
 * @brief LGP Perlin Interference Weave - Dual-strip phase-offset noise creates moiré
 * 
 * Effect ID: 80
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MOIRE
 * 
 * Audio-reactive: Beat modulates phase separation between strips
 * - Beat → phase separation modulation
 * - Flux → weave intensity
 * - Chroma → colour offset between strips
 * 
 * Instance State:
 * - m_noiseX, m_noiseY: Base noise field coordinates
 * - m_phaseOffset: Phase offset between strips (audio-modulated)
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinInterferenceWeaveEffect : public plugins::IEffect {
public:
    LGPPerlinInterferenceWeaveEffect();
    ~LGPPerlinInterferenceWeaveEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Noise field coordinates
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    
    // Phase offset between strips (for interference)
    float m_phaseOffset;
    
    // Time accumulator
    uint16_t m_time;
    
    // Audio state tracking
    uint32_t m_lastHopSeq;
    uint8_t m_dominantChromaBin;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

