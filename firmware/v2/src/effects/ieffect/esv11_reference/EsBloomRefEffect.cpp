/**
 * @file EsBloomRefEffect.cpp
 * @brief ES v1.1 "Bloom" reference show (chromagram bloom).
 */

#include "EsBloomRefEffect.h"
#include "EsV11RefUtil.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsBloomRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void EsBloomRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    const float* chroma = ctx.audio.controlBus.es_chroma_raw;
    if (!ctx.audio.available) {
        return;
    }

    // Matches the on-device ES v1.1_320 "Bloom" mode logic:
    // - Sample chromagram across the strip
    // - Apply a squared response for punchiness
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float v = clamp01(interp12(chroma, progress) * 2.0f);
        v *= v;
        const CRGB c = hsvProgress(ctx, progress, v);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsBloomRefEffect::cleanup() {
    // No resources to free.
}

const plugins::EffectMetadata& EsBloomRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Bloom (Ref)",
        "ES v1.1 reference: chroma bloom (squared punch)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference

