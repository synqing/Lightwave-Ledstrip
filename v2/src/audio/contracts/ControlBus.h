#pragma once
#include "config/features.h"
#if FEATURE_AUDIO_SYNC

#include "AudioTime.h"
#include <cstdint>
#include <cstring>

namespace lightwaveos {
namespace audio {

static constexpr uint8_t CONTROLBUS_NUM_BANDS = 8;
static constexpr uint8_t CONTROLBUS_NUM_CHROMA = 12;

/**
 * @brief Audio analysis frame published at hop rate (62.5 Hz)
 */
struct ControlBusFrame {
    AudioTime t;                                    // Timestamp
    float rms = 0.0f;                               // Root-mean-square energy [0,1]
    float flux = 0.0f;                              // Spectral flux (change) [0,1]
    float bands[CONTROLBUS_NUM_BANDS] = {0};        // 8 frequency bands [0,1]
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};      // 12 pitch class bins [0,1]
    float drive = 0.0f;                             // Smoothed energy for effects [0,1]
    float punch = 0.0f;                             // Transient detector [0,1]
    bool beat_detected = false;                     // Beat flag for this frame
    float beat_strength = 0.0f;                     // Beat intensity [0,1]
};

/**
 * @brief Raw input from audio analysis before smoothing
 */
struct ControlBusRawInput {
    float rms = 0.0f;
    float flux = 0.0f;
    float bands[CONTROLBUS_NUM_BANDS] = {0};
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};
};

/**
 * @brief ControlBus processor with attack/release smoothing
 */
class ControlBus {
public:
    ControlBus() = default;

    /**
     * @brief Process raw input with envelope smoothing
     * @param now Current audio time
     * @param raw Raw analysis values
     */
    void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw);

    /**
     * @brief Get the latest processed frame
     */
    const ControlBusFrame& GetFrame() const { return m_frame; }

    /**
     * @brief Set attack time (0-1, lower = faster)
     */
    void SetAttack(float attack) { m_attack = attack; }

    /**
     * @brief Set release time (0-1, lower = faster)
     */
    void SetRelease(float release) { m_release = release; }

private:
    float smooth(float current, float target, float attack, float release);

    ControlBusFrame m_frame;
    float m_prevBands[CONTROLBUS_NUM_BANDS] = {0};
    float m_attack = 0.3f;   // Fast attack
    float m_release = 0.85f; // Slow release
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
