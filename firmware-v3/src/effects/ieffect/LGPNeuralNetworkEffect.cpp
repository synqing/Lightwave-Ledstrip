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
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Synaptic firing patterns with radial signal propagation
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    // Initialize neurons on first run
    if (!m_initialized) {
        for (uint8_t n = 0; n < 20; n++) {
            m_neurons[n] = random8(HALF_LENGTH);
            m_neuronState[n] = 0;
        }
        for (uint8_t s = 0; s < 10; s++) {
            m_signalStrength[s] = 0;
        }
        m_initialized = true;
    }

    // Background neural tissue
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint8_t tissue = (uint8_t)(inoise8(dist * 5, m_time >> 3) >> 2);
        CRGB tissueColor1 = CRGB(tissue >> 1, 0, tissue);
        CRGB tissueColor2 = CRGB(tissue >> 2, 0, tissue >> 1);

        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < STRIP_LENGTH) ctx.leds[left1] = tissueColor1;
        if (right1 < STRIP_LENGTH) ctx.leds[right1] = tissueColor1;

        uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
        uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
        if (left2 < ctx.ledCount) ctx.leds[left2] = tissueColor2;
        if (right2 < ctx.ledCount) ctx.leds[right2] = tissueColor2;
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
        uint8_t dist = m_neurons[n];
        uint8_t intensity = scale8(m_neuronState[n], ctx.brightness);
        CRGB neuronColor = CRGB(intensity, intensity >> 3, intensity >> 1);

        if (dist < HALF_LENGTH) {
            uint16_t left1 = CENTER_LEFT - dist;
            uint16_t right1 = CENTER_RIGHT + dist;
            if (left1 < STRIP_LENGTH) ctx.leds[left1] = neuronColor;
            if (right1 < STRIP_LENGTH) ctx.leds[right1] = neuronColor;

            uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
            uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
            if (left2 < ctx.ledCount) ctx.leds[left2] = neuronColor;
            if (right2 < ctx.ledCount) ctx.leds[right2] = neuronColor;
        }

        // Dendrites
        for (int8_t d = -2; d <= 2; d++) {
            if (d != 0) {
                int16_t dendDist = (int16_t)dist + d;
                if (dendDist >= 0 && dendDist < (int16_t)HALF_LENGTH) {
                    uint8_t dendIntensity = (uint8_t)(intensity >> (1 + abs(d)));
                    CRGB dendColor1 = CRGB(dendIntensity >> 2, 0, dendIntensity >> 3);
                    CRGB dendColor2 = CRGB(dendIntensity >> 3, 0, dendIntensity >> 2);

                    uint16_t left1 = CENTER_LEFT - (uint16_t)dendDist;
                    uint16_t right1 = CENTER_RIGHT + (uint16_t)dendDist;
                    if (left1 < STRIP_LENGTH) ctx.leds[left1] = dendColor1;
                    if (right1 < STRIP_LENGTH) ctx.leds[right1] = dendColor1;

                    uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - (uint16_t)dendDist;
                    uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + (uint16_t)dendDist;
                    if (left2 < ctx.ledCount) ctx.leds[left2] = dendColor2;
                    if (right2 < ctx.ledCount) ctx.leds[right2] = dendColor2;
                }
            }
        }
    }

    // Update and render signals
    for (uint8_t s = 0; s < 10; s++) {
        if (m_signalStrength[s] > 0) {
            m_signalPos[s] += (random8(2) == 0) ? 1 : -1;
            m_signalStrength[s] = scale8(m_signalStrength[s], 240);

            if (m_signalPos[s] >= 0 && m_signalPos[s] < (int8_t)HALF_LENGTH) {
                uint8_t sigIntensity = scale8(m_signalStrength[s], ctx.brightness);
                uint8_t sigR = sigIntensity >> 1;
                uint8_t sigG = sigIntensity >> 2;
                uint8_t sigB = sigIntensity;
                CRGB sigColor = CRGB(sigR, sigG, sigB);

                uint16_t dist = (uint16_t)m_signalPos[s];
                uint16_t left1 = CENTER_LEFT - dist;
                uint16_t right1 = CENTER_RIGHT + dist;
                if (left1 < STRIP_LENGTH) ctx.leds[left1] = sigColor;
                if (right1 < STRIP_LENGTH) ctx.leds[right1] = sigColor;

                uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
                uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
                if (left2 < ctx.ledCount) ctx.leds[left2] = sigColor;
                if (right2 < ctx.ledCount) ctx.leds[right2] = sigColor;
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
