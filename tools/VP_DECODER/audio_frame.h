#ifndef AUDIO_FRAME_H
#define AUDIO_FRAME_H

#include <stdint.h>

// The number of frequency bins provided by the FFT analysis.
constexpr int FFT_BIN_COUNT = 96;

// An immutable data contract representing a single snapshot of audio analysis.
// This struct is passed from the audio processor to the visual pipeline.
// Effects MUST treat this data as read-only.
struct AudioFrame {
    // Raw frequency data from the FFT.
    const float* frequency_bins; // Size: FFT_BIN_COUNT

    // Pre-calculated energy values for different spectral zones.
    float total_energy = 0.0f;
    float bass_energy = 0.0f;
    float mid_energy = 0.0f;
    float high_energy = 0.0f;

    // The single source of truth for silence detection.
    // If true, the visual pipeline should render black.
    bool silence = true;

    // Detected transient (e.g., drum hit) flag.
    bool transient_detected = false;

    // --- SB+ additions ---
    // Beat detection results for this frame.
    bool beat_detected = false;
    // Confidence of beat detection (0.0–1.0).
    float beat_confidence = 0.0f;
    // Current tempo estimate in BPM (0 if unknown).
    float bpm_estimate = 0.0f;
};

#endif // AUDIO_FRAME_H 