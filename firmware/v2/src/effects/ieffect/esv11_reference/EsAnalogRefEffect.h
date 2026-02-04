/**
 * @file EsAnalogRefEffect.h
 * @brief ES v1.1 "Analog" reference show (VU dot).
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsAnalogRefEffect : public plugins::IEffect {
public:
    EsAnalogRefEffect() = default;
    ~EsAnalogRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_vuSmooth = 0.000001f;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference

