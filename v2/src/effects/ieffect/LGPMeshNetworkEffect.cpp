/**
 * @file LGPMeshNetworkEffect.cpp
 * @brief LGP Mesh Network effect implementation
 */

#include "LGPMeshNetworkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPMeshNetworkEffect::LGPMeshNetworkEffect()
    : m_phase(0.0f)
{
}

bool LGPMeshNetworkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPMeshNetworkEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Interconnected node patterns like neural networks
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * 0.02f;

    const int nodeCount = 12;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int n = 0; n < nodeCount; n++) {
        float nodePos = (float)n / (float)nodeCount * (float)HALF_LENGTH;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distToNode = fabsf((float)centerPairDistance((uint16_t)i) - nodePos);

            if (distToNode < 3.0f) {
                // Node core
                uint8_t nodeBright = (uint8_t)(255.0f * intensityNorm);
                ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20)), nodeBright);
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20) + 128),
                                                                      nodeBright);
                }
            } else if (distToNode < 20.0f) {
                // Connections to nearby nodes
                float connection = sinf(distToNode * 0.5f + m_phase + n);
                connection *= expf(-distToNode * 0.1f);

                uint8_t connBright = (uint8_t)(fabsf(connection) * 128.0f * intensityNorm);
                CRGB connColor = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20)), connBright);
                ctx.leds[i] = connColor;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    CRGB connColor2 = ctx.palette.getColor((uint8_t)(ctx.gHue + (n * 20) + 128), connBright);
                    ctx.leds[i + STRIP_LENGTH] = connColor2;
                }
            }
        }
    }
}

void LGPMeshNetworkEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMeshNetworkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Mesh Network",
        "Interconnected node graph",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
