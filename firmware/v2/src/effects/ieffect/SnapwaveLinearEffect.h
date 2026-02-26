/**
 * @file SnapwaveLinearEffect.h
 * @brief Scrolling waveform with time-based oscillation
 *
 * ORIGINAL SNAPWAVE ALGORITHM - Restored from SensoryBridge light_mode_snapwave()
 *
 * Visual behaviour:
 * 1. Dot bounces based on time-based oscillation + chromagram
 * 2. HISTORY BUFFER tracks previous dot positions
 * 3. Trail renders at previous positions with fading brightness
 * 4. Mirrored for CENTRE ORIGIN compliance
 *
 * The characteristic "snap" comes from tanh() normalisation.
 *
 * Per-zone state: ZoneComposer reuses one instance across up to 4 zones.
 * ALL temporal state is dimensioned [kMaxZones] and indexed by ctx.zoneId
 * to prevent cross-zone contamination.
 *
 * Effect ID: 98
 * Family: PARTY (audio-reactive)
 *
 * @author LightwaveOS Team
 * @version 5.0.0 - Per-zone PSRAM state for ZoneComposer compliance
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SnapwaveLinearEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_SNAPWAVE_LINEAR;

    SnapwaveLinearEffect() = default;
    ~SnapwaveLinearEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // ============================================================================
    // Zone & Algorithm Constants
    // ============================================================================
    static constexpr uint8_t kMaxZones = 4;

    // Peak smoothing: 2% new, 98% old (very aggressive)
    static constexpr float PEAK_ATTACK = 0.02f;
    static constexpr float PEAK_DECAY = 0.98f;

    // History buffer for trails
    static constexpr uint16_t HISTORY_SIZE = 40;  // 40 frames of history = ~333ms at 120fps

    // Trail fade: each history step gets dimmer
    // 0.85^40 ≈ 0.001, so oldest entries are nearly black
    static constexpr float TRAIL_FADE_FACTOR = 0.85f;

    // Oscillation parameters
    static constexpr float BASE_FREQUENCY = 0.001f;     // Base frequency for sin()
    static constexpr float PHASE_SPREAD = 0.5f;         // Phase offset per chromagram note
    static constexpr float TANH_SCALE = 2.0f;           // tanh() input multiplier
    static constexpr float NOTE_THRESHOLD = 0.1f;       // Min chromagram value to contribute
    static constexpr float AMPLITUDE_MIX = 0.7f;        // Oscillation x peak mix factor
    static constexpr float ENERGY_GATE_THRESHOLD = 0.05f; // RMS below this = silence (no movement)

    // Colour thresholds
    static constexpr float COLOR_THRESHOLD = 0.05f;     // Min brightness for colour contribution

    // ============================================================================
    // Per-zone State (indexed by ctx.zoneId)
    // ============================================================================

    // Smoothed peak follower -- per-zone to prevent cross-zone bleed
    float m_peakSmoothed[kMaxZones] = {};

    // Ring buffer write position -- per-zone
    uint8_t m_historyIndex[kMaxZones] = {};

    // ============================================================================
    // PSRAM-Allocated Buffers (per-zone dimensioned)
    // ============================================================================
#ifndef NATIVE_BUILD
    struct SnapwavePsram {
        uint8_t distanceHistory[kMaxZones][HISTORY_SIZE];
        CRGB    colorHistory[kMaxZones][HISTORY_SIZE];
    };
    SnapwavePsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    // ============================================================================
    // Helper Methods
    // ============================================================================

    float computeOscillation(const plugins::EffectContext& ctx);
    CRGB computeChromaColor(const plugins::EffectContext& ctx);
    void pushHistory(int z, uint8_t distance, CRGB color);
    void renderHistoryToLeds(int z, plugins::EffectContext& ctx);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
