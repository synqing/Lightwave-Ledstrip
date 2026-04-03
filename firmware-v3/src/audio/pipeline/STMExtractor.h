/**
 * @file STMExtractor.h
 * @brief Reduced spectrotemporal modulation extractor for dual-edge rendering.
 */

#pragma once

#include "config/features.h"

#if FEATURE_AUDIO_SYNC

#include "config/audio_config.h"

#include <cstddef>
#include <cstdint>

namespace lightwaveos::audio {

class STMExtractor {
public:
    static constexpr uint8_t MEL_BANDS = 16;
    static constexpr uint8_t SPECTRAL_MEL_BANDS = 128;
    static constexpr uint8_t SPECTRAL_FFT_BINS = 42;
    static constexpr uint8_t SPECTRAL_BINS = 42;
    static constexpr uint8_t TEMPORAL_FRAMES = 16;
    static constexpr uint16_t INPUT_BINS = 256;

    STMExtractor();

    /**
     * @brief Process one 256-bin magnitude frame into STM outputs.
     * @param bins256      Normalised FFT magnitudes.
     * @param temporalOut  Temporal modulation output [MEL_BANDS].
     * @param spectralOut  Spectral modulation output [SPECTRAL_BINS].
     * @return true once the temporal history is warm and outputs are valid.
     */
    bool process(const float* bins256, float* temporalOut, float* spectralOut);

    /**
     * @brief Reset all internal STM state.
     */
    void reset();

private:
    static constexpr uint8_t MAX_MEL_WEIGHTS = 32;
    static constexpr uint16_t FFT_SIZE = 512;

    struct MelBand {
        uint16_t startBin = 0;
        uint8_t weightCount = 0;
        float weights[MAX_MEL_WEIGHTS] = {0.0f};
    };

    static constexpr float kMinMelHz = 50.0f;
    static constexpr float kTemporalTargetHz = 4.0f;

    // The STM spec targets 32 kHz K1 production builds with a 7 kHz mel ceiling.
    // Lower-rate builds use a reduced ceiling so the sparse fixed-weight layout
    // stays within the static footprint budget without widening the weight table.
    static constexpr float selectMaxMelHz() {
        return (SAMPLE_RATE >= 30000U) ? 7000.0f
             : (SAMPLE_RATE >= 15000U) ? 4000.0f
                                       : 3500.0f;
    }

    static float hzToMel(float hz);
    static float melToHz(float mel);
    static void normaliseVector(float* values, uint8_t count);

    void initialiseMelBands();
    void initialiseSpectralMelBands();
    void applyMelFilterbank(const float* bins256, float* melOut) const;
    void applySpectralMelFilterbank(const float* bins256, float* melOut128) const;
    void updateHistory(const float* melFrame);
    void updateTemporalModulation(float* temporalOut);
    void computeSpectralModulation(const float* bins256, float* spectralOut);

    MelBand m_melBands[MEL_BANDS];
    float m_melHistory[TEMPORAL_FRAMES][MEL_BANDS] = {{0.0f}};
    float m_goertzelState[MEL_BANDS][2] = {{0.0f}};
    float m_melFrame[MEL_BANDS] = {0.0f};
    float m_spectralFftBuffer[SPECTRAL_MEL_BANDS] = {0.0f};
    float m_spectralMagnitudes[SPECTRAL_MEL_BANDS / 2] = {0.0f};
    uint8_t m_writeIndex = 0;
    uint8_t m_framesFilled = 0;
    float m_goertzelCoeff = 0.0f;
};

static_assert(sizeof(STMExtractor) <= 8192, "STMExtractor must remain within 8 KB");

}  // namespace lightwaveos::audio

#endif  // FEATURE_AUDIO_SYNC
