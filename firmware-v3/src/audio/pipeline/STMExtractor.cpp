/**
 * @file STMExtractor.cpp
 * @brief Reduced spectrotemporal modulation extractor implementation.
 */

#include "STMExtractor.h"

#if FEATURE_AUDIO_SYNC

#include <algorithm>
#include <cmath>
#include <cstring>

#include "FFT.h"

namespace lightwaveos::audio {

STMExtractor::STMExtractor() {
    initialiseMelBands();
    m_goertzelCoeff =
        2.0f * cosf((2.0f * static_cast<float>(M_PI) * kTemporalTargetHz) / audio::HOP_RATE_HZ);
    reset();
}

float STMExtractor::hzToMel(float hz) {
    return 2595.0f * log10f(1.0f + (hz / 700.0f));
}

float STMExtractor::melToHz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

void STMExtractor::normaliseVector(float* values, uint8_t count) {
    float peak = 0.0f;
    for (uint8_t i = 0; i < count; ++i) {
        if (values[i] > peak) {
            peak = values[i];
        }
    }

    if (peak <= 1e-6f) {
        std::memset(values, 0, sizeof(float) * count);
        return;
    }

    const float invPeak = 1.0f / peak;
    for (uint8_t i = 0; i < count; ++i) {
        values[i] *= invPeak;
    }
}

void STMExtractor::initialiseMelBands() {
    const float melMin = hzToMel(kMinMelHz);
    const float melMax = hzToMel(selectMaxMelHz());
    const float binHz = static_cast<float>(audio::SAMPLE_RATE) / static_cast<float>(FFT_SIZE);

    float melPoints[MEL_BANDS + 2] = {0.0f};
    uint16_t binPoints[MEL_BANDS + 2] = {0U};

    for (uint8_t i = 0; i < MEL_BANDS + 2; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(MEL_BANDS + 1);
        melPoints[i] = melMin + ((melMax - melMin) * t);
        const float hz = melToHz(melPoints[i]);
        const uint16_t bin = static_cast<uint16_t>(lroundf(hz / binHz));
        binPoints[i] = std::min<uint16_t>(INPUT_BINS - 1U, bin);
    }

    for (uint8_t band = 0; band < MEL_BANDS; ++band) {
        MelBand& melBand = m_melBands[band];
        const uint16_t left = binPoints[band];
        const uint16_t centre = std::max<uint16_t>(left + 1U, binPoints[band + 1U]);
        const uint16_t right = std::max<uint16_t>(centre + 1U, binPoints[band + 2U]);

        melBand.startBin = left;
        melBand.weightCount = 0;
        std::memset(melBand.weights, 0, sizeof(melBand.weights));

        float weightSum = 0.0f;
        for (uint16_t bin = left; bin <= right && melBand.weightCount < MAX_MEL_WEIGHTS; ++bin) {
            float weight = 0.0f;
            if (bin < centre) {
                weight = static_cast<float>(bin - left) /
                         static_cast<float>(std::max<uint16_t>(1U, centre - left));
            } else if (bin == centre) {
                weight = 1.0f;
            } else {
                weight = static_cast<float>(right - bin) /
                         static_cast<float>(std::max<uint16_t>(1U, right - centre));
            }

            if (weight > 0.0f) {
                melBand.weights[melBand.weightCount++] = weight;
                weightSum += weight;
            }
        }

        if (weightSum > 1e-6f) {
            const float invWeightSum = 1.0f / weightSum;
            for (uint8_t i = 0; i < melBand.weightCount; ++i) {
                melBand.weights[i] *= invWeightSum;
            }
        }
    }
}

void STMExtractor::applyMelFilterbank(const float* bins256, float* melOut) const {
    for (uint8_t band = 0; band < MEL_BANDS; ++band) {
        const MelBand& melBand = m_melBands[band];
        float sum = 0.0f;
        for (uint8_t weightIndex = 0; weightIndex < melBand.weightCount; ++weightIndex) {
            const uint16_t binIndex = static_cast<uint16_t>(melBand.startBin + weightIndex);
            sum += bins256[binIndex] * melBand.weights[weightIndex];
        }
        melOut[band] = sum;
    }
    normaliseVector(melOut, MEL_BANDS);
}

void STMExtractor::updateHistory(const float* melFrame) {
    std::memcpy(m_melHistory[m_writeIndex], melFrame, sizeof(float) * MEL_BANDS);
    m_writeIndex = static_cast<uint8_t>((m_writeIndex + 1U) % TEMPORAL_FRAMES);
    if (m_framesFilled < TEMPORAL_FRAMES) {
        ++m_framesFilled;
    }
}

void STMExtractor::updateTemporalModulation(float* temporalOut) {
    for (uint8_t band = 0; band < MEL_BANDS; ++band) {
        float mean = 0.0f;
        for (uint8_t frame = 0; frame < TEMPORAL_FRAMES; ++frame) {
            const uint8_t historyIndex = static_cast<uint8_t>((m_writeIndex + frame) % TEMPORAL_FRAMES);
            mean += m_melHistory[historyIndex][band];
        }
        mean /= static_cast<float>(TEMPORAL_FRAMES);

        float s1 = 0.0f;
        float s2 = 0.0f;
        for (uint8_t frame = 0; frame < TEMPORAL_FRAMES; ++frame) {
            const uint8_t historyIndex = static_cast<uint8_t>((m_writeIndex + frame) % TEMPORAL_FRAMES);
            const float sample = m_melHistory[historyIndex][band] - mean;
            const float s0 = sample + (m_goertzelCoeff * s1) - s2;
            s2 = s1;
            s1 = s0;
        }

        const float power = std::max(0.0f, (s1 * s1) + (s2 * s2) - (m_goertzelCoeff * s1 * s2));
        temporalOut[band] = sqrtf(power);
        m_goertzelState[band][0] = s1;
        m_goertzelState[band][1] = s2;
    }

    normaliseVector(temporalOut, MEL_BANDS);
}

void STMExtractor::computeSpectralModulation(const float* melFrame, float* spectralOut) {
    std::memcpy(m_fftBuffer, melFrame, sizeof(float) * TEMPORAL_FRAMES);
    fft::rfft(m_fftBuffer, TEMPORAL_FRAMES);
    fft::magnitudes(m_fftBuffer, m_fftMagnitudes, TEMPORAL_FRAMES);

    for (uint8_t i = 0; i < SPECTRAL_BINS; ++i) {
        spectralOut[i] = m_fftMagnitudes[i];
    }

    normaliseVector(spectralOut, SPECTRAL_BINS);
}

bool STMExtractor::process(const float* bins256, float* temporalOut, float* spectralOut) {
    if (!bins256 || !temporalOut || !spectralOut) {
        return false;
    }

    applyMelFilterbank(bins256, m_melFrame);
    updateHistory(m_melFrame);

    if (m_framesFilled < TEMPORAL_FRAMES) {
        std::memset(temporalOut, 0, sizeof(float) * MEL_BANDS);
        std::memset(spectralOut, 0, sizeof(float) * SPECTRAL_BINS);
        return false;
    }

    updateTemporalModulation(temporalOut);
    computeSpectralModulation(m_melFrame, spectralOut);
    return true;
}

void STMExtractor::reset() {
    std::memset(m_melHistory, 0, sizeof(m_melHistory));
    std::memset(m_goertzelState, 0, sizeof(m_goertzelState));
    std::memset(m_melFrame, 0, sizeof(m_melFrame));
    std::memset(m_fftBuffer, 0, sizeof(m_fftBuffer));
    std::memset(m_fftMagnitudes, 0, sizeof(m_fftMagnitudes));
    m_writeIndex = 0;
    m_framesFilled = 0;
}

}  // namespace lightwaveos::audio

#endif  // FEATURE_AUDIO_SYNC
