/**
 * @file EsOctaveRefEffect.h
 * @brief ES v1.1 "Octave" reference show (chromagram strip).
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsOctaveRefEffect : public plugins::IEffect {
public:
    EsOctaveRefEffect() = default;
    ~EsOctaveRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference

