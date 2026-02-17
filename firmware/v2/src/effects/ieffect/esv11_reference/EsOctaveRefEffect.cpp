/**
 * @file EsOctaveRefEffect.cpp
 * @brief ES v1.1 "Octave" reference show (chromagram strip).
 *
 * Per-zone PSRAM state and dt-corrected follower coefficients.
 * Rendering logic is unchanged from the original.
 */

#include "EsOctaveRefEffect.h"
#include "EsV11RefUtil.h"

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsOctaveRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    std::memset(m_ps, 0, sizeof(PsramData));

    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_ps->maxFollower[z] = 0.15f;
    }
    return true;
}

void EsOctaveRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    if (!m_ps) return;

    if (!ctx.audio.available) {
        return;
    }

    const int z = ctx.zoneId;

    // Prefer raw ES chroma, fallback to contract chroma.
    const float* chroma = ctx.audio.controlBus.es_chroma_raw;
    float maxRaw = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        if (chroma[i] > maxRaw) maxRaw = chroma[i];
    }
    if (maxRaw < 0.0001f) {
        chroma = ctx.audio.controlBus.chroma;
    }

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float smoothAlpha = 1.0f - expf(-dt / 0.060f);  // ~60 ms one-pole
    float frameMax = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        m_ps->chromaSmooth[z][i] += (clamp01(chroma[i]) - m_ps->chromaSmooth[z][i]) * smoothAlpha;
        if (m_ps->chromaSmooth[z][i] > frameMax) frameMax = m_ps->chromaSmooth[z][i];
    }

    // dt-corrected asymmetric follower (attack tau ~0.058 s, decay tau ~0.547 s)
    if (frameMax > m_ps->maxFollower[z]) {
        const float attackAlpha = 1.0f - expf(-dt / kMaxFollowerAttackTau);
        m_ps->maxFollower[z] += (frameMax - m_ps->maxFollower[z]) * attackAlpha;
    } else {
        const float decayAlpha = 1.0f - expf(-dt / kMaxFollowerDecayTau);
        m_ps->maxFollower[z] -= (m_ps->maxFollower[z] - frameMax) * decayAlpha;
    }
    if (m_ps->maxFollower[z] < 0.04f) m_ps->maxFollower[z] = 0.04f;
    const float invFollower = 1.0f / m_ps->maxFollower[z];

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float mag = clamp01(interp12(m_ps->chromaSmooth[z], progress) * invFollower);
        const CRGB c = hsvProgress(ctx, progress, mag);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsOctaveRefEffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& EsOctaveRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Octave (Ref)",
        "ES v1.1 reference: chromagram strip (centre-origin mirror)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference
