/**
 * @file LGPNeuralNetworkEffect.cpp
 * @brief LGP Neural Network effect implementation
 */

#include "LGPNeuralNetworkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPNeuralNetworkEffect::LGPNeuralNetworkEffect()
    : m_time(0)
    , m_neurons{}
    , m_neuronState{}
    , m_signalPos{}
    , m_signalStrength{}
    , m_initialized(false)
{
}

bool LGPNeuralNetworkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    memset(m_neurons, 0, sizeof(m_neurons));
    memset(m_neuronState, 0, sizeof(m_neuronState));
    memset(m_signalPos, 0, sizeof(m_signalPos));
    memset(m_signalStrength, 0, sizeof(m_signalStrength));
    m_initialized = false;
    return true;
}

void LGPNeuralNetworkEffect::render(plugins::EffectContext& ctx) {
    // Fade to prevent color accumulation from additive blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, 25);

    // Synaptic firing patterns with signal propagation
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    // Initialize neurons on first run
    if (!m_initialized) {
        for (uint8_t n = 0; n < 20; n++) {
            m_neurons[n] = random8(STRIP_LENGTH);
            m_neuronState[n] = 0;
        }
        for (uint8_t s = 0; s < 10; s++) {
            m_signalStrength[s] = 0;
        }
        m_initialized = true;
    }

    // Background neural tissue
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t tissue = (uint8_t)(inoise8(i * 5, m_time >> 3) >> 2);
        ctx.leds[i] = CRGB(tissue >> 1, 0, tissue);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(tissue >> 2, 0, tissue >> 1);
        }
    }

    // Update neurons
    for (uint8_t n = 0; n < 20; n++) {
        if (m_neuronState[n] > 0) {
            m_neuronState[n] = scale8(m_neuronState[n], 230);
        } else {
            if (random8() < 16) {
                m_neuronState[n] = 255;

                // Spawn signal
                for (uint8_t s = 0; s < 10; s++) {
                    if (m_signalStrength[s] == 0) {
                        m_signalPos[s] = (int8_t)m_neurons[n];
                        m_signalStrength[s] = 255;
                        break;
                    }
                }
            }
        }

        // Render neuron
        uint8_t pos = m_neurons[n];
        uint8_t intensity = scale8(m_neuronState[n], ctx.brightness);
        CRGB neuronColor = CRGB(intensity, intensity >> 3, intensity >> 1);

        if (pos < STRIP_LENGTH) {
            ctx.leds[pos] = neuronColor;
            if (pos + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[pos + STRIP_LENGTH] = neuronColor;
            }
        }

        // Dendrites
        for (int8_t d = -2; d <= 2; d++) {
            if (d != 0) {
                int16_t dendPos = (int16_t)pos + d;
                if (dendPos >= 0 && dendPos < STRIP_LENGTH) {
                    uint8_t dendIntensity = (uint8_t)(intensity >> (1 + abs(d)));
                    // Use qadd8 to prevent overflow accumulation
                    ctx.leds[dendPos].r = qadd8(ctx.leds[dendPos].r, dendIntensity >> 2);
                    ctx.leds[dendPos].b = qadd8(ctx.leds[dendPos].b, dendIntensity >> 3);
                    if (dendPos + STRIP_LENGTH < ctx.ledCount) {
                        uint16_t idx = dendPos + STRIP_LENGTH;
                        ctx.leds[idx].r = qadd8(ctx.leds[idx].r, dendIntensity >> 3);
                        ctx.leds[idx].b = qadd8(ctx.leds[idx].b, dendIntensity >> 2);
                    }
                }
            }
        }
    }

    // Update and render signals
    for (uint8_t s = 0; s < 10; s++) {
        if (m_signalStrength[s] > 0) {
            m_signalPos[s] += (random8(2) == 0) ? 1 : -1;
            m_signalStrength[s] = scale8(m_signalStrength[s], 240);

            if (m_signalPos[s] >= 0 && m_signalPos[s] < STRIP_LENGTH) {
                uint8_t sigIntensity = scale8(m_signalStrength[s], ctx.brightness);
                uint8_t sigR = sigIntensity >> 1;
                uint8_t sigG = sigIntensity >> 2;
                uint8_t sigB = sigIntensity;

                // Use qadd8 to prevent overflow accumulation
                uint16_t pos = m_signalPos[s];
                ctx.leds[pos].r = qadd8(ctx.leds[pos].r, sigR);
                ctx.leds[pos].g = qadd8(ctx.leds[pos].g, sigG);
                ctx.leds[pos].b = qadd8(ctx.leds[pos].b, sigB);
                if (pos + STRIP_LENGTH < ctx.ledCount) {
                    uint16_t idx = pos + STRIP_LENGTH;
                    ctx.leds[idx].r = qadd8(ctx.leds[idx].r, sigR);
                    ctx.leds[idx].g = qadd8(ctx.leds[idx].g, sigG);
                    ctx.leds[idx].b = qadd8(ctx.leds[idx].b, sigB);
                }
            }
        }
    }
}

void LGPNeuralNetworkEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPNeuralNetworkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Neural Network",
        "Firing synaptic pathways",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
