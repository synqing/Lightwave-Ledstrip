#include <Arduino.h>
#include <esp_dsp.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

class StmBenchmark {
public:
    static constexpr uint16_t INPUT_BINS = 256;
    static constexpr uint8_t MEL_BANDS = 16;
    static constexpr uint8_t TEMPORAL_FRAMES = 16;
    static constexpr uint8_t SPECTRAL_BINS = 7;
    static constexpr uint16_t FFT_SIZE = 512;
    static constexpr uint8_t MAX_MEL_WEIGHTS = 32;

    struct MelBand {
        uint16_t startBin = 0;
        uint8_t weightCount = 0;
        float weights[MAX_MEL_WEIGHTS] = {0.0f};
    };

    StmBenchmark() {
        initialiseMelBands();
        m_goertzelCoeff = 2.0f * cosf((2.0f * static_cast<float>(M_PI) * 4.0f) / 125.0f);
        reset();
    }

    void reset() {
        std::memset(m_history, 0, sizeof(m_history));
        std::memset(m_temporalState, 0, sizeof(m_temporalState));
        std::memset(m_inputBins, 0, sizeof(m_inputBins));
        std::memset(m_melFrame, 0, sizeof(m_melFrame));
        std::memset(m_temporalOut, 0, sizeof(m_temporalOut));
        std::memset(m_spectralOut, 0, sizeof(m_spectralOut));
        std::memset(m_fftBuffer, 0, sizeof(m_fftBuffer));
        std::memset(m_fftMagnitudes, 0, sizeof(m_fftMagnitudes));
        m_writeIndex = 0;
        m_framesFilled = 0;
    }

    void makePinkSpectrum(uint32_t frameIndex) {
        float peak = 1e-6f;
        for (uint16_t i = 0; i < INPUT_BINS; ++i) {
            const float bin = static_cast<float>(i + 1U);
            const float slope = 1.0f / sqrtf(bin);
            const float slowRipple = 0.55f + 0.45f * sinf(frameIndex * 0.11f + i * 0.07f);
            const float fastRipple = 0.65f + 0.35f * cosf(frameIndex * 0.19f + i * 0.03f);
            const float tonalPush = 0.85f + 0.15f * sinf(frameIndex * 0.04f + i * 0.015f);
            const float value = slope * slowRipple * fastRipple * tonalPush;
            m_inputBins[i] = value;
            if (value > peak) {
                peak = value;
            }
        }
        const float invPeak = 1.0f / peak;
        m_inputBins[0] = 0.0f;
        for (uint16_t i = 1; i < INPUT_BINS; ++i) {
            m_inputBins[i] *= invPeak;
        }
    }

    void warmup(uint32_t seedBase) {
        reset();
        for (uint8_t i = 0; i < TEMPORAL_FRAMES; ++i) {
            makePinkSpectrum(seedBase + i);
            applyMelFilterbank(m_inputBins, m_melFrame);
            updateHistory(m_melFrame);
        }
    }

