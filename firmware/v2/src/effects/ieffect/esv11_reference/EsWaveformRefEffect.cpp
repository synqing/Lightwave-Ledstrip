/**
 * @file EsWaveformRefEffect.cpp
 * @brief ES v1.1 "Waveform" reference show (time-domain strip).
 */

#include "EsWaveformRefEffect.h"
#include "EsV11RefUtil.h"

#include <math.h>

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

static inline float interp128(const float a[128], float progress01) {
    float x = clamp01(progress01) * 127.0f;
    uint8_t idx = static_cast<uint8_t>(x);
    float frac = x - static_cast<float>(idx);
    if (idx >= 127) return a[127];
    return lerp(a[idx], a[idx + 1], frac);
}

static inline float computeAlpha(float cutoffHz, float sampleRateHz) {
    // One-pole low-pass coefficient for y[n] = y[n-1] + alpha * (x[n] - y[n-1])
    // alpha = 1 - exp(-2*pi*fc/fs)
    const float twoPi = 6.283185307179586f;
    float x = -twoPi * cutoffHz / sampleRateHz;
    float e = expf(x);
    float a = 1.0f - e;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    return a;
}

bool EsWaveformRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_alpha = computeAlpha(CUTOFF_HZ, static_cast<float>(SAMPLE_RATE_HZ));
    return true;
}

void EsWaveformRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    if (!ctx.audio.available) {
        return;
    }

    // ES reference behaviour: copy last NUM_LEDS samples, low-pass filter, auto-scale by max, then HSV gradient.
    // LWLS provides a fixed-size waveform snapshot; interpolate it across HALF_LENGTH and mirror from centre.
    for (uint8_t i = 0; i < 128; i++) {
        m_samples[i] = clamp01(ctx.audio.getWaveformAmplitude(i));
    }

    // Low-pass filter (3 passes) to emulate ES smoothing.
    for (uint8_t pass = 0; pass < FILTER_PASSES; pass++) {
        float y = 0.0f;
        for (uint8_t i = 0; i < 128; i++) {
            float x = m_samples[i];
            y = y + m_alpha * (x - y);
            m_samples[i] = y;
        }
    }

    float maxVal = 0.000001f;
    for (uint8_t i = 0; i < 128; i++) {
        if (m_samples[i] > maxVal) maxVal = m_samples[i];
    }
    const float autoScale = 1.0f / maxVal;

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float sample = clamp01(interp128(m_samples, progress) * autoScale);
        const CRGB c = hsvProgress(ctx, progress, sample);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsWaveformRefEffect::cleanup() {
    // No resources to free.
}

const plugins::EffectMetadata& EsWaveformRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Waveform (Ref)",
        "ES v1.1 reference: waveform strip (centre-origin mirror)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference

