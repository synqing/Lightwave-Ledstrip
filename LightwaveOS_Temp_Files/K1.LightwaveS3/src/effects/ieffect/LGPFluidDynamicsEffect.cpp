/**
 * @file LGPFluidDynamicsEffect.cpp
 * @brief LGP Fluid Dynamics effect implementation
 */

#include "LGPFluidDynamicsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPFluidDynamicsEffect::LGPFluidDynamicsEffect()
    : m_time(0)
    , m_velocity{}
    , m_pressure{}
{
}

bool LGPFluidDynamicsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    memset(m_velocity, 0, sizeof(m_velocity));
    memset(m_pressure, 0, sizeof(m_pressure));
    return true;
}

void LGPFluidDynamicsEffect::render(plugins::EffectContext& ctx) {
    // Laminar and turbulent flow visualization
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    float speedNorm = ctx.speed / 50.0f;
    float reynolds = speedNorm;

    // Update fluid simulation
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Pressure gradient
        float gradientForce = 0.0f;
        if (i > 0 && i < STRIP_LENGTH - 1) {
            gradientForce = (m_pressure[i - 1] - m_pressure[i + 1]) * 0.1f;
        }

        // Turbulence
        float turbulence = (inoise8(i * 5, m_time) - 128) / 128.0f * reynolds;

        // Update velocity
        m_velocity[i] += gradientForce + turbulence * 0.1f;
        m_velocity[i] *= 0.95f;

        // Update pressure
        m_pressure[i] += m_velocity[i] * 0.1f;
        m_pressure[i] *= 0.98f;

        // Add source/sink at center
        if (centerPairDistance(i) < 5) {
            m_pressure[i] += sin8(m_time >> 2) / 255.0f;
        }
    }

    // Render flow
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float vel = fabsf(m_velocity[i]) * 255.0f;
        if (vel > 255.0f) vel = 255.0f;
        uint8_t speed8 = (uint8_t)vel;

        float brightnessFloat = (m_pressure[i] + 1.0f) * 127.0f;
        if (brightnessFloat < 0.0f) brightnessFloat = 0.0f;
        if (brightnessFloat > 255.0f) brightnessFloat = 255.0f;
        uint8_t brightness = scale8((uint8_t)brightnessFloat, ctx.brightness);

        float distFromCenter = (float)centerPairDistance(i);
        uint8_t paletteIndex = (uint8_t)(m_velocity[i] * 20.0f) + (uint8_t)(distFromCenter / 4.0f);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(ctx.gHue + paletteIndex + 60),
                scale8(brightness, (uint8_t)(200 + speed8 / 4)));
        }
    }
}

void LGPFluidDynamicsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPFluidDynamicsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Fluid Dynamics",
        "Fluid flow simulation",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
