/**
 * @file LGPMycelialNetworkEffect.cpp
 * @brief LGP Mycelial Network effect implementation
 */

#include "LGPMycelialNetworkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPMycelialNetworkEffect::LGPMycelialNetworkEffect()
    : m_nutrientPhase(0.0f)
    , m_initialized(false)
{
    for (int i = 0; i < 16; i++) {
        m_tipPositions[i] = 0.0f;
        m_tipVelocities[i] = 0.0f;
        m_tipActive[i] = false;
        m_tipAge[i] = 0.0f;
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_networkDensity[i] = 0.0f;
    }
}

bool LGPMycelialNetworkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_nutrientPhase = 0.0f;
    m_initialized = false;
    return true;
}

void LGPMycelialNetworkEffect::render(plugins::EffectContext& ctx) {
    // Fungal hyphal growth with fractal branching and nutrient flow
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    m_nutrientPhase += speed * 0.05f;

    if (!m_initialized) {
        for (int t = 0; t < 16; t++) {
            m_tipPositions[t] = (float)CENTER_LEFT;
            m_tipVelocities[t] = 0.0f;
            m_tipActive[t] = false;
            m_tipAge[t] = 0.0f;
        }
        m_tipActive[0] = true;
        m_tipVelocities[0] = 0.5f;
        m_tipActive[1] = true;
        m_tipVelocities[1] = -0.5f;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            m_networkDensity[i] = 0.0f;
        }

        m_initialized = true;
    }

    const float branchProbability = 0.005f;
    const uint8_t numTips = 8;

    // Update tips
    for (int t = 0; t < 16; t++) {
        if (m_tipActive[t]) {
            m_tipPositions[t] += m_tipVelocities[t] * speed;
            m_tipAge[t] += speed * 0.01f;

            if (m_tipPositions[t] < 0.0f || m_tipPositions[t] >= STRIP_LENGTH) {
                m_tipActive[t] = false;
            }

            if (random8() < (uint8_t)(branchProbability * 255.0f)) {
                for (int newTip = 0; newTip < numTips; newTip++) {
                    if (!m_tipActive[newTip]) {
                        m_tipActive[newTip] = true;
                        m_tipPositions[newTip] = m_tipPositions[t];
                        m_tipVelocities[newTip] = -m_tipVelocities[t] * (0.5f + random8() / 255.0f * 0.5f);
                        m_tipAge[newTip] = 0.0f;
                        break;
                    }
                }
            }
        } else if (random8() < 5) {
            m_tipActive[t] = true;
            m_tipPositions[t] = (float)CENTER_LEFT;
            m_tipVelocities[t] = (random8() > 127 ? 1.0f : -1.0f) * (0.3f + random8() / 255.0f * 0.4f);
            m_tipAge[t] = 0.0f;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.ledCount, 8);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        m_networkDensity[i] *= 0.998f;

        float tipGlow = 0.0f;
        for (int t = 0; t < 16; t++) {
            if (m_tipActive[t]) {
                float distToTip = fabsf((float)i - m_tipPositions[t]);
                if (distToTip < 5.0f) {
                    tipGlow += (5.0f - distToTip) / 5.0f * intensity;
                    m_networkDensity[i] = fminf(1.0f, m_networkDensity[i] + 0.02f);
                }
            }
        }

        const float flowDirection = 0.5f;
        float nutrientWave = sinf(normalizedDist * 10.0f - m_nutrientPhase * flowDirection * 3.0f);
        float nutrientBrightness = m_networkDensity[i] * (0.5f + nutrientWave * 0.5f);

        uint8_t hue1 = (uint8_t)(140 + (uint8_t)(ctx.gHue * 0.3f));
        uint8_t hue2 = (uint8_t)(160 + (uint8_t)(ctx.gHue * 0.3f));

        float brightness1 = tipGlow * 200.0f + m_networkDensity[i] * 80.0f + nutrientBrightness * 60.0f;
        float brightness2 = tipGlow * 150.0f + m_networkDensity[i] * 90.0f + nutrientBrightness * 70.0f;

        if (brightness1 > 100.0f && brightness2 > 100.0f) {
            hue1 = (uint8_t)(40 + (uint8_t)(ctx.gHue * 0.2f));
            hue2 = (uint8_t)(50 + (uint8_t)(ctx.gHue * 0.2f));
            brightness1 = fminf(255.0f, brightness1 * 1.3f);
            brightness2 = fminf(255.0f, brightness2 * 1.3f);
        }

        ctx.leds[i] = CHSV(hue1, 200, (uint8_t)constrain(brightness1, 0.0f, 255.0f));
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, 200, (uint8_t)constrain(brightness2, 0.0f, 255.0f));
        }
    }
}

void LGPMycelialNetworkEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMycelialNetworkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Mycelial Network",
        "Fungal network expansion",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
