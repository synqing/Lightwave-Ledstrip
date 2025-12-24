#include "ControlBus.h"
#if FEATURE_AUDIO_SYNC

namespace lightwaveos {
namespace audio {

float ControlBus::smooth(float current, float target, float attack, float release) {
    float factor = (target > current) ? attack : release;
    return current + (target - current) * (1.0f - factor);
}

void ControlBus::UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw) {
    m_frame.t = now;

    // Smooth RMS with attack/release envelope
    m_frame.rms = smooth(m_frame.rms, raw.rms, m_attack, m_release);

    // Compute flux from band changes (sum of positive deltas)
    float fluxSum = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        float delta = raw.bands[i] - m_prevBands[i];
        if (delta > 0) fluxSum += delta;
        m_prevBands[i] = raw.bands[i];

        // Smooth bands
        m_frame.bands[i] = smooth(m_frame.bands[i], raw.bands[i], m_attack, m_release);
    }
    m_frame.flux = smooth(m_frame.flux, fluxSum, m_attack, m_release);

    // Smooth chroma bins
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_frame.chroma[i] = smooth(m_frame.chroma[i], raw.chroma[i], m_attack, m_release);
    }

    // Compute drive (smoothed overall energy for effects)
    m_frame.drive = smooth(m_frame.drive, raw.rms, 0.1f, 0.95f);

    // Compute punch (transient detector - fast attack, slow release)
    float punchTarget = (raw.rms > m_frame.rms * 1.5f) ? raw.rms : 0.0f;
    m_frame.punch = smooth(m_frame.punch, punchTarget, 0.1f, 0.9f);
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
