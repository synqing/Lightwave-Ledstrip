/**
 * @file LGPNeuralNetworkRadialEffect.cpp
 * @brief LGP Neural Network (Radial) - Radial synaptic pathways
 *
 * Radial variant of LGPNeuralNetworkEffect.
 * Neurons placed at radial distances from center pair.
 * Signals propagate inward/outward as concentric rings.
 */

#include "LGPNeuralNetworkRadialEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPNeuralNetworkRadialEffect::LGPNeuralNetworkRadialEffect()
    : m_time(0)
    , m_neurons{}
    , m_neuronState{}
    , m_signalPos{}
    , m_signalStrength{}
    , m_initialized(false)
{
}

bool LGPNeuralNetworkRadialEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    memset(m_neurons, 0, sizeof(m_neurons));
    memset(m_neuronState, 0, sizeof(m_neuronState));
    memset(m_signalPos, 0, sizeof(m_signalPos));
    memset(m_signalStrength, 0, sizeof(m_signalStrength));
    m_initialized = false;
    return true;
}

void LGPNeuralNetworkRadialEffect::render(plugins::EffectContext& ctx) {
    // Fade background
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    // Initialize neurons at radial distances
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

    // Background neural tissue — radial Perlin noise
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint8_t tissue = (uint8_t)(inoise8(dist * 5, m_time >> 3) >> 2);
        CRGB tissueColor1 = CRGB(tissue >> 1, 0, tissue);
        CRGB tissueColor2 = CRGB(tissue >> 2, 0, tissue >> 1);

        // Symmetric center pair for strip 1
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < STRIP_LENGTH) ctx.leds[left1] = tissueColor1;
        if (right1 < STRIP_LENGTH) ctx.leds[right1] = tissueColor1;

        // Strip 2
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

        // Render neuron at radial distance
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

        // Dendrites — nearby radial distances
        for (int8_t d = -2; d <= 2; d++) {
            if (d != 0) {
                int16_t dendDist = (int16_t)dist + d;
                if (dendDist >= 0 && dendDist < (int16_t)HALF_LENGTH) {
                    uint8_t dendIntensity = (uint8_t)(intensity >> (1 + abs(d)));
                    CRGB dendColor1 = CRGB(dendIntensity >> 2, 0, dendIntensity >> 3);
                    CRGB dendColor2 = CRGB(dendIntensity >> 3, 0, dendIntensity >> 2);

                    uint16_t dl1 = CENTER_LEFT - (uint16_t)dendDist;
                    uint16_t dr1 = CENTER_RIGHT + (uint16_t)dendDist;
                    if (dl1 < STRIP_LENGTH) ctx.leds[dl1] = dendColor1;
                    if (dr1 < STRIP_LENGTH) ctx.leds[dr1] = dendColor1;
                    uint16_t dl2 = STRIP_LENGTH + CENTER_LEFT - (uint16_t)dendDist;
                    uint16_t dr2 = STRIP_LENGTH + CENTER_RIGHT + (uint16_t)dendDist;
                    if (dl2 < ctx.ledCount) ctx.leds[dl2] = dendColor2;
                    if (dr2 < ctx.ledCount) ctx.leds[dr2] = dendColor2;
                }
            }
        }
    }

    // Update and render signals — propagate in/out radially
    for (uint8_t s = 0; s < 10; s++) {
        if (m_signalStrength[s] > 0) {
            m_signalPos[s] += (random8(2) == 0) ? 1 : -1;
            m_signalStrength[s] = scale8(m_signalStrength[s], 240);

            if (m_signalPos[s] >= 0 && m_signalPos[s] < (int8_t)HALF_LENGTH) {
                uint8_t sigIntensity = scale8(m_signalStrength[s], ctx.brightness);
                CRGB sigColor = CRGB(sigIntensity >> 1, sigIntensity >> 2, sigIntensity);

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

void LGPNeuralNetworkRadialEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPNeuralNetworkRadialEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Neural Network R",
        "Radial synaptic pathways",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
