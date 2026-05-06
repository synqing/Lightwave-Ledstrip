/**
 * @file BeatParitySpriteEffect.h
 * @brief Phase 5 Move 5.7 (restructured) — beat-parity sprite radiation, standalone.
 *
 * The original Move 5.7 spec was an additive sprite injection on top of the
 * SbK1Bloom hero. Hardware showed catastrophic visual saturation: sprites
 * stacking onto an already-bright bloom output produced a smeared, washed
 * mess. The fix is to make this a STANDALONE effect on a black/near-black
 * background, not a modification of an existing effect.
 *
 * The first restructure attempt also failed on hardware ("DOES NOT WORK").
 * Three causes, all proven from source:
 *   1. The original spawn gate `tempoBeatTick && es_phase01 ≈ 0.65` is
 *      mathematically impossible — `m_beatPhase` wraps to ~0 in the same
 *      backend code block where `tick=true` is set, so the two conditions
 *      can NEVER co-fire (EsV11Backend.cpp:312-321).
 *   2. The simplified gate `tempoBeatTick` alone still fails on chord-only
 *      music because tempo lock requires percussive transients.
 *   3. The 128 B sprite array sat as an instance value member, violating
 *      the IEffect.h PSRAM allocation policy.
 *
 * Restructure (S1..S12 in commit message):
 *   - Sprite array → PSRAM via PsramData allocator helpers (S1).
 *   - Spawn primary trigger → bus.kickTrigger (band-ratio onset, no
 *     tempo-lock dependency, fires reliably on chord-only music) (S2).
 *   - Tempo beat tick used only to update parity tracking (S3).
 *   - Round-robin overwrite oldest if sprite pool full (S4).
 *   - Hue from circularChromaHueSmoothed, sampled AT spawn (S5/S6).
 *   - SubpixelRenderer at fractional radii — no integer kernel (S7).
 *   - No always-alive RMS bed; musical events own visible structure (S8).
 *   - Fixed dt-correct trail fade, independent of raw frame RMS (S9).
 *   - Speed knob via getSafeRawDeltaSeconds() (S10).
 *   - kTempoLockGate 0.4 → 0.30 per m2_adversarial/expected_results.md:127 (S11).
 *   - Extra TRACE_* counters for hardware diagnosis (S12).
 *
 * ── Visual signature ─────────────────────────────────────────────────────
 *
 *   - Black/near-black background so silence dissolves and cheap
 *     amplitude-meter behaviour cannot own the fixture.
 *   - On every kick onset (or downbeat accent) at sufficient confidence,
 *     a sprite spawns at LEDs 79/80 and radiates outward over ~0.6 s,
 *     fading from full intensity to black as it travels to the edges.
 *   - Up to 8 sprites alive concurrently — bars at high tempo overlap.
 *   - Hue is a continuously-updated circular-chroma mean, sampled AT
 *     spawn for stable per-sprite colour; raw RMS can only add a bounded
 *     brightness accent after an event has created state.
 *
 * ── Topology compliance ──────────────────────────────────────────────────
 *
 *   - Centre origin: every sprite originates at LEDs 79/80 (radius 0).
 *   - Strict mirror: each sprite writes to (CENTER_LEFT - r) and
 *     (CENTER_RIGHT + r) with the same colour and intensity.
 *   - Reflective twin: strip 2 (160..319) mirrors strip 1 (0..159).
 *
 * Effect ID: 0x2102 (FAMILY_K1_NATIVE)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class BeatParitySpriteEffect : public plugins::IEffect {
public:
#ifndef NATIVE_BUILD
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_BEAT_PARITY_SPRITE;
#endif

    static constexpr uint8_t kMaxSprites          = 8;
    static constexpr float   kSpriteLifetimeSec   = 0.6f;
    // S11: tempo-confidence floor lowered from 0.4 to 0.30 per
    // m2_adversarial/expected_results.md:127 — 0.30 is the runtime gate
    // observed on hardware where tempoLocked transitions become reliable.
    static constexpr float   kTempoLockGate       = 0.30f;
    static constexpr float   kMaxSpriteIntensity  = 0.9f;
    // Downbeat accent multiplier — when a kick happens to coincide with a
    // tempo-locked downbeat we paint a brighter, longer-lived sprite.
    static constexpr float   kDownbeatBoost       = 1.25f;

    /// Per-sprite state. Each sprite is 16 B; an array of 8 is 128 B which
    /// exceeds the 64 B threshold and therefore lives in PSRAM (see PsramData).
    struct Sprite {
        float   ageSec;
        float   lifetimeSec;
        float   intensity;
        uint8_t hueByte;
        bool    active;
    };

    /// PSRAM-resident state. Per IEffect.h PSRAM ALLOCATION POLICY
    /// (>64 B buffers MUST be allocated from PSRAM via heap_caps_malloc with
    /// MALLOC_CAP_SPIRAM), the sprite array must NOT be a value member.
    /// Public so the translation-unit-local allocateState/freeState helpers
    /// can name it; treat as an implementation detail.
    struct PsramData {
        Sprite sprites[kMaxSprites] = {};
    };

    BeatParitySpriteEffect() = default;
    ~BeatParitySpriteEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

#ifdef NATIVE_BUILD
    bool testForceSpawn(uint8_t hueByte, float intensity);
    uint8_t activeSpriteCount() const;
#endif

private:
    PsramData* m_ps             = nullptr;  // S1: PSRAM-allocated sprite pool
    uint8_t    m_nextSlot       = 0;        // S4: round-robin write index
    uint8_t    m_lastBeatInBar  = 0xFF;     // Latched beat-in-bar (S3)
    float      m_chromaAngle    = 0.0f;     // S5: continuously-updated chroma EMA
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
