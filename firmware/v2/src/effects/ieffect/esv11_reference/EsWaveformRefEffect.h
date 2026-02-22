/**
 * @file EsWaveformRefEffect.h
 * @brief ES v1.1 "Waveform" reference show (time-domain strip).
 *
 * Per-zone state: ZoneComposer reuses one effect instance across up to 4 zones,
 * setting ctx.zoneId before each render(). All temporal buffers are dimensioned
 * [kMaxZones] to prevent cross-zone contamination.
 */

#pragma once

#include "../../../plugins/api/IEffect.h"
#include "config/audio_config.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsWaveformRefEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_ES_WAVEFORM;

    EsWaveformRefEffect() = default;
    ~EsWaveformRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint8_t  kMaxZones      = 4;
    static constexpr uint16_t SAMPLE_RATE_HZ = lightwaveos::audio::SAMPLE_RATE;
    static constexpr float CUTOFF_HZ = 2110.0f;  // ES: 110 + 2000*(1.0 - bass); bass disabled in ref.
    static constexpr uint8_t FILTER_PASSES = 3;
    static constexpr uint8_t HISTORY_FRAMES = 4;

    // Large buffers in PSRAM — dimensioned per-zone to avoid cross-zone contamination.
#ifndef NATIVE_BUILD
    struct EsWaveformPsram {
        float samples[kMaxZones][128];
        float history[kMaxZones][HISTORY_FRAMES][128];
    };
    EsWaveformPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    float m_alpha = 0.0f;

    // Per-zone temporal state
    uint32_t m_lastHopSeq[kMaxZones]   = {};
    uint8_t  m_historyIndex[kMaxZones] = {};
    bool     m_historyPrimed[kMaxZones] = {};
};

} // namespace lightwaveos::effects::ieffect::esv11_reference
