/**
 * @file TempoOutput.h
 * @brief Legacy tempo output struct for backward compatibility
 *
 * This header provides backward compatibility for code that references
 * TempoOutput. Tempo functionality is now part of MusicalGrid.
 *
 * NOTE: This is a DIFFERENT struct from audio::TempoTrackerOutput defined
 * in tempo/TempoTracker.h. That struct has additional fields (locked,
 * beat_strength, phase01). This legacy struct exists only for backward
 * compatibility with code expecting the simpler interface.
 */

#pragma once

#include "MusicalGrid.h"

namespace lightwaveos {
namespace audio {

/**
 * @brief Legacy tempo output struct for backward compatibility
 * @deprecated Use MusicalGridSnapshot instead
 */
struct TempoOutput {
    float bpm = 120.0f;
    float confidence = 0.0f;
    float beatPhase = 0.0f;
    bool beatTick = false;

    // Construct from MusicalGridSnapshot for compatibility
    static TempoOutput fromSnapshot(const MusicalGridSnapshot& snap) {
        TempoOutput out;
        out.bpm = snap.bpm_smoothed;
        out.confidence = snap.tempo_confidence;
        out.beatPhase = snap.beat_phase01;
        out.beatTick = snap.beat_tick;
        return out;
    }
};

} // namespace audio
} // namespace lightwaveos
