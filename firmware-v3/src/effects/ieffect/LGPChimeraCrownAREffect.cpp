/**
 * @file LGPChimeraCrownAREffect.cpp
 * @brief Chimera Crown (5-Layer AR) — REWRITTEN
 *
 * Kuramoto-ish oscillator coupling on 160 elements with direct audio-reactive
 * composition. Base maths from LGPChimeraCrownEffect preserved exactly.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Physics: Two-pass Kuramoto model
 *   Pass 1: compute local order parameter Rlocal[i] via windowed cosine kernel
 *   Pass 2: update theta[i] via Kuramoto coupling + intrinsic omega[i]
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPChimeraCrownAREffect.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

#include "../../utils/Log.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

// =========================================================================
// Constants
// =========================================================================

static constexpr float kTwoPi = 6.28318530717958647692f;
static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kBassTau    = 0.050f;
static constexpr float kTrebleTau  = 0.040f;
static constexpr float kChromaTau  = 0.300f;
static constexpr float kFollowerAttackTau = 0.058f;
static constexpr float kFollowerDecayTau  = 0.500f;
static constexpr float kFollowerFloor     = 0.04f;
static constexpr float kImpactDecayTau = 0.180f;

// =========================================================================
// Local helpers
// =========================================================================

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

static inline float smoothstep(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

static inline float centreGlue(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float d = static_cast<float>(i) - mid;
    return 0.30f + 0.70f * expf(-(d * d) * 0.0016f);
}

static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPChimeraCrownAREffect::LGPChimeraCrownAREffect()
    : m_ps(nullptr)
    , m_t(0.0f)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
{
}

bool LGPChimeraCrownAREffect::init(plugins::EffectContext& ctx) {
    m_t = 0.0f;
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;

    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<ChimeraPsram*>(heap_caps_malloc(sizeof(ChimeraPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPChimeraCrownAREffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(ChimeraPsram));

    // Deterministic hash-based init for theta/omega (no rand())
    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint32_t h = hash32(0xC0FFEEu ^ (static_cast<uint32_t>(i) * 2654435761u));
        float u = static_cast<float>(h & 0xFFFFu) / 65535.0f;
        m_ps->theta[i] = u * kTwoPi;

        uint32_t h2 = hash32(h ^ 0xA5A5A5A5u);
        float v = (static_cast<float>(static_cast<int>(h2 & 0xFFFFu) - 32768) / 32768.0f);
        m_ps->omega[i] = 0.20f * v;
        m_ps->Rlocal[i] = 0.0f;
    }
#endif

    return true;
}

// =========================================================================
// render() — direct audio-reactive composition
// =========================================================================

void LGPChimeraCrownAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // STEP 1: Direct ControlBus reads
    const float rawBass = ctx.audio.available ? ctx.audio.bass() : 0.0f;
    const float rawTreble = ctx.audio.available ? ctx.audio.treble() : 0.0f;
    const float beatStr = ctx.audio.available ? ctx.audio.beatStrength() : 0.0f;
    const float silScale = ctx.audio.available ? ctx.audio.silentScale() : 0.0f;
    const float* chroma = ctx.audio.available ? ctx.audio.chroma() : nullptr;

    // STEP 2: Single-stage smoothing
    m_bass += (rawBass - m_bass) * (1.0f - expf(-dt / kBassTau));
    m_treble += (rawTreble - m_treble) * (1.0f - expf(-dt / kTrebleTau));

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
            float delta = target - m_chromaAngle;
            while (delta > kPi) delta -= kTwoPi;
            while (delta < -kPi) delta += kTwoPi;
            m_chromaAngle += delta * (1.0f - expf(-dt / kChromaTau));
            if (m_chromaAngle < 0.0f) m_chromaAngle += kTwoPi;
            if (m_chromaAngle >= kTwoPi) m_chromaAngle -= kTwoPi;
        }
    }

    // STEP 3: Max followers
    {
        float aA = 1.0f - expf(-dt / kFollowerAttackTau);
        float dA = 1.0f - expf(-dt / kFollowerDecayTau);
        if (m_bass > m_bassMax) m_bassMax += (m_bass - m_bassMax) * aA;
        else m_bassMax += (m_bass - m_bassMax) * dA;
        if (m_bassMax < kFollowerFloor) m_bassMax = kFollowerFloor;

        if (m_treble > m_trebleMax) m_trebleMax += (m_treble - m_trebleMax) * aA;
        else m_trebleMax += (m_treble - m_trebleMax) * dA;
        if (m_trebleMax < kFollowerFloor) m_trebleMax = kFollowerFloor;
    }
    const float normBass = clamp01(m_bass / m_bassMax);
    const float normTreble = clamp01(m_treble / m_trebleMax);

    // STEP 4: Impact (continuous beatStrength rise, exponential decay)
    if (beatStr > m_impact) m_impact = beatStr;
    m_impact *= expf(-dt / kImpactDecayTau);

    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // KURAMOTO PHYSICS (PRESERVED EXACTLY)
    // =================================================================

    // Structure controls coupling strength K and window width W
    // normBass drives coupling (was m_structure driven by bass + flux)
    const float structureDrive = clamp01(0.35f + 0.50f * normBass + 0.15f * normTreble);

    const float baseW = 10.0f + 10.0f * speedNorm;
    const float W = baseW * (0.65f + 0.35f * structureDrive);
    static constexpr int kMaxWindowRadius = 24;
    int iW = static_cast<int>(W);
    if (iW < 2) iW = 2;
    if (iW > kMaxWindowRadius) iW = kMaxWindowRadius;

    const float baseK = 1.4f + 1.2f * speedNorm;
    const float K = baseK * (0.70f + 0.50f * structureDrive);

    const float alpha = 0.55f;
    // motionRate replaced: bass energy + speed drives integration step
    const float motionRate = 0.6f + 0.8f * normBass + 0.3f * m_impact;
    const float dtK = (0.035f + 0.030f * speedNorm) * motionRate;

    // Cap coupling window at high speed to keep render cost bounded.
    const float invW = 1.0f / (W + 1e-6f);
    static float kernel[2 * kMaxWindowRadius + 1];
    for (int dj = -iW; dj <= iW; dj++) {
        const float t = fabsf(static_cast<float>(dj)) * invW;
        kernel[dj + kMaxWindowRadius] = 0.5f + 0.5f * cosf(kPi * t);
    }

    // Precompute sin/cos(theta) once for both Kuramoto passes.
    static float thetaSin[STRIP_LENGTH];
    static float thetaCos[STRIP_LENGTH];
    for (int i = 0; i < STRIP_LENGTH; i++) {
        thetaSin[i] = sinf(m_ps->theta[i]);
        thetaCos[i] = cosf(m_ps->theta[i]);
    }

    // Pass 1: compute local order parameter Rlocal via windowed cosine kernel
#ifndef NATIVE_BUILD
    for (int i = 0; i < STRIP_LENGTH; i++) {
        float csum = 0.0f, ssum = 0.0f, wsum = 0.0f;
        for (int dj = -iW; dj <= iW; dj++) {
            int j = i + dj;
            if (j < 0) j += STRIP_LENGTH;
            if (j >= STRIP_LENGTH) j -= STRIP_LENGTH;
            const float w = kernel[dj + kMaxWindowRadius];
            csum += w * thetaCos[j];
            ssum += w * thetaSin[j];
            wsum += w;
        }
        const float inv = 1.0f / (wsum + 1e-6f);
        const float c = csum * inv;
        const float s = ssum * inv;
        m_ps->Rlocal[i] = clamp01(sqrtf(c * c + s * s));
    }

    // Pass 2: update theta via Kuramoto coupling
    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float thetaShift = m_ps->theta[i] + alpha;
        const float sinShift = sinf(thetaShift);
        const float cosShift = cosf(thetaShift);
        float sum = 0.0f, wsum = 0.0f;
        for (int dj = -iW; dj <= iW; dj++) {
            int j = i + dj;
            if (j < 0) j += STRIP_LENGTH;
            if (j >= STRIP_LENGTH) j -= STRIP_LENGTH;
            const float w = kernel[dj + kMaxWindowRadius];
            sum += w * (thetaSin[j] * cosShift - thetaCos[j] * sinShift);
            wsum += w;
        }
        const float coupling = (K / (wsum + 1e-6f)) * sum;
        m_ps->theta[i] += (m_ps->omega[i] + coupling) * dtK;
        if (m_ps->theta[i] > kTwoPi) m_ps->theta[i] -= kTwoPi;
        if (m_ps->theta[i] < 0.0f) m_ps->theta[i] += kTwoPi;
    }
#endif

    m_t += dtK;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

#ifndef NATIVE_BUILD
    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float R = m_ps->Rlocal[i];
        const float glue = centreGlue(i);

        // Crown: smoothstep of R, power for extra punch
        float crown = smoothstep(0.55f, 0.92f, R);
        crown = powf(crown, 1.35f);

        // Bed: sin-based base pattern
        float bed = 0.30f + 0.70f * (0.5f + 0.5f * sinf(m_ps->theta[i]));
        bed = powf(bed, 1.15f) * 0.55f;

        // Grain: hash-based texture (stronger in unsynced regions)
        const uint32_t h = hash32(static_cast<uint32_t>(i) * 2654435761u ^ static_cast<uint32_t>(m_t * 1000.0f));
        const float n = static_cast<float>(h & 0xFFu) / 255.0f;
        const float grain = (1.0f - smoothstep(0.35f, 0.75f, R)) * (n - 0.5f) * 0.10f;

        // Geometry: bed + crown + grain, modulated by glue
        float geometry = (bed + crown) * glue + grain;

        // Impact: additive spike on crown peaks
        float impactAdd = m_impact * crown * glue * 0.35f;

        // Compose brightness: geometry x normBass x silScale x beatMod
        float brightness = (geometry * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal hue: chord-driven anchor + crown-driven shift
        float hueShift = lerpf(18.0f, 55.0f, crown)
                       - lerpf(0.0f, 14.0f, 1.0f - R);
        uint8_t hue = baseHue + static_cast<uint8_t>(hueShift);

        writeDualLocked(ctx, i, CHSV(hue, ctx.saturation, val));
    }
#endif

    lightwaveos::effects::cinema::apply(ctx, speedNorm, lightwaveos::effects::cinema::QualityPreset::LIGHTWEIGHT);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPChimeraCrownAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPChimeraCrownAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chimera Crown (5L-AR)",
        "Kuramoto sync + fracture dynamics with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPChimeraCrownAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPChimeraCrownAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPChimeraCrownAREffect::setParameter(const char*, float) { return false; }
float LGPChimeraCrownAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
