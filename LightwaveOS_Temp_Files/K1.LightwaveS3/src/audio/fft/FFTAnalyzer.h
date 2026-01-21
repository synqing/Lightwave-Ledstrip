#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include "../tempo/TempoEngine.h" // For TEMPO_ENGINE_NOVELTY_FFT_BINS

namespace lightwaveos {
namespace audio {

class FFTAnalyzer {
public:
    static constexpr uint16_t SPECTRUM_BINS = TEMPO_ENGINE_NOVELTY_FFT_BINS;

    FFTAnalyzer() {
        reset();
    }

    void init() {
        reset();
    }

    void reset() {
        std::memset(m_spectrum, 0, sizeof(m_spectrum));
    }

    // Process FFT from half-rate sample history
    // Returns true if a new FFT frame was computed
    bool processFromHalfRateHistory(const float* sampleHistory, 
                                   uint16_t historyLength,
                                   uint16_t writeIndex, 
                                   uint16_t available) {
        // Dummy implementation: Return false to disable FFT processing
        return false;
    }

    const float* spectrum() const {
        return m_spectrum;
    }

private:
    float m_spectrum[SPECTRUM_BINS];
};

} // namespace audio
} // namespace lightwaveos
