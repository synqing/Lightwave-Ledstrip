/**
 * @file LGPQuantumTunnelingEffect.cpp
 * @brief LGP Quantum Tunneling effect implementation
 */

#include "LGPQuantumTunnelingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPQuantumTunnelingEffect::LGPQuantumTunnelingEffect()
    : m_time(0)
    , m_particlePos{}
    , m_particleEnergy{}
    , m_particleActive{}
{
}

bool LGPQuantumTunnelingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    memset(m_particlePos, 0, sizeof(m_particlePos));
    memset(m_particleEnergy, 0, sizeof(m_particleEnergy));
    memset(m_particleActive, 0, sizeof(m_particleActive));
    return true;
}

void LGPQuantumTunnelingEffect::render(plugins::EffectContext& ctx) {
    // Particles tunnel through energy barriers with probability waves
    m_time = (uint16_t)(m_time + (ctx.speed >> 1));

    const uint8_t barrierCount = 3;
    const uint8_t barrierWidth = 20;
    const uint8_t tunnelProbability = 64;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

    // Draw energy barriers
    for (uint8_t b = 0; b < barrierCount; b++) {
        uint8_t barrierPos = (uint8_t)((b + 1) * STRIP_LENGTH / (barrierCount + 1));

        for (int8_t w = -(int8_t)(barrierWidth / 2); w <= (int8_t)(barrierWidth / 2); w++) {
            int16_t pos = (int16_t)barrierPos + w;
            if (pos >= 0 && pos < STRIP_LENGTH) {
                uint8_t barrierBright = (uint8_t)(60 - abs(w) * 3);
                ctx.leds[pos] = CHSV(160, 255, barrierBright);
                if (pos + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[pos + STRIP_LENGTH] = CHSV(160, 255, barrierBright);
                }
            }
        }
    }

    // Spawn particles from center periodically
    if ((ctx.frameNumber % 60) == 0) {
        for (uint8_t p = 0; p < 10; p++) {
            if (!m_particleActive[p]) {
                m_particlePos[p] = CENTER_LEFT;
                m_particleEnergy[p] = (uint8_t)(100 + random8(155));
                m_particleActive[p] = true;
                break;
            }
        }
    }

    // Update particles
    for (uint8_t p = 0; p < 10; p++) {
        if (m_particleActive[p]) {
            int8_t direction = (p % 2) ? 1 : -1;

            // Check for barrier collision
            bool atBarrier = false;
            for (uint8_t b = 0; b < barrierCount; b++) {
                uint8_t barrierPos = (uint8_t)((b + 1) * STRIP_LENGTH / (barrierCount + 1));
                if (abs((int)m_particlePos[p] - (int)barrierPos) < barrierWidth / 2) {
                    atBarrier = true;

                    if (random8() < tunnelProbability) {
                        // TUNNEL THROUGH!
                        m_particlePos[p] = (uint8_t)(m_particlePos[p] + direction * barrierWidth);

                        // Flash effect - use particle color instead of white
                        uint8_t flashHue = (uint8_t)(ctx.gHue + p * 25);
                        for (int8_t f = -5; f <= 5; f++) {
                            int16_t flashPos = (int16_t)m_particlePos[p] + f;
                            if (flashPos >= 0 && flashPos < STRIP_LENGTH) {
                                uint8_t flashBright = (uint8_t)(255 - abs(f) * 20);
                                ctx.leds[flashPos] = CHSV(flashHue, 255, flashBright);
                                if (flashPos + STRIP_LENGTH < ctx.ledCount) {
                                    ctx.leds[flashPos + STRIP_LENGTH] = CHSV((uint8_t)(flashHue + 128), 255, flashBright);
                                }
                            }
                        }
                    } else {
                        // Reflect with energy loss
                        m_particleEnergy[p] = scale8(m_particleEnergy[p], 200);
                    }
                    break;
                }
            }

            if (!atBarrier) {
                m_particlePos[p] = (uint8_t)(m_particlePos[p] + direction * 2);
            }

            // Deactivate at edges
            if (m_particlePos[p] <= 0 || m_particlePos[p] >= STRIP_LENGTH - 1) {
                m_particleActive[p] = false;
                continue;
            }

            // Draw particle wave packet
            for (int8_t w = -10; w <= 10; w++) {
                int16_t wavePos = (int16_t)m_particlePos[p] + w;
                if (wavePos >= 0 && wavePos < STRIP_LENGTH) {
                    uint8_t waveBright = (uint8_t)(m_particleEnergy[p] * expf(-abs(w) * 0.2f));
                    uint8_t hue = (uint8_t)(ctx.gHue + p * 25);

                    CRGB waveColor = CHSV(hue, 255, waveBright);
                    ctx.leds[wavePos].r = qadd8(ctx.leds[wavePos].r, waveColor.r);
                    ctx.leds[wavePos].g = qadd8(ctx.leds[wavePos].g, waveColor.g);
                    ctx.leds[wavePos].b = qadd8(ctx.leds[wavePos].b, waveColor.b);
                    if (wavePos + STRIP_LENGTH < ctx.ledCount) {
                        CRGB waveColor2 = CHSV((uint8_t)(hue + 128), 255, waveBright);
                        ctx.leds[wavePos + STRIP_LENGTH].r = qadd8(ctx.leds[wavePos + STRIP_LENGTH].r, waveColor2.r);
                        ctx.leds[wavePos + STRIP_LENGTH].g = qadd8(ctx.leds[wavePos + STRIP_LENGTH].g, waveColor2.g);
                        ctx.leds[wavePos + STRIP_LENGTH].b = qadd8(ctx.leds[wavePos + STRIP_LENGTH].b, waveColor2.b);
                    }
                }
            }

            // Energy decay
            m_particleEnergy[p] = scale8(m_particleEnergy[p], 250);
            if (m_particleEnergy[p] < 10) {
                m_particleActive[p] = false;
            }
        }
    }
}

void LGPQuantumTunnelingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPQuantumTunnelingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Quantum Tunneling",
        "Particles passing through barriers",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
