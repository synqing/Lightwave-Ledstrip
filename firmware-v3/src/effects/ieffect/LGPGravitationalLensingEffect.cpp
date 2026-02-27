// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPGravitationalLensingEffect.cpp
 * @brief LGP Gravitational Lensing effect implementation
 */

#include "LGPGravitationalLensingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPGravitationalLensingEffect::LGPGravitationalLensingEffect()
    : m_time(0)
    , m_massPos{40.0f, 80.0f, 120.0f}
    , m_massVel{0.5f, -0.3f, 0.4f}
{
}

bool LGPGravitationalLensingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_massPos[0] = 40.0f;
    m_massPos[1] = 80.0f;
    m_massPos[2] = 120.0f;
    m_massVel[0] = 0.5f;
    m_massVel[1] = -0.3f;
    m_massVel[2] = 0.4f;
    return true;
}

void LGPGravitationalLensingEffect::render(plugins::EffectContext& ctx) {
    // Light bends around invisible massive objects creating Einstein rings
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    float speedNorm = ctx.speed / 50.0f;
    const uint8_t massCount = 2;
    float massStrength = ctx.brightness / 255.0f;

    // Update mass positions
    for (uint8_t m = 0; m < massCount; m++) {
        m_massPos[m] += m_massVel[m] * speedNorm;

        if (m_massPos[m] < 20.0f || m_massPos[m] > (float)STRIP_LENGTH - 20.0f) {
            m_massVel[m] = -m_massVel[m];
        }
    }

    fill_solid(ctx.leds, ctx.ledCount, CRGB::Black);

    // Generate light rays from center
    for (int16_t ray = -40; ray <= 40; ray += 2) {
        for (int8_t direction = -1; direction <= 1; direction += 2) {
            float rayPos = (float)CENTER_LEFT;
            float rayAngle = ray * 0.02f * direction;

            for (uint8_t step = 0; step < 80; step++) {
                float totalDeflection = 0.0f;

                for (uint8_t m = 0; m < massCount; m++) {
                    float dist = fabsf(rayPos - m_massPos[m]);
                    if (dist < 40.0f && dist > 1.0f) {
                        float deflection = massStrength * 20.0f / (dist * dist);
                        if (rayPos > m_massPos[m]) {
                            deflection = -deflection;
                        }
                        totalDeflection += deflection;
                    }
                }

                rayAngle += totalDeflection * 0.01f;
                rayPos += cosf(rayAngle) * 2.0f * direction;

                int16_t pixelPos = (int16_t)rayPos;
                if (pixelPos >= 0 && pixelPos < STRIP_LENGTH) {
                    uint8_t paletteIndex = (uint8_t)(fabsf(totalDeflection) * 20.0f);
                    uint8_t brightness = (uint8_t)(255 - step * 3);

                    // Clamp brightness to avoid white saturation
                    if (fabsf(totalDeflection) > 0.5f) {
                        brightness = 240;  // Slightly below 255 to avoid white guardrail
                        paletteIndex = (uint8_t)(fabsf(totalDeflection) * 30.0f);  // Use deflection-based color
                    }

                    ctx.leds[pixelPos] += ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
                    if (pixelPos + STRIP_LENGTH < ctx.ledCount) {
                        ctx.leds[pixelPos + STRIP_LENGTH] += ctx.palette.getColor(
                            (uint8_t)(ctx.gHue + paletteIndex + 64),
                            brightness);
                    }
                }

                if (rayPos < 0.0f || rayPos >= STRIP_LENGTH) {
                    break;
                }
            }
        }
    }
}

void LGPGravitationalLensingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGravitationalLensingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Gravitational Lensing",
        "Light bending around mass",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
