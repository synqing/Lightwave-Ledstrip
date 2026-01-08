/**
 * @file ChromaExtractor.cpp
 * @brief Chroma extractor implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "ChromaExtractor.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace lightwaveos {
namespace audio {
namespace k1 {

ChromaExtractor::ChromaExtractor() {
}

void ChromaExtractor::init() {
    // No initialization needed
}

uint8_t ChromaExtractor::semitoneToChroma(uint8_t semitone_idx) const {
    // A2 is semitone 0, which is A (chroma 9)
    // Map: A=9, A#=10, B=11, C=0, C#=1, D=2, D#=3, E=4, F=5, F#=6, G=7, G#=8
    // Since A2 is the first bin, we need to offset
    // A2 = 0 → chroma 9 (A)
    // A#2 = 1 → chroma 10 (A#)
    // B2 = 2 → chroma 11 (B)
    // C3 = 3 → chroma 0 (C)
    // etc.
    
    // Starting from A (chroma 9), add semitone index, mod 12
    return (9 + semitone_idx) % 12;
}

void ChromaExtractor::extract(const float* harmony_bins, float* chroma_out) {
    if (harmony_bins == nullptr || chroma_out == nullptr) {
        return;
    }

    // Initialize chroma bins
    memset(chroma_out, 0, 12 * sizeof(float));

    // Sum semitone bins into chroma classes
    for (uint8_t i = 0; i < HARMONY_BINS; i++) {
        uint8_t chroma_class = semitoneToChroma(i);
        chroma_out[chroma_class] += harmony_bins[i];
    }

    // Sum-normalize
    float sum = 0.0f;
    for (int i = 0; i < 12; i++) {
        sum += chroma_out[i];
    }

    if (sum > 0.001f) {
        float inv_sum = 1.0f / sum;
        for (int i = 0; i < 12; i++) {
            chroma_out[i] *= inv_sum;
        }
    }
}

float ChromaExtractor::keyClarity(const float* chroma) const {
    if (chroma == nullptr) {
        return 0.0f;
    }

    // Find maximum chroma value
    float max_val = 0.0f;
    for (int i = 0; i < 12; i++) {
        if (chroma[i] > max_val) {
            max_val = chroma[i];
        }
    }

    // Compute variance (peakiness metric)
    float mean = 1.0f / 12.0f;  // Uniform distribution mean
    float variance = 0.0f;
    for (int i = 0; i < 12; i++) {
        float diff = chroma[i] - mean;
        variance += diff * diff;
    }
    variance /= 12.0f;

    // Key clarity: combination of max value and variance
    // Higher max and higher variance = clearer key
    float clarity = max_val * (1.0f + variance * 2.0f);
    return std::min(1.0f, clarity);
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

