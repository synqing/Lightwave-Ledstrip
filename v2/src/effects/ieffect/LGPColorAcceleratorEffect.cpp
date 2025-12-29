/**
 * @file LGPColorAcceleratorEffect.cpp
 * @brief LGP Color Accelerator effect implementation
 */

#include "LGPColorAcceleratorEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPColorAcceleratorEffect::LGPColorAcceleratorEffect()
    : m_redParticle(0.0f)
    , m_blueParticle(STRIP_LENGTH - 1.0f)
    , m_collision(false)
    , m_debrisRadius(0.0f)
{
}

bool LGPColorAcceleratorEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_redParticle = 0.0f;
    m_blueParticle = STRIP_LENGTH - 1.0f;
    m_collision = false;
    m_debrisRadius = 0.0f;
    return true;
}

void LGPColorAcceleratorEffect::render(plugins::EffectContext& ctx) {
    // RGB particles accelerate from edges and collide at center
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    if (!m_collision) {
        m_redParticle += speed * 10.0f * (1.0f + (m_redParticle / STRIP_LENGTH));
        m_blueParticle -= speed * 10.0f * (1.0f + ((STRIP_LENGTH - m_blueParticle) / STRIP_LENGTH));

        // Draw particle trails
        for (int t = 0; t < 20; t++) {
            int redPos = (int)m_redParticle - t;
            int bluePos = (int)m_blueParticle + t;

            if (redPos >= 0 && redPos < STRIP_LENGTH) {
                uint8_t trailBright = (uint8_t)((255 - t * 12) * intensity);
                ctx.leds[redPos] = CRGB(trailBright, 0, 0);
            }

            if (bluePos >= 0 && bluePos < STRIP_LENGTH) {
                uint8_t trailBright = (uint8_t)((255 - t * 12) * intensity);
                if (bluePos + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[bluePos + STRIP_LENGTH] = CRGB(0, 0, trailBright);
                }
            }
        }

        // Check for collision
        if (m_redParticle >= CENTER_LEFT - 5 && m_blueParticle <= CENTER_LEFT + 5) {
            m_collision = true;
            m_debrisRadius = 0.0f;
        }
    } else {
        m_debrisRadius += speed * 8.0f;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = (float)centerPairDistance((uint16_t)i);

            if (distFromCenter <= m_debrisRadius) {
                uint8_t debrisHue = random8();
                uint8_t debrisBright = (uint8_t)(255.0f * (1.0f - distFromCenter / m_debrisRadius) * intensity);

                // Use saturating add to prevent color overflow
                CRGB debrisColor = CHSV(debrisHue, 255, debrisBright);
                if (random8(2) == 0) {
                    ctx.leds[i].r = qadd8(ctx.leds[i].r, debrisColor.r);
                    ctx.leds[i].g = qadd8(ctx.leds[i].g, debrisColor.g);
                    ctx.leds[i].b = qadd8(ctx.leds[i].b, debrisColor.b);
                } else if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH].r = qadd8(ctx.leds[i + STRIP_LENGTH].r, debrisColor.r);
                    ctx.leds[i + STRIP_LENGTH].g = qadd8(ctx.leds[i + STRIP_LENGTH].g, debrisColor.g);
                    ctx.leds[i + STRIP_LENGTH].b = qadd8(ctx.leds[i + STRIP_LENGTH].b, debrisColor.b);
                }
            }
        }

        if (m_debrisRadius > HALF_LENGTH) {
            m_collision = false;
            m_redParticle = 0.0f;
            m_blueParticle = STRIP_LENGTH - 1.0f;
        }
    }
}

void LGPColorAcceleratorEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPColorAcceleratorEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Color Accelerator",
        "Color cycling with momentum",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
