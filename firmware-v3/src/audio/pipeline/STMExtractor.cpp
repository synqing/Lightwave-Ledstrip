/**
 * @file STMExtractor.cpp
 * @brief Reduced spectrotemporal modulation extractor implementation.
 */

#include "STMExtractor.h"

#if FEATURE_AUDIO_SYNC

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#include <esp_log.h>
#endif

#include "FFT.h"

// -----------------------------------------------------------------------
// 128-band spectral mel filterbank — static storage, initialised once
// -----------------------------------------------------------------------
namespace {

static constexpr uint8_t kSpectralMelBands = 128;
static constexpr uint8_t kMaxSpectralMelWeights = 8;

struct SpectralMelBand {
    uint16_t startBin = 0;
    uint8_t weightCount = 0;
    float weights[kMaxSpectralMelWeights] = {0.0f};
};

static SpectralMelBand s_spectralMelBands[kSpectralMelBands];
static bool s_spectralMelInitialised = false;

}  // namespace

namespace lightwaveos::audio {

STMExtractor::STMExtractor() {
    initialiseMelBands();
    initialiseSpectralMelBands();
    m_goertzelCoeff =
        2.0f * cosf((2.0f * static_cast<float>(M_PI) * kTemporalTargetHz) / audio::HOP_RATE_HZ);
    reset();
#if defined(ESP_PLATFORM)
    ESP_LOGW("STM", "128-band spectral init OK, SPECTRAL_BINS=%u, MEL_BANDS=%u, FFT_BINS=%u",
             static_cast<unsigned>(SPECTRAL_BINS),
             static_cast<unsigned>(SPECTRAL_MEL_BANDS),
             static_cast<unsigned>(SPECTRAL_FFT_BINS));
#endif
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

// -----------------------------------------------------------------------
// 128-band spectral mel filterbank initialisation (called once from ctor)
// -----------------------------------------------------------------------
void STMExtractor::initialiseSpectralMelBands() {
    if (s_spectralMelInitialised) {
        return;
    }

    const float melMin = hzToMel(kMinMelHz);
    const float melMax = hzToMel(selectMaxMelHz());
    const float binHz = static_cast<float>(audio::SAMPLE_RATE) / static_cast<float>(FFT_SIZE);

    // kSpectralMelBands + 2 boundary points for triangular filters
    static constexpr uint8_t kNumPoints = kSpectralMelBands + 2;  // 130
    float melPoints[kNumPoints] = {0.0f};
    uint16_t binPoints[kNumPoints] = {0U};

    for (uint8_t i = 0; i < kNumPoints; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSpectralMelBands + 1);
        melPoints[i] = melMin + ((melMax - melMin) * t);
        const float hz = melToHz(melPoints[i]);
        const uint16_t bin = static_cast<uint16_t>(lroundf(hz / binHz));
        binPoints[i] = std::min<uint16_t>(INPUT_BINS - 1U, bin);
    }

    for (uint8_t band = 0; band < kSpectralMelBands; ++band) {
        SpectralMelBand& melBand = s_spectralMelBands[band];
        const uint16_t left = binPoints[band];
        const uint16_t centre = std::max<uint16_t>(left + 1U, binPoints[band + 1U]);
        const uint16_t right = std::max<uint16_t>(centre + 1U, binPoints[band + 2U]);

        melBand.startBin = left;
        melBand.weightCount = 0;
        std::memset(melBand.weights, 0, sizeof(melBand.weights));

        float weightSum = 0.0f;
        for (uint16_t bin = left; bin <= right && melBand.weightCount < kMaxSpectralMelWeights; ++bin) {
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

    s_spectralMelInitialised = true;
}

void STMExtractor::applySpectralMelFilterbank(const float* bins256, float* melOut128) const {
    for (uint8_t band = 0; band < kSpectralMelBands; ++band) {
        const SpectralMelBand& melBand = s_spectralMelBands[band];
        float sum = 0.0f;
        for (uint8_t weightIndex = 0; weightIndex < melBand.weightCount; ++weightIndex) {
            const uint16_t binIndex = static_cast<uint16_t>(melBand.startBin + weightIndex);
            sum += bins256[binIndex] * melBand.weights[weightIndex];
        }
        melOut128[band] = sum;
    }
}

void STMExtractor::computeSpectralModulation(const float* bins256, float* spectralOut) {
    // 1. Apply 128-band mel filterbank to bins256 → 128-element mel frame
    applySpectralMelFilterbank(bins256, m_spectralFftBuffer);

    // 2. 128-point real FFT on the mel frame (spatial frequency analysis)
#if defined(ESP_PLATFORM)
    const int64_t t0 = esp_timer_get_time();
#endif
    fft::rfft(m_spectralFftBuffer, SPECTRAL_MEL_BANDS);

    // 3. Extract magnitudes for bins 0..63 (full half-spectrum)
    fft::magnitudes(m_spectralFftBuffer, m_spectralMagnitudes, SPECTRAL_MEL_BANDS);

    // 4. Copy bins 0..41 (≤6 cycles/octave across 7 octaves) and peak-normalise
    for (uint8_t i = 0; i < SPECTRAL_FFT_BINS; ++i) {
        spectralOut[i] = m_spectralMagnitudes[i];
    }
    normaliseVector(spectralOut, SPECTRAL_FFT_BINS);

#if defined(ESP_PLATFORM)
    const int64_t t1 = esp_timer_get_time();
    static uint32_t callCount = 0;
    if (++callCount % 1000 == 0) {
        ESP_LOGI("STM", "spectral128: %lld us", (long long)(t1 - t0));
    }
#endif
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
    computeSpectralModulation(bins256, spectralOut);
    return true;
}

void STMExtractor::reset() {
    std::memset(m_melHistory, 0, sizeof(m_melHistory));
    std::memset(m_goertzelState, 0, sizeof(m_goertzelState));
    std::memset(m_melFrame, 0, sizeof(m_melFrame));
    std::memset(m_spectralFftBuffer, 0, sizeof(m_spectralFftBuffer));
    std::memset(m_spectralMagnitudes, 0, sizeof(m_spectralMagnitudes));
    m_writeIndex = 0;
    m_framesFilled = 0;
}

}  // namespace lightwaveos::audio

#endif  // FEATURE_AUDIO_SYNC
