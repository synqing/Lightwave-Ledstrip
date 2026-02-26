/**
 * @file LGPNeuralNetworkEffect.cpp
 * @brief LGP Neural Network effect implementation
 */

#include "LGPNeuralNetworkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.25f;
constexpr float kFireProbability = 0.063f;
constexpr float kSignalDecay = 0.941f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.10f, 1.0f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"fire_probability", "Fire Probability", 0.01f, 0.20f, kFireProbability, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"signal_decay", "Signal Decay", 0.85f, 0.99f, kSignalDecay, plugins::EffectParameterType::FLOAT, 0.005f, "damping", "", false},
};
}

LGPNeuralNetworkEffect::LGPNeuralNetworkEffect()
    : m_time(0)
    , m_neurons{}
    , m_neuronState{}
    , m_signalPos{}
    , m_signalStrength{}
    , m_initialized(false)
    , m_phaseRate(kPhaseRate)
    , m_fireProbability(kFireProbability)
    , m_signalDecay(kSignalDecay)
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
    m_phaseRate = kPhaseRate;
    m_fireProbability = kFireProbability;
    m_signalDecay = kSignalDecay;
    return true;
}

void LGPNeuralNetworkEffect::render(plugins::EffectContext& ctx) {
    // Fade to prevent color accumulation from additive blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Synaptic firing patterns with signal propagation
    const uint16_t phaseStep = (uint16_t)fmaxf(1.0f, ctx.speed * m_phaseRate);
    m_time = (uint16_t)(m_time + phaseStep);

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
            if (random8() < (uint8_t)(m_fireProbability * 255.0f)) {
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
                    ctx.leds[dendPos] = CRGB(dendIntensity >> 2, 0, dendIntensity >> 3);
                    if (dendPos + STRIP_LENGTH < ctx.ledCount) {
                        uint16_t idx = dendPos + STRIP_LENGTH;
                        ctx.leds[idx] = CRGB(dendIntensity >> 3, 0, dendIntensity >> 2);
                    }
                }
            }
        }
    }

    // Update and render signals
    for (uint8_t s = 0; s < 10; s++) {
        if (m_signalStrength[s] > 0) {
            m_signalPos[s] += (random8(2) == 0) ? 1 : -1;
            m_signalStrength[s] = scale8(m_signalStrength[s], (uint8_t)(m_signalDecay * 255.0f));

            if (m_signalPos[s] >= 0 && m_signalPos[s] < STRIP_LENGTH) {
                uint8_t sigIntensity = scale8(m_signalStrength[s], ctx.brightness);
                uint8_t sigR = sigIntensity >> 1;
                uint8_t sigG = sigIntensity >> 2;
                uint8_t sigB = sigIntensity;

                uint16_t pos = m_signalPos[s];
                ctx.leds[pos] = CRGB(sigR, sigG, sigB);
                if (pos + STRIP_LENGTH < ctx.ledCount) {
                    uint16_t idx = pos + STRIP_LENGTH;
                    ctx.leds[idx] = CRGB(sigR, sigG, sigB);
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

uint8_t LGPNeuralNetworkEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPNeuralNetworkEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPNeuralNetworkEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.10f, 1.0f);
        return true;
    }
    if (strcmp(name, "fire_probability") == 0) {
        m_fireProbability = constrain(value, 0.01f, 0.20f);
        return true;
    }
    if (strcmp(name, "signal_decay") == 0) {
        m_signalDecay = constrain(value, 0.85f, 0.99f);
        return true;
    }
    return false;
}

float LGPNeuralNetworkEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "fire_probability") == 0) return m_fireProbability;
    if (strcmp(name, "signal_decay") == 0) return m_signalDecay;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
