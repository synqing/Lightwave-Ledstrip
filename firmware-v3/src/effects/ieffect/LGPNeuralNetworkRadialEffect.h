/**
 * @file LGPNeuralNetworkRadialEffect.h
 * @brief LGP Neural Network (Radial) - Radial synaptic pathways
 *
 * Radial variant of LGPNeuralNetworkEffect.
 * Neurons at radial distances from center, signals propagate inward/outward.
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN
 *
 * Instance State:
 * - m_time: Time accumulator
 * - m_neurons/m_neuronState: Neuron radial positions and activity
 * - m_signalPos/m_signalStrength: Signal propagation state (radial)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPNeuralNetworkRadialEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_NEURAL_NETWORK_RADIAL;

    LGPNeuralNetworkRadialEffect();
    ~LGPNeuralNetworkRadialEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    uint8_t m_neurons[20];       // radial distances from center
    uint8_t m_neuronState[20];
    int8_t m_signalPos[10];      // radial distance (signed for direction tracking)
    uint8_t m_signalStrength[10];
    bool m_initialized;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