    void applyMelFilterbank(const float* bins256, float* melOut) const {
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

    void updateHistory(const float* melFrame) {
        std::memcpy(m_history[m_writeIndex], melFrame, sizeof(float) * MEL_BANDS);
        m_writeIndex = static_cast<uint8_t>((m_writeIndex + 1U) % TEMPORAL_FRAMES);
        if (m_framesFilled < TEMPORAL_FRAMES) {
            ++m_framesFilled;
        }
    }

    void computeTemporal(float* temporalOut) {
        if (m_framesFilled < TEMPORAL_FRAMES) {
            std::memset(temporalOut, 0, sizeof(float) * MEL_BANDS);
            return;
        }

        for (uint8_t band = 0; band < MEL_BANDS; ++band) {
            float mean = 0.0f;
            for (uint8_t frame = 0; frame < TEMPORAL_FRAMES; ++frame) {
                const uint8_t historyIndex = static_cast<uint8_t>((m_writeIndex + frame) % TEMPORAL_FRAMES);
                mean += m_history[historyIndex][band];
            }
            mean /= static_cast<float>(TEMPORAL_FRAMES);

            float s1 = 0.0f;
            float s2 = 0.0f;
            for (uint8_t frame = 0; frame < TEMPORAL_FRAMES; ++frame) {
                const uint8_t historyIndex = static_cast<uint8_t>((m_writeIndex + frame) % TEMPORAL_FRAMES);
                const float sample = m_history[historyIndex][band] - mean;
                const float s0 = sample + (m_goertzelCoeff * s1) - s2;
                s2 = s1;
                s1 = s0;
            }

            const float power = std::max(0.0f, (s1 * s1) + (s2 * s2) - (m_goertzelCoeff * s1 * s2));
            temporalOut[band] = sqrtf(power);
            m_temporalState[band][0] = s1;
            m_temporalState[band][1] = s2;
        }

        normaliseVector(temporalOut, MEL_BANDS);
    }

    void computeSpectral(const float* melFrame, float* spectralOut) {
        std::memcpy(m_fftBuffer, melFrame, sizeof(float) * TEMPORAL_FRAMES);

        dsps_fft2r_fc32(m_fftBuffer, TEMPORAL_FRAMES / 2);
        dsps_bit_rev2r_fc32(m_fftBuffer, TEMPORAL_FRAMES / 2);
        dsps_cplx2real_fc32(m_fftBuffer, TEMPORAL_FRAMES / 2);

        m_fftMagnitudes[0] = fabsf(m_fftBuffer[0]);
        for (uint8_t k = 1; k < TEMPORAL_FRAMES / 2; ++k) {
            const float re = m_fftBuffer[2 * k];
            const float im = m_fftBuffer[2 * k + 1];
            m_fftMagnitudes[k] = sqrtf((re * re) + (im * im));
        }

        for (uint8_t i = 0; i < SPECTRAL_BINS; ++i) {
            spectralOut[i] = m_fftMagnitudes[i];
        }
        normaliseVector(spectralOut, SPECTRAL_BINS);
    }

    bool processFull(uint32_t frameIndex) {
        makePinkSpectrum(frameIndex);
        applyMelFilterbank(m_inputBins, m_melFrame);
        updateHistory(m_melFrame);
        if (m_framesFilled < TEMPORAL_FRAMES) {
            std::memset(m_temporalOut, 0, sizeof(m_temporalOut));
            std::memset(m_spectralOut, 0, sizeof(m_spectralOut));
            return false;
        }
        computeTemporal(m_temporalOut);
        computeSpectral(m_melFrame, m_spectralOut);
        return true;
    }

    const float* inputBins() const { return m_inputBins; }
    float* melFrame() { return m_melFrame; }
    float* temporalOut() { return m_temporalOut; }
    float* spectralOut() { return m_spectralOut; }

private:
    static constexpr float kMinMelHz = 50.0f;
    static constexpr float kMaxMelHz = 7000.0f;

    static float hzToMel(float hz) {
        return 2595.0f * log10f(1.0f + (hz / 700.0f));
    }

    static float melToHz(float mel) {
        return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
    }

    static void normaliseVector(float* values, uint8_t count) {
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

    void initialiseMelBands() {
        const float melMin = hzToMel(kMinMelHz);
        const float melMax = hzToMel(kMaxMelHz);
        const float binHz = 32000.0f / static_cast<float>(FFT_SIZE);

        float melPoints[MEL_BANDS + 2] = {0.0f};
        uint16_t binPoints[MEL_BANDS + 2] = {0};

        for (uint8_t i = 0; i < MEL_BANDS + 2; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(MEL_BANDS + 1);
            melPoints[i] = melMin + ((melMax - melMin) * t);
            const float hz = melToHz(melPoints[i]);
            const uint16_t bin = static_cast<uint16_t>(lroundf(hz / binHz));
            binPoints[i] = std::min<uint16_t>(INPUT_BINS - 1, bin);
        }

        for (uint8_t band = 0; band < MEL_BANDS; ++band) {
            MelBand& melBand = m_melBands[band];
            const uint16_t left = binPoints[band];
            const uint16_t centre = std::max<uint16_t>(left + 1, binPoints[band + 1]);
            const uint16_t right = std::max<uint16_t>(centre + 1, binPoints[band + 2]);

            melBand.startBin = left;
            melBand.weightCount = 0;
            std::memset(melBand.weights, 0, sizeof(melBand.weights));

            float weightSum = 0.0f;
            for (uint16_t bin = left; bin <= right && melBand.weightCount < MAX_MEL_WEIGHTS; ++bin) {
                float weight = 0.0f;
                if (bin < centre) {
                    weight = static_cast<float>(bin - left) /
                             static_cast<float>(std::max<uint16_t>(1, centre - left));
                } else if (bin == centre) {
                    weight = 1.0f;
                } else {
                    weight = static_cast<float>(right - bin) /
                             static_cast<float>(std::max<uint16_t>(1, right - centre));
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

    MelBand m_melBands[MEL_BANDS];
    float m_history[TEMPORAL_FRAMES][MEL_BANDS] = {{0.0f}};
    float m_temporalState[MEL_BANDS][2] = {{0.0f}};
    float m_inputBins[INPUT_BINS] = {0.0f};
    float m_melFrame[MEL_BANDS] = {0.0f};
    float m_temporalOut[MEL_BANDS] = {0.0f};
    float m_spectralOut[SPECTRAL_BINS] = {0.0f};
    float m_fftBuffer[TEMPORAL_FRAMES] = {0.0f};
    float m_fftMagnitudes[TEMPORAL_FRAMES / 2] = {0.0f};
    uint8_t m_writeIndex = 0;
    uint8_t m_framesFilled = 0;
    float m_goertzelCoeff = 0.0f;
};

struct TimingStats {
    uint32_t minUs = 0;
    uint32_t medianUs = 0;
    uint32_t p99Us = 0;
};

constexpr size_t kIterations = 1000;
constexpr uint32_t kThresholdUs = 100;

StmBenchmark g_stepBench;
StmBenchmark g_fullBench;
uint32_t g_melSamples[kIterations] = {0};
uint32_t g_bufferSamples[kIterations] = {0};
uint32_t g_temporalSamples[kIterations] = {0};
uint32_t g_spectralSamples[kIterations] = {0};
uint32_t g_fullSamples[kIterations] = {0};

TimingStats computeStats(uint32_t* values, size_t count) {
    std::sort(values, values + count);
    TimingStats stats;
    stats.minUs = values[0];
    stats.medianUs = values[count / 2];
    const size_t p99Index = ((count - 1U) * 99U) / 100U;
    stats.p99Us = values[p99Index];
    return stats;
}

void printStats(const char* label, const TimingStats& stats) {
    Serial.printf("[STM-BENCH] %s: min=%luus med=%luus p99=%luus\n",
                  label,
                  static_cast<unsigned long>(stats.minUs),
                  static_cast<unsigned long>(stats.medianUs),
                  static_cast<unsigned long>(stats.p99Us));
}

void runBenchmark() {
    g_stepBench.warmup(1000);
    g_fullBench.warmup(5000);

    for (size_t iteration = 0; iteration < kIterations; ++iteration) {
        const uint32_t seed = static_cast<uint32_t>(iteration * 3U);

        g_stepBench.makePinkSpectrum(seed);

        uint64_t startUs = esp_timer_get_time();
        g_stepBench.applyMelFilterbank(g_stepBench.inputBins(), g_stepBench.melFrame());
        g_melSamples[iteration] = static_cast<uint32_t>(esp_timer_get_time() - startUs);

        startUs = esp_timer_get_time();
        g_stepBench.updateHistory(g_stepBench.melFrame());
        g_bufferSamples[iteration] = static_cast<uint32_t>(esp_timer_get_time() - startUs);

        startUs = esp_timer_get_time();
        g_stepBench.computeTemporal(g_stepBench.temporalOut());
        g_temporalSamples[iteration] = static_cast<uint32_t>(esp_timer_get_time() - startUs);

        startUs = esp_timer_get_time();
        g_stepBench.computeSpectral(g_stepBench.melFrame(), g_stepBench.spectralOut());
        g_spectralSamples[iteration] = static_cast<uint32_t>(esp_timer_get_time() - startUs);

        startUs = esp_timer_get_time();
        g_fullBench.processFull(seed);
        g_fullSamples[iteration] = static_cast<uint32_t>(esp_timer_get_time() - startUs);
    }

    const TimingStats melStats = computeStats(g_melSamples, kIterations);
    const TimingStats bufferStats = computeStats(g_bufferSamples, kIterations);
    const TimingStats temporalStats = computeStats(g_temporalSamples, kIterations);
    const TimingStats spectralStats = computeStats(g_spectralSamples, kIterations);
    const TimingStats fullStats = computeStats(g_fullSamples, kIterations);

    printStats("mel_filterbank", melStats);
    printStats("buffer_update", bufferStats);
    printStats("temporal_mod", temporalStats);
    printStats("spectral_mod", spectralStats);
    printStats("full_pipeline", fullStats);

    const bool pass = fullStats.p99Us < kThresholdUs;
    Serial.printf("[STM-BENCH] RESULT: %s (p99=%luus vs threshold=%luus)\n",
                  pass ? "PASS" : "FAIL",
                  static_cast<unsigned long>(fullStats.p99Us),
                  static_cast<unsigned long>(kThresholdUs));
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);

    const esp_err_t err2 = dsps_fft2r_init_fc32(nullptr, StmBenchmark::TEMPORAL_FRAMES / 2);
    const esp_err_t err4 = dsps_fft4r_init_fc32(nullptr, StmBenchmark::TEMPORAL_FRAMES / 2);
    if ((err2 != ESP_OK && err2 != ESP_ERR_INVALID_STATE) ||
        (err4 != ESP_OK && err4 != ESP_ERR_INVALID_STATE)) {
        Serial.printf("[STM-BENCH] RESULT: FAIL (fft_init=%d/%d)\n",
                      static_cast<int>(err2),
                      static_cast<int>(err4));
        return;
    }

    runBenchmark();
}

void loop() {
    delay(1000);
}
