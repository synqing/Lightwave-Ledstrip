/**
 * @file SbWaveform310RefEffect.h
 * @brief Sensory Bridge 3.1.0 reference: waveform light show mode (centre-origin adaptation)
 *
 * This is a port of `light_mode_waveform()` from SensoryBridge-3.1.0.
 * It uses:
 * - 4-frame waveform history averaging
 * - Mood-dependent per-sample smoothing
 * - Note-chromagram → colour summation (chromatic mode) or single hue (non-chromatic)
 * - Peak follower scaling (`waveform_peak_scaled_last * 4.0`)
 * - Per-zone state (ZoneComposer reuses one instance across zones)
 * - dt-corrected colour smoothing for frame-rate independence
 *
 * Effect ID: 109
 */

#pragma once

#include "../../CoreEffects.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

class SbWaveform310RefEffect final : public plugins::IEffect {
public:
    SbWaveform310RefEffect() = default;
    ~SbWaveform310RefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr int      kMaxZones      = 4;
    static constexpr uint8_t  WAVEFORM_POINTS = lightwaveos::audio::CONTROLBUS_WAVEFORM_N;
    static constexpr uint8_t  HISTORY_FRAMES  = 4;
    static constexpr uint16_t kHalfLength     = lightwaveos::effects::HALF_LENGTH;

    // Per-zone scalar state (small — lives in DRAM with the class instance)
    uint32_t m_lastHopSeq[kMaxZones]           = {};
    uint8_t  m_historyIndex[kMaxZones]         = {};
    bool     m_historyPrimed[kMaxZones]        = {};
    float    m_waveformPeakScaledLast[kMaxZones] = {};
    float    m_waveformMaxFollower[kMaxZones]  = {};

    // PSRAM-allocated — large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
#ifndef NATIVE_BUILD
    struct SbWaveform310Psram {
        int16_t waveformHistory[kMaxZones][HISTORY_FRAMES][WAVEFORM_POINTS];
        float   waveformLast[kMaxZones][kHalfLength];
        float   sumColourLast[kMaxZones][3];
    };
    SbWaveform310Psram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
