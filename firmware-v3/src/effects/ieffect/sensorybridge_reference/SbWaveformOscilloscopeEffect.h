/**
 * @file SbWaveformOscilloscopeEffect.h
 * @brief SB 3.0.0 Waveform oscilloscope — exact parity + brightness-boosted variant
 *
 * True port of Sensory Bridge 3.0.0's light_mode_waveform()
 * (lightshow_modes.h:128-228). This is the full-strip oscilloscope that
 * renders the raw audio waveform shape across all LEDs, with 4-frame
 * history averaging, per-pixel EMA smoothing, and chromagram colour
 * synthesis.
 *
 * Two thin derived classes:
 *   - SbWaveformOscilloscopeEffect       (EID 0x130C) — exact parity, no boost
 *   - SbWaveformOscilloscopyBrightEffect  (EID 0x130D) — 1.5x brightness boost
 *
 * Depends on SbK1BaseEffect for chromagram colour synthesis, peak envelope
 * following, and auto-hue-shift. See SbK1BaseEffect.h for the base contract.
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

// =========================================================================
// PSRAM layout — allocated once in init(), freed in cleanup()
// =========================================================================

struct SbWfOscPsram {
    float waveformLast[80];          ///< Per-pixel smoothed waveform (SB's waveform_last)
    float sumColourFloat[3];         ///< RGB colour EMA state
    float wfPeakScaledLast;          ///< Local peak smoother
    int16_t wfHistory[4][128];       ///< 4-frame waveform ring buffer
    uint8_t wfHistoryIdx;            ///< Ring buffer write index (0-3)
};

// =========================================================================
// Base oscilloscope effect — concrete implementations override brightnessBoost()
// =========================================================================

class SbWaveformOscilloscopeBase : public SbK1BaseEffect {
public:
    SbWaveformOscilloscopeBase() = default;
    ~SbWaveformOscilloscopeBase() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void cleanup() override;

protected:
    void renderEffect(plugins::EffectContext& ctx) override;

    /// Override to apply brightness boost to chroma bins. Returns 1.0 for parity.
    virtual float brightnessBoost() const { return 1.0f; }

    // Parameter interface
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t kHalfLength   = lightwaveos::effects::HALF_LENGTH;    // 80
    static constexpr uint16_t kStripLength  = lightwaveos::effects::STRIP_LENGTH;   // 160
    static constexpr uint16_t kCenterLeft   = lightwaveos::effects::CENTER_LEFT;    // 79
    static constexpr uint16_t kCenterRight  = lightwaveos::effects::CENTER_RIGHT;   // 80
    static constexpr uint8_t  kWfPoints     = 128;
    static constexpr uint8_t  kHistFrames   = 4;

    // Effect parameters
    float m_chromaHue   = 0.0f;
    float m_contrast    = 1.0f;

    static constexpr uint8_t kParamCount = 2;
    static const plugins::EffectParameter s_params[kParamCount];

    // PSRAM pointer
    SbWfOscPsram* m_ps = nullptr;
};

// =========================================================================
// Exact parity (no brightness boost)
// =========================================================================

class SbWaveformOscilloscopeEffect final : public SbWaveformOscilloscopeBase {
public:
    static constexpr lightwaveos::EffectId kId = 0x130C; // EID_SB_WAVEFORM_OSCILLOSCOPE
    SbWaveformOscilloscopeEffect() = default;
    ~SbWaveformOscilloscopeEffect() override = default;

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    float brightnessBoost() const override { return 1.0f; }
};

// =========================================================================
// 1.5x brightness boost variant
// =========================================================================

class SbWaveformOscilloscopyBrightEffect final : public SbWaveformOscilloscopeBase {
public:
    static constexpr lightwaveos::EffectId kId = 0x130D; // EID_SB_WAVEFORM_OSCILLOSCOPE_BRIGHT
    SbWaveformOscilloscopyBrightEffect() = default;
    ~SbWaveformOscilloscopyBrightEffect() override = default;

    const plugins::EffectMetadata& getMetadata() const override;

protected:
    float brightnessBoost() const override { return 1.5f; }
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
