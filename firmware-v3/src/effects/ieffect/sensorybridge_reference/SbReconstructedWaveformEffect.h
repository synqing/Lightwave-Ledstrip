/**
 * @file SbReconstructedWaveformEffect.h
 * @brief SB Dynamic Arc — energy-driven fill/retreat arc effect
 *
 * Visualises audio energy as a crescendo/release arc: when energy builds
 * over seconds the strip progressively fills outward from centre, and when
 * energy releases the light retreats inward. The bright element is a narrow
 * 3-pixel edge at the fill boundary; the interior behind it is kept very dim.
 *
 * This is the slowest, most deliberate effect in the SB family. Fill level
 * changes over seconds, not frames.
 *
 * Pipeline:
 *   1. Decay trail buffer (rate adapts to trend direction)
 *   2. Sample RMS into history, compute rolling trend
 *   3. Integrate trend into fill level with inertia
 *   4. Draw sparse fill edge + dim interior
 *   5. Centre-origin mirror + strip copy
 *
 * Depends on SbK1BaseEffect for audio pipeline (RMS access).
 */

#pragma once

#include "SbK1BaseEffect.h"
#include "../../CoreEffects.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

class SbReconstructedWaveformEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x1311; // EID_SB_RECONSTRUCTED_WAVEFORM
    SbReconstructedWaveformEffect() = default;
    ~SbReconstructedWaveformEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    // No user-tuneable parameters — effect is fully automatic
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80
    static constexpr uint8_t  kRmsHistSize  = 32;   ///< ~4 seconds at 8 samples/second

    // PSRAM-allocated state (large buffers MUST live in PSRAM)
    struct SbReconWfPsram {
        CRGB trailBuffer[160];      ///< Persistent pixel buffer for frame-to-frame trails
        float fillLevel;            ///< 0.0 (empty) to 1.0 (full strip). Slow integrator.
        float smoothedRms;          ///< Current energy level (EMA-smoothed)
        float rmsTrend;             ///< Rolling slope of RMS (positive=building, negative=releasing)
        float rmsHistory[32];       ///< ~4 seconds of RMS history at ~8 samples/second
        uint8_t rmsHistIdx;         ///< Circular index into rmsHistory
        uint32_t lastTrendUpdate;   ///< Timestamp for trend sampling (millis)
    };

#ifndef NATIVE_BUILD
    SbReconWfPsram* m_ps = nullptr;
#else
    SbReconWfPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
