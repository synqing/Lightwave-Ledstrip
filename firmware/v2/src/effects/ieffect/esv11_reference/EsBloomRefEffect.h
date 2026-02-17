/**
 * @file EsBloomRefEffect.h
 * @brief ES v1.1 "Bloom" reference show (chromagram bloom).
 *
 * Per-zone PSRAM state: ZoneComposer reuses one effect instance across up to
 * 4 zones, so ALL temporal state is indexed by ctx.zoneId.  Large buffers
 * live in PSRAM (DRAM is reserved for WiFi / FreeRTOS / DMA).
 *
 * dt-corrected follower coefficients ensure frame-rate-independent behaviour
 * at any render cadence (60, 120, or variable FPS).
 */

#pragma once

#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsBloomRefEffect : public plugins::IEffect {
public:
    EsBloomRefEffect() = default;
    ~EsBloomRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ------------------------------------------------------------------------
    // Fixed limits (match LightwaveOS zone model)
    // ------------------------------------------------------------------------
    static constexpr int kMaxZones    = 4;
    static constexpr int kChromaBins  = 12;
    static constexpr int kHalfLength  = 80;   // LEDs per half-strip

    // PSRAM-allocated per-zone state (>64 bytes total — DRAM forbidden)
    struct PsramData {
        float chromaSmooth[kMaxZones][kChromaBins];
        float prev[kMaxZones][kHalfLength];
        float maxFollower[kMaxZones];
    };
    PsramData* m_ps = nullptr;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference
