/**
 * @file WaveformParityEffect.h
 * @brief Sensory Bridge 3.1.0 waveform mode — true parity port for LightwaveOS v2
 *
 * GOAL: Match the visual behaviour of:
 *   void light_mode_waveform()  (Sensory Bridge firmware: lightshow_modes.h v3.1.0)
 *
 * Architecture (lessons from BloomParityEffect):
 *   - Waveform brightness computed as INTENSITY only (grayscale 0..1)
 *   - Palette mapping applied at output stage (ONE getColor() per LED)
 *   - Per-zone state (ZoneComposer reuses one instance across zones)
 *   - dt-corrected smoothing for frame-rate independence
 *   - Dynamic waveform normalisation (replaces SB's fixed /128 divisor)
 *   - SET_CENTER_PAIR for centre-origin dual-strip compliance
 *
 * Parity spine (must remain in this exact order each frame):
 *  1) Push current waveform into 4-frame history ring (on new audio hop)
 *  2) Smooth peak follower (dt-corrected)
 *  3) For each LED (centre → edge):
 *     a) Interpolate waveform from 128 samples to 80 LED positions
 *     b) Average 4 history frames
 *     c) Normalise by dynamic max follower
 *     d) Temporal smooth per-LED (MOOD-based, dt-corrected)
 *     e) Brightness: clamp(0.5 + smoothed * 0.5, 0, 1) * peak
 *  4) Palette mapping: spatial + gHue → palette position, brightness → palette value
 *  5) SET_CENTER_PAIR to write symmetric LEDs
 *
 * Effect ID: 123
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect {

class WaveformParityEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_OPAL_FILM;
    WaveformParityEffect() = default;
    ~WaveformParityEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;

    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ------------------------------------------------------------------------
    // Fixed limits (match LightwaveOS zone model)
    // ------------------------------------------------------------------------
    static constexpr uint8_t  kMaxZones       = 4;
    static constexpr uint8_t  kWaveformPoints = 128;  // CONTROLBUS_WAVEFORM_N
    static constexpr uint8_t  kHistoryFrames  = 4;    // SB averages 4 frames
    static constexpr uint16_t kHalfLength     = HALF_LENGTH;  // 80 LEDs per half-strip

    // ⚠️ PSRAM-ALLOCATED — large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
    struct PsramData {
        int16_t waveformHistory[kMaxZones][kHistoryFrames][kWaveformPoints];
        float   waveformLast[kMaxZones][kHalfLength];
    };
    PsramData* m_ps = nullptr;

    // Per-zone state
    uint32_t m_lastHopSeq[kMaxZones];
    uint8_t  m_historyIndex[kMaxZones];
    bool     m_historyPrimed[kMaxZones];

    // Per-zone dynamic normalisation (replaces SB's fixed /128 divisor)
    float    m_maxFollower[kMaxZones];

    // Per-zone smoothed peak follower (SB parity: waveform_peak_scaled_last)
    float    m_peakSmoothed[kMaxZones];

    // Helpers
    static inline float clamp01(float v) {
        return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
};

} // namespace lightwaveos::effects::ieffect
