/**
 * @file AudioWaveformEffect.h
 * @brief Scrolling waveform visualization with trails and chromagram color
 *
 * CENTER ORIGIN compliant adaptation of SensoryBridge waveform mode.
 * Shows scrolling waveform emanating from center with dynamic trails.
 *
 * Effect ID: 72
 * Family: PARTY
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | WAVEFORM
 *
 * @author LightwaveOS Team
 * @version 3.0.0 - Added trails via dynamic fading and shift
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class AudioWaveformEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_AUDIO_WAVEFORM;

    AudioWaveformEffect() = default;
    ~AudioWaveformEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ============================================================================
    // Algorithm Constants (matched to original SensoryBridge)
    // ============================================================================

    // Peak smoothing: 5% new, 95% old (original ratio)
    static constexpr float PEAK_SMOOTH_NEW = 0.05f;
    static constexpr float PEAK_SMOOTH_OLD = 0.95f;

    // Color smoothing: 5% new, 95% old (original ratio)
    static constexpr float COLOR_SMOOTH_NEW = 0.05f;
    static constexpr float COLOR_SMOOTH_OLD = 0.95f;

    // Brightness threshold for chromagram bins
    static constexpr float CHROMA_THRESHOLD = 0.05f;

    // Brightness boost for chromagram (original uses 1.5x)
    static constexpr float CHROMA_BOOST = 1.5f;

    // ============================================================================
    // TRAIL CONSTANTS (from original SensoryBridge)
    // ============================================================================

    // Base fade amount per frame (0.90 = 10% fade per frame)
    static constexpr float BASE_FADE = 0.90f;

    // Maximum fade reduction based on amplitude (original: 0.10)
    // Higher amplitude = less fade = longer trails
    static constexpr float MAX_FADE_REDUCTION = 0.10f;

    // ============================================================================
    // State Variables
    // ============================================================================

    // Smoothed peak amplitude (0.0 to 1.0)
    float m_peakSmoothed = 0.0f;

    // Sum colour state (RGB smoothing)
    float m_sumColorLast[3] = {0.0f, 0.0f, 0.0f};

    // Circular chroma EMA state (radians)
    float m_chromaAngle = 0.0f;

    // Flag for first frame (need to clear buffer once)
    bool m_initialized = false;

    // ============================================================================
    // Helper Methods
    // ============================================================================

    CRGB computeChromaColor(const plugins::EffectContext& ctx);
    void applyDynamicFade(plugins::EffectContext& ctx, float amplitude);
    void shiftLedsOutward(plugins::EffectContext& ctx);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
