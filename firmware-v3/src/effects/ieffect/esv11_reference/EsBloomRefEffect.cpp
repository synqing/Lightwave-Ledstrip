/**
 * @file EsBloomRefEffect.cpp
 * @brief ES v1.1 "Bloom" reference show (chromagram bloom).
 *
 * Per-zone PSRAM state and dt-corrected follower coefficients.
 * ZoneComposer reuses one instance across up to 4 zones, setting
 * ctx.zoneId (0-3) before each render() call.
 *
 * Follower dt-correction derivation (60 fps reference):
 *   attack alpha 0.25  -> tau = -1 / (60 * ln(1 - 0.25)) = 0.058 s
 *   decay  alpha 0.02  -> tau = -1 / (60 * ln(1 - 0.02)) = 0.825 s
 */

#include "EsBloomRefEffect.h"
#include "EsV11RefUtil.h"

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

// ============================================================================
// Follower time constants (derived from per-frame alpha at 60 fps)
// ============================================================================
static constexpr float kAttackTau = 0.058f;   // tau for attack alpha 0.25 @ 60 fps
static constexpr float kDecayTau  = 0.825f;   // tau for decay  alpha 0.02 @ 60 fps

// ============================================================================
// IEffect interface
// ============================================================================

bool EsBloomRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Allocate per-zone buffers in PSRAM (DRAM is reserved for WiFi/FreeRTOS)
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            return false;
        }
    }
    std::memset(m_ps, 0, sizeof(PsramData));

    // Initialise per-zone follower floor
    for (int z = 0; z < kMaxZones; ++z) {
        m_ps->maxFollower[z] = 0.15f;
    }
    return true;
}

void EsBloomRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    if (!m_ps) return;

    if (!ctx.audio.available) {
        return;
    }

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    // Prefer raw ES chroma, fallback to contract chroma when needed.
    const float* chroma = ctx.audio.controlBus.es_chroma_raw;
    float maxRaw = 0.0f;
    for (uint8_t i = 0; i < kChromaBins; ++i) {
        if (chroma[i] > maxRaw) maxRaw = chroma[i];
    }
    if (maxRaw < 0.0001f) {
        chroma = ctx.audio.controlBus.chroma;
    }

    // dt from raw (SPEED-independent) time — audio signal processing
    const float dt = ctx.getSafeRawDeltaSeconds();

    // Chroma smoothing (one-pole, tau ~60 ms)
    const float smoothAlpha = 1.0f - expf(-dt / 0.060f);
    float frameMax = 0.0f;
    for (uint8_t i = 0; i < kChromaBins; ++i) {
        m_ps->chromaSmooth[z][i] += (clamp01(chroma[i]) - m_ps->chromaSmooth[z][i]) * smoothAlpha;
        if (m_ps->chromaSmooth[z][i] > frameMax) frameMax = m_ps->chromaSmooth[z][i];
    }

    // Adaptive max follower — dt-corrected attack/decay
    const float attackAlpha = 1.0f - expf(-dt / kAttackTau);
    const float decayAlpha  = 1.0f - expf(-dt / kDecayTau);

    if (frameMax > m_ps->maxFollower[z]) {
        m_ps->maxFollower[z] += (frameMax - m_ps->maxFollower[z]) * attackAlpha;
    } else {
        m_ps->maxFollower[z] += (frameMax - m_ps->maxFollower[z]) * decayAlpha;
    }
    if (m_ps->maxFollower[z] < 0.04f) m_ps->maxFollower[z] = 0.04f;

    const float invFollower = 1.0f / m_ps->maxFollower[z];
    const float feedbackDecay = powf(0.86f, dt * 60.0f);

    float bloomLine[HALF_LENGTH];

    // Matches the on-device ES v1.1_320 "Bloom" mode logic:
    // - Sample chromagram across the strip
    // - Apply a squared response for punchiness
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float v = clamp01(interp12(m_ps->chromaSmooth[z], progress) * invFollower * 2.0f);
        v *= v;
        m_ps->prev[z][dist] *= feedbackDecay;
        const float combined = clamp01(v + m_ps->prev[z][dist] * 0.45f);
        m_ps->prev[z][dist] = combined;
        bloomLine[dist] = combined;
    }

    // Small smear kernel so centre energy reads as bloom body instead of needles.
    float smeared[HALF_LENGTH];
    if (HALF_LENGTH > 0) {
        smeared[0] = bloomLine[0] * 0.70f + ((HALF_LENGTH > 1) ? bloomLine[1] * 0.30f : 0.0f);
        for (uint16_t dist = 1; dist + 1 < HALF_LENGTH; ++dist) {
            smeared[dist] = bloomLine[dist - 1] * 0.20f + bloomLine[dist] * 0.60f + bloomLine[dist + 1] * 0.20f;
        }
        if (HALF_LENGTH > 1) {
            smeared[HALF_LENGTH - 1] = bloomLine[HALF_LENGTH - 2] * 0.30f + bloomLine[HALF_LENGTH - 1] * 0.70f;
        }
    }

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        const CRGB c = hsvProgress(ctx, progress, clamp01(smeared[dist]));
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void EsBloomRefEffect::cleanup() {
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
}

const plugins::EffectMetadata& EsBloomRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Bloom (Ref)",
        "ES v1.1 reference: chroma bloom (squared punch)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference
