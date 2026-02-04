/**
 * @file EsSpectrumRefEffect.cpp
 * @brief ES v1.1 "Spectrum" reference show (64-bin spectrum strip).
 */

#include "EsSpectrumRefEffect.h"
#include "EsV11RefUtil.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsSpectrumRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void EsSpectrumRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    const float* bins = ctx.audio.controlBus.es_bins64_raw;
    if (!ctx.audio.available) {
        return;
    }

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float mag = clamp01(interp64(bins, progress));
        const CRGB c = hsvProgress(ctx, progress, mag);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsSpectrumRefEffect::cleanup() {
    // No resources to free.
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

