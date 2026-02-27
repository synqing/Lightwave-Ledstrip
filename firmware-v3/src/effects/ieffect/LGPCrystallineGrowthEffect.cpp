// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
    // Crystal formation with light refraction
    m_time = (uint16_t)(m_time + (ctx.speed >> 3));

    // Initialize crystal seeds
    if (!m_initialized) {
        for (uint8_t c = 0; c < 10; c++) {
            m_seeds[c] = random8(STRIP_LENGTH);
            m_size[c] = 0;
            m_hue[c] = random8();
        }
        m_initialized = true;
    }

    // Background substrate
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t substrate = (uint8_t)(20 + (inoise8(i * 10, m_time) >> 4));
        ctx.leds[i] = CRGB(substrate >> 2, substrate >> 2, substrate);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(substrate >> 3, substrate >> 3, substrate >> 1);
        }
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
            m_seeds[c] = random8(STRIP_LENGTH);
            m_hue[c] = random8(30);
        }

        // Render crystal
        uint8_t pos = m_seeds[c];

        for (int8_t facet = -(int8_t)m_size[c]; facet <= (int8_t)m_size[c]; facet++) {
            int16_t facetPos = (int16_t)pos + facet;
            if (facetPos >= 0 && facetPos < STRIP_LENGTH) {
                uint8_t facetBrightness = (uint8_t)(255 - (abs(facet) * 255 / (m_size[c] + 1)));
                facetBrightness = scale8(facetBrightness, ctx.brightness);

                uint8_t paletteIndex = (uint8_t)(m_hue[c] + abs(facet));

                CRGB color1 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), facetBrightness);
                CRGB color2 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 30), scale8(facetBrightness, 200));

                ctx.leds[facetPos] = blend(ctx.leds[facetPos], color1, 128);
                if (facetPos + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[facetPos + STRIP_LENGTH] = blend(ctx.leds[facetPos + STRIP_LENGTH], color2, 128);
                }
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
