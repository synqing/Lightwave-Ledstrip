/**
 * @file EsOctaveRefEffect.cpp
 * @brief ES v1.1 "Octave" reference show (chromagram strip).
 */

#include "EsOctaveRefEffect.h"
#include "EsV11RefUtil.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsOctaveRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void EsOctaveRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    const float* chroma = ctx.audio.controlBus.es_chroma_raw;
    if (!ctx.audio.available) {
        return;
    }

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float mag = clamp01(interp12(chroma, progress));
        const CRGB c = hsvProgress(ctx, progress, mag);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsOctaveRefEffect::cleanup() {
    // No resources to free.
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

