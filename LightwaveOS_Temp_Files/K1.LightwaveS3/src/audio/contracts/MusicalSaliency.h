/**
 * @file MusicalSaliency.h
 * @brief Musical saliency metrics for adaptive audio-visual intelligence
 *
 * This module computes "what's perceptually important" in the current audio,
 * enabling effects to respond to the MOST SALIENT features rather than all
 * audio signals equally.
 *
 * Part of the Musical Intelligence System (MIS) - Phase 1
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {

/**
 * @brief Types of musical saliency
 *
 * Each type represents a different dimension of "musical importance"
 */
enum class SaliencyType : uint8_t {
    HARMONIC = 0,   ///< Chord/key changes (slow, emotional)
    RHYTHMIC = 1,   ///< Beat pattern changes (fast, structural)
    TIMBRAL = 2,    ///< Spectral character changes (texture)
    DYNAMIC = 3     ///< Loudness envelope changes (energy)
};

/**
 * @brief Musical saliency metrics computed per audio hop
 *
 * These metrics indicate what's "musically important" RIGHT NOW.
 * Effects should respond primarily to the dominant saliency type,
 * not blindly to all audio signals.
 *
 * All values are normalized 0.0-1.0 where:
 * - 0.0 = no change / not salient
 * - 1.0 = maximum change / highly salient
 */
struct MusicalSaliencyFrame {
    //-------------------------------------------------------------------------
    // Novelty Metrics (higher = something changed)
    //-------------------------------------------------------------------------

    /**
     * @brief Harmonic novelty - chord/key changes
     *
     * High when: chord root changes, major/minor shift, key modulation
     * Drives: Color/palette changes, mood shifts
     * Temporal class: SLOW (500ms-5s)
     */
    float harmonicNovelty = 0.0f;

    /**
     * @brief Rhythmic novelty - beat pattern changes
     *
     * High when: beat drops, tempo changes, rhythm pattern shifts
     * Drives: Motion speed, pulse timing
     * Temporal class: REACTIVE (100-300ms)
     */
    float rhythmicNovelty = 0.0f;

    /**
     * @brief Timbral novelty - spectral character changes
     *
     * High when: instrument changes, frequency distribution shifts
     * Drives: Texture, shimmer, complexity
     * Temporal class: SUSTAINED (300ms-2s)
     */
    float timbralNovelty = 0.0f;

    /**
     * @brief Dynamic novelty - loudness envelope changes
     *
     * High when: crescendo, decrescendo, sudden volume changes
     * Drives: Intensity, brightness, expansion
     * Temporal class: REACTIVE (100-300ms)
     */
    float dynamicNovelty = 0.0f;

    //-------------------------------------------------------------------------
    // Derived Saliency (composite metrics)
    //-------------------------------------------------------------------------

    /**
     * @brief Overall saliency score (0.0-1.0)
     *
     * Weighted combination of all novelty types.
     * Used for "something interesting is happening" detection.
     */
    float overallSaliency = 0.0f;

    /**
     * @brief Which saliency type is currently dominant
     *
     * Effects can use this to decide WHAT to respond to.
     * Cast to SaliencyType enum for comparison.
     */
    uint8_t dominantType = static_cast<uint8_t>(SaliencyType::DYNAMIC);

    //-------------------------------------------------------------------------
    // History state (for computing derivatives)
    //-------------------------------------------------------------------------

    /**
     * @brief Previous chord root (for harmonic novelty)
     */
    uint8_t prevChordRoot = 0;

    /**
     * @brief Previous chord type (for harmonic novelty)
     */
    uint8_t prevChordType = 0;

    /**
     * @brief Previous flux value (for timbral novelty)
     */
    float prevFlux = 0.0f;

    /**
     * @brief Previous RMS value (for dynamic novelty)
     */
    float prevRms = 0.0f;

    /**
     * @brief Beat interval history for variance calculation
     * Ring buffer of last 4 beat intervals (ms)
     */
    float beatIntervalHistory[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint8_t beatIntervalIdx = 0;
    float lastBeatTimeMs = 0.0f;

    //-------------------------------------------------------------------------
    // Smoothing state
    //-------------------------------------------------------------------------

    /**
     * @brief Smoothed harmonic novelty (asymmetric: fast rise, slow fall)
     */
    float harmonicNoveltySmooth = 0.0f;

    /**
     * @brief Smoothed rhythmic novelty
     */
    float rhythmicNoveltySmooth = 0.0f;

    /**
     * @brief Smoothed timbral novelty
     */
    float timbralNoveltySmooth = 0.0f;

    /**
     * @brief Smoothed dynamic novelty
     */
    float dynamicNoveltySmooth = 0.0f;

    //-------------------------------------------------------------------------
    // Methods
    //-------------------------------------------------------------------------

    /**
     * @brief Get the dominant saliency type as enum
     */
    SaliencyType getDominantType() const {
        return static_cast<SaliencyType>(dominantType);
    }

    /**
     * @brief Check if a specific saliency type is above threshold
     */
    bool isSalient(SaliencyType type, float threshold = 0.3f) const {
        switch (type) {
            case SaliencyType::HARMONIC: return harmonicNoveltySmooth > threshold;
            case SaliencyType::RHYTHMIC: return rhythmicNoveltySmooth > threshold;
            case SaliencyType::TIMBRAL:  return timbralNoveltySmooth > threshold;
            case SaliencyType::DYNAMIC:  return dynamicNoveltySmooth > threshold;
            default: return false;
        }
    }

    /**
     * @brief Get novelty value for a specific type
     */
    float getNovelty(SaliencyType type) const {
        switch (type) {
            case SaliencyType::HARMONIC: return harmonicNoveltySmooth;
            case SaliencyType::RHYTHMIC: return rhythmicNoveltySmooth;
            case SaliencyType::TIMBRAL:  return timbralNoveltySmooth;
            case SaliencyType::DYNAMIC:  return dynamicNoveltySmooth;
            default: return 0.0f;
        }
    }
};

/**
 * @brief Tuning parameters for saliency computation
 */
struct SaliencyTuning {
    // Smoothing time constants (seconds)
    float harmonicRiseTime = 0.15f;   // Fast rise for chord changes
    float harmonicFallTime = 0.80f;   // Slow fall for sustained mood
    float rhythmicRiseTime = 0.05f;   // Very fast for beat detection
    float rhythmicFallTime = 0.30f;   // Medium fall
    float timbralRiseTime = 0.10f;    // Fast rise for spectral changes
    float timbralFallTime = 0.50f;    // Medium fall
    float dynamicRiseTime = 0.08f;    // Fast rise for transients
    float dynamicFallTime = 0.40f;    // Medium fall

    // Thresholds for novelty detection
    float harmonicChangeThreshold = 0.5f;  // Chord must change significantly
    float fluxDerivativeThreshold = 0.05f; // Was 0.1 - lowered for timbral sensitivity
    float rmsDerivativeThreshold = 0.02f;  // Was 0.05 - lowered for dynamic sensitivity
    float beatVarianceThreshold = 0.15f;   // Beat interval variance threshold

    // Weights for overall saliency computation
    float harmonicWeight = 0.25f;
    float rhythmicWeight = 0.30f;
    float timbralWeight = 0.20f;
    float dynamicWeight = 0.25f;
};

} // namespace audio
} // namespace lightwaveos
