/**
 * @file LGPRileyDissonanceEffect.cpp
 * @brief LGP Riley Dissonance effect implementation
 */

#include "LGPRileyDissonanceEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPRileyDissonanceEffect::LGPRileyDissonanceEffect()
    : m_patternPhase(0.0f)
{
}

bool LGPRileyDissonanceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_patternPhase = 0.0f;
    return true;
}

void LGPRileyDissonanceEffect::render(plugins::EffectContext& ctx) {
    // Op art perceptual instability inspired by Bridget Riley
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    m_patternPhase += speed * 0.02f;

    const float baseFreq = 12.0f;
    const float freqMismatch = 0.08f;
    float freq1 = baseFreq * (1.0f + freqMismatch / 2.0f);
    float freq2 = baseFreq * (1.0f - freqMismatch / 2.0f);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float pattern1 = sinf(normalizedDist * freq1 * TWO_PI + m_patternPhase);
        float pattern2 = sinf(normalizedDist * freq2 * TWO_PI - m_patternPhase);

        // Apply contrast enhancement
        float contrast = 1.0f + intensity * 4.0f;
        pattern1 = tanhf(pattern1 * contrast) / tanhf(contrast);
        pattern2 = tanhf(pattern2 * contrast) / tanhf(contrast);

        float rivalryZone = fabsf(pattern1 - pattern2);

        float bright1 = 128.0f + (pattern1 * 127.0f * intensity);
        float bright2 = 128.0f + (pattern2 * 127.0f * intensity);

        uint8_t brightness1 = (uint8_t)constrain(bright1, 0.0f, 255.0f);
        uint8_t brightness2 = (uint8_t)constrain(bright2, 0.0f, 255.0f);

        // Complementary colours increase dissonance
        uint8_t hue1 = ctx.gHue;
        uint8_t hue2 = (uint8_t)(ctx.gHue + 128);
        const uint8_t sat = 200;

        if (rivalryZone > 0.5f) {
            hue1 = (uint8_t)(hue1 + (uint8_t)(rivalryZone * 30.0f));
            hue2 = (uint8_t)(hue2 - (uint8_t)(rivalryZone * 30.0f));
        }

        ctx.leds[i] = CHSV(hue1, sat, brightness1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, sat, brightness2);
        }
    }
}

void LGPRileyDissonanceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPRileyDissonanceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Riley Dissonance",
        "Op-art visual vibration",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
