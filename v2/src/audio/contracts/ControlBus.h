#pragma once
#include <stdint.h>
#include <math.h>
#include "AudioTime.h"

namespace lightwaveos::audio {

static constexpr uint8_t CONTROLBUS_NUM_BANDS  = 8;
static constexpr uint8_t CONTROLBUS_NUM_CHROMA = 12;

/**
 * @brief Raw (unsmoothed) per-hop measurements produced by the audio thread.
 */
struct ControlBusRawInput {
    float rms = 0.0f;   // 0..1
    float flux = 0.0f;  // 0..1 (novelty proxy)

    float bands[CONTROLBUS_NUM_BANDS] = {0};     // 0..1
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};   // 0..1 (optional in Phase 2)
};

/**
 * @brief Published control frame consumed by renderer (BY VALUE).
 */
struct ControlBusFrame {
    AudioTime t;
    uint32_t hop_seq = 0;

    float rms = 0.0f;
    float flux = 0.0f;

    float bands[CONTROLBUS_NUM_BANDS] = {0};
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};
};

/**
 * @brief Smoothed DSP control signals at hop cadence.
 *
 * Audio thread owns a ControlBus instance; each hop:
 *  - UpdateFromHop(now, raw)
 *  - Publish(GetFrame()) to SnapshotBuffer<ControlBusFrame>
 */
class ControlBus {
public:
    ControlBus();

    void Reset();

    void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw);

    ControlBusFrame GetFrame() const { return m_frame; }

private:
    ControlBusFrame m_frame{};

    // One-pole smoothing state
    float m_rms_s = 0.0f;
    float m_flux_s = 0.0f;
    float m_bands_s[CONTROLBUS_NUM_BANDS] = {0};
    float m_chroma_s[CONTROLBUS_NUM_CHROMA] = {0};

    // Tunables (Phase-2 defaults; keep them stable until you port Tab5 constants)
    float m_alpha_fast = 0.35f;  // fast response
    float m_alpha_slow = 0.12f;  // slower response
};

} // namespace lightwaveos::audio
