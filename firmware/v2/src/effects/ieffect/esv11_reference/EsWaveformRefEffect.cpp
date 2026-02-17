/**
 * @file EsWaveformRefEffect.cpp
 * @brief ES v1.1 "Waveform" reference show (time-domain strip).
 */

#include "EsWaveformRefEffect.h"
#include "EsV11RefUtil.h"

#include <math.h>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

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
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<EsWaveformPsram*>(
            heap_caps_malloc(sizeof(EsWaveformPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(EsWaveformPsram));
#endif
    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_lastHopSeq[z]   = 0;
        m_historyIndex[z]  = 0;
        m_historyPrimed[z] = false;
    }
    return true;
}

void EsWaveformRefEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    clearAll(ctx);

    if (!ctx.audio.available) {
        return;
    }

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    const bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq[z]);
    if (newHop || !m_historyPrimed[z]) {
        m_lastHopSeq[z] = ctx.audio.controlBus.hop_seq;
        if (!m_historyPrimed[z]) {
            for (uint8_t f = 0; f < HISTORY_FRAMES; ++f) {
                for (uint8_t i = 0; i < 128; ++i) {
                    m_ps->history[z][f][i] = ctx.audio.getWaveformNormalized(i);
                }
            }
            m_historyPrimed[z] = true;
            m_historyIndex[z] = 0;
        } else {
            for (uint8_t i = 0; i < 128; ++i) {
                m_ps->history[z][m_historyIndex[z]][i] = ctx.audio.getWaveformNormalized(i);
            }
            m_historyIndex[z] = static_cast<uint8_t>((m_historyIndex[z] + 1u) % HISTORY_FRAMES);
        }
    }

    // Average history to preserve waveform morphology with stable motion.
    for (uint8_t i = 0; i < 128; ++i) {
        float sum = 0.0f;
        for (uint8_t f = 0; f < HISTORY_FRAMES; ++f) {
            sum += m_ps->history[z][f][i];
        }
        m_ps->samples[z][i] = sum / static_cast<float>(HISTORY_FRAMES);
    }

    for (uint8_t pass = 0; pass < FILTER_PASSES; pass++) {
        float y = 0.0f;
        for (uint8_t i = 0; i < 128; i++) {
            float x = m_ps->samples[z][i];
            y = y + m_alpha * (x - y);
            m_ps->samples[z][i] = y;
        }
    }

    float maxAbs = 0.000001f;
    for (uint8_t i = 0; i < 128; i++) {
        const float a = fabsf(m_ps->samples[z][i]);
        if (a > maxAbs) maxAbs = a;
    }
    const float autoScale = 1.0f / maxAbs;

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float signedSample = interp128(m_ps->samples[z], progress) * autoScale;
        if (signedSample < -1.0f) signedSample = -1.0f;
        if (signedSample > 1.0f) signedSample = 1.0f;
        const float sample = clamp01(0.5f + signedSample * 0.5f);
        const CRGB c = hsvProgress(ctx, progress, sample);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsWaveformRefEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
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
