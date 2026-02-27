// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPSolitonWavesEffect.cpp
 * @brief LGP Soliton Waves effect implementation
 */

#include "LGPSolitonWavesEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPSolitonWavesEffect::LGPSolitonWavesEffect()
    : m_pos{20.0f, 60.0f, 100.0f, 140.0f}
    , m_vel{1.0f, -0.8f, 1.2f, -1.1f}
    , m_amp{255, 200, 230, 180}
    , m_hue{0, 60, 120, 180}
{
}

bool LGPSolitonWavesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_pos[0] = 20.0f;
    m_pos[1] = 60.0f;
    m_pos[2] = 100.0f;
    m_pos[3] = 140.0f;
    m_vel[0] = 1.0f;
    m_vel[1] = -0.8f;
    m_vel[2] = 1.2f;
    m_vel[3] = -1.1f;
    m_amp[0] = 255;
    m_amp[1] = 200;
    m_amp[2] = 230;
    m_amp[3] = 180;
    m_hue[0] = 0;
    m_hue[1] = 60;
    m_hue[2] = 120;
    m_hue[3] = 180;
    return true;
}

void LGPSolitonWavesEffect::render(plugins::EffectContext& ctx) {
    // Self-reinforcing wave packets that maintain shape
    float speedNorm = ctx.speed / 50.0f;
    const uint8_t solitonCount = 4;
    const float damping = 0.996f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint8_t s = 0; s < solitonCount; s++) {
        m_pos[s] += m_vel[s] * speedNorm;

        if (m_pos[s] < 0.0f || m_pos[s] >= STRIP_LENGTH) {
            m_vel[s] = -m_vel[s];
            m_pos[s] = constrain(m_pos[s], 0.0f, (float)(STRIP_LENGTH - 1));
        }

        // Check for collisions
        for (uint8_t other = s + 1; other < solitonCount; other++) {
            float dist = fabsf(m_pos[s] - m_pos[other]);
            if (dist < 10.0f) {
                float tempVel = m_vel[s];
                m_vel[s] = m_vel[other];
                m_vel[other] = tempVel;

                int16_t collisionPos = (int16_t)((m_pos[s] + m_pos[other]) / 2.0f);
                if (collisionPos >= 0 && collisionPos < STRIP_LENGTH) {
                    // Use blended color from colliding solitons instead of white
                    uint8_t blendHue = (uint8_t)((m_hue[s] + m_hue[other]) / 2);
                    uint8_t blendBright = (uint8_t)(qadd8(m_amp[s], m_amp[other]) / 2);
                    uint8_t brightU8 = (uint8_t)((blendBright * ctx.brightness) / 255);
                    ctx.leds[collisionPos] = ctx.palette.getColor((uint8_t)(blendHue + ctx.gHue), brightU8);
                    if (collisionPos + STRIP_LENGTH < ctx.ledCount) {
                        uint8_t brightU8_2 = (uint8_t)((scale8(blendBright, 200) * ctx.brightness) / 255);
                        ctx.leds[collisionPos + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(blendHue + ctx.gHue + 30), brightU8_2);
                    }
                }
            }
        }

        // Draw soliton - sech^2 profile
        for (int16_t dx = -20; dx <= 20; dx++) {
            int16_t pos = (int16_t)m_pos[s] + dx;
            if (pos >= 0 && pos < STRIP_LENGTH) {
                float sech = 1.0f / coshf(dx * 0.15f);
                float profile = sech * sech;

                uint8_t brightness = (uint8_t)(m_amp[s] * profile);
                uint8_t hue = (uint8_t)(m_hue[s] + ctx.gHue);

                uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
                CRGB solitonColor = ctx.palette.getColor(hue, brightU8);
                ctx.leds[pos] = solitonColor;
                if (pos + STRIP_LENGTH < ctx.ledCount) {
                    uint8_t brightU8_2 = (uint8_t)((scale8(brightness, 200) * ctx.brightness) / 255);
                    CRGB strip2Color = ctx.palette.getColor((uint8_t)(hue + 30), brightU8_2);
                    ctx.leds[pos + STRIP_LENGTH] = strip2Color;
                }
            }
        }

        m_amp[s] = (uint8_t)(m_amp[s] * damping);

        // Regenerate dead solitons
        if (m_amp[s] < 50) {
            m_pos[s] = (float)random16(STRIP_LENGTH);
            m_vel[s] = (random8(2) ? 1 : -1) * (0.5f + random8(100) / 100.0f);
            m_amp[s] = (uint8_t)(200 + random8(55));
            m_hue[s] = random8();
        }
    }
}

void LGPSolitonWavesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSolitonWavesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Soliton Waves",
        "Self-reinforcing wave packets",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
