/**
 * @file LGPIFSBioRelicAREffect.cpp
 * @brief IFS Botanical Relic (5-Layer AR) -- REWRITTEN
 *
 * Barnsley fern IFS with histogram projection and direct audio-reactive
 * composition. Physics: 4-transform Barnsley IFS (probabilities
 * 0.01/0.85/0.07/0.07). Points mirrored for centre-origin symmetry,
 * projected onto 160-LED histogram.
 *
 * Divergence fixes: Direct ControlBus reads, single-stage smoothing (50ms),
 * asymmetric max follower (58ms/500ms, floor 0.04), no brightness floors,
 * beatStrength() continuous, squared brightness for punch.
 *
 * Centre-origin compliant. Dual-strip locked.
 */

#include "LGPIFSBioRelicAREffect.h"
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

static inline uint32_t lcg_next(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

static inline float distN_from_index(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    return fabsf((float)i - mid) / mid;
}

static inline float centreGlue(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float d = (float)i - mid;
    return 0.30f + 0.70f * expf(-(d * d) * 0.0016f);
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPIFSBioRelicAREffect::LGPIFSBioRelicAREffect()
    : m_ps(nullptr)
    , m_px(0.0f), m_py(0.0f), m_t(0.0f), m_rng(0xBADC0DEu)
    , m_bass(0.0f), m_treble(0.0f), m_chromaAngle(0.0f)
    , m_bassMax(0.15f), m_trebleMax(0.15f)
    , m_impact(0.0f)
{
}

bool LGPIFSBioRelicAREffect::init(plugins::EffectContext& ctx) {
    m_px   = 0.0f;
    m_py   = 0.0f;
    m_t    = 0.0f;
    m_rng  = 0xBADC0DEu;
    m_bass = 0.0f; m_treble = 0.0f; m_chromaAngle = 0.0f;
    m_bassMax = 0.15f; m_trebleMax = 0.15f;
    m_impact = 0.0f;

    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<IFSPsram*>(heap_caps_malloc(sizeof(IFSPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPIFSBioRelicAREffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(IFSPsram));
#endif

    return true;
}

// =========================================================================
// render() -- direct audio-reactive composition
// =========================================================================

void LGPIFSBioRelicAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float dtVis = ctx.getSafeDeltaSeconds();
    (void)dtVis;
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

    // Beat modulation
    const float beatMod = 0.3f + 0.7f * beatStr;

    // =================================================================
    // IFS BARNSLEY FERN PHYSICS
    // =================================================================

    m_t += (0.010f + 0.020f * speedNorm) * (0.7f + 0.6f * normBass);

    // Histogram decay rate: normTreble modulates persistence
    float decay = (0.92f + 0.06f * (1.0f - speedNorm))
                  * (0.90f + 0.10f * (1.0f - normTreble));

#ifndef NATIVE_BUILD
    for (int i = 0; i < STRIP_LENGTH; i++) m_ps->hist[i] *= decay;

    // Points per frame: bass + speed drive iteration count
    int P = 220 + (int)(520.0f * speedNorm * (0.6f + 0.6f * normBass));

    // Barnsley-style IFS (classic fern family), mirrored for centre-origin
    for (int k = 0; k < P; k++) {
        uint32_t r = lcg_next(m_rng);
        float u = (float)(r & 0xFFFFu) / 65535.0f;

        float x = m_px, y = m_py;
        float nx, ny;

        if (u < 0.01f) {
            nx = 0.0f;
            ny = 0.16f * y;
        } else if (u < 0.86f) {
            nx = 0.85f * x + 0.04f * y;
            ny = -0.04f * x + 0.85f * y + 1.6f;
        } else if (u < 0.93f) {
            nx = 0.20f * x - 0.26f * y;
            ny = 0.23f * x + 0.22f * y + 1.6f;
        } else {
            nx = -0.15f * x + 0.28f * y;
            ny = 0.26f * x + 0.24f * y + 0.44f;
        }

        m_px = nx;
        m_py = ny;

        // Mirror x for centre-origin symmetry
        float ax = fabsf(nx);
        float yy = ny;

        float yN = clamp01(yy / 10.0f);
        float xN = clamp01(ax / 3.0f);

        float radial = clamp01(0.10f + 0.78f * (0.70f * yN + 0.30f * xN));

        int bin = (int)lroundf(radial * (STRIP_LENGTH - 1));
        if (bin >= 0 && bin < STRIP_LENGTH) {
            float pulse = 0.85f + 0.15f * sinf(m_t * 0.7f + radial * 6.0f);
            // Impact adds a brightness burst to the IFS scatter
            float impactBoost = 1.0f + 0.5f * m_impact;
            m_ps->hist[bin] += 0.80f * pulse * impactBoost;
        }
    }

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    float maxH = 1e-6f;
    for (int i = 0; i < STRIP_LENGTH; i++) maxH = fmaxf(maxH, m_ps->hist[i]);
    float inv = 1.0f / maxH;

    // Hue from chroma
    uint8_t baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / kTwoPi)) + ctx.gHue;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);
        float glue = centreGlue(i);

        int bin = (int)lroundf(dn * (STRIP_LENGTH - 1));
        bin = (bin < 0) ? 0 : (bin >= STRIP_LENGTH) ? STRIP_LENGTH - 1 : bin;

        float v = clamp01(m_ps->hist[bin] * inv);
        float veins = powf(v, 1.65f);
        float spec  = powf(veins, 2.2f) * 0.45f;

        // Vein geometry x centre glue
        float veinGeom = clamp01((0.18f + 0.82f * veins + spec) * glue);

        // Impact: additive vein pulse
        float impactAdd = m_impact * veins * glue * 0.25f;

        // Compose: geometry * normBass * silScale * beatMod
        float brightness = (veinGeom * normBass + impactAdd) * beatMod * silScale;

        // Squared for punch
        brightness *= brightness;

        uint8_t val = static_cast<uint8_t>(clamp01(brightness) * 255.0f);
        val = scale8(val, ctx.brightness);

        // Tonal: museum relic hue with chroma anchor
        float hueShift = 10.0f * dn + 35.0f * spec;
        uint8_t hue = baseHue + static_cast<uint8_t>(hueShift);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, val));
    }
#endif

    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}

// =========================================================================
// cleanup / metadata / parameters
// =========================================================================

void LGPIFSBioRelicAREffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPIFSBioRelicAREffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP IFS Bio Relic (5L-AR)",
        "Barnsley fern botanical relic with direct audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPIFSBioRelicAREffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* LGPIFSBioRelicAREffect::getParameter(uint8_t) const { return nullptr; }
bool LGPIFSBioRelicAREffect::setParameter(const char*, float) { return false; }
float LGPIFSBioRelicAREffect::getParameter(const char*) const { return 0.0f; }

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
