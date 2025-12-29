/**
 * @file K1Messages.h
 * @brief Message types for K1-Lightwave beat tracker integration
 *
 * Defines the data structures passed from K1 audio pipeline (Core 1)
 * to the renderer (Core 0) via lock-free queues.
 *
 * K1TempoUpdate: Tempo changes from Stage 3 (Tactus Resolver)
 * K1BeatEvent: Beat ticks from Stage 4 (PLL Beat Clock)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Tempo update message from K1 beat tracker
 *
 * Published by K1 Stage 3 (Tactus Resolver) when:
 * - BPM estimate changes significantly
 * - Confidence changes (lock state transitions)
 *
 * Queue: LockFreeQueue<K1TempoUpdate, 8>
 * Rate: ~10 Hz (throttled, only on significant change)
 */
struct K1TempoUpdate {
    uint32_t timestamp_ms;  ///< millis() when tempo was estimated
    float bpm;              ///< Estimated BPM (60-180 range)
    float confidence;       ///< Confidence [0,1] from Stage 3
    bool is_locked;         ///< True if tracker is in LOCKED state
};

/**
 * @brief Beat event message from K1 beat tracker
 *
 * Published by K1 Stage 4 (PLL Beat Clock) on each beat tick.
 * The renderer uses these to trigger beat-reactive effects.
 *
 * Queue: LockFreeQueue<K1BeatEvent, 16>
 * Rate: 1-3 Hz (BPM / 60)
 */
struct K1BeatEvent {
    uint32_t timestamp_ms;  ///< millis() when beat was detected
    float phase01;          ///< Phase within beat [0,1) at detection time
    int beat_in_bar;        ///< Position in bar (0-3 for 4/4 time)
    bool is_downbeat;       ///< True if beat_in_bar == 0
    float strength;         ///< Novelty z-score mapped to [0,1]
};

/**
 * @brief K1 pipeline state summary
 *
 * Optional: snapshot of full K1 state for debugging/visualization.
 * Not typically queued - read directly from K1Pipeline via atomic.
 */
struct K1StateSnapshot {
    uint32_t timestamp_ms;

    // Stage 1: Novelty
    float novelty_z;        ///< Current novelty z-score [-6, +6]
    bool onset_detected;    ///< True if onset above threshold

    // Stage 2: Resonators
    float top_bpm;          ///< Top resonator BPM
    float top_magnitude;    ///< Top resonator magnitude

    // Stage 3: Tactus
    float tactus_bpm;       ///< Resolved tactus BPM
    float tactus_confidence; ///< Tactus confidence [0,1]

    // Stage 4: Beat Clock
    float phase01;          ///< Current beat phase [0,1)
    float freq_hz;          ///< PLL frequency (Hz)
    bool is_locked;         ///< Lock state
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos
