/**
 * @file EsSpectrumRefEffect.h
 * @brief ES v1.1 "Spectrum" reference show (64-bin spectrum strip).
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsSpectrumRefEffect : public plugins::IEffect {
public:
    EsSpectrumRefEffect() = default;
    ~EsSpectrumRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference

