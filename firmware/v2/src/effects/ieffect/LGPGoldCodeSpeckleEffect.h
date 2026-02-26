/**
 * @file LGPGoldCodeSpeckleEffect.h
 * @brief LGP Gold-Code Speckle Morph - Phase-plate holographic grain illusion
 *
 * Effect ID: 0x1B02  (EID_LGP_GOLD_CODE_SPECKLE)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | HOLOGRAPHIC | AUDIO_REACTIVE
 *
 * Two deterministic pseudo-random "hologram codes" (LFSR-generated bit
 * patterns) slowly crossfade to produce holographic grain/speckle drift
 * that does not look like discrete LEDs on a Light Guide Plate.
 *
 * State budget: ~36 bytes (no PSRAM, no heap allocation in render).
 *
 * LFSR polynomial: x^16 + x^14 + x^13 + x^11 + 1
 *   feedback = bit0 ^ bit2 ^ bit3 ^ bit5
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGoldCodeSpeckleEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_GOLD_CODE_SPECKLE;

    LGPGoldCodeSpeckleEffect();
    ~LGPGoldCodeSpeckleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
    // LFSR state (~4 bytes)
    uint16_t m_lfsrA;
    uint16_t m_lfsrB;

    // LFSR step timer (~4 bytes)
    float m_lfsrTimer;

    // Slow crossfade phase 0..2*PI over ~10s (~4 bytes)
    float m_mixPhase;

    // Chroma hue smoothing state (~4 bytes)
    float m_chromaAngle;

    // Time accumulator for carrier wave (~4 bytes)
    float m_timeOffset;

    // Fallback animation phase (~4 bytes)
    float m_fallbackPhase;

#if FEATURE_AUDIO_SYNC
    // Hop sequence tracking for audio (~4 bytes)
    uint32_t m_lastHopSeq;

    // Smoothed chroma bins (~48 bytes -- stored as members, each <64B total)
    float m_chromaSmoothed[12];
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
