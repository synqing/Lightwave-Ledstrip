/**
 * @file PlasmaEffect.cpp
 * @brief Plasma effect implementation
 */

#include "PlasmaEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

PlasmaEffect::PlasmaEffect()
    : m_plasmaTime(0)
{
}

bool PlasmaEffect::init(plugins::EffectContext& ctx) {
    m_plasmaTime = 0;
    return true;
}

void PlasmaEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN PLASMA - Plasma field from center
    // Narrative integration: speed modulated by tempo multiplier
    using namespace utils;
    using namespace lightwaveos::narrative;
    
    // Apply narrative tempo multiplier to speed (opt-in, backward compatible)
    float narrativeSpeed = ctx.speed;
    if (NARRATIVE.isEnabled()) {
        narrativeSpeed *= NARRATIVE.getTempoMultiplier();
    }
    
    m_plasmaTime += (uint16_t)narrativeSpeed;
    if (m_plasmaTime > 65535) m_plasmaTime = m_plasmaTime % 65536;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Use utility function for center-based sin16 calculations
        float v1 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 8.0f, m_plasmaTime);
        float v2 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 5.0f, (uint16_t)(-m_plasmaTime * 0.75f));
        float v3 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 3.0f, (uint16_t)(m_plasmaTime * 0.5f));

        uint8_t paletteIndex = (uint8_t)((v1 + v2 + v3) * 10.0f + 15.0f) + ctx.gHue;
        uint8_t brightness = (uint8_t)((v1 + v2 + 2.0f) * 63.75f);

        CRGB color = ctx.palette.getColor(paletteIndex, brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void PlasmaEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& PlasmaEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Plasma",
        "Smoothly shifting color plasma",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

