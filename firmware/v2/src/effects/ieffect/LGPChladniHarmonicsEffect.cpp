/**
 * @file LGPChladniHarmonicsEffect.cpp
 * @brief LGP Chladni Harmonics effect implementation
 */

#include "LGPChladniHarmonicsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPChladniHarmonicsEffect::LGPChladniHarmonicsEffect()
    : m_vibrationPhase(0.0f)
    , m_mixPhase(0.0f)
{
}

bool LGPChladniHarmonicsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_vibrationPhase = 0.0f;
    m_mixPhase = 0.0f;
    return true;
}

void LGPChladniHarmonicsEffect::render(plugins::EffectContext& ctx) {
    // Visualizes acoustic resonance patterns on vibrating plates
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    const int modeNumber = 4;

    m_vibrationPhase += speed * 0.08f;
    m_mixPhase += speed * 0.05f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        float normalizedPos = distFromCenter / (float)HALF_LENGTH;

        // Mode shape: standing wave pattern
        float modeShape = sinf(modeNumber * PI * normalizedPos);

        // Add mixing with nearby modes
        float mix1 = sinf((modeNumber + 1) * PI * normalizedPos) * sinf(m_mixPhase);
        float mix2 = sinf((modeNumber - 1) * PI * normalizedPos) * cosf(m_mixPhase * 1.3f);
        float mixedMode = modeShape * 0.75f + (mix1 + mix2) * 0.25f * 0.5f;

        // Temporal oscillation
        float temporalOscillation = cosf(m_vibrationPhase);
        float plateDisplacement = mixedMode * temporalOscillation;

        // Sand particle visualization
        float nodeStrength = 1.0f / (fabsf(modeShape) + 0.1f);
        nodeStrength = constrain(nodeStrength, 0.0f, 3.0f);

        float antinodeStrength = fabsf(plateDisplacement) * intensity;

        float particleBrightness = nodeStrength * (1.0f - intensity) * 0.3f;
        float motionBrightness = antinodeStrength * intensity;
        float totalBrightness = (particleBrightness + motionBrightness) * 255.0f;

        uint8_t brightness = (uint8_t)constrain(totalBrightness, 20.0f, 255.0f);

        uint8_t hue1 = (uint8_t)(ctx.gHue + (uint8_t)(plateDisplacement * 30.0f));
        uint8_t hue2 = (uint8_t)(ctx.gHue + 128 + (uint8_t)(plateDisplacement * 30.0f));

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue1, brightU8);

        if (i + STRIP_LENGTH < ctx.ledCount) {
            float bottomDisplacement = -plateDisplacement;
            float bottomBrightness = (particleBrightness + fabsf(bottomDisplacement) * intensity) * 255.0f;
            uint8_t bottomBrightU8 = (uint8_t)constrain(bottomBrightness, 20.0f, 255.0f);
            bottomBrightU8 = (uint8_t)((bottomBrightU8 * ctx.brightness) / 255);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, bottomBrightU8);
        }
    }
}

void LGPChladniHarmonicsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChladniHarmonicsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chladni Harmonics",
        "Resonant nodal patterns",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
