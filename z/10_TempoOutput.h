#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {

/**
 * @brief Tempo tracker output for effect consumption
 *
 * Provides all the information effects need for beat-synchronized visuals.
 * Decoupled from the beat tracker implementation (TempoTracker/Emotiscope).
 */
struct TempoOutput {
    float bpm;                      ///< Current detected tempo
    float phase01;                  ///< Phase [0, 1) - 0 = beat instant
    float confidence;               ///< Confidence [0, 1] - how dominant this tempo is
    bool beat_tick;                 ///< True for one frame at beat instant
    bool locked;                    ///< True if confidence > threshold
    float beat_strength;            ///< Smoothed magnitude of winning bin
};

} // namespace audio
} // namespace lightwaveos
