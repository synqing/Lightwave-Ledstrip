/**
 * @file BeatParitySpriteEffect.cpp
 * @brief Phase 5 Move 5.7 (restructured) — standalone implementation.
 *
 * See header for visual signature, topology compliance, and the rationale
 * behind the S1..S12 restructure. Every non-trivial change cites its spec
 * section in an inline comment so the orchestrator can audit the diff.
 */

#include "BeatParitySpriteEffect.h"

#include "../CoreEffects.h"                  // CENTER_LEFT/RIGHT, HALF_LENGTH, STRIP_LENGTH
#include "ChromaUtils.h"                     // S5: circularChromaHueSmoothed
#include "../enhancement/SmoothingEngine.h"  // S7: SubpixelRenderer
#include "../../config/Trace.h"

#include <FastLED.h>          // fadeToBlackBy, scale8, qadd8
#include <cstdint>
#include <cstdlib>            // std::malloc/std::free for native build

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "effects/PersistenceHelpers.h"
using lightwaveos::effects::persistence::fadeToBlackByDt;
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

inline float clamp01(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x;
}

// ─── S1: PSRAM allocator helpers ────────────────────────────────────────
// Mirrors RadialTimeScopeEffect.cpp:49-79 verbatim with the type swapped.
// Native test build has no PSRAM; fall back to plain heap so the same
// alloc → init → render → cleanup code path is exercised in unit tests.
inline BeatParitySpriteEffect::PsramData* allocateState() {
#ifdef NATIVE_BUILD
    auto* p = static_cast<BeatParitySpriteEffect::PsramData*>(
        std::malloc(sizeof(BeatParitySpriteEffect::PsramData)));
    if (p != nullptr) {
        new (p) BeatParitySpriteEffect::PsramData();
    }
    return p;
#else
    auto* p = static_cast<BeatParitySpriteEffect::PsramData*>(
        heap_caps_malloc(sizeof(BeatParitySpriteEffect::PsramData), MALLOC_CAP_SPIRAM));
    if (p != nullptr) {
        // Placement-new so the sprite array's default member initialisers run.
        new (p) BeatParitySpriteEffect::PsramData();
    }
    return p;
#endif
}

inline void freeState(BeatParitySpriteEffect::PsramData* p) {
    if (p == nullptr) return;
    p->~PsramData();
#ifdef NATIVE_BUILD
    std::free(p);
#else
    heap_caps_free(p);
#endif
}

}  // anonymous namespace

bool BeatParitySpriteEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Idempotent init — re-arming after cleanup() must not leak.
    if (m_ps == nullptr) {
        m_ps = allocateState();
        if (m_ps == nullptr) {
            // PSRAM exhausted — render() guards against m_ps==nullptr,
            // but signal failure to the dispatcher so it can fall back.
            return false;
        }
    }

    // S1: explicitly clear each slot in case the allocator returned a
    // recycled region that did not satisfy the default initialiser.
    for (uint8_t i = 0; i < kMaxSprites; ++i) {
        m_ps->sprites[i] = Sprite{};
    }

    m_nextSlot      = 0;
    m_lastBeatInBar = 0xFF;
    m_chromaAngle   = 0.0f;
    return true;
}

void BeatParitySpriteEffect::render(plugins::EffectContext& ctx) {
    TRACE_SCOPE("bps_render");

    if (m_ps == nullptr) return;
    if (ctx.leds == nullptr || ctx.ledCount == 0) return;

    // ─── Source the audio context ─────────────────────────────────────────
    float audioConfidence = 0.0f;
    float silentScale     = 1.0f;
    float rmsLevel        = 0.0f;
    bool  kickFired       = false;
    bool  tempoTick       = false;
    float tempoConfidence = 0.0f;
    uint8_t beatInBar     = 0;
    const float* chromaPtr = nullptr;
    static constexpr float kZeroChroma[12] = {0};

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        audioConfidence = ctx.audio.audioConfidence();
        silentScale     = ctx.audio.silentScale();
        rmsLevel        = ctx.audio.rms();
        // S2: kickTrigger is the PRIMARY spawn signal — band-ratio detector
        // with no tempo-lock dependency, fires reliably on chord-only music.
        kickFired       = ctx.audio.isKickHit();
        tempoTick       = ctx.audio.tempoBeatTick();
        tempoConfidence = ctx.audio.tempoBeatConfidence();
        beatInBar       = ctx.audio.beatInBar();
        chromaPtr       = ctx.audio.chroma();
    } else {
        chromaPtr = kZeroChroma;
    }
#else
    chromaPtr = kZeroChroma;
