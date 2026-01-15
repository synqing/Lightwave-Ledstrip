/**
 * @file config.h
 * @brief Emotiscope DSP configuration constants
 *
 * Ported from Emotiscope.HIL/v1.1_build/global_defines.h and microphone.h
 * These constants define the core DSP parameters for audio analysis.
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {
namespace emotiscope {

// ============================================================================
// Audio Capture Configuration
// ============================================================================

/// I2S sample rate in Hz (Emotiscope native rate)
constexpr uint32_t SAMPLE_RATE = 12800;

/// Samples per audio capture chunk
constexpr uint16_t CHUNK_SIZE = 64;

/// Derived: hop rate in Hz (SAMPLE_RATE / CHUNK_SIZE = 200 Hz)
constexpr float HOP_RATE_HZ = static_cast<float>(SAMPLE_RATE) / CHUNK_SIZE;

/// Derived: hop duration in ms
constexpr float HOP_DURATION_MS = (CHUNK_SIZE * 1000.0f) / SAMPLE_RATE;

// ============================================================================
// Goertzel Frequency Analysis
// ============================================================================

/// Number of semitone-spaced frequency bins (A1 to C7)
constexpr uint16_t NUM_FREQS = 64;

/// Length of sample history buffer (~320ms at 12.8kHz)
constexpr uint16_t SAMPLE_HISTORY_LENGTH = 4096;

/// Window lookup table size (Gaussian window)
constexpr uint16_t WINDOW_LOOKUP_SIZE = 4096;

/// Number of samples to average for smooth spectrogram
constexpr uint8_t NUM_SPECTROGRAM_AVERAGE_SAMPLES = 12;

/// Gaussian window sigma parameter
constexpr float GAUSSIAN_SIGMA = 0.8f;

// ============================================================================
// Chromagram Configuration
// ============================================================================

/// Number of pitch classes (C, C#, D, ..., B)
constexpr uint8_t NUM_CHROMA = 12;

// ============================================================================
// Tempo/Beat Detection
// ============================================================================

/// Number of tempo bins (one per BPM from TEMPO_LOW to TEMPO_HIGH-1)
constexpr uint16_t NUM_TEMPI = 96;

/// Lowest BPM to detect
constexpr uint16_t TEMPO_LOW = 48;

/// Highest BPM to detect (exclusive: 48-143 BPM range)
constexpr uint16_t TEMPO_HIGH = TEMPO_LOW + NUM_TEMPI;

/// Novelty curve logging rate (frames per second)
constexpr uint16_t NOVELTY_LOG_HZ = 50;

/// Length of novelty curve history (~20.48 seconds at 50 Hz)
constexpr uint16_t NOVELTY_HISTORY_LENGTH = 1024;

/// Beat phase shift percentage (anticipation offset)
constexpr float BEAT_SHIFT_PERCENT = 0.16f;

/// Reference FPS for phase calculations (GPU frame rate)
constexpr float REFERENCE_FPS = 120.0f;

// ============================================================================
// VU Meter Configuration
// ============================================================================

/// Number of samples in VU history for noise floor estimation
constexpr uint8_t NUM_VU_LOG_SAMPLES = 20;

/// Number of samples for VU smoothing
constexpr uint8_t NUM_VU_SMOOTH_SAMPLES = 12;

// ============================================================================
// Mathematical Constants
// ============================================================================

constexpr float TWOPI = 6.28318530f;
constexpr float FOURPI = 12.56637061f;
constexpr float SIXPI = 18.84955593f;

// ============================================================================
// Musical Note Frequencies (quarter-step table, 256 entries)
// A1 = 55Hz at index 0, increases logarithmically
// ============================================================================

/// Bottom note index in quarter-step table (A1 = index 12)
constexpr uint16_t BOTTOM_NOTE = 12;

/// Step size in quarter-steps (2 = half-step = semitone)
constexpr uint16_t NOTE_STEP = 2;

/// Quarter-step note frequency table (256 entries)
/// Index 0 = A1 (55 Hz), each step = 1/4 semitone
constexpr float NOTES[] = {
    55.0f, 56.635235f, 58.27047f, 60.00294f, 61.73541f, 63.5709f, 65.40639f, 67.351025f,
    69.29566f, 71.355925f, 73.41619f, 75.59897f, 77.78175f, 80.09432f, 82.40689f, 84.856975f,
    87.30706f, 89.902835f, 92.49861f, 95.248735f, 97.99886f, 100.91253f, 103.8262f, 106.9131f,
    110.0f, 113.27045f, 116.5409f, 120.00585f, 123.4708f, 127.1418f, 130.8128f, 134.70205f,
    138.5913f, 142.71185f, 146.8324f, 151.19795f, 155.5635f, 160.18865f, 164.8138f, 169.71395f,
    174.6141f, 179.80565f, 184.9972f, 190.49745f, 195.9977f, 201.825f, 207.6523f, 213.82615f,
    220.0f, 226.54095f, 233.0819f, 240.0118f, 246.9417f, 254.28365f, 261.6256f, 269.4041f,
    277.1826f, 285.4237f, 293.6648f, 302.3959f, 311.127f, 320.3773f, 329.6276f, 339.4279f,
    349.2282f, 359.6113f, 369.9944f, 380.9949f, 391.9954f, 403.65005f, 415.3047f, 427.65235f,
    440.0f, 453.0819f, 466.1638f, 480.02355f, 493.8833f, 508.5672f, 523.2511f, 538.8082f,
    554.3653f, 570.8474f, 587.3295f, 604.79175f, 622.254f, 640.75455f, 659.2551f, 678.8558f,
    698.4565f, 719.22265f, 739.9888f, 761.98985f, 783.9909f, 807.30015f, 830.6094f, 855.3047f,
    880.0f, 906.16375f, 932.3275f, 960.04705f, 987.7666f, 1017.1343f, 1046.502f, 1077.6165f,
    1108.731f, 1141.695f, 1174.659f, 1209.5835f, 1244.508f, 1281.509f, 1318.51f, 1357.7115f,
    1396.913f, 1438.4455f, 1479.978f, 1523.98f, 1567.982f, 1614.6005f, 1661.219f, 1710.6095f,
    1760.0f, 1812.3275f, 1864.655f, 1920.094f, 1975.533f, 2034.269f, 2093.005f, 2155.233f,
    2217.461f, 2283.3895f, 2349.318f, 2419.167f, 2489.016f, 2563.018f, 2637.02f, 2715.4225f,
    2793.825f, 2876.8905f, 2959.956f, 3047.96f, 3135.964f, 3229.2005f, 3322.437f, 3421.2185f,
    3520.0f, 3624.655f, 3729.31f, 3840.1875f, 3951.065f, 4068.537f, 4186.009f, 4310.4655f,
    4434.922f, 4566.779f, 4698.636f, 4838.334f, 4978.032f, 5126.0365f, 5274.041f, 5430.8465f,
    5587.652f, 5753.7815f, 5919.911f, 6095.919f, 6271.927f, 6458.401f, 6644.875f, 6842.4375f,
    7040.0f, 7249.31f, 7458.62f, 7680.375f, 7902.13f, 8137.074f, 8372.018f, 8620.931f,
    8869.844f, 9133.558f, 9397.272f, 9676.668f, 9956.064f, 10252.072f, 10548.08f, 10861.69f,
    11175.3f, 11507.56f, 11839.82f, 12191.835f, 12543.85f, 12916.8f, 13289.75f, 13684.875f,
    14080.0f, 14498.62f, 14917.24f, 15360.75f, 15804.26f, 16274.145f
};

} // namespace emotiscope
} // namespace audio
} // namespace lightwaveos
