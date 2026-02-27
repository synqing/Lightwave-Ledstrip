/**
 * @file LGPKdVSolitonPairEffect.cpp
 * @brief LGP KdV Soliton Pair -- two sech^2 solitons pass through each other unchanged
 *
 * Korteweg--de Vries soliton pair rendered in distance-from-centre space.
 * Taller soliton travels faster; both pass through each other with an
 * additive "spark" at the collision region, then re-emerge intact.
 *
 * Timed 12-second loop:
 *   Stage 0 (6 s): Approach -- solitons travel inward from opposite edges
 *   Stage 1 (3 s): Collision -- overlap near centre, bright spark
 *   Stage 2 (3 s): Re-emergence -- solitons travel outward, wrap back
 */

#include "LGPKdVSolitonPairEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
const plugins::EffectParameter kParameters[] = {
    {"a1", "Amplitude 1", 0.10f, 3.0f, 1.00f,
     plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"a2", "Amplitude 2", 0.10f, 3.0f, 0.55f,
     plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"width1", "Width 1", 0.05f, 1.0f, 0.18f,
     plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"width2", "Width 2", 0.05f, 1.0f, 0.23f,
     plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"spark_gain", "Spark Gain", 0.0f, 20.0f, 6.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "blend", "", false},
    {"stage0_dur", "Stage 0 Duration", 0.2f, 30.0f, 6.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
    {"stage1_dur", "Stage 1 Duration", 0.2f, 30.0f, 3.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
    {"stage2_dur", "Stage 2 Duration", 0.2f, 30.0f, 3.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "s", false},
    {"base_vel1", "Base Velocity 1", 0.1f, 60.0f, 14.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "led/s", false},
    {"base_vel2", "Base Velocity 2", 0.1f, 60.0f, 8.0f,
     plugins::EffectParameterType::FLOAT, 0.1f, "timing", "led/s", false},
    {"spark_hue_shift", "Spark Hue Shift", 0.0f, 255.0f, 20.0f,
     plugins::EffectParameterType::INT, 1.0f, "colour", "", false},
    {"strip2_hue_shift", "Strip 2 Hue Shift", 0.0f, 255.0f, 30.0f,
     plugins::EffectParameterType::INT, 1.0f, "colour", "", false},
    {"strip2_bright", "Strip 2 Brightness", 0.0f, 255.0f, 217.0f,
     plugins::EffectParameterType::INT, 1.0f, "colour", "", false},
};

static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}
}

// ---------------------------------------------------------------------------
// KdV constants
// ---------------------------------------------------------------------------
static constexpr float kMaxDist    = 80.0f;   // Half-strip length (distance range)

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
LGPKdVSolitonPairEffect::LGPKdVSolitonPairEffect()
    : m_soliton1Pos(0.0f)
    , m_soliton2Pos(0.0f)
    , m_time(0.0f)
    , m_stage(0)
    , m_stageTime(0.0f)
    , m_chromaAngle(0.0f)
    , m_beatPulse(0.0f)
    , m_fallbackPhase(0.0f)
    , m_lastHopSeq(0)
{
    for (uint8_t i = 0; i < 12; ++i) m_chromaSmoothed[i] = 0.0f;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
bool LGPKdVSolitonPairEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Start solitons at opposite edges of distance space
    m_soliton1Pos = kMaxDist;     // Starts at edge
    m_soliton2Pos = kMaxDist;     // Also starts at edge (different speed makes them separate)
    m_time        = 0.0f;
    m_stage       = 0;
    m_stageTime   = 0.0f;
    m_chromaAngle = 0.0f;
    m_beatPulse   = 0.0f;
    m_fallbackPhase = 0.0f;
    m_lastHopSeq  = 0;

    for (uint8_t i = 0; i < 12; ++i) m_chromaSmoothed[i] = 0.0f;

    return true;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void LGPKdVSolitonPairEffect::render(plugins::EffectContext& ctx) {
    // =====================================================================
    // Safe delta time
    // =====================================================================
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt    = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;

    // =====================================================================
    // Audio reactivity
    // =====================================================================
    float rmsModulation = 1.0f;        // Amplitude modulation from RMS
    uint8_t chromaHueOffset = 0;       // Hue offset from chroma analysis

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // RMS modulates soliton amplitude +/-15%
        float rms = ctx.audio.rms();
        rmsModulation = 1.0f + (rms - 0.5f) * 0.30f;  // 0.85 .. 1.15
        if (rmsModulation < 0.85f) rmsModulation = 0.85f;
        if (rmsModulation > 1.15f) rmsModulation = 1.15f;

        // Beat triggers amplitude pulse on soliton 1
        if (ctx.audio.isOnBeat()) {
            float bs = ctx.audio.beatStrength();
            if (bs > m_beatPulse) m_beatPulse = bs;
        }

        // Update chromagram targets on new hops only
        if (ctx.audio.controlBus.hop_seq != m_lastHopSeq) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            for (uint8_t i = 0; i < 12; ++i) {
                m_chromaSmoothed[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
        }
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.25f);
    } else {
        // No audio fallback
        m_fallbackPhase += speedNorm * 0.3f * dt;
        if (m_fallbackPhase > 100.0f) m_fallbackPhase -= 100.0f;
    }
#else
    // No audio feature compiled
    m_fallbackPhase += speedNorm * 0.3f * dt;
    if (m_fallbackPhase > 100.0f) m_fallbackPhase -= 100.0f;
#endif

    // Decay beat pulse (dt-corrected)
    m_beatPulse = effects::chroma::dtDecay(m_beatPulse, 0.88f, rawDt);

    // =====================================================================
    // Timed sequence: advance stage clock
    // =====================================================================
    m_stageTime += dt * speedNorm;
    m_time      += dt * speedNorm;

    // Determine current stage boundaries
    float stageDur = 0.0f;
    switch (m_stage) {
        case 0: stageDur = m_stage0Dur; break;
        case 1: stageDur = m_stage1Dur; break;
        case 2: stageDur = m_stage2Dur; break;
        default: stageDur = m_stage0Dur; break;
    }

    if (m_stageTime >= stageDur) {
        m_stageTime -= stageDur;
        m_stage = (m_stage + 1) % 3;

        // Reset soliton positions at stage 0 start (new loop)
        if (m_stage == 0) {
            m_soliton1Pos = kMaxDist;
            m_soliton2Pos = kMaxDist;
        }
    }

    // =====================================================================
    // Update soliton positions
    // =====================================================================
    // Effective amplitudes with audio modulation
    float effA1 = m_a1 * rmsModulation * (1.0f + m_beatPulse * 0.25f);
    float effA2 = m_a2 * rmsModulation;

    // KdV: velocity proportional to amplitude
    float vel1 = m_baseVel1 * speedNorm * (effA1 / fmaxf(m_a1, 0.001f));
    float vel2 = m_baseVel2 * speedNorm * (effA2 / fmaxf(m_a2, 0.001f));

    switch (m_stage) {
        case 0:
            // Approach: both travel inward (decreasing distance)
            m_soliton1Pos -= vel1 * dt;
            m_soliton2Pos -= vel2 * dt;
            break;
        case 1:
            // Collision: continue through each other
            m_soliton1Pos -= vel1 * dt;
            m_soliton2Pos -= vel2 * dt;
            break;
        case 2:
            // Re-emergence: travel outward (increasing distance)
            m_soliton1Pos += vel1 * dt;
            m_soliton2Pos += vel2 * dt;
            break;
    }

    // Clamp to valid range (no hard wrap -- the timed sequence handles reset)
    if (m_soliton1Pos < -10.0f) m_soliton1Pos = -10.0f;
    if (m_soliton1Pos > kMaxDist + 10.0f) m_soliton1Pos = kMaxDist + 10.0f;
    if (m_soliton2Pos < -10.0f) m_soliton2Pos = -10.0f;
    if (m_soliton2Pos > kMaxDist + 10.0f) m_soliton2Pos = kMaxDist + 10.0f;

    // =====================================================================
    // Fade for trail persistence
    // =====================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =====================================================================
    // Render loop: per-LED in strip 1 (mirrored via centerPairDistance)
    // =====================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        float d = static_cast<float>(centerPairDistance(i));

        // sech^2 profiles
        float z1 = (d - m_soliton1Pos) * m_width1;
        float cosh1 = coshf(z1);
        float u1 = effA1 / (cosh1 * cosh1);

        float z2 = (d - m_soliton2Pos) * m_width2;
        float cosh2 = coshf(z2);
        float u2 = effA2 / (cosh2 * cosh2);

        // Additive combination (KdV superposition)
        float total = u1 + u2;
        if (total > 1.5f) total = 1.5f;  // Soft clamp for headroom

        // Collision spark: product of overlapping solitons
        float spark = u1 * u2 * m_sparkGain;
        if (spark > 1.0f) spark = 1.0f;

        // Final brightness
        float rawBright = total + spark * 0.5f;  // Spark adds extra glow
        if (rawBright > 1.0f) rawBright = 1.0f;
        uint8_t brightness = static_cast<uint8_t>(rawBright * ctx.brightness);

        // Hue: palette base + chroma offset + spark warm shift
        uint8_t baseHue = ctx.gHue + chromaHueOffset;
        uint8_t sparkShift = static_cast<uint8_t>(spark * m_sparkHueShift);
        uint8_t hue = static_cast<uint8_t>(baseHue + sparkShift);

        // Strip 1
        CRGB color1 = ctx.palette.getColor(hue, brightness);
        ctx.leds[i] = color1;

        // Strip 2: +30 hue offset, 85% brightness
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t bright2 = scale8(brightness, m_strip2Bright);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                static_cast<uint8_t>(hue + m_strip2HueShift), bright2);
        }
    }
}

// ---------------------------------------------------------------------------
// cleanup
// ---------------------------------------------------------------------------
void LGPKdVSolitonPairEffect::cleanup() {
    // No resources to free -- all state is in-class members
}

// ---------------------------------------------------------------------------
// getMetadata
// ---------------------------------------------------------------------------
const plugins::EffectMetadata& LGPKdVSolitonPairEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP KdV Soliton Pair",
        "Two sech^2 solitons pass through each other unchanged -- KdV physics",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPKdVSolitonPairEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPKdVSolitonPairEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPKdVSolitonPairEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (std::strcmp(name, "a1") == 0) { m_a1 = clampf(value, 0.10f, 3.0f); return true; }
    if (std::strcmp(name, "a2") == 0) { m_a2 = clampf(value, 0.10f, 3.0f); return true; }
    if (std::strcmp(name, "width1") == 0) { m_width1 = clampf(value, 0.05f, 1.0f); return true; }
    if (std::strcmp(name, "width2") == 0) { m_width2 = clampf(value, 0.05f, 1.0f); return true; }
    if (std::strcmp(name, "spark_gain") == 0) { m_sparkGain = clampf(value, 0.0f, 20.0f); return true; }
    if (std::strcmp(name, "stage0_dur") == 0) { m_stage0Dur = clampf(value, 0.2f, 30.0f); return true; }
    if (std::strcmp(name, "stage1_dur") == 0) { m_stage1Dur = clampf(value, 0.2f, 30.0f); return true; }
    if (std::strcmp(name, "stage2_dur") == 0) { m_stage2Dur = clampf(value, 0.2f, 30.0f); return true; }
    if (std::strcmp(name, "base_vel1") == 0) { m_baseVel1 = clampf(value, 0.1f, 60.0f); return true; }
    if (std::strcmp(name, "base_vel2") == 0) { m_baseVel2 = clampf(value, 0.1f, 60.0f); return true; }
    if (std::strcmp(name, "spark_hue_shift") == 0) {
        m_sparkHueShift = static_cast<uint8_t>(clampf(value, 0.0f, 255.0f) + 0.5f);
        return true;
    }
    if (std::strcmp(name, "strip2_hue_shift") == 0) {
        m_strip2HueShift = static_cast<uint8_t>(clampf(value, 0.0f, 255.0f) + 0.5f);
        return true;
    }
    if (std::strcmp(name, "strip2_bright") == 0) {
        m_strip2Bright = static_cast<uint8_t>(clampf(value, 0.0f, 255.0f) + 0.5f);
        return true;
    }
    return false;
}

float LGPKdVSolitonPairEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (std::strcmp(name, "a1") == 0) return m_a1;
    if (std::strcmp(name, "a2") == 0) return m_a2;
    if (std::strcmp(name, "width1") == 0) return m_width1;
    if (std::strcmp(name, "width2") == 0) return m_width2;
    if (std::strcmp(name, "spark_gain") == 0) return m_sparkGain;
    if (std::strcmp(name, "stage0_dur") == 0) return m_stage0Dur;
    if (std::strcmp(name, "stage1_dur") == 0) return m_stage1Dur;
    if (std::strcmp(name, "stage2_dur") == 0) return m_stage2Dur;
    if (std::strcmp(name, "base_vel1") == 0) return m_baseVel1;
    if (std::strcmp(name, "base_vel2") == 0) return m_baseVel2;
    if (std::strcmp(name, "spark_hue_shift") == 0) return static_cast<float>(m_sparkHueShift);
    if (std::strcmp(name, "strip2_hue_shift") == 0) return static_cast<float>(m_strip2HueShift);
    if (std::strcmp(name, "strip2_bright") == 0) return static_cast<float>(m_strip2Bright);
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