#endif

    TRACE_COUNTER("bps_audio_conf",     (int)(audioConfidence * 1000));
    TRACE_COUNTER("bps_tempo_conf",     (int)(tempoConfidence * 1000));
    TRACE_COUNTER("bps_beat_in_bar",    (int)beatInBar);
    TRACE_COUNTER("bps_kick_trigger",   kickFired ? 1 : 0);                 // S12
    TRACE_COUNTER("bps_silent_scale",   (int)(silentScale * 1000));         // S12

    // ─── Frame timing ────────────────────────────────────────────────────
    // S10: use getSafeRawDeltaSeconds — beat timing must remain independent
    // of the SPEED knob. The speed knob is applied separately to the sprite
    // expansion rate further below.
    const float dt = ctx.getSafeRawDeltaSeconds();

    // ─── S5: continuously-running circular chroma angle ──────────────────
    // Update EVERY frame so the angle is always available at spawn time.
    // tau=0.20s gives a smooth, perceptual hue track that does not flick
    // on bin-flip edges (the failure mode of the old argmax helper).
    const uint8_t hueByteNow = effects::chroma::circularChromaHueSmoothed(
        chromaPtr, m_chromaAngle, dt, 0.20f);

    // ─── S8: bed layer ────────────────────────────────────────────────────
    // Dim rms-driven floor written FIRST so subsequent fadeToBlackBy + sprite
    // composition compose on top. Uses `=` (not `+=`) because this is the
    // floor; the floor never accumulates, it only sets the baseline.
    {
        const float rmsCl = clamp01(rmsLevel);
        const uint8_t bedBright = scale8((uint8_t)(rmsCl * 255.0f), 40);  // ≤16% floor
        if (bedBright > 0) {
            const CRGB bedCol = ctx.palette.getColor(ctx.gHue, bedBright);
            for (uint16_t i = 0; i < ctx.ledCount; ++i) {
                ctx.leds[i] = bedCol;
            }
        }
    }

    // ─── S9: audio-energy-adaptive fadeToBlackBy ─────────────────────────
    // Loud → short trails (fadeAmount near 18, ~93% retention).
    // Quiet → longer trails (fadeAmount near 30, ~88% retention).
    {
        const float rmsCl = clamp01(rmsLevel);
        const uint8_t fadeAmount = (uint8_t)(18.0f + 12.0f * (1.0f - rmsCl));
        fadeToBlackByDt(ctx.leds, ctx.ledCount, fadeAmount, ctx.getSafeDeltaSeconds());
    }

    // ─── Hard silence gate ────────────────────────────────────────────────
    // If the audio chain reports silence/low confidence, skip spawn + sprite
    // update entirely; existing sprite trails fade naturally via the bed
    // and fadeToBlackBy above.
    if (audioConfidence < 0.10f || silentScale < 0.20f) {
        TRACE_INSTANT("bps_silence_gate");
        return;
    }

    // ─── S3: latch beat-in-bar on tempo ticks for parity tracking ────────
    // The parity (odd/even) reading is sampled at sprite-spawn time below,
    // decoupled from the spawn trigger so a kick on an even-numbered beat
    // still spawns (parity only modulates accent strength).
    if (tempoTick) {
        if (m_lastBeatInBar == beatInBar) {
            TRACE_INSTANT("bps_beat_unchanged");                            // S12
        }
        m_lastBeatInBar = beatInBar;
    }

    const bool tempoLocked = (tempoConfidence >= kTempoLockGate);
    if (!tempoLocked) {
        TRACE_INSTANT("bps_tempo_unlocked");                                // S12
    }

    // ─── S2/S4: spawn on kickTrigger, round-robin overwrite oldest ───────
    if (kickFired) {
        TRACE_INSTANT("bps_kick_fired");

        // Parity is reported via TRACE — not used as a hard gate. Captain
        // can A/B whether to gate spawns on parity at runtime; for the
        // current build a kick always spawns so chord-only music with no
        // tempo lock still produces visible sprites.
        const bool isOddParity = ((m_lastBeatInBar & 0x01) == 1);
        if (!isOddParity) {
            TRACE_INSTANT("bps_even_parity_skip");                          // S12
        }

        // Downbeat accent: when a kick coincides with a tempo-locked
        // tempoBeatTick, the sprite is brighter and lives slightly longer.
        const bool downbeatAccent = (tempoTick && tempoLocked);
        const float accent = downbeatAccent ? kDownbeatBoost : 1.0f;

        // S4: round-robin — write into m_nextSlot, advance, modulo.
        // No "drop-on-full" branch — the oldest sprite is overwritten.
        const uint8_t slot = m_nextSlot;
        m_nextSlot = static_cast<uint8_t>((m_nextSlot + 1) % kMaxSprites);

        Sprite& s = m_ps->sprites[slot];
        s = Sprite{};                                                       // reset
        s.ageSec      = 0.0f;
        s.lifetimeSec = kSpriteLifetimeSec * (downbeatAccent ? 1.10f : 1.0f);
        s.intensity   = clamp01(kMaxSpriteIntensity * clamp01(audioConfidence) * accent);
        s.hueByte     = hueByteNow;                                         // S5: snapshot
        s.active      = true;

        TRACE_INSTANT("bps_sprite_spawn");
        TRACE_COUNTER("bps_dominant_chroma", (int)hueByteNow);              // S12
    }

    // ─── Update + render sprites ──────────────────────────────────────────
    // S10: the speed knob scales sprite EXPANSION RATE only (cosmetic).
    const float speedNorm    = (float)ctx.speed / 50.0f;  // ~0.02..2.0
    const float speedDt      = dt * (speedNorm < 0.1f ? 0.1f : speedNorm);
    const float silenceMul   = clamp01(silentScale);
    const uint16_t total     = ctx.ledCount;
    const uint16_t stripLen  = (total >= STRIP_LENGTH) ? STRIP_LENGTH : total;

    uint8_t  activeCount    = 0;
    uint32_t oldestAgeMs    = 0;

    for (uint8_t s = 0; s < kMaxSprites; ++s) {
        Sprite& sp = m_ps->sprites[s];
        if (!sp.active) continue;

        sp.ageSec += speedDt;
        if (sp.ageSec >= sp.lifetimeSec) {
            sp.active = false;
            continue;
        }
        ++activeCount;

        const uint32_t ageMs = (uint32_t)(sp.ageSec * 1000.0f);
        if (ageMs > oldestAgeMs) oldestAgeMs = ageMs;

        const float t          = sp.ageSec / sp.lifetimeSec;       // 0..1
        const float radiusF    = t * static_cast<float>(HALF_LENGTH);
        const float fadeAlpha  = (1.0f - t) * sp.intensity * silenceMul;

        if (fadeAlpha <= 0.0f) continue;

        // S6: sprite colour from the palette using the per-sprite hueByte
        // captured at spawn — stable colour for the sprite's whole lifetime.
        // S7: SubpixelRenderer interpolates between adjacent LEDs with qadd8
        // saturating addition internally, eliminating the wagon-wheel
        // aliasing the old integer kernel produced.
        const uint8_t alpha8 = (uint8_t)(clamp01(fadeAlpha) * 255.0f + 0.5f);
        if (alpha8 == 0) continue;

        const CRGB col = ctx.palette.getColor(sp.hueByte, 255);

        // Strip 1 — left arm (CENTER_LEFT - r), right arm (CENTER_RIGHT + r).
        const float pLeft1  = static_cast<float>(CENTER_LEFT)  - radiusF;
        const float pRight1 = static_cast<float>(CENTER_RIGHT) + radiusF;

        if (pLeft1 >= 0.0f && pLeft1 < static_cast<float>(stripLen) - 1.0f) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds, ctx.ledCount, pLeft1, col, alpha8);
        }
        if (pRight1 >= 0.0f && pRight1 < static_cast<float>(stripLen) - 1.0f) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds, ctx.ledCount, pRight1, col, alpha8);
        }

        // Strip 2 reflective twin — same fractional offsets shifted by
        // STRIP_LENGTH. Bounds-checked so a non-standard ledCount does not
        // overrun.
        const float pLeft2  = pLeft1  + static_cast<float>(STRIP_LENGTH);
        const float pRight2 = pRight1 + static_cast<float>(STRIP_LENGTH);

        if (pLeft2 >= static_cast<float>(STRIP_LENGTH) &&
            pLeft2 < static_cast<float>(total) - 1.0f) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds, ctx.ledCount, pLeft2, col, alpha8);
        }
        if (pRight2 >= static_cast<float>(STRIP_LENGTH) &&
            pRight2 < static_cast<float>(total) - 1.0f) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds, ctx.ledCount, pRight2, col, alpha8);
        }
    }

    TRACE_COUNTER("bps_active_sprites",       (int)activeCount);
    TRACE_COUNTER("bps_oldest_sprite_age_ms", (int)oldestAgeMs);            // S12
}

