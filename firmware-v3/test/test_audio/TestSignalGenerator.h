/**
 * @file TestSignalGenerator.h
 * @brief Synthetic test signal generation for audio pipeline benchmarking
 *
 * Generates standardized test signals for validating and benchmarking
 * the LightwaveOS audio pipeline. Signal types are based on the
 * validation framework from the Sensory Bridge comparative analysis.
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lightwaveos::audio::test {

/**
 * @brief Test signal types for audio pipeline validation
 */
enum class SignalType : uint8_t {
    SILENCE = 0,          ///< Zero samples (noise floor measurement)
    SINE_WAVE,            ///< Pure sine at specified frequency
    WHITE_NOISE,          ///< Random noise, flat spectrum
    PINK_NOISE,           ///< 1/f noise, -3dB/octave
    IMPULSE,              ///< Single full-scale impulse
    IMPULSE_TRAIN,        ///< Periodic impulses at specified interval
    LEVEL_SWEEP,          ///< Gradual amplitude ramp
    MULTI_TONE            ///< Multiple simultaneous frequencies
};

/**
 * @brief Configuration for test signal generation
 */
struct SignalConfig {
    SignalType type = SignalType::SILENCE;
    uint32_t sampleRate = 16000;      ///< Sample rate in Hz
    float frequency = 440.0f;          ///< Primary frequency (Hz)
    float amplitude = 0.5f;            ///< Amplitude [0, 1] (normalized to int16 range)
    float durationMs = 100.0f;         ///< Duration in milliseconds

    // For impulse train
    float intervalMs = 500.0f;         ///< Interval between impulses

    // For level sweep
    float startAmplitude = 0.0f;       ///< Start amplitude for sweep
    float endAmplitude = 1.0f;         ///< End amplitude for sweep

    // For multi-tone
    float frequencies[8] = {60, 120, 250, 500, 1000, 2000, 4000, 7800};
    uint8_t numFrequencies = 8;
};

/**
 * @brief Test signal generator for audio pipeline benchmarking
 *
 * Generates synthetic audio signals for validating frequency detection,
 * AGC behavior, transient response, and noise rejection.
 */
class TestSignalGenerator {
public:
    TestSignalGenerator() : m_rngState(0x12345678) {}

    /**
     * @brief Generate a test signal into a buffer
     *
     * @param buffer Output buffer for generated samples
     * @param numSamples Number of samples to generate
     * @param config Signal configuration
     */
    void generate(int16_t* buffer, size_t numSamples, const SignalConfig& config) {
        switch (config.type) {
            case SignalType::SILENCE:
                generateSilence(buffer, numSamples);
                break;
            case SignalType::SINE_WAVE:
                generateSine(buffer, numSamples, config);
                break;
            case SignalType::WHITE_NOISE:
                generateWhiteNoise(buffer, numSamples, config);
                break;
            case SignalType::PINK_NOISE:
                generatePinkNoise(buffer, numSamples, config);
                break;
            case SignalType::IMPULSE:
                generateImpulse(buffer, numSamples, config);
                break;
            case SignalType::IMPULSE_TRAIN:
                generateImpulseTrain(buffer, numSamples, config);
                break;
            case SignalType::LEVEL_SWEEP:
                generateLevelSweep(buffer, numSamples, config);
                break;
            case SignalType::MULTI_TONE:
                generateMultiTone(buffer, numSamples, config);
                break;
        }
    }

    /**
     * @brief Generate a pure sine wave
     *
     * @param buffer Output buffer
     * @param numSamples Number of samples
     * @param frequency Frequency in Hz
     * @param sampleRate Sample rate in Hz
     * @param amplitude Normalized amplitude [0, 1]
     */
    void generateSine(int16_t* buffer, size_t numSamples,
                      float frequency, uint32_t sampleRate, float amplitude = 0.5f) {
        const float scale = amplitude * 32767.0f;
        const float omega = 2.0f * static_cast<float>(M_PI) * frequency / static_cast<float>(sampleRate);

        for (size_t i = 0; i < numSamples; ++i) {
            float sample = std::sin(omega * static_cast<float>(i));
            buffer[i] = static_cast<int16_t>(sample * scale);
        }
    }

    /**
     * @brief Generate white noise with specified amplitude
     */
    void generateWhiteNoise(int16_t* buffer, size_t numSamples, float amplitude = 0.5f) {
        const float scale = amplitude * 32767.0f;

        for (size_t i = 0; i < numSamples; ++i) {
            // Generate uniform random in [-1, 1]
            float sample = (nextRandom() * 2.0f) - 1.0f;
            buffer[i] = static_cast<int16_t>(sample * scale);
        }
    }

