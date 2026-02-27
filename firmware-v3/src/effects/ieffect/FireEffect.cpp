/**
 * @file FireEffect.cpp
 * @brief Fire effect implementation
 */

#include "FireEffect.h"
#include "../CoreEffects.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

FireEffect::FireEffect() {}

bool FireEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<FirePsram*>(
            heap_caps_malloc(sizeof(FirePsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(FirePsram));
#endif
    return true;
}

void FireEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    using namespace lightwaveos::narrative;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->fireHeat[i] = qsub8(m_ps->fireHeat[i], random8(0, ((55 * 10) / STRIP_LENGTH) + 2));
    }

    for (int k = 1; k < STRIP_LENGTH - 1; k++) {
        m_ps->fireHeat[k] = (m_ps->fireHeat[k - 1] + m_ps->fireHeat[k] + m_ps->fireHeat[k + 1]) / 3;
    }

    float narrativeTension = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeTension = NARRATIVE.getTension();
    }
    uint8_t sparkChance = (uint8_t)((80 + ctx.speed) * (0.5f + narrativeTension * 0.5f));
    if (random8() < sparkChance) {
        int center = CENTER_LEFT + random8(2);
        m_ps->fireHeat[center] = qadd8(m_ps->fireHeat[center], random8(160, 255));
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        CRGB color = HeatColor(m_ps->fireHeat[i]);
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void FireEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& FireEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Fire",
        "Realistic fire simulation radiating from centre",
        plugins::EffectCategory::FIRE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

