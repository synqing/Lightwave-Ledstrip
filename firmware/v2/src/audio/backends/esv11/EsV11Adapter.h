/**
 * @file EsV11Adapter.h
 * @brief Maps ES v1.1_320 outputs into LWLS audio contracts.
 */

#pragma once

#include "config/features.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cstdint>

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
};

} // namespace lightwaveos::audio::esv11

#endif