    /**
     * @brief Seed the random number generator for reproducible tests
     */
    void seed(uint32_t s) {
        m_rngState = s;
    }

    // =========================================================================
    // Convenience methods for common test signals
    // =========================================================================

    /**
     * @brief Generate Goertzel target frequency test signal
     *
     * @param buffer Output buffer
     * @param numSamples Number of samples
     * @param bandIndex Goertzel band index (0-7)
     * @param amplitude Normalized amplitude [0, 1]
     */
    void generateGoertzelTarget(int16_t* buffer, size_t numSamples,
                                 uint8_t bandIndex, float amplitude = 0.5f) {
        static const float TARGET_FREQS[8] = {60, 120, 250, 500, 1000, 2000, 4000, 7800};
        float freq = (bandIndex < 8) ? TARGET_FREQS[bandIndex] : 1000.0f;
        generateSine(buffer, numSamples, freq, 16000, amplitude);
    }

    /**
     * @brief Generate calibration tone (440 Hz, -12 dBFS)
     */
    void generateCalibrationTone(int16_t* buffer, size_t numSamples) {
        // -12 dBFS = 10^(-12/20) = 0.251
        generateSine(buffer, numSamples, 440.0f, 16000, 0.251f);
    }

private:
    uint32_t m_rngState;

    // Simple xorshift32 PRNG for reproducibility
    float nextRandom() {
        m_rngState ^= m_rngState << 13;
        m_rngState ^= m_rngState >> 17;
        m_rngState ^= m_rngState << 5;
        return static_cast<float>(m_rngState) / static_cast<float>(UINT32_MAX);
    }

    void generateSilence(int16_t* buffer, size_t numSamples) {
        std::memset(buffer, 0, numSamples * sizeof(int16_t));
    }

    void generateSine(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        generateSine(buffer, numSamples, cfg.frequency, cfg.sampleRate, cfg.amplitude);
    }

    void generateWhiteNoise(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        generateWhiteNoise(buffer, numSamples, cfg.amplitude);
    }

    void generatePinkNoise(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        // Pink noise using Paul Kellet's refined method
        // Approximates 1/f spectrum with 6 first-order IIR filters
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        const float scale = cfg.amplitude * 32767.0f * 0.11f;  // Scaled for unity RMS

        for (size_t i = 0; i < numSamples; ++i) {
            float white = (nextRandom() * 2.0f) - 1.0f;

            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;

            float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;

            buffer[i] = static_cast<int16_t>(pink * scale);
        }
    }

    void generateImpulse(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        std::memset(buffer, 0, numSamples * sizeof(int16_t));
        if (numSamples > 0) {
            buffer[0] = static_cast<int16_t>(cfg.amplitude * 32767.0f);
        }
    }

    void generateImpulseTrain(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        std::memset(buffer, 0, numSamples * sizeof(int16_t));

        size_t intervalSamples = static_cast<size_t>(
            cfg.intervalMs * static_cast<float>(cfg.sampleRate) / 1000.0f
        );
        if (intervalSamples == 0) intervalSamples = 1;

        int16_t impulseValue = static_cast<int16_t>(cfg.amplitude * 32767.0f);
        for (size_t i = 0; i < numSamples; i += intervalSamples) {
            buffer[i] = impulseValue;
        }
    }

    void generateLevelSweep(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        const float omega = 2.0f * static_cast<float>(M_PI) * cfg.frequency /
                           static_cast<float>(cfg.sampleRate);

        for (size_t i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(numSamples);
            float amp = cfg.startAmplitude + t * (cfg.endAmplitude - cfg.startAmplitude);
            float sample = std::sin(omega * static_cast<float>(i));
            buffer[i] = static_cast<int16_t>(sample * amp * 32767.0f);
        }
    }

    void generateMultiTone(int16_t* buffer, size_t numSamples, const SignalConfig& cfg) {
        std::memset(buffer, 0, numSamples * sizeof(int16_t));

        // Sum multiple sine waves with reduced amplitude each
        float perToneAmp = cfg.amplitude / static_cast<float>(cfg.numFrequencies);

        for (uint8_t f = 0; f < cfg.numFrequencies && f < 8; ++f) {
            const float omega = 2.0f * static_cast<float>(M_PI) * cfg.frequencies[f] /
                               static_cast<float>(cfg.sampleRate);
            const float scale = perToneAmp * 32767.0f;

            for (size_t i = 0; i < numSamples; ++i) {
                float sample = std::sin(omega * static_cast<float>(i));
                buffer[i] += static_cast<int16_t>(sample * scale);
            }
        }
    }
};

} // namespace lightwaveos::audio::test
