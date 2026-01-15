/**
 * @file HarmonyBank.h
 * @brief Harmony-focused Goertzel bank for chroma and stability
 *
 * Covers 200-2000 Hz (semitone resolution) with 64 bins.
 * Uses moderate attack/release AGC to preserve harmonic structure.
 *
 * Architecture:
 * - GoertzelBank: 64 bins, variable window sizes (1024 @ 200 Hz → 256 @ 2000 Hz)
 * - NoiseFloor: Per-bin noise estimation (1.0s time constant)
 * - AGC: Moderate attack (50ms) / moderate release (300ms)
 * - NoveltyFlux: 64-bin spectral flux for harmonic change detection
 * - ChromaExtractor: 12-bin chroma (A-G#)
 * - ChromaStability: Harmonic stability metric [0,1]
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
#include "ChromaExtractor.h"
#include "ChromaStability.h"

namespace lightwaveos {
namespace audio {

/**
 * @brief Harmony-focused Goertzel bank for chroma and stability
 *
 * Configuration:
 * - 64 bins covering 200-2000 Hz (semitone resolution)
 * - Window sizes: Vary from 1024 (200 Hz) to 256 (2000 Hz)
 * - AGC: attackTime=50ms, releaseTime=300ms, targetLevel=0.7
 * - NoiseFloor: timeConstant=1.0s
 * - Chroma stability: windowSize=8 frames
 *
 * Memory footprint: ~1.5 KB
 */
class HarmonyBank {
public:
    static constexpr uint8_t NUM_BINS = 64;
    static constexpr uint8_t NUM_CHROMA = 12;
    static constexpr float MIN_FREQ = 200.0f;
    static constexpr float MAX_FREQ = 2000.0f;

    /**
     * @brief Construct HarmonyBank with default configuration
     */
    HarmonyBank();

    /**
     * @brief Destructor (frees ChromaStability heap allocation)
     */
    ~HarmonyBank();

    /**
     * @brief Process audio ring buffer and compute harmony features
     *
     * Extracts 64-bin spectrum, applies noise floor and AGC,
     * computes spectral flux, chroma, and harmonic stability.
     *
     * @param ringBuffer Audio samples (2048-sample ring buffer)
     */
    void process(const AudioRingBuffer<float, 2048>& ringBuffer);

    /**
     * @brief Get onset flux strength [0.0, ∞)
     *
     * Higher values indicate stronger harmonic changes.
     */
    float getFlux() const { return m_flux; }

    /**
     * @brief Get 64-bin magnitude spectrum (after AGC + noise floor)
     *
     * @return Pointer to 64-element float array
     */
    const float* getMagnitudes() const { return m_magnitudes; }

    /**
     * @brief Get 12-bin chroma (A-G#)
     *
     * @return Pointer to 12-element float array
     */
    const float* getChroma() const { return m_chroma; }

    /**
     * @brief Get harmonic stability [0.0, 1.0]
     *
     * 0.0 = unstable (rapid harmonic changes)
     * 1.0 = stable (consistent harmonic structure)
     */
    float getStability() const {
        return m_stability ? m_stability->getStability() : 0.0f;
    }

private:
    GoertzelBank m_goertzel;        ///< 64-bin Goertzel analyzer
    NoiseFloor m_noiseFloor;        ///< Per-bin noise floor tracker
    AGC m_agc;                      ///< Moderate attack/release AGC
    NoveltyFlux m_noveltyFlux;      ///< 64-bin spectral flux
    ChromaExtractor m_chromaExtractor; ///< 12-bin chroma extraction
    ChromaStability* m_stability;   ///< Chroma stability tracker (heap-allocated)

    float m_magnitudes[NUM_BINS];   ///< Output magnitude spectrum
    float m_chroma[NUM_CHROMA];     ///< 12-bin chroma (A-G#)
    float m_flux;                   ///< Onset flux strength
};

} // namespace audio
} // namespace lightwaveos
