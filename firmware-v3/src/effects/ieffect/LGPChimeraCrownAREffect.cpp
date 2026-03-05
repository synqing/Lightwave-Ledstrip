/**
 * @file LGPChimeraCrownAREffect.cpp
 * @brief Chimera Crown (5-Layer Audio-Reactive)
 *
 * Kuramoto-ish oscillator coupling on 160 elements with full 5-layer AR composition.
 * Base maths from LGPChimeraCrownEffect, restructured into explicit bed/structure/impact/tonal/memory layers.
 *
 * Physics: Two-pass Kuramoto model
 *   Pass 1: compute local order parameter Rlocal[i] via windowed cosine kernel
 *   Pass 2: update theta[i] via Kuramoto coupling + intrinsic omega[i]
 *
 * Audio mapping:
 *   Bed       - RMS-driven crown brightness base (tau 0.45s)
 *   Structure - coupling K + window W from bass + flux (tau 0.15s) → sync strength
 *   Impact    - beat-triggered crown spike (decay 0.22s)
 *   Memory    - post-impact sync persistence (decay 0.90s)
 *   Tonal     - chord hue anchor
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
// Local helpers (shared with HolyShitBangersPack pattern)
// =========================================================================

static constexpr float kPi  = 3.14159265358979323846f;
static constexpr float kTau = 6.28318530717958647692f;

static inline float fract(float x) { return x - floorf(x); }

static inline float smoothstep(float a, float b, float x) {
    const float t = lowrisk_ar::clamp01f((x - a) / (b - a));
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

static inline uint8_t luminanceToBr(float wave01, float master) {
    const float base = 0.055f;
    float out = lowrisk_ar::clamp01f(base + (1.0f - base) * lowrisk_ar::clamp01f(wave01));
    out = out / (1.0f + 0.60f * out);  // filmic shoulder
    out *= master;
    return static_cast<uint8_t>(lowrisk_ar::clampf(out * 255.0f, 0.0f, 255.0f));
}

// =========================================================================
// Construction / init / cleanup
// =========================================================================

LGPChimeraCrownAREffect::LGPChimeraCrownAREffect()
    : m_ps(nullptr)
    , m_t(0.0f)
    , m_bed(0.3f)
    , m_structure(0.5f)
    , m_impact(0.0f)
    , m_memory(0.0f)
{
}

bool LGPChimeraCrownAREffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_ar.modeSmooth = 2.0f;
    m_t           = 0.0f;
    m_bed         = 0.3f;
    m_structure   = 0.5f;
    m_impact      = 0.0f;
    m_memory      = 0.0f;

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
        m_ps->theta[i] = u * kTau;

        uint32_t h2 = hash32(h ^ 0xA5A5A5A5u);
        float v = (static_cast<float>(static_cast<int>(h2 & 0xFFFFu) - 32768) / 32768.0f);
        m_ps->omega[i] = 0.20f * v;
        m_ps->Rlocal[i] = 0.0f;
    }
#endif

    return true;
}

// =========================================================================
// render() — 5-Layer composition
// =========================================================================

void LGPChimeraCrownAREffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif

    // --- Timing ---
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    const float speedNorm = ctx.speed / 50.0f;

    // --- Signals (shared infrastructure handles presence gate + fallback) ---
    const lowrisk_ar::ArSignalSnapshot sig =
        lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);
    const lowrisk_ar::ArModulationProfile mod =
        lowrisk_ar::buildModulation(m_controls, sig, m_ar, ctx);
    m_ar.tonalHue = mod.baseHue;

    // =================================================================
    // LAYER UPDATES
    // =================================================================

    // Bed: RMS-driven crown brightness (tau 0.45s)
    const float bedTarget = lowrisk_ar::clamp01f(0.25f + 0.75f * sig.rms);
    m_bed = lowrisk_ar::smoothTo(m_bed, bedTarget, dtSignal, 0.45f);

    // Structure: coupling strength + window width from bass + flux (tau 0.15s)
    const float structTarget = lowrisk_ar::clamp01f(
        0.35f + 0.35f * sig.bass + 0.20f * sig.flux + 0.10f * sig.rhythmic);
    m_structure = lowrisk_ar::smoothTo(m_structure, structTarget, dtSignal, 0.15f);

    // Impact: beat-triggered crown spike (decay 0.22s)
    if (sig.beatTick) m_impact = fmaxf(m_impact, 0.90f);
    m_impact = lowrisk_ar::decay(m_impact, dtSignal, 0.22f);

    // Memory: accumulated sync persistence (slow decay, fed by beat + flux)
    m_memory = lowrisk_ar::clamp01f(
        lowrisk_ar::decay(m_memory, dtSignal, 0.90f)
        + 0.12f * m_impact + 0.06f * sig.flux);
    lowrisk_ar::applyBedImpactMemoryMix(
        m_controls,
        static_cast<float>(ctx.rawTotalTimeMs),
        m_bed,
        m_impact,
        m_memory);

    // =================================================================
    // KURAMOTO PHYSICS
    // =================================================================

    // Structure controls coupling strength K and window width W
    const float baseW = 10.0f + 10.0f * speedNorm;
    const float W = baseW * (0.65f + 0.35f * m_structure);
    static constexpr int kMaxWindowRadius = 24;
    int iW = static_cast<int>(W);
    if (iW < 2) iW = 2;
    if (iW > kMaxWindowRadius) iW = kMaxWindowRadius;

    const float baseK = 1.4f + 1.2f * speedNorm;
    const float K = baseK * (0.70f + 0.50f * m_structure);

    const float alpha = 0.55f;
    const float dt = (0.035f + 0.030f * speedNorm) * mod.motionRate;

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
        m_ps->Rlocal[i] = lowrisk_ar::clamp01f(sqrtf(c * c + s * s));
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
        m_ps->theta[i] += (m_ps->omega[i] + coupling) * dt;
        if (m_ps->theta[i] > kTau) m_ps->theta[i] -= kTau;
        if (m_ps->theta[i] < 0.0f) m_ps->theta[i] += kTau;
    }
#endif

    m_t += dt;

    // =================================================================
    // PER-PIXEL RENDER
    // =================================================================

    const float bedBright = 0.25f + 0.75f * m_bed;
    const float master = (ctx.brightness / 255.0f)
                         * lowrisk_ar::clamp01f(m_ar.audioPresence) * mod.brightnessScale;

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

        // Bed x structured geometry
        float structuredGeom = (bed + crown) * glue + grain;

        // Impact: additive spike on crown peaks
        const float impactAdd = m_impact * crown * glue * 0.35f;

        // Memory: gentle additive glow
        const float memoryAdd = m_memory * (0.5f + 0.5f * crown) * 0.18f;

        // Compose: bed x structure + impact + memory
        float brightness = bedBright * structuredGeom + impactAdd + memoryAdd;
        brightness = lowrisk_ar::clamp01f(brightness);

        const uint8_t br = luminanceToBr(brightness, master);

        // Tonal hue: chord-driven anchor + crown-driven shift
        const float hueShift = lowrisk_ar::lerpf(18.0f, 55.0f, crown)
                             - lowrisk_ar::lerpf(0.0f, 14.0f, 1.0f - R)
                             + sig.harmonic * 20.0f;
        const uint8_t hue = static_cast<uint8_t>(m_ar.tonalHue + hueShift);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
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
        "Kuramoto sync + fracture dynamics with 5-layer audio-reactive composition",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPChimeraCrownAREffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPChimeraCrownAREffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPChimeraCrownAREffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPChimeraCrownAREffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
