/**
 * @file EsV11Adapter.h
 * @brief Maps ES v1.1_320 outputs into LWLS audio contracts.
 */

#pragma once

#include "config/features.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cstdint>
#include <cstddef>

#include "../../contracts/ControlBus.h"
#include "../../contracts/AudioTime.h"
#include "EsV11Backend.h"

namespace lightwaveos::audio::esv11 {

class EsV11Adapter {
public:
    EsV11Adapter() = default;

    void reset();

    /**
     * @brief Build a ControlBusFrame from the latest ES outputs.
     * @param out Frame to populate (BY VALUE publish).
     * @param es Latest ES outputs.
     * @param hopSeq Monotonic hop sequence (50 Hz publish cadence).
     */
    void buildFrame(lightwaveos::audio::ControlBusFrame& out,
                    const EsV11Outputs& es,
                    uint32_t hopSeq);

private:
    // Adaptive normalisation follower for bins64Adaptive
    float m_binsMaxFollower = 0.1f;
    float m_chromaMaxFollower = 0.2f;

    // Heavy smoothing state (slow envelope)
    float m_heavyBands[lightwaveos::audio::CONTROLBUS_NUM_BANDS] = {0};
    float m_heavyChroma[lightwaveos::audio::CONTROLBUS_NUM_CHROMA] = {0};

    uint8_t m_beatInBar = 0;

    // --------------------------------------------------------------------
    // Sensory Bridge parity side-car (3.1.0 waveform)
    //
    // ES v1.1 provides waveform + 64-bin magnitudes, but the SB light shows
    // expect additional “sweet spot” scaling and note-chromagram aggregation.
    // We compute those here so SB parity effects can run on the ES backend.
    // --------------------------------------------------------------------
    static constexpr uint8_t SB_WAVEFORM_POINTS = lightwaveos::audio::CONTROLBUS_WAVEFORM_N;
    static constexpr uint8_t SB_WAVEFORM_HISTORY = 4;
    int16_t m_sbWaveformHistory[SB_WAVEFORM_HISTORY][SB_WAVEFORM_POINTS] = {{0}};
    uint8_t m_sbWaveformHistoryIndex = 0;
    float m_sbMaxWaveformValFollower = 750.0f;  // Sensory Bridge sweet spot min level
    float m_sbWaveformPeakScaled = 0.0f;
    float m_sbWaveformPeakScaledLast = 0.0f;
    float m_sbNoteChroma[lightwaveos::audio::CONTROLBUS_NUM_CHROMA] = {0};
    float m_sbChromaMaxVal = 0.0001f;

    // --------------------------------------------------------------------
    // Lightweight onset detection (Stage B support)
    //
    // LWLS path uses GoertzelAnalyzer for snare/hihat onset.
    // ES path derives onsets from 64-bin spectrum band energy.
    // Snare: bins 5-10 (~150-300 Hz), Hihat: bins 50-60 (~6-12 kHz)
    // --------------------------------------------------------------------
    float m_prevSnareEnergy = 0.0f;
    float m_prevHihatEnergy = 0.0f;
};

} // namespace lightwaveos::audio::esv11

#endif
