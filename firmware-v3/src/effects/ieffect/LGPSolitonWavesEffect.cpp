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
    : m_pos{10.0f, 30.0f, 50.0f, 70.0f}
    , m_vel{0.8f, -0.6f, 1.0f, -0.9f}
    , m_amp{255, 200, 230, 180}
    , m_hue{0, 60, 120, 180}
{
}

bool LGPSolitonWavesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_pos[0] = 10.0f;
    m_pos[1] = 30.0f;
    m_pos[2] = 50.0f;
    m_pos[3] = 70.0f;
    m_vel[0] = 0.8f;
    m_vel[1] = -0.6f;
    m_vel[2] = 1.0f;
    m_vel[3] = -0.9f;
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
    // Radial soliton shells that preserve centre-origin invariants
    float speedNorm = ctx.speed / 50.0f;
    const uint8_t solitonCount = 4;
    const float damping = 0.996f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint8_t s = 0; s < solitonCount; s++) {
        m_pos[s] += m_vel[s] * speedNorm;

        if (m_pos[s] < 0.0f || m_pos[s] >= (float)HALF_LENGTH) {
            m_vel[s] = -m_vel[s];
            m_pos[s] = constrain(m_pos[s], 0.0f, (float)(HALF_LENGTH - 1));
        }

        // Check for collisions
        for (uint8_t other = s + 1; other < solitonCount; other++) {
            float dist = fabsf(m_pos[s] - m_pos[other]);
            if (dist < 8.0f) {
                float tempVel = m_vel[s];
                m_vel[s] = m_vel[other];
                m_vel[other] = tempVel;

                uint16_t collisionDist = (uint16_t)((m_pos[s] + m_pos[other]) / 2.0f);
                if (collisionDist < HALF_LENGTH) {
                    uint8_t blendHue = (uint8_t)((m_hue[s] + m_hue[other]) / 2);
                    uint8_t blendBright = (uint8_t)(qadd8(m_amp[s], m_amp[other]) / 2);
                    uint8_t brightU8 = (uint8_t)((blendBright * ctx.brightness) / 255);
                    CRGB collisionColor = ctx.palette.getColor((uint8_t)(blendHue + ctx.gHue), brightU8);
                    SET_CENTER_PAIR(ctx, collisionDist, collisionColor);
                }
            }
        }

        // Draw soliton - sech^2 profile
        for (int16_t dr = -15; dr <= 15; dr++) {
            int16_t r = (int16_t)m_pos[s] + dr;
            if (r >= 0 && r < (int16_t)HALF_LENGTH) {
                float sech = 1.0f / coshf(dr * 0.18f);
                float profile = sech * sech;

                uint8_t brightness = (uint8_t)(m_amp[s] * profile);
                uint8_t hue = (uint8_t)(m_hue[s] + ctx.gHue);

                uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
                CRGB solitonColor = ctx.palette.getColor(hue, brightU8);

                uint16_t left1 = CENTER_LEFT - (uint16_t)r;
                uint16_t right1 = CENTER_RIGHT + (uint16_t)r;
                if (left1 < STRIP_LENGTH) ctx.leds[left1] = solitonColor;
                if (right1 < STRIP_LENGTH) ctx.leds[right1] = solitonColor;

                uint8_t brightU8_2 = (uint8_t)((scale8(brightness, 200) * ctx.brightness) / 255);
                CRGB strip2Color = ctx.palette.getColor((uint8_t)(hue + 30), brightU8_2);
                uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - (uint16_t)r;
                uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + (uint16_t)r;
                if (left2 < ctx.ledCount) ctx.leds[left2] = strip2Color;
                if (right2 < ctx.ledCount) ctx.leds[right2] = strip2Color;
            }
        }

        m_amp[s] = (uint8_t)(m_amp[s] * damping);

        // Regenerate dead solitons
        if (m_amp[s] < 50) {
            m_pos[s] = (float)random16(HALF_LENGTH);
            m_vel[s] = (random8(2) ? 1 : -1) * (0.4f + random8(100) / 100.0f);
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
