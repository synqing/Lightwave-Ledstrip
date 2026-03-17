/**
 * @file SbFullSpectrumEffect.h
 * @brief SB Full Spectrum — high-resolution frequency display
 *
 * Maps all 64 Goertzel bins to 80 pixels via linear interpolation for a
 * detailed frequency spectrum visualisation. Bass (110 Hz) at the centre,
 * treble (4186 Hz) at the edges.
 *
 * Features:
 *   1. Linear interpolation of 64 bins → 80 pixels
 *   2. Per-pixel asymmetric EMA smoothing (fast attack, slow decay) in PSRAM
 *   3. Frequency-position hue mapping with gHue rotation
 *   4. dt-corrected smoothing via baseProcessAudio m_dt
 *   5. Mirror right half → left half (centre-origin)
 *   6. Copy strip 1 → strip 2
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

    // PSRAM-allocated per-pixel smoothing buffer
    struct SbFullSpectrumPsram {
        float smoothed[80];  ///< Per-pixel smoothed energy
    };

#ifndef NATIVE_BUILD
    SbFullSpectrumPsram* m_ps = nullptr;
#else
    SbFullSpectrumPsram* m_ps = nullptr;
#endif
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
