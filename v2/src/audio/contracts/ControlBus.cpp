#include "ControlBus.h"

namespace lightwaveos::audio {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

ControlBus::ControlBus() { Reset(); }

void ControlBus::Reset() {
    m_frame = ControlBusFrame{};
    m_rms_s = 0.0f;
    m_flux_s = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i)  m_bands_s[i] = 0.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) m_chroma_s[i] = 0.0f;
}

void ControlBus::UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw) {
    m_frame.t = now;
    m_frame.hop_seq++;

    // RMS: smooth fast
    m_rms_s = lerp(m_rms_s, clamp01(raw.rms), m_alpha_fast);
    m_frame.rms = m_rms_s;

    // Flux: smooth slightly slower (stabilizes novelty)
    m_flux_s = lerp(m_flux_s, clamp01(raw.flux), m_alpha_slow);
    m_frame.flux = m_flux_s;

    // Bands
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        m_bands_s[i] = lerp(m_bands_s[i], clamp01(raw.bands[i]), m_alpha_slow);
        m_frame.bands[i] = m_bands_s[i];
    }

    // Chroma (optional Phase 2: can remain zero until Phase 3)
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_chroma_s[i] = lerp(m_chroma_s[i], clamp01(raw.chroma[i]), m_alpha_slow);
        m_frame.chroma[i] = m_chroma_s[i];
    }
}

} // namespace lightwaveos::audio
