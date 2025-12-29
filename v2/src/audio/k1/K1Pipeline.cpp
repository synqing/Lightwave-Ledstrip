/**
 * @file K1Pipeline.cpp
 * @brief K1-Lightwave Beat Tracker Pipeline Implementation
 */

#include "K1Pipeline.h"
#include "K1DebugMacros.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

float K1Pipeline::fluxToZScore(float flux) {
    // Scale Lightwave flux [0,1] to K1 z-score [-6, +6]
    // flux=0 → z=-6, flux=0.5 → z=0, flux=1 → z=+6
    return (flux * 12.0f) - 6.0f;
}

void K1Pipeline::begin(uint32_t now_ms) {
    resonators_.begin(now_ms);
    tactus_.begin(now_ms);
    beat_clock_.begin(now_ms);

    last_t_ms_ = now_ms;
    first_frame_ = true;
}

void K1Pipeline::reset() {
    uint32_t now = last_t_ms_;
    resonators_.begin(now);
    tactus_.reset();
    beat_clock_.begin(now);
    first_frame_ = true;
}

bool K1Pipeline::processNovelty(float flux, uint32_t t_ms, K1PipelineOutput& out) {
    K1_DEBUG_DECL();
    K1_DEBUG_START(t_ms);

    bool lock_changed = false;

    // Scale flux to z-score
    float novelty_z = fluxToZScore(flux);
    K1_DEBUG_NOVELTY(novelty_z);

    // Compute delta time for beat clock tick
    float delta_sec = 0.016f;  // Default 16ms
    if (!first_frame_ && t_ms > last_t_ms_) {
        delta_sec = (float)(t_ms - last_t_ms_) / 1000.0f;
        if (delta_sec > 0.1f) delta_sec = 0.1f;  // Clamp to 100ms max
    }
    first_frame_ = false;
    last_t_ms_ = t_ms;

    // Stage 2: Resonator Bank (runs at ~10 Hz internally)
    K1ResonatorFrame resonator_out;
    bool resonator_updated = resonators_.update(novelty_z, t_ms, resonator_out);

    // Stage 3 & 4: Only update when resonators produce output
    if (resonator_updated) {
        last_resonator_frame_ = resonator_out;
        K1_DEBUG_CAPTURE_RESONATORS(last_resonator_frame_);

        // Stage 3: Tactus Resolver
        K1TactusFrame tactus_out;
        tactus_.updateFromResonators(resonator_out, tactus_out);

        // Check for lock change
        if (tactus_out.locked != last_tactus_frame_.locked) {
            lock_changed = true;
            K1_DEBUG_FLAG_LOCK_CHANGED();
        }
        last_tactus_frame_ = tactus_out;
        K1_DEBUG_CAPTURE_TACTUS(last_tactus_frame_);

        // Stage 4: Beat Clock PLL update
        beat_clock_.updateFromTactus(tactus_out, t_ms, last_beat_clock_);
    }

    // Always tick the beat clock to advance phase
    beat_clock_.tick(t_ms, delta_sec, last_beat_clock_);
    K1_DEBUG_CAPTURE_CLOCK(last_beat_clock_);

    // Fill output
    out.t_ms = t_ms;
    out.phase01 = last_beat_clock_.phase01;
    out.beat_tick = last_beat_clock_.beat_tick;
    out.bpm = last_beat_clock_.bpm;
    out.confidence = last_beat_clock_.confidence;
    out.locked = last_beat_clock_.locked;

#if FEATURE_K1_DEBUG
    update_count_++;
    K1_DEBUG_END(debug_ring_, update_count_);
#endif

    return lock_changed;
}

void K1Pipeline::tick(uint32_t now_ms, float delta_sec, K1PipelineOutput& out) {
    // Just advance beat clock phase (no novelty input)
    beat_clock_.tick(now_ms, delta_sec, last_beat_clock_);

    out.t_ms = now_ms;
    out.phase01 = last_beat_clock_.phase01;
    out.beat_tick = last_beat_clock_.beat_tick;
    out.bpm = last_beat_clock_.bpm;
    out.confidence = last_beat_clock_.confidence;
    out.locked = last_beat_clock_.locked;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos
