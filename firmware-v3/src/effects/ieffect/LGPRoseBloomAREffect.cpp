/**
 * @file LGPRoseBloomAREffect.cpp
 * @brief Rose Bloom (5-Layer Audio-Reactive) — REWRITTEN
 *
 * Rhodonea curve blooming petals. Petal count driven by mid-frequency
 * audio content. Audio drives brightness directly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing,
 * asymmetric max follower, no brightness floors, SET_CENTER_PAIR.
 *
 * Centre-origin compliant. Dual-strip mirrored.
 */

#include "LGPRoseBloomAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kMidTau     = 0.055f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;
static constexpr float kBassSlowTau    = 0.150f;   // Mode E
static constexpr float kMidSlowTau     = 0.150f;   // Mode E
static constexpr float kImpactSlowTau  = 0.600f;   // Mode G

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}
static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

LGPRoseBloomAREffect::LGPRoseBloomAREffect() = default;

LGPRoseBloomAREffect::RoseBloomMode LGPRoseBloomAREffect::s_roseBloomMode = LGPRoseBloomAREffect::RoseBloomMode::Baseline;
LGPRoseBloomAREffect::RoseBloomMode LGPRoseBloomAREffect::s_lastRoseBloomMode = LGPRoseBloomAREffect::RoseBloomMode::Baseline;

const char* LGPRoseBloomAREffect::getRoseBloomModeName(RoseBloomMode m) {
    switch (m) {
        case RoseBloomMode::Baseline: return "Baseline (shipped)";
        case RoseBloomMode::A:        return "A: downbeat envelope";
        case RoseBloomMode::B:        return "B: beat envelope";
        case RoseBloomMode::C:        return "C: tempo-locked LFO";
        case RoseBloomMode::D:        return "D: petal-count latch";
        case RoseBloomMode::E:        return "E: dual-layer EMA";
        case RoseBloomMode::F:        return "F: chroma phrase";
        case RoseBloomMode::G:        return "G: multi-scale impact";
        case RoseBloomMode::H:        return "H: petal spring";
        case RoseBloomMode::I:        return "I: adaptive fade";
        default:                      return "?";
    }
}

bool LGPRoseBloomAREffect::init(plugins::EffectContext& ctx) {
    for (uint8_t zi = 0; zi < kMaxZones; ++zi) {
        m_t[zi] = 0.0f;
        m_bass[zi] = 0.0f;
        m_mid[zi] = 0.0f;
        m_chromaAngle[zi] = 0.0f;
        m_bassMax[zi] = 0.15f;
        m_midMax[zi] = 0.15f;
        m_petalK[zi] = 5.0f;
        m_impact[zi] = 0.0f;
        m_barEnvelope[zi] = 0.0f;
        m_prevBeatFired[zi] = false;
        m_prevDownbeatFired[zi] = false;
        m_iBeatLift[zi] = 0.0f;
        m_petalLatch[zi] = 3.0f;
        m_latchReleaseTime[zi] = 0.0f;
        m_petalVel[zi] = 0.0f;
        m_normBassSlow[zi] = 0.0f;
        m_normMidSlow[zi] = 0.0f;
        m_impactSlow[zi] = 0.0f;
        m_chromaPhrase[zi] = 0.0f;
    }
    lightwaveos::effects::cinema::reset();
    return true;
}

void LGPRoseBloomAREffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // ----- Runtime test-mode local overlays (Captain hardware A/B framework) -----
    const auto mode = s_roseBloomMode;
    if (mode != s_lastRoseBloomMode) {
        // FC guard: zero state owned by the mode being LEFT (anti-momentum bleed).
        if (s_lastRoseBloomMode == RoseBloomMode::H) {
            for (uint8_t zi = 0; zi < kMaxZones; ++zi) m_petalVel[zi] = 0.0f;
        }
        s_lastRoseBloomMode = mode;
    }
    const bool modeIsA = (mode == RoseBloomMode::A);
    const bool modeIsB = (mode == RoseBloomMode::B);
    const bool modeIsC = (mode == RoseBloomMode::C);
    const bool modeIsD = (mode == RoseBloomMode::D);
    const bool modeIsE = (mode == RoseBloomMode::E);
    const bool modeIsF = (mode == RoseBloomMode::F);
    const bool modeIsG = (mode == RoseBloomMode::G);
    const bool modeIsH = (mode == RoseBloomMode::H);
    const bool modeIsI = (mode == RoseBloomMode::I);

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawMid = ctx.audio.available ? ctx.audio.mid() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
    m_bass[z] += (rawBass - m_bass[z]) * (1.0f - expf(-dt / kBassTau));
    m_mid[z] += (rawMid - m_mid[z]) * (1.0f - expf(-dt / kMidTau));

    // Circular chroma EMA
    if (chroma) {
        float sx = 0.0f, sy = 0.0f;
        for (int i = 0; i < 12; i++) {
            float angle = static_cast<float>(i) * (kTwoPi / 12.0f);
            sx += chroma[i] * cosf(angle);
            sy += chroma[i] * sinf(angle);
        }
        if (sx * sx + sy * sy > 0.0001f) {
            float target = atan2f(sy, sx);
            if (target < 0.0f) target += kTwoPi;
            float delta = target - m_chromaAngle[z];
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle[z] += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle[z] < 0.0f) m_chromaAngle[z] += kTwoPi;
            if (m_chromaAngle[z] >= kTwoPi) m_chromaAngle[z] -= kTwoPi;
        }
    }

    // STEP 3: Max followers
    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass[z] > m_bassMax[z]) m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * aA;
        else m_bassMax[z] += (m_bass[z] - m_bassMax[z]) * dA;
        if (m_bassMax[z] < kFollowerFloor) m_bassMax[z] = kFollowerFloor;

        if (m_mid[z] > m_midMax[z]) m_midMax[z] += (m_mid[z] - m_midMax[z]) * aA;
        else m_midMax[z] += (m_mid[z] - m_midMax[z]) * dA;
        if (m_midMax[z] < kFollowerFloor) m_midMax[z] = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass[z] / m_bassMax[z]);
    const float normMid = clamp01(m_mid[z] / m_midMax[z]);

    // Mode E: dual-layer slow EMA chase (kBassSlowTau / kMidSlowTau)
    if (modeIsE) {
        m_normBassSlow[z] += (normBass - m_normBassSlow[z]) * (1.0f - expf(-dt / kBassSlowTau));
        m_normMidSlow[z] += (normMid - m_normMidSlow[z]) * (1.0f - expf(-dt / kMidSlowTau));
    }

    // STEP 4: Impact
    if (beatStr > m_impact[z]) m_impact[z] = beatStr;
    m_impact[z] *= expf(-dt / kImpactDecayTau);

    // Mode G: slow impact channel (parallel to fast m_impact, kImpactSlowTau)
    if (modeIsG) {
        if (beatStr > m_impactSlow[z]) m_impactSlow[z] = beatStr;
        m_impactSlow[z] *= expf(-dt / kImpactSlowTau);
    }

    // Modes A/B: rising-edge-armed slow envelope (LIFT only, ceiling-clamped per FC)
    const bool timingReliable = ctx.audio.available && ctx.audio.timingReliable();
    if (modeIsA && timingReliable) {
        const bool downbeat = ctx.audio.isOnDownbeat();
        if (downbeat && !m_prevDownbeatFired[z]) m_barEnvelope[z] = beatStr;
        m_prevDownbeatFired[z] = downbeat;
        const float bpm = (ctx.audio.bpm() > 30.0f) ? ctx.audio.bpm() : 120.0f;
        m_barEnvelope[z] *= expf(-dt / (60.0f / bpm));
    } else if (modeIsB && timingReliable) {
        const bool beat = ctx.audio.isOnBeat();
        if (beat && !m_prevBeatFired[z]) m_barEnvelope[z] = beatStr;
        m_prevBeatFired[z] = beat;
        const float bpm = (ctx.audio.bpm() > 30.0f) ? ctx.audio.bpm() : 120.0f;
        m_barEnvelope[z] *= expf(-dt / ((60.0f / bpm) * 0.5f));
    } else {
        // Decay envelope toward zero when mode inactive or timing unreliable (FC guard)
        m_barEnvelope[z] *= expf(-dt / 0.4f);
    }

    // Mode I: independent beat-lift envelope for adaptive fadeAmt
    if (modeIsI && timingReliable) {
        const bool beat = ctx.audio.isOnBeat();
        if (beat && !m_prevBeatFired[z]) m_iBeatLift[z] = beatStr;
        m_prevBeatFired[z] = beat;
    }
    if (!modeIsB) {  // Avoid double-write to m_prevBeatFired when Mode B active
        m_iBeatLift[z] *= expf(-dt / 0.5f);
    }

    // STEP 5: Rose visual parameters
    const float beatMod = 0.3f + 0.7f * beatStr;

    // Petal count driven by mid energy (3-7 petals)
    float kfTarget = 3.0f + 4.0f * normMid;

    if (modeIsD) {
        // Mode D: petal-count latch on downbeat with bar-period release
        if (timingReliable && ctx.audio.isOnDownbeat() && !m_prevDownbeatFired[z]) {
            m_petalLatch[z] = clampf(kfTarget, 3.0f, 7.0f);
            const float bpm = (ctx.audio.bpm() > 30.0f) ? ctx.audio.bpm() : 120.0f;
            m_latchReleaseTime[z] = (ctx.audio.tempoConfidence() > 0.3f) ? (240.0f / bpm) : 2.0f;
        }
        if (timingReliable) m_prevDownbeatFired[z] = ctx.audio.isOnDownbeat();
        if (m_latchReleaseTime[z] > 0.0f) {
            m_latchReleaseTime[z] -= dt;
            if (m_latchReleaseTime[z] < 0.0f) m_latchReleaseTime[z] = 0.0f;
        }
        const float effectiveTarget = (m_latchReleaseTime[z] > 0.0f) ? fmaxf(kfTarget, m_petalLatch[z]) : kfTarget;
        const float petalAlpha = 1.0f - expf(-dt / 0.25f);
        m_petalK[z] += (effectiveTarget - m_petalK[z]) * petalAlpha;
    } else if (modeIsH) {
        // Mode H: critically-damped spring with FC-mandated velocity clamp + boundary-zero
        constexpr float kSpringStiffness = 50.0f;
        constexpr float kSpringDamping   = 14.142136f;  // 2 * sqrtf(50)
        constexpr float kVelClamp        = 8.0f;
        const float disp = m_petalK[z] - kfTarget;
        const float accel = -kSpringStiffness * disp - kSpringDamping * m_petalVel[z];
        m_petalVel[z] += accel * dt;
        if (m_petalVel[z] >  kVelClamp) m_petalVel[z] =  kVelClamp;
        if (m_petalVel[z] < -kVelClamp) m_petalVel[z] = -kVelClamp;
        m_petalK[z] += m_petalVel[z] * dt;
        if (m_petalK[z] <= 3.0f && m_petalVel[z] < 0.0f) m_petalVel[z] = 0.0f;
        if (m_petalK[z] >= 7.0f && m_petalVel[z] > 0.0f) m_petalVel[z] = 0.0f;
    } else {
        // Baseline: original exponential lerp
        const float petalAlpha = 1.0f - expf(-dt / 0.25f);
        m_petalK[z] += (kfTarget - m_petalK[z]) * petalAlpha;
    }
    m_petalK[z] = clampf(m_petalK[z], 3.0f, 7.0f);

    // Motion (Mode C: tempo-locked rate when reliable)
    float tRate = 0.3f + 1.8f * speedNorm;
    bool useTempoLfo = false;
    if (modeIsC && timingReliable && ctx.audio.tempoConfidence() > 0.4f) {
        const float bpm = ctx.audio.bpm();
        tRate = (bpm / 60.0f) / 4.0f * kTwoPi;  // 4-beat bar period (rad/s for sin arg)
        useTempoLfo = true;
    }
    m_t[z] += tRate * dtVis;

    // Bloom modulation: opening/closing (Mode C: sin(m_t) directly when tempo-locked)
    float bloomMod = useTempoLfo
        ? (0.55f + 0.45f * sinf(m_t[z]))
        : (0.55f + 0.45f * sinf(m_t[z] * 0.35f));

    // Band width: tighter with impact
    float bandWidth = clampf(0.14f - 0.04f * m_impact[z], 0.08f, 0.18f);

    // Hue from chroma (Mode F: blend with phrase anchor at 15%)
    float effectiveAngle = m_chromaAngle[z];
    if (modeIsF) {
        if (timingReliable && ctx.audio.isOnDownbeat() && !m_prevDownbeatFired[z]) {
            float dPh = m_chromaAngle[z] - m_chromaPhrase[z];
            while (dPh > kPi) dPh -= kTwoPi;
            while (dPh < -kPi) dPh += kTwoPi;
            m_chromaPhrase[z] += dPh * 0.5f;
        }
        if (timingReliable) m_prevDownbeatFired[z] = ctx.audio.isOnDownbeat();
        const float bpm = (timingReliable && ctx.audio.bpm() > 30.0f) ? ctx.audio.bpm() : 120.0f;
        const float phraseSec = (timingReliable && ctx.audio.tempoConfidence() > 0.6f) ? (8.0f * 60.0f / bpm) : 16.0f;
        float dPh = m_chromaAngle[z] - m_chromaPhrase[z];
        while (dPh > kPi) dPh -= kTwoPi;
        while (dPh < -kPi) dPh += kTwoPi;
        m_chromaPhrase[z] += dPh * (1.0f - expf(-dt / phraseSec));
        if (m_chromaPhrase[z] < 0.0f) m_chromaPhrase[z] += kTwoPi;
        if (m_chromaPhrase[z] >= kTwoPi) m_chromaPhrase[z] -= kTwoPi;
        float dBlend = m_chromaPhrase[z] - m_chromaAngle[z];
        while (dBlend > kPi) dBlend -= kTwoPi;
        while (dBlend < -kPi) dBlend += kTwoPi;
        effectiveAngle = m_chromaAngle[z] + dBlend * 0.15f;
        if (effectiveAngle < 0.0f) effectiveAngle += kTwoPi;
        if (effectiveAngle >= kTwoPi) effectiveAngle -= kTwoPi;
    }
    uint8_t baseHue = static_cast<uint8_t>(effectiveAngle * (255.0f / kTwoPi)) + ctx.gHue;

    // Trail persistence (Mode I: lengthen on beat, FC floor of 7 to prevent 120 FPS smearing)
    float fadeF = 20.0f + 35.0f * (1.0f - normBass);
    if (modeIsI) fadeF *= (1.0f - 0.4f * m_iBeatLift[z]);
    uint8_t fadeAmt = static_cast<uint8_t>(clampf(fadeF, modeIsI ? 7.0f : 14.0f, 55.0f));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmt);

    // STEP 6: Per-pixel render — centre-outward, 4-way symmetric
    const float mid = static_cast<float>(HALF_LENGTH - 1);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        const float progress = (HALF_LENGTH <= 1) ? 0.0f
            : (static_cast<float>(dist) / mid);

        // 1D rhodonea: distance from LED to curve position
        // On 1D strip, theta is either 0 or pi. Use progress as radial position.
        float r = static_cast<float>(dist);
        float rCurve = fabsf(cosf(m_petalK[z] * kPi * progress)) * mid * bloomMod;
        float distToCurve = fabsf(r - rCurve) / mid;

        // Gaussian band around curve
        float band = expf(-distToCurve * distToCurve / (bandWidth * bandWidth));

        // Breathing
        float breathing = 0.90f + 0.10f * cosf(kTwoPi * (progress + m_t[z] * 0.25f));

        // Impact flash at petal edges (Mode G: blend with slow channel)
        const float effImpact = modeIsG ? (0.65f * m_impact[z] + 0.35f * m_impactSlow[z]) : m_impact[z];
        float impactAdd = effImpact * band * 0.35f;

        // Effective bass (Mode E: 70/30 fast/slow blend)
        const float effBass = modeIsE ? (0.7f * normBass + 0.3f * m_normBassSlow[z]) : normBass;

        // Compose brightness: audio x geometry x beat x silence
        float brightness = (effBass * band * breathing + impactAdd) * beatMod * silScale;

        // Modes A/B: multiplicative LIFT (FC ceiling-clamp 0.30 — squaring blows out > 1.4)
        if ((modeIsA || modeIsB) && m_barEnvelope[z] > 0.0f) {
            const float lift = clampf(0.5f * m_barEnvelope[z], 0.0f, 0.30f);
            brightness *= (1.0f + lift);
        }

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Hue: chroma base + band offset
        uint8_t hue = baseHue + static_cast<uint8_t>(band * 45.0f + progress * 20.0f);

        SET_CENTER_PAIR(ctx, dist, CHSV(hue, ctx.saturation, val));
    }

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

void LGPRoseBloomAREffect::cleanup() {}

const plugins::EffectMetadata& LGPRoseBloomAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Rose Bloom (5L-AR)",
        "Rhodonea curve blooming petals with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM, 1
    };
    return meta;
}
uint8_t LGPRoseBloomAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPRoseBloomAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPRoseBloomAREffect::setParameter(const char*, float) { return false; }
float LGPRoseBloomAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
