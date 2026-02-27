/**
 * @file KuramotoTransportEffect.h
 * @brief Event-driven transport effect powered by invisible Kuramoto field
 *
 * Architecture (the key insight):
 *   Oscillators (invisible) -> derived events/velocity -> Bloom-style transport -> LEDs
 *
 * This should *not* look like a classic "ring pulse" audio reactive pattern because:
 *   - Audio only steers the invisible engine (K, spread, noise, kicks)
 *   - Visible output is stateful transported light substance
 *   - Events (phase slips, coherence edges) trigger injections, not direct mapping
 *
 * Acceptance criteria:
 * A1. Field invisibility: freeze transport injection -> LEDs go dark (oscillators keep running)
 * A2. Regime steering: changing sync_ratio changes event topology, not just brightness
 * A3. Subpixel motion: advection looks liquid (no stairstep LED hopping)
 * A4. Coherence-edge injections: filaments born at edges, carried by velocity field
 */

#pragma once

#include <stdint.h>
#include <esp_heap_caps.h>

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#include "KuramotoOscillatorField.h"
#include "KuramotoFeatureExtractor.h"
#include "KuramotoTransportBuffer.h"
#include "../../config/effect_ids.h"

namespace lightwaveos::effects::ieffect {

/**
 * @brief KuramotoTransportEffect
 *
 * A proof-of-concept effect implementing:
 *   Oscillators (invisible) -> derived events/velocity -> Bloom-style transport -> LEDs
 *
 * This should *not* look like a classic "ring pulse" audio reactive pattern, because:
 *   - audio only steers the invisible engine
 *   - visible output is stateful transported light substance
 */
class KuramotoTransportEffect final : public lightwaveos::plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_KURAMOTO_TRANSPORT;
    KuramotoTransportEffect();

    bool init(lightwaveos::plugins::EffectContext& ctx) override;
    void render(lightwaveos::plugins::EffectContext& ctx) override;
    void cleanup() override;
    const lightwaveos::plugins::EffectMetadata& getMetadata() const override;

    // Two custom knobs. Keep it minimal.
    uint8_t getParameterCount() const override { return 2; }
    const lightwaveos::plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static float clamp01(float x) {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    }

    // Map 0..1 to an integer radius (2..16). Large radii are costly and not always better.
    static uint16_t radiusFrom01(float r01) {
        r01 = clamp01(r01);
        return (uint16_t)(2 + (uint16_t)(r01 * 14.0f + 0.5f));
    }

    // Map 0..1 to coupling/spread regime control.
    // 0 = chaotic / incoherent, 1 = coherent / synced.
    static void computeRegime(float sync01, float& outK, float& outSpread) {
        sync01 = clamp01(sync01);
        // Coupling rises with sync; spread falls with sync.
        outK      = 0.6f + 3.2f * sync01;          // ~[0.6,3.8]
        outSpread = 2.6f - 2.1f * sync01;          // ~[2.6,0.5]
        if (outSpread < 0.15f) outSpread = 0.15f;
    }

    // Persistent state
    KuramotoOscillatorField m_field;
    KuramotoTransportBuffer m_transport;

    // PSRAM-ALLOCATED -- scratch buffers reused every render call
    struct PsramScratch {
        float velocity[KuramotoOscillatorField::N];
        float coherence[KuramotoOscillatorField::N];
        float event[KuramotoOscillatorField::N];
    };
    PsramScratch* m_scratch = nullptr;

    // Parameters
    float m_syncRatio01 = 0.55f; // custom knob #1
    float m_radius01    = 0.50f; // custom knob #2

    // Palette drift (slow)
    float m_palettePhase = 0.0f;

    // Metadata + params
    static const lightwaveos::plugins::EffectMetadata s_meta;
    static const lightwaveos::plugins::EffectParameter s_params[2];
};

} // namespace lightwaveos::effects::ieffect
