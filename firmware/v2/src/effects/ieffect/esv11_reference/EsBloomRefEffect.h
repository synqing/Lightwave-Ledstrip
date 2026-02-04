/**
 * @file EsBloomRefEffect.h
 * @brief ES v1.1 "Bloom" reference show (chromagram bloom).
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsBloomRefEffect : public plugins::IEffect {
public:
    EsBloomRefEffect() = default;
    ~EsBloomRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference

