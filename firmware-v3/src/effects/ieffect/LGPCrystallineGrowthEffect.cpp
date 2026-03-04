/**
 * @file LGPCrystallineGrowthEffect.cpp
 * @brief LGP Crystalline Growth effect implementation
 */

#include "LGPCrystallineGrowthEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPCrystallineGrowthEffect::LGPCrystallineGrowthEffect()
    : m_time(0)
    , m_seeds{}
    , m_size{}
    , m_hue{}
    , m_initialized(false)
{
}

bool LGPCrystallineGrowthEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    memset(m_seeds, 0, sizeof(m_seeds));
    memset(m_size, 0, sizeof(m_size));
    memset(m_hue, 0, sizeof(m_hue));
    m_initialized = false;
    return true;
}

void LGPCrystallineGrowthEffect::render(plugins::EffectContext& ctx) {
    // Crystal formation with light refraction — centre-origin addressing
    m_time = (uint16_t)(m_time + (ctx.speed >> 3));

    // Initialize crystal seeds (distance from center)
    if (!m_initialized) {
        for (uint8_t c = 0; c < 10; c++) {
            m_seeds[c] = random8(HALF_LENGTH);
            m_size[c] = 0;
            m_hue[c] = random8();
        }
        m_initialized = true;
    }

    // Background substrate — iterate by distance from center
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint8_t substrate = (uint8_t)(20 + (inoise8(dist * 10, m_time) >> 4));
        CRGB color = CRGB(substrate >> 2, substrate >> 2, substrate);
        SET_CENTER_PAIR(ctx, dist, color);
    }

    // Update crystals
    for (uint8_t c = 0; c < 10; c++) {
        // Grow crystals
        if (m_size[c] < 20 && random8() < 32) {
            m_size[c]++;
        }

        // Reset fully grown crystals occasionally
        if (m_size[c] >= 20 && random8() < 5) {
            m_size[c] = 0;
            m_seeds[c] = random8(HALF_LENGTH);
            m_hue[c] = random8(30);
        }

        // Render crystal — seed position is distance from center
        uint8_t seedDist = m_seeds[c];

        for (int8_t facet = -(int8_t)m_size[c]; facet <= (int8_t)m_size[c]; facet++) {
            int16_t dist = (int16_t)seedDist + facet;
            if (dist >= 0 && dist < HALF_LENGTH) {
                uint8_t facetBrightness = (uint8_t)(255 - (abs(facet) * 255 / (m_size[c] + 1)));
                facetBrightness = scale8(facetBrightness, ctx.brightness);

                uint8_t paletteIndex = (uint8_t)(m_hue[c] + abs(facet));

                CRGB color1 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), facetBrightness);
                CRGB color2 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 30), scale8(facetBrightness, 200));

                // Blend facets onto both strips symmetrically from center
                uint16_t left1  = CENTER_LEFT - (uint16_t)dist;
                uint16_t right1 = CENTER_RIGHT + (uint16_t)dist;
                uint16_t left2  = STRIP_LENGTH + CENTER_LEFT - (uint16_t)dist;
                uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + (uint16_t)dist;

                if (left1  < ctx.ledCount) { ctx.leds[left1]  = blend(ctx.leds[left1],  color1, 128); }
                if (right1 < ctx.ledCount) { ctx.leds[right1] = blend(ctx.leds[right1], color1, 128); }
                if (left2  < ctx.ledCount) { ctx.leds[left2]  = blend(ctx.leds[left2],  color2, 128); }
                if (right2 < ctx.ledCount) { ctx.leds[right2] = blend(ctx.leds[right2], color2, 128); }
            }
        }
    }
}

void LGPCrystallineGrowthEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPCrystallineGrowthEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Crystalline Growth",
        "Growing crystal facets",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
