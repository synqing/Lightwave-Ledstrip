/**
 * @file EsSpectrumRefEffect.cpp
 * @brief ES v1.1 "Spectrum" reference show (64-bin spectrum strip).
 *
 * Per-zone PSRAM state and dt-corrected follower coefficients.
 * ZoneComposer reuses one instance across zones — all temporal state
 * is indexed by ctx.zoneId to prevent cross-zone contamination.
 */

#include "EsSpectrumRefEffect.h"
#include "EsV11RefUtil.h"
#include "../../../utils/Log.h"

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsSpectrumRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Allocate large buffers in PSRAM (DRAM is too precious).
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("EsSpectrumRefEffect: PSRAM alloc failed (%u bytes)",
                     (unsigned)sizeof(PsramData));
            return false;
        }
    }
    std::memset(m_ps, 0, sizeof(PsramData));

    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_ps->maxFollower[z] = 0.20f;
    }
    return true;
}

void EsSpectrumRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    if (!m_ps) return;

    if (!ctx.audio.available) {
        return;
    }

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    // Prefer raw ES bins, fallback to contract bins when raw stream is empty.
    const float* bins = ctx.audio.controlBus.es_bins64_raw;
    float maxRaw = 0.0f;
    for (uint8_t i = 0; i < kBinCount; ++i) {
        if (bins[i] > maxRaw) maxRaw = bins[i];
    }
    if (maxRaw < 0.0001f) {
        bins = ctx.audio.controlBus.bins64;
    }

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float smoothAlpha = 1.0f - expf(-dt / 0.050f);  // ~50 ms smoothing

    float frameMax = 0.0f;
    for (uint8_t i = 0; i < kBinCount; ++i) {
        m_ps->binsSmooth[z][i] += (clamp01(bins[i]) - m_ps->binsSmooth[z][i]) * smoothAlpha;
        if (m_ps->binsSmooth[z][i] > frameMax) frameMax = m_ps->binsSmooth[z][i];
    }

    // Adaptive follower — dt-corrected one-pole (attack/decay asymmetric).
    const float attackAlpha = 1.0f - expf(-dt / kFollowerAttackTau);
    const float decayAlpha  = 1.0f - expf(-dt / kFollowerDecayTau);

    if (frameMax > m_ps->maxFollower[z]) {
        m_ps->maxFollower[z] += (frameMax - m_ps->maxFollower[z]) * attackAlpha;
    } else {
        m_ps->maxFollower[z] += (frameMax - m_ps->maxFollower[z]) * decayAlpha;
    }
    if (m_ps->maxFollower[z] < 0.05f) m_ps->maxFollower[z] = 0.05f;
    const float invFollower = 1.0f / m_ps->maxFollower[z];

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float mag = clamp01(interp64(m_ps->binsSmooth[z], progress) * invFollower);
        mag = powf(mag, 0.85f);  // Slight post-shape to keep lows visible.
        const CRGB c = hsvProgress(ctx, progress, mag);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsSpectrumRefEffect::cleanup() {
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
}

const plugins::EffectMetadata& EsSpectrumRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Spectrum (Ref)",
        "ES v1.1 reference: 64-bin spectrum (centre-origin mirror)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference
