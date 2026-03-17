/**
 * @file SbFullSpectrumEffect.h
 * @brief SB Beat Pulse — radial expansion pulse on each beat
 *
 * When a beat lands, a bright ring launches outward from centre.
 * Between beats, the ring contracts as the beat envelope decays.
 * In the absence of beats, gentle RMS-driven breathing provides
 * ambient visual motion.
 *
 * Features:
 *   1. Beat-envelope driven radial expansion (snap high, decay ~150ms)
 *   2. Sparse 3-pixel wavefront ring (only the leading edge is drawn)
 *   3. Audio-coupled trail decay (punchy fade in loud passages)
 *   4. RMS-breathing fallback when no beat is detected
 *   5. Mirror right half -> left half (centre-origin)
 *   6. Copy strip 1 -> strip 2
 *
 * Depends on SbK1BaseEffect for baseProcessAudio (provides m_dt).
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

class SbFullSpectrumEffect final : public SbK1BaseEffect {
public:
    static constexpr lightwaveos::EffectId kId = 0x130F; // EID_SB_FULL_SPECTRUM
    SbFullSpectrumEffect() = default;
    ~SbFullSpectrumEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80

    // PSRAM-allocated beat pulse state + trail persistence buffer
    struct SbFullSpectrumPsram {
        CRGB  trailBuffer[160];   ///< Persistent pixel buffer for frame-to-frame trails
        float beatEnvelope;       ///< 0-1, snaps to 1 on beat, decays between
        float smoothedRms;        ///< Smoothed RMS for fallback breathing when no beat
    };

#ifndef NATIVE_BUILD
    SbFullSpectrumPsram* m_ps = nullptr;
#else
    SbFullSpectrumPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
