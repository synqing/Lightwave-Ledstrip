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
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        m_bands_s[i] = 0.0f;
        m_heavy_bands_s[i] = 0.0f;
    }
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_chroma_s[i] = 0.0f;
        m_heavy_chroma_s[i] = 0.0f;
    }
    // Waveform array is zero-initialized by ControlBusFrame{} constructor
}

void ControlBus::setSmoothing(float alphaFast, float alphaSlow) {
    if (alphaFast < 0.0f) alphaFast = 0.0f;
    if (alphaFast > 1.0f) alphaFast = 1.0f;
    if (alphaSlow < 0.0f) alphaSlow = 0.0f;
    if (alphaSlow > 1.0f) alphaSlow = 1.0f;
    m_alpha_fast = alphaFast;
    m_alpha_slow = alphaSlow;
}

void ControlBus::setAttackRelease(float bandAttack, float bandRelease,
                                   float heavyBandAttack, float heavyBandRelease) {
    m_band_attack = clamp01(bandAttack);
    m_band_release = clamp01(bandRelease);
    m_heavy_band_attack = clamp01(heavyBandAttack);
    m_heavy_band_release = clamp01(heavyBandRelease);
}

void ControlBus::UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw) {
    m_frame.t = now;
    m_frame.hop_seq++;

    m_frame.fast_rms = clamp01(raw.rms);
    m_rms_s = lerp(m_rms_s, m_frame.fast_rms, m_alpha_fast);
    m_frame.rms = m_rms_s;

    m_frame.fast_flux = clamp01(raw.flux);
    m_flux_s = lerp(m_flux_s, m_frame.fast_flux, m_alpha_slow);
    m_frame.flux = m_flux_s;

    // LGP_SMOOTH: Asymmetric attack/release for bands
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        float target = clamp01(raw.bands[i]);
        // Fast attack, slow release for normal bands
        float alpha = (target > m_bands_s[i]) ? m_band_attack : m_band_release;
        m_bands_s[i] = lerp(m_bands_s[i], target, alpha);
        m_frame.bands[i] = m_bands_s[i];

        // Extra-smoothed heavy bands for ambient effects
        float heavyAlpha = (target > m_heavy_bands_s[i]) ? m_heavy_band_attack : m_heavy_band_release;
        m_heavy_bands_s[i] = lerp(m_heavy_bands_s[i], target, heavyAlpha);
        m_frame.heavy_bands[i] = m_heavy_bands_s[i];
    }

    // LGP_SMOOTH: Asymmetric attack/release for chroma
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        float target = clamp01(raw.chroma[i]);
        float alpha = (target > m_chroma_s[i]) ? m_band_attack : m_band_release;
        m_chroma_s[i] = lerp(m_chroma_s[i], target, alpha);
        m_frame.chroma[i] = m_chroma_s[i];

        float heavyAlpha = (target > m_heavy_chroma_s[i]) ? m_heavy_band_attack : m_heavy_band_release;
        m_heavy_chroma_s[i] = lerp(m_heavy_chroma_s[i], target, heavyAlpha);
        m_frame.heavy_chroma[i] = m_heavy_chroma_s[i];
    }

    for (uint8_t i = 0; i < CONTROLBUS_WAVEFORM_N; ++i) {
        m_frame.waveform[i] = raw.waveform[i];
    }
}

} // namespace lightwaveos::audio
