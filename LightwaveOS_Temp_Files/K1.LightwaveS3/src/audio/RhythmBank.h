/**
 * @file RhythmBank.h
 * @brief Rhythm-focused Goertzel bank for onset detection
 *
 * Covers 60-600 Hz (kick drums, snares, toms) with 24 bins.
 * Uses fast attack/slow release AGC to preserve onset transients.
 *
 * Architecture:
 * - GoertzelBank: 24 bins, variable window sizes
 * - NoiseFloor: Per-bin noise estimation (1.0s time constant)
 * - AGC: Fast attack (10ms) / slow release (500ms) for transient preservation
 * - NoveltyFlux: 24-bin spectral flux for onset detection
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Phase 2 Integration
 */

#pragma once

#include <cstdint>
#include <cstring>
#include "AudioRingBuffer.h"
#include "GoertzelBank.h"
#include "NoiseFloor.h"
#include "AGC.h"
#include "NoveltyFlux.h"

namespace lightwaveos {
namespace audio {

/**
 * @brief Rhythm-focused Goertzel bank for onset detection
 *
 * Configuration:
 * - 24 bins covering 60-600 Hz
 * - Window sizes: 1536 (bass), 512 (mid), 256 (high)
 * - AGC: attackTime=10ms, releaseTime=500ms, targetLevel=0.7
 * - NoiseFloor: timeConstant=1.0s
 *
 * Memory footprint: ~600 bytes
 */
class RhythmBank {
public:
    static constexpr uint8_t NUM_BINS = 24;
    static constexpr float MIN_FREQ = 60.0f;
    static constexpr float MAX_FREQ = 600.0f;

    /**
     * @brief Construct RhythmBank with default configuration
     */
    RhythmBank();

    /**
     * @brief Process audio ring buffer and compute rhythm features
     *
     * Extracts 24-bin spectrum, applies noise floor and AGC,
     * computes spectral flux for onset detection.
     *
     * @param ringBuffer Audio samples (2048-sample ring buffer)
     */
    void process(const AudioRingBuffer<float, 2048>& ringBuffer);

    /**
     * @brief Get onset flux strength [0.0, ∞)
     *
     * Higher values indicate stronger onsets (kick, snare transients).
     */
    float getFlux() const { return m_flux; }

    /**
     * @brief Get 24-bin magnitude spectrum (after AGC + noise floor)
     *
     * @return Pointer to 24-element float array
     */
    const float* getMagnitudes() const { return m_magnitudes; }

private:
    GoertzelBank m_goertzel;        ///< 24-bin Goertzel analyzer
    NoiseFloor m_noiseFloor;        ///< Per-bin noise floor tracker
    AGC m_agc;                      ///< Fast attack/slow release AGC
    NoveltyFlux m_noveltyFlux;      ///< 24-bin spectral flux

    float m_magnitudes[NUM_BINS];   ///< Output magnitude spectrum
    float m_flux;                   ///< Onset flux strength
};

} // namespace audio
} // namespace lightwaveos