void BeatParitySpriteEffect::cleanup() {
    freeState(m_ps);
    m_ps            = nullptr;
    m_nextSlot      = 0;
    m_lastBeatInBar = 0xFF;
    m_chromaAngle   = 0.0f;
}

const plugins::EffectMetadata& BeatParitySpriteEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Parity Sprite",
        "Sprites radiate from LEDs 79/80 outward on kick onsets; soft bed between hits.",
        plugins::EffectCategory::PARTY,
        1,
        nullptr,
        plugins::EffectRoleFlags::SELF_TRAILING
    };
#ifndef NATIVE_BUILD
    meta.id = kId;
#endif
    return meta;
}

#ifdef NATIVE_BUILD
bool BeatParitySpriteEffect::testForceSpawn(uint8_t hueByte, float intensity) {
    if (m_ps == nullptr) return false;
    // Match production round-robin behaviour so tests exercise the same path.
    const uint8_t slot = m_nextSlot;
    m_nextSlot = static_cast<uint8_t>((m_nextSlot + 1) % kMaxSprites);

    Sprite& s = m_ps->sprites[slot];
    s = Sprite{};
    s.ageSec      = 0.0f;
    s.lifetimeSec = kSpriteLifetimeSec;
    s.intensity   = clamp01(intensity);
    s.hueByte     = hueByte;
    s.active      = true;
    return true;
}

uint8_t BeatParitySpriteEffect::activeSpriteCount() const {
    if (m_ps == nullptr) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < kMaxSprites; ++i) {
        if (m_ps->sprites[i].active) ++n;
    }
    return n;
}
#endif

}  // namespace ieffect
}  // namespace effects
}  // namespace lightwaveos
