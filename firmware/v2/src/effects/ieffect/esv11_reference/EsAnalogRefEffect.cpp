/**
 * @file EsAnalogRefEffect.cpp
 * @brief ES v1.1 "Analog" reference show (VU dot).
 */

#include "EsAnalogRefEffect.h"
#include "EsV11RefUtil.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <cmath>
#endif

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsAnalogRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_vuSmooth = 0.000001f;
    return true;
}

void EsAnalogRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    float vu = 0.0f;
    if (ctx.audio.available) {
        // Use raw ES VU level to preserve ES semantics for this reference show.
        vu = clamp01(ctx.audio.controlBus.es_vu_level_raw);
    }

    const float speed01 = clamp01(ctx.speed / 100.0f);
    const float mixSpeed = 0.005f + 0.145f * speed01;
    m_vuSmooth = vu * mixSpeed + m_vuSmooth * (1.0f - mixSpeed);

    const float dotPos = clamp01(m_vuSmooth);
    const float radius = dotPos * static_cast<float>(HALF_LENGTH - 1);

    // Draw a soft dot at the computed radius from centre.
    // ES uses a "dot" primitive with motion blur; this is a compact approximation.
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = std::fabs(static_cast<float>(dist) - radius);
        if (d > 2.0f) continue;
        const float w = 1.0f - (d / 2.0f);
        const CRGB c = hsvProgress(ctx, dotPos, w);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsAnalogRefEffect::cleanup() {
    // No resources to free.
}

const plugins::EffectMetadata& EsAnalogRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Analog (Ref)",
        "ES v1.1 reference: VU dot (centre-origin)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference
