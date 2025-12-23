/**
 * @file LGPNeuralNetworkEffect.h
 * @brief LGP Neural Network - Firing synaptic pathways
 * 
 * Effect ID: 37
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_neurons/m_neuronState: Neuron positions and activity
 * - m_signalPos/m_signalStrength: Signal propagation state
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPNeuralNetworkEffect : public plugins::IEffect {
public:
    LGPNeuralNetworkEffect();
    ~LGPNeuralNetworkEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    uint8_t m_neurons[20];
    uint8_t m_neuronState[20];
    int8_t m_signalPos[10];
    uint8_t m_signalStrength[10];
    bool m_initialized;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
