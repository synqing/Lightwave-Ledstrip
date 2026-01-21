/**
 * @file types.h
 * @brief Emotiscope DSP data structures
 *
 * Ported from Emotiscope.HIL/v1.1_build/types.h
 * Contains structures for Goertzel frequency bins and tempo detection.
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {
namespace emotiscope {

/**
 * @brief Goertzel frequency bin state
 *
 * Each bin tracks a specific frequency using the Goertzel algorithm.
 * 64 bins are used, spanning A1 (55 Hz) to C7 (2093 Hz) in semitone steps.
 */
struct FreqBin {
    float target_freq = 0.0f;           ///< Target frequency in Hz
    float coeff = 0.0f;                 ///< Goertzel coefficient (2 * cos(w))
    float window_step = 0.0f;           ///< Step through window lookup table
    float magnitude = 0.0f;             ///< Current magnitude (0.0-1.0, auto-ranged)
    float magnitude_full_scale = 0.0f;  ///< Magnitude before auto-ranging
    float magnitude_last = 0.0f;        ///< Previous magnitude (for novelty)
    float novelty = 0.0f;               ///< Spectral flux (positive change)
    uint16_t block_size = 0;            ///< Number of samples to process
};

/**
 * @brief Tempo bin state for beat detection
 *
 * Each bin tracks a specific BPM using Goertzel on the novelty curve.
 * 96 bins cover 48-143 BPM range.
 */
struct TempoBin {
    float target_tempo_hz = 0.0f;       ///< Target tempo in Hz (BPM/60)
    float coeff = 0.0f;                 ///< Goertzel coefficient
    float sine = 0.0f;                  ///< sin(w) for phase calculation
    float cosine = 0.0f;                ///< cos(w) for phase calculation
    float window_step = 0.0f;           ///< Step through window lookup
    float phase = 0.0f;                 ///< Current phase angle (radians)
    float phase_target = 0.0f;          ///< Target phase (unused currently)
    bool phase_inverted = false;        ///< Phase inversion tracking
    float phase_radians_per_frame = 0.0f; ///< Phase advance per reference frame
    float beat = 0.0f;                  ///< Beat signal (sin(phase), -1 to 1)
    float magnitude = 0.0f;             ///< Tempo strength (0.0-1.0, auto-ranged)
    float magnitude_full_scale = 0.0f;  ///< Magnitude before auto-ranging
    uint32_t block_size = 0;            ///< Novelty samples to process
};

/**
 * @brief Output structure for effects integration
 *
 * This structure provides the data that effects need.
 * Updated each audio hop and published to ControlBus.
 */
struct EmotiscopeOutput {
    // Spectrogram (64 bins)
    float spectrogram[64] = {0};        ///< Current magnitude per bin (0.0-1.0)
    float spectrogram_smooth[64] = {0}; ///< Smoothed magnitude (12-sample average)

    // Chromagram (12 pitch classes)
    float chromagram[12] = {0};         ///< Pitch class energy (0.0-1.0)

    // VU Metering
    float vu_level = 0.0f;              ///< Current loudness (0.0-1.0)
    float vu_max = 0.0f;                ///< Peak level since last reset
    float vu_floor = 0.0f;              ///< Estimated noise floor

    // Novelty (spectral flux)
    float novelty_curve[1024] = {0};    ///< Raw novelty history
    float novelty_normalized[1024] = {0}; ///< Normalized novelty history
    float current_novelty = 0.0f;       ///< Latest novelty value

    // Tempo detection
    float tempi_magnitude[96] = {0};    ///< Magnitude per tempo bin
    float tempi_phase[96] = {0};        ///< Phase per tempo bin (radians)
    float tempi_beat[96] = {0};         ///< Beat signal per tempo bin

    // Top tempo results
    uint8_t top_bpm_index = 0;          ///< Index of strongest tempo bin
    float top_bpm_magnitude = 0.0f;     ///< Strength of top tempo
    float tempo_confidence = 0.0f;      ///< Overall tempo confidence

    // State flags
    bool silence_detected = false;      ///< True if audio is silent/quiet
    float silence_level = 0.0f;         ///< Silence amount (0=loud, 1=silent)

    // Sequence number (for freshness detection)
    uint32_t hop_seq = 0;               ///< Increments each audio hop
};

} // namespace emotiscope
} // namespace audio
} // namespace lightwaveos
