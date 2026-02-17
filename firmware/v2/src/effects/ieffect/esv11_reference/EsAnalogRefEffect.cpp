/**
 * @file EsAnalogRefEffect.cpp
 * @brief ES v1.1 "Analog" reference show (VU dot).
 *
 * Per-zone state: vuSmooth is indexed by ctx.zoneId to prevent
 * cross-zone contamination when ZoneComposer reuses this instance.
 * Rendering logic is unchanged from the original.
 */

#include "EsAnalogRefEffect.h"
#include "EsV11RefUtil.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif
#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsAnalogRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_vuSmooth[z] = 0.000001f;
    }
    return true;
}

void EsAnalogRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    const int z = ctx.zoneId;

    float vu = 0.0f;
    if (ctx.audio.available) {
        // Use raw ES VU level to preserve ES semantics for this reference show.
        vu = clamp01(ctx.audio.controlBus.es_vu_level_raw);
    }

    // Keep VU tracking tied to raw signal time (not SPEED-scaled effect time).
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float alpha = 1.0f - expf(-dt / 0.090f);  // ~90 ms one-pole response
    m_vuSmooth[z] += (vu - m_vuSmooth[z]) * alpha;

    const float dotPos = clamp01(m_vuSmooth[z]);
    const float radius = dotPos * static_cast<float>(HALF_LENGTH - 1);
    const float speed01 = clamp01(ctx.speed / 100.0f);
    const float dotWidth = 1.5f + speed01 * 1.5f;  // SPEED only changes visual softness.

    // Draw a soft dot at the computed radius from centre.
    // ES uses a "dot" primitive with motion blur; this is a compact approximation.
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = std::fabs(static_cast<float>(dist) - radius);
        if (d > dotWidth) continue;
        const float w = 1.0f - (d / dotWidth);
        const CRGB c = hsvProgress(ctx, dotPos, w);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsAnalogRefEffect::cleanup() {
    // No resources to free (DRAM member array, no PSRAM allocation).
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
