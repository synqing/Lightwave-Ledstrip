/**
 * @file EsAnalogRefEffect.h
 * @brief ES v1.1 "Analog" reference show (VU dot).
 *
 * Per-zone state: ZoneComposer reuses one instance across up to 4
 * zones, so vuSmooth is indexed by ctx.zoneId.
 * Only 16 bytes total -- small enough for DRAM (no PSRAM needed).
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
    static constexpr uint8_t kMaxZones = 4;
    float m_vuSmooth[kMaxZones] = {};
};

} // namespace lightwaveos::effects::ieffect::esv11_reference

