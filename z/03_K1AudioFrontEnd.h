/**
 * @file K1AudioFrontEnd.h
 * @brief K1 Dual-Bank Goertzel Front-End Orchestrator
 *
 * Produces AudioFeatureFrame every hop (125 Hz).
 * HarmonyBank computes every 2 hops (62.5 Hz).
 * Overload policy: drop harmony, never rhythm.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include "K1Types.h"
#include "K1Spec.h"
#include "AudioRingBuffer.h"
#include "WindowBank.h"
#include "GoertzelBank.h"
#include "NoiseFloor.h"
#include "AGC.h"
#include "NoveltyFlux.h"
#include "ChromaExtractor.h"
#include "ChromaStability.h"
#include "K1GoertzelTables_16k.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief K1 Audio Front-End orchestrator
 *
 * Coordinates all K1 modules to produce AudioFeatureFrame output.
 */
class K1AudioFrontEnd {
public:
    /**
     * @brief Constructor
     */
    K1AudioFrontEnd();

    /**
     * @brief Destructor
     */
    ~K1AudioFrontEnd();

    /**
     * @brief Initialize front-end
     *
     * @return true if successful
     */
    bool init();

    /**
     * @brief Process one hop of audio
     *
     * @param chunk Audio chunk (128 samples)
     * @param is_clipping Clipping flag
     * @return AudioFeatureFrame output
     */
    AudioFeatureFrame processHop(const AudioChunk& chunk, bool is_clipping);

    /**
     * @brief Get current feature frame (last processed)
     */
    const AudioFeatureFrame& getCurrentFrame() const { return m_currentFrame; }

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return m_initialized; }

private:
    AudioRingBuffer m_ringBuffer;        ///< Audio history ring buffer
    WindowBank m_windowBank;             ///< Window LUT bank
    GoertzelBank m_rhythmBank;           ///< Rhythm bank (24 bins)
    GoertzelBank m_harmonyBank;          ///< Harmony bank (64 bins)
    NoiseFloor m_rhythmNoiseFloor;        ///< Rhythm noise floor
    NoiseFloor m_harmonyNoiseFloor;       ///< Harmony noise floor
    AGC m_rhythmAGC;                     ///< Rhythm AGC (attenuation-only)
    AGC m_harmonyAGC;                    ///< Harmony AGC (mild boost)
    NoveltyFlux m_noveltyFlux;           ///< Novelty flux calculator
    ChromaExtractor m_chromaExtractor;    ///< Chroma extractor
    ChromaStability m_chromaStability;   ///< Chroma stability tracker

    // Scratch buffers
    float m_rhythmMags[RHYTHM_BINS];     ///< Rhythm magnitudes (post-processing)
    float m_harmonyMags[HARMONY_BINS];   ///< Harmony magnitudes (post-processing)
    float m_rhythmMagsRaw[RHYTHM_BINS];  ///< Rhythm raw magnitudes
    float m_harmonyMagsRaw[HARMONY_BINS]; ///< Harmony raw magnitudes
    float m_chroma12[12];                ///< Chroma output

    AudioFeatureFrame m_currentFrame;     ///< Current feature frame
    uint32_t m_hopIndex;                  ///< Hop counter
    bool m_initialized;                   ///< Initialization flag

    /**
     * @brief Extract unique N values from bin tables
     *
     * @param specs Bin specifications
     * @param num_bins Number of bins
     * @param out_N Output array (must have space for at least 10 values)
     * @param out_count Output count
     */
    void extractUniqueN(const GoertzelBinSpec* specs, size_t num_bins, uint16_t* out_N, size_t* out_count);
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

