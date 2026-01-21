/**
 * @file ChromaExtractor.h
 * @brief Chroma extraction from harmony bins
 *
 * Maps 64 semitone bins → 12 chroma bins (sum-normalized).
 * Provides keyClarity() peakiness metric.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include "K1Spec.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Chroma extractor
 *
 * Converts 64 semitone bins to 12 pitch classes (chroma).
 */
class ChromaExtractor {
public:
    /**
     * @brief Constructor
     */
    ChromaExtractor();

    /**
     * @brief Initialize chroma extractor
     */
    void init();

    /**
     * @brief Extract chroma from harmony bins
     *
     * @param harmony_bins 64 semitone bin magnitudes
     * @param chroma_out Output chroma array (12 elements, sum-normalized)
     */
    void extract(const float* harmony_bins, float* chroma_out);

    /**
     * @brief Compute key clarity (peakiness metric)
     *
     * Measures how "peaky" the chroma distribution is.
     * Higher values indicate clearer key center.
     *
     * @param chroma Chroma array (12 elements)
     * @return Key clarity value [0.0, 1.0]
     */
    float keyClarity(const float* chroma) const;

private:
    /**
     * @brief Map semitone index to chroma class
     *
     * @param semitone_idx Semitone index (0-63, A2=0)
     * @return Chroma class (0-11, C=0, C#=1, ..., B=11)
     */
    uint8_t semitoneToChroma(uint8_t semitone_idx) const;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

