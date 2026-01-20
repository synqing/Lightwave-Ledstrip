/**
 * @file OceanEffect.cpp
 * @brief Ocean effect implementation
 */

#include "OceanEffect.h"
#include "../CoreEffects.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

OceanEffect::OceanEffect()
    : m_waterOffset(0)
{
}

bool OceanEffect::init(plugins::EffectContext& ctx) {
    m_waterOffset = 0;
    return true;
}

void OceanEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN OCEAN - Waves emanate from center 79/80
    // Narrative integration: wave intensity modulated by narrative intensity
    using namespace lightwaveos::narrative;
    
    m_waterOffset += ctx.speed / 2;

    if (m_waterOffset > 65535) m_waterOffset = m_waterOffset % 65536;
    
    // Get narrative intensity for wave modulation (opt-in, backward compatible)
    float narrativeIntensity = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeIntensity = NARRATIVE.getIntensity();
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from CENTER PAIR
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Create wave motion from center outward
        uint8_t wave1 = sin8((uint16_t)(distFromCenter * 10) + m_waterOffset);
        uint8_t wave2 = sin8((uint16_t)(distFromCenter * 7) - m_waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Apply narrative intensity modulation to wave amplitude
        combinedWave = (uint8_t)(combinedWave * narrativeIntensity);

        // Ocean colors from deep blue to cyan - use palette system
        uint8_t hue = 160 + (combinedWave >> 3);
        uint8_t brightness = 100 + (uint8_t)((combinedWave >> 1) * narrativeIntensity);
        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        uint8_t paletteIdx = (uint8_t)(hue + ctx.gHue);
        CRGB color = ctx.palette.getColor(paletteIdx, brightU8);

        // Both strips
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void OceanEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& OceanEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ocean",
        "Deep ocean wave patterns from centre point",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
