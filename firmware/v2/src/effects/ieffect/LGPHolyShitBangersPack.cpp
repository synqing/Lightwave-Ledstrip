#include "LGPHolyShitBangersPack.h"
#include "LGPFilmPost.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif
#include "../../utils/Log.h"


// AUTO_TUNABLES_BULK_BEGIN:LGPHyperbolicPortalEffect
namespace {
constexpr float kLGPHyperbolicPortalEffectSpeedScale = 1.0f;
constexpr float kLGPHyperbolicPortalEffectOutputGain = 1.0f;
constexpr float kLGPHyperbolicPortalEffectCentreBias = 1.0f;

float gLGPHyperbolicPortalEffectSpeedScale = kLGPHyperbolicPortalEffectSpeedScale;
float gLGPHyperbolicPortalEffectOutputGain = kLGPHyperbolicPortalEffectOutputGain;
float gLGPHyperbolicPortalEffectCentreBias = kLGPHyperbolicPortalEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPHyperbolicPortalEffectParameters[] = {
    {"lgphyperbolic_portal_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPHyperbolicPortalEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgphyperbolic_portal_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPHyperbolicPortalEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgphyperbolic_portal_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPHyperbolicPortalEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPHyperbolicPortalEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPLorenzRibbonEffect
namespace {
constexpr float kLGPLorenzRibbonEffectSpeedScale = 1.0f;
constexpr float kLGPLorenzRibbonEffectOutputGain = 1.0f;
constexpr float kLGPLorenzRibbonEffectCentreBias = 1.0f;

float gLGPLorenzRibbonEffectSpeedScale = kLGPLorenzRibbonEffectSpeedScale;
float gLGPLorenzRibbonEffectOutputGain = kLGPLorenzRibbonEffectOutputGain;
float gLGPLorenzRibbonEffectCentreBias = kLGPLorenzRibbonEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPLorenzRibbonEffectParameters[] = {
    {"lgplorenz_ribbon_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPLorenzRibbonEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgplorenz_ribbon_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPLorenzRibbonEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgplorenz_ribbon_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPLorenzRibbonEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPLorenzRibbonEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPCatastropheCausticsEffect
namespace {
constexpr float kLGPCatastropheCausticsEffectSpeedScale = 1.0f;
constexpr float kLGPCatastropheCausticsEffectOutputGain = 1.0f;
constexpr float kLGPCatastropheCausticsEffectCentreBias = 1.0f;

float gLGPCatastropheCausticsEffectSpeedScale = kLGPCatastropheCausticsEffectSpeedScale;
float gLGPCatastropheCausticsEffectOutputGain = kLGPCatastropheCausticsEffectOutputGain;
float gLGPCatastropheCausticsEffectCentreBias = kLGPCatastropheCausticsEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPCatastropheCausticsEffectParameters[] = {
    {"lgpcatastrophe_caustics_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPCatastropheCausticsEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpcatastrophe_caustics_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPCatastropheCausticsEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpcatastrophe_caustics_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPCatastropheCausticsEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPCatastropheCausticsEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPIFSBioRelicEffect
namespace {
constexpr float kLGPIFSBioRelicEffectSpeedScale = 1.0f;
constexpr float kLGPIFSBioRelicEffectOutputGain = 1.0f;
constexpr float kLGPIFSBioRelicEffectCentreBias = 1.0f;

float gLGPIFSBioRelicEffectSpeedScale = kLGPIFSBioRelicEffectSpeedScale;
float gLGPIFSBioRelicEffectOutputGain = kLGPIFSBioRelicEffectOutputGain;
float gLGPIFSBioRelicEffectCentreBias = kLGPIFSBioRelicEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPIFSBioRelicEffectParameters[] = {
    {"lgpifsbio_relic_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPIFSBioRelicEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpifsbio_relic_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPIFSBioRelicEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpifsbio_relic_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPIFSBioRelicEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPIFSBioRelicEffect


// AUTO_TUNABLES_BULK_BEGIN:LGPChimeraCrownEffect
namespace {
constexpr float kLGPChimeraCrownEffectSpeedScale = 1.0f;
constexpr float kLGPChimeraCrownEffectOutputGain = 1.0f;
constexpr float kLGPChimeraCrownEffectCentreBias = 1.0f;

float gLGPChimeraCrownEffectSpeedScale = kLGPChimeraCrownEffectSpeedScale;
float gLGPChimeraCrownEffectOutputGain = kLGPChimeraCrownEffectOutputGain;
float gLGPChimeraCrownEffectCentreBias = kLGPChimeraCrownEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPChimeraCrownEffectParameters[] = {
    {"lgpchimera_crown_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPChimeraCrownEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpchimera_crown_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPChimeraCrownEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpchimera_crown_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPChimeraCrownEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPChimeraCrownEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float HS_PI  = 3.14159265358979323846f;
static constexpr float HS_TAU = 6.28318530717958647692f;

static inline float clamp01(float x) { return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x; }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstep(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
static inline float fract(float x) { return x - floorf(x); }
static inline float gaussian(float x, float sigma) {
    float s2 = sigma * sigma;
    return expf(-(x * x) / (2.0f * s2));
}
static inline float artanh_safe(float r) {
    // 0.5*ln((1+r)/(1-r)) with clamp near 1
    r = std::clamp(r, 0.0f, 0.9995f);
    return 0.5f * logf((1.0f + r) / (1.0f - r));
}
static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static inline uint32_t lcg_next(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

// Premium "centre glue": keeps the middle welded into the wings.
static inline float centreGlue(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    float d = (float)i - mid;
    return 0.30f + 0.70f * expf(-(d * d) * 0.0016f);
}

// Dual-strip LOCK. No wing rivalry, ever.
static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

// Luminance shaping: "velvet blacks + glass highlights" without harsh clipping.
static inline uint8_t waveToBr(float w01, float master) {
    const float base = 0.055f;
    float out = clamp01(base + (1.0f - base) * clamp01(w01));
    // mild filmic shoulder
    out = out / (1.0f + 0.60f * out);
    out *= master;
    return (uint8_t)std::clamp((int)lroundf(out * 255.0f), 0, 255);
}

static inline float distN_from_index(int i) {
    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;
    return fabsf((float)i - mid) * invMid; // 0..1
}

// -----------------------------------------------------------------------------
// 1) Chimera Crown
// -----------------------------------------------------------------------------
LGPChimeraCrownEffect::LGPChimeraCrownEffect() : m_t(0.0f) {}

bool LGPChimeraCrownEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPChimeraCrownEffect
    gLGPChimeraCrownEffectSpeedScale = kLGPChimeraCrownEffectSpeedScale;
    gLGPChimeraCrownEffectOutputGain = kLGPChimeraCrownEffectOutputGain;
    gLGPChimeraCrownEffectCentreBias = kLGPChimeraCrownEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPChimeraCrownEffect

    m_t = 0.0f;
    lightwaveos::effects::cinema::reset();

#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<ChimeraPsram*>(heap_caps_malloc(sizeof(ChimeraPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPChimeraCrownEffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(ChimeraPsram));
#else
    (void)m_ps;
    return true;
#endif

    // Deterministic "random" initial phases/frequencies (no rand()).
    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint32_t h = hash32(0xC0FFEEu ^ (uint32_t)i * 2654435761u);
        float u = (float)(h & 0xFFFFu) / 65535.0f;
        m_ps->theta[i] = u * HS_TAU;

        uint32_t h2 = hash32(h ^ 0xA5A5A5A5u);
        float v = ((float)((int)(h2 & 0xFFFFu) - 32768) / 32768.0f);
        m_ps->omega[i] = 0.20f * v;
        m_ps->Rlocal[i] = 0.0f;
    }
    return true;
}

void LGPChimeraCrownEffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    const int   W = 10 + (int)(10.0f * speedNorm);
    const float K = 1.4f + 1.2f * speedNorm;
    const float alpha = 0.55f;
    const float dt = 0.035f + 0.030f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float csum = 0.0f, ssum = 0.0f, wsum = 0.0f;
        for (int dj = -W; dj <= W; dj++) {
            int j = i + dj;
            if (j < 0) j += STRIP_LENGTH;
            if (j >= STRIP_LENGTH) j -= STRIP_LENGTH;
            float t = fabsf((float)dj) / (float)W;
            float w = 0.5f + 0.5f * cosf(HS_PI * t);
            csum += w * cosf(m_ps->theta[j]);
            ssum += w * sinf(m_ps->theta[j]);
            wsum += w;
        }
        float inv = 1.0f / (wsum + 1e-6f);
        float c = csum * inv;
        float s = ssum * inv;
        m_ps->Rlocal[i] = clamp01(sqrtf(c * c + s * s));
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float sum = 0.0f, wsum = 0.0f;
        for (int dj = -W; dj <= W; dj++) {
            int j = i + dj;
            if (j < 0) j += STRIP_LENGTH;
            if (j >= STRIP_LENGTH) j -= STRIP_LENGTH;
            float t = fabsf((float)dj) / (float)W;
            float w = 0.5f + 0.5f * cosf(HS_PI * t);
            sum += w * sinf(m_ps->theta[j] - m_ps->theta[i] - alpha);
            wsum += w;
        }
        float coupling = (K / (wsum + 1e-6f)) * sum;
        m_ps->theta[i] += (m_ps->omega[i] + coupling) * dt;
        if (m_ps->theta[i] > HS_TAU) m_ps->theta[i] -= HS_TAU;
        if (m_ps->theta[i] < 0.0f) m_ps->theta[i] += HS_TAU;
    }

    m_t += dt;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float R = m_ps->Rlocal[i];
        float glue = centreGlue(i);
        float crown = smoothstep(0.55f, 0.92f, R);
        crown = powf(crown, 1.35f);
        float bed = 0.30f + 0.70f * (0.5f + 0.5f * sinf(m_ps->theta[i]));
        bed = powf(bed, 1.15f) * 0.55f;
        uint32_t h = hash32((uint32_t)i * 2654435761u ^ (uint32_t)(m_t * 1000.0f));
        float n = (float)(h & 0xFFu) / 255.0f;
        float grain = (1.0f - smoothstep(0.35f, 0.75f, R)) * (n - 0.5f) * 0.10f;
        float wave = clamp01((bed + crown) * glue + grain);
        uint8_t br = waveToBr(wave, master);
        float hueShift = lerp(18.0f, 55.0f, crown) - lerp(0.0f, 14.0f, 1.0f - R);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)hueShift);
        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPChimeraCrownEffect
uint8_t LGPChimeraCrownEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPChimeraCrownEffectParameters) / sizeof(kLGPChimeraCrownEffectParameters[0]));
}

const plugins::EffectParameter* LGPChimeraCrownEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPChimeraCrownEffectParameters[index];
}

bool LGPChimeraCrownEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpchimera_crown_effect_speed_scale") == 0) {
        gLGPChimeraCrownEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpchimera_crown_effect_output_gain") == 0) {
        gLGPChimeraCrownEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpchimera_crown_effect_centre_bias") == 0) {
        gLGPChimeraCrownEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPChimeraCrownEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpchimera_crown_effect_speed_scale") == 0) return gLGPChimeraCrownEffectSpeedScale;
    if (strcmp(name, "lgpchimera_crown_effect_output_gain") == 0) return gLGPChimeraCrownEffectOutputGain;
    if (strcmp(name, "lgpchimera_crown_effect_centre_bias") == 0) return gLGPChimeraCrownEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPChimeraCrownEffect

void LGPChimeraCrownEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
#endif
}

const plugins::EffectMetadata& LGPChimeraCrownEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Chimera Crown",
        "Coherent + incoherent domains (sync fracture line)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// -----------------------------------------------------------------------------
// 2) Catastrophe Caustics
// -----------------------------------------------------------------------------
LGPCatastropheCausticsEffect::LGPCatastropheCausticsEffect() : m_t(0.0f) {}

bool LGPCatastropheCausticsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPCatastropheCausticsEffect
    gLGPCatastropheCausticsEffectSpeedScale = kLGPCatastropheCausticsEffectSpeedScale;
    gLGPCatastropheCausticsEffectOutputGain = kLGPCatastropheCausticsEffectOutputGain;
    gLGPCatastropheCausticsEffectCentreBias = kLGPCatastropheCausticsEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPCatastropheCausticsEffect

    m_t = 0.0f;
    lightwaveos::effects::cinema::reset();
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<CausticsPsram*>(heap_caps_malloc(sizeof(CausticsPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPCatastropheCausticsEffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(CausticsPsram));
#else
    return true;
#endif
    return true;
}

void LGPCatastropheCausticsEffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.012f + 0.040f * speedNorm);

    const float decay = 0.88f + 0.08f * (1.0f - speedNorm);
    for (int i = 0; i < STRIP_LENGTH; i++) m_ps->I[i] *= decay;

    // Focus pull controls caustic tightness
    float z = 6.0f + 22.0f * (0.5f + 0.5f * sinf(m_t * 0.35f));
    float strength = 1.6f + 1.4f * speedNorm;

    // Lens "thickness field" h(x,t): slow warp + centre bias
    auto hfield = [&](float x) -> float {
        float xn = x / (float)(STRIP_LENGTH - 1);   // 0..1
        float c = xn - 0.5f;
        float centre = expf(-(c * c) * 9.0f);
        float h = 0.55f * sinf(HS_TAU * (xn * 1.1f) + m_t * 0.7f)
                + 0.35f * sinf(HS_TAU * (xn * 2.3f) - m_t * 0.4f)
                + 0.20f * sinf(HS_TAU * (xn * 3.7f) + m_t * 0.25f);
        return h * (0.35f + 0.65f * centre);
    };

    // Ray histogram: rays start at x, deflect by dh/dx, land at X = x + z * a(x)
    for (int s = 0; s < STRIP_LENGTH; s++) {
        float x = (float)s;

        float hm1 = hfield((s == 0) ? 0.0f : (float)(s - 1));
        float hp1 = hfield((s == STRIP_LENGTH - 1) ? (float)(STRIP_LENGTH - 1) : (float)(s + 1));
        float dhdx = 0.5f * (hp1 - hm1);

        float a = strength * dhdx;               // deflection angle proxy
        float X = x + z * a;

        int bin = (int)lroundf(X);
        if (bin >= 0 && bin < STRIP_LENGTH) {
            float glue = centreGlue(bin);
            m_ps->I[bin] += 0.90f + 0.45f * glue;
        }
    }

    float maxI = 1e-6f;
    for (int i = 0; i < STRIP_LENGTH; i++) maxI = std::max(maxI, m_ps->I[i]);
    float invMax = 1.0f / maxI;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float glue = centreGlue(i);
        float v = m_ps->I[i] * invMax;
        float fil = powf(v, 1.85f);
        int im1 = (i == 0) ? 0 : (i - 1);
        int ip1 = (i == STRIP_LENGTH - 1) ? (STRIP_LENGTH - 1) : (i + 1);
        float lap = m_ps->I[im1] - 2.0f * m_ps->I[i] + m_ps->I[ip1];
        float cusp = clamp01(fabsf(lap) * invMax * 2.4f);

        float wave = clamp01((0.18f + 0.82f * fil + 0.25f * cusp) * glue);
        uint8_t br = waveToBr(wave, master);

        // Colourist rule: caustics are luminance; hue drift is tiny and "refractive"
        float hueDrift = 14.0f * (v - 0.5f) + 10.0f * cusp;
        uint8_t hue = (uint8_t)(ctx.gHue + (int)hueDrift);

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPCatastropheCausticsEffect
uint8_t LGPCatastropheCausticsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPCatastropheCausticsEffectParameters) / sizeof(kLGPCatastropheCausticsEffectParameters[0]));
}

const plugins::EffectParameter* LGPCatastropheCausticsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPCatastropheCausticsEffectParameters[index];
}

bool LGPCatastropheCausticsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpcatastrophe_caustics_effect_speed_scale") == 0) {
        gLGPCatastropheCausticsEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpcatastrophe_caustics_effect_output_gain") == 0) {
        gLGPCatastropheCausticsEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpcatastrophe_caustics_effect_centre_bias") == 0) {
        gLGPCatastropheCausticsEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPCatastropheCausticsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpcatastrophe_caustics_effect_speed_scale") == 0) return gLGPCatastropheCausticsEffectSpeedScale;
    if (strcmp(name, "lgpcatastrophe_caustics_effect_output_gain") == 0) return gLGPCatastropheCausticsEffectOutputGain;
    if (strcmp(name, "lgpcatastrophe_caustics_effect_centre_bias") == 0) return gLGPCatastropheCausticsEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPCatastropheCausticsEffect

void LGPCatastropheCausticsEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
#endif
}

const plugins::EffectMetadata& LGPCatastropheCausticsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Catastrophe Caustics",
        "Ray-envelope filaments (focus pull + cusp spark)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// -----------------------------------------------------------------------------
// 3) Hyperbolic Portal
// -----------------------------------------------------------------------------
LGPHyperbolicPortalEffect::LGPHyperbolicPortalEffect() : m_t(0.0f) {}
bool LGPHyperbolicPortalEffect::init(plugins::EffectContext& ctx) { (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPHyperbolicPortalEffect
    gLGPHyperbolicPortalEffectSpeedScale = kLGPHyperbolicPortalEffectSpeedScale;
    gLGPHyperbolicPortalEffectOutputGain = kLGPHyperbolicPortalEffectOutputGain;
    gLGPHyperbolicPortalEffectCentreBias = kLGPHyperbolicPortalEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPHyperbolicPortalEffect
 m_t = 0.0f; lightwaveos::effects::cinema::reset(); return true; }

void LGPHyperbolicPortalEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.030f * speedNorm);

    // Phase rotation = "portal turning in depth"
    float phi = m_t * (0.55f + 0.55f * speedNorm);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);             // 0..1 Euclidean radius
        float glue = centreGlue(i);

        // Hyperbolic coordinate blows up near edge: u = artanh(r)
        float r = dn * 0.985f;
        float u = artanh_safe(r);

        // Multi-band hyperbolic ribs (density rises toward edges automatically)
        float w1 = 3.8f;
        float w2 = 7.6f;
        float p  = sinf(w1 * u + phi) + 0.62f * sinf(w2 * u - 0.7f * phi);

        // "Tiling ribs": turn sine into sharp-ish bands
        float ribs = powf(smoothstep(0.15f, 1.0f, fabsf(p)), 1.35f);

        // Centre calm; edge intricate (but still glued)
        float edgeBoost = smoothstep(0.15f, 1.0f, dn);
        float wave = clamp01((0.22f + 0.78f * ribs) * (0.70f + 0.30f * edgeBoost) * glue);

        uint8_t br = waveToBr(wave, master);

        // Colour: largely fixed; minor shift with hyperbolic depth so it feels "volumetric"
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(12.0f * edgeBoost) + (int)(18.0f * ribs));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPHyperbolicPortalEffect
uint8_t LGPHyperbolicPortalEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPHyperbolicPortalEffectParameters) / sizeof(kLGPHyperbolicPortalEffectParameters[0]));
}

const plugins::EffectParameter* LGPHyperbolicPortalEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPHyperbolicPortalEffectParameters[index];
}

bool LGPHyperbolicPortalEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgphyperbolic_portal_effect_speed_scale") == 0) {
        gLGPHyperbolicPortalEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgphyperbolic_portal_effect_output_gain") == 0) {
        gLGPHyperbolicPortalEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgphyperbolic_portal_effect_centre_bias") == 0) {
        gLGPHyperbolicPortalEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPHyperbolicPortalEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgphyperbolic_portal_effect_speed_scale") == 0) return gLGPHyperbolicPortalEffectSpeedScale;
    if (strcmp(name, "lgphyperbolic_portal_effect_output_gain") == 0) return gLGPHyperbolicPortalEffectOutputGain;
    if (strcmp(name, "lgphyperbolic_portal_effect_centre_bias") == 0) return gLGPHyperbolicPortalEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPHyperbolicPortalEffect

void LGPHyperbolicPortalEffect::cleanup() {}

const plugins::EffectMetadata& LGPHyperbolicPortalEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Hyperbolic Portal",
        "Edge densification via atanh(r) (Poincaré vibe)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// -----------------------------------------------------------------------------
// 4) Lorenz Ribbon
// -----------------------------------------------------------------------------
LGPLorenzRibbonEffect::LGPLorenzRibbonEffect()
    : m_x(0.1f), m_y(0.0f), m_z(0.0f), m_head(0), m_t(0.0f)
{
}

bool LGPLorenzRibbonEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPLorenzRibbonEffect
    gLGPLorenzRibbonEffectSpeedScale = kLGPLorenzRibbonEffectSpeedScale;
    gLGPLorenzRibbonEffectOutputGain = kLGPLorenzRibbonEffectOutputGain;
    gLGPLorenzRibbonEffectCentreBias = kLGPLorenzRibbonEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPLorenzRibbonEffect

    m_x = 0.1f; m_y = 0.0f; m_z = 0.0f;
    m_head = 0;
    m_t = 0.0f;
    lightwaveos::effects::cinema::reset();
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<LorenzPsram*>(heap_caps_malloc(sizeof(LorenzPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPLorenzRibbonEffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(LorenzPsram));
#else
    return true;
#endif
    return true;
}

void LGPLorenzRibbonEffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    const float sigma = 10.0f;
    const float rho   = 28.0f;
    const float beta  = 8.0f / 3.0f;
    float dt = 0.0065f + 0.010f * speedNorm;
    int sub = 2 + (int)(4.0f * speedNorm);
    for (int s = 0; s < sub; s++) {
        float dx = sigma * (m_y - m_x);
        float dy = m_x * (rho - m_z) - m_y;
        float dz = m_x * m_y - beta * m_z;

        m_x += dx * dt;
        m_y += dy * dt;
        m_z += dz * dt;
        m_t += dt;

        float r = (0.55f * fabsf(m_x) / 22.0f) + (0.45f * m_z / 55.0f);
        r = clamp01(r);

        m_ps->trail[m_head] = r;
        m_head = (uint8_t)((m_head + 1) % TRAIL_N);
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);
        float glue = centreGlue(i);
        float best = 0.0f;

        for (int k = 0; k < TRAIL_N; k++) {
            int idx = (int)m_head - 1 - k;
            while (idx < 0) idx += TRAIL_N;
            float r = m_ps->trail[idx];

            float age = (float)k / (float)(TRAIL_N - 1);     // 0 newest .. 1 oldest
            float fade = powf(1.0f - age, 1.65f);

            float w = 0.040f + 0.030f * (1.0f - speedNorm);  // thickness
            float g = expf(-fabsf(dn - r) / w) * fade;

            best = std::max(best, g);
        }

        // Add a thin specular edge that makes it feel like a "ribbon"
        float spec = powf(best, 2.2f) * 0.55f;

        float wave = clamp01((0.22f + 0.78f * best + spec) * glue);
        uint8_t br = waveToBr(wave, master);

        // Colour tied to local "chaotic energy" (speed proxy)
        float energy = clamp01((fabsf(m_x) / 22.0f) * 0.6f + (fabsf(m_y) / 28.0f) * 0.4f);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(25.0f * energy) + (int)(45.0f * spec));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPLorenzRibbonEffect
uint8_t LGPLorenzRibbonEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPLorenzRibbonEffectParameters) / sizeof(kLGPLorenzRibbonEffectParameters[0]));
}

const plugins::EffectParameter* LGPLorenzRibbonEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPLorenzRibbonEffectParameters[index];
}

bool LGPLorenzRibbonEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgplorenz_ribbon_effect_speed_scale") == 0) {
        gLGPLorenzRibbonEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgplorenz_ribbon_effect_output_gain") == 0) {
        gLGPLorenzRibbonEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgplorenz_ribbon_effect_centre_bias") == 0) {
        gLGPLorenzRibbonEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPLorenzRibbonEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgplorenz_ribbon_effect_speed_scale") == 0) return gLGPLorenzRibbonEffectSpeedScale;
    if (strcmp(name, "lgplorenz_ribbon_effect_output_gain") == 0) return gLGPLorenzRibbonEffectOutputGain;
    if (strcmp(name, "lgplorenz_ribbon_effect_centre_bias") == 0) return gLGPLorenzRibbonEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPLorenzRibbonEffect

void LGPLorenzRibbonEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
#endif
}

const plugins::EffectMetadata& LGPLorenzRibbonEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Lorenz Ribbon",
        "Chaotic attractor ribbon (never repeats the same way twice)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// -----------------------------------------------------------------------------
// 5) IFS Botanical Relic (Barnsley-inspired, mirrored)
// -----------------------------------------------------------------------------
LGPIFSBioRelicEffect::LGPIFSBioRelicEffect()
    : m_px(0.0f), m_py(0.0f), m_rng(0x12345678u), m_t(0.0f)
{
}

bool LGPIFSBioRelicEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPIFSBioRelicEffect
    gLGPIFSBioRelicEffectSpeedScale = kLGPIFSBioRelicEffectSpeedScale;
    gLGPIFSBioRelicEffectOutputGain = kLGPIFSBioRelicEffectOutputGain;
    gLGPIFSBioRelicEffectCentreBias = kLGPIFSBioRelicEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPIFSBioRelicEffect

    m_px = 0.0f; m_py = 0.0f;
    m_rng = 0xBADC0DEu;
    m_t = 0.0f;
    lightwaveos::effects::cinema::reset();
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<IFSPsram*>(heap_caps_malloc(sizeof(IFSPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("LGPIFSBioRelicEffect: PSRAM alloc failed");
            return false;
        }
    }
    memset(m_ps, 0, sizeof(IFSPsram));
#else
    return true;
#endif
    return true;
}

void LGPIFSBioRelicEffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.020f * speedNorm);

    float decay = 0.92f + 0.06f * (1.0f - speedNorm);
    for (int i = 0; i < STRIP_LENGTH; i++) m_ps->hist[i] *= decay;

    // Points per frame (budget-friendly)
    int P = 220 + (int)(520.0f * speedNorm);

    // Barnsley-style IFS coefficients (classic fern family), then MIRROR x for symmetry
    for (int k = 0; k < P; k++) {
        uint32_t r = lcg_next(m_rng);
        float u = (float)(r & 0xFFFFu) / 65535.0f;

        float x = m_px, y = m_py;
        float nx, ny;

        // Probabilities (approx classic): 0.01, 0.85, 0.07, 0.07
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

        // Normalise/mirror into a "relic" radial coordinate:
        // - Mirror x to enforce centre-origin symmetry
        // - Use a mix of height and lateral detail to make veins
        float ax = fabsf(nx);
        float yy = ny;

        // Fern typically spans y ~ [0..~10]; clamp gracefully.
        float yN = clamp01(yy / 10.0f);
        float xN = clamp01(ax / 3.0f);

        // Radial distance preference: more structure near centre, veins outward
        float radial = clamp01(0.10f + 0.78f * (0.70f * yN + 0.30f * xN));

        int bin = (int)lroundf(radial * (STRIP_LENGTH - 1));
        if (bin >= 0 && bin < STRIP_LENGTH) {
            float pulse = 0.85f + 0.15f * sinf(m_t * 0.7f + radial * 6.0f);
            m_ps->hist[bin] += 0.80f * pulse;
        }
    }

    float maxH = 1e-6f;
    for (int i = 0; i < STRIP_LENGTH; i++) maxH = std::max(maxH, m_ps->hist[i]);
    float inv = 1.0f / maxH;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dn = distN_from_index(i);
        float glue = centreGlue(i);

        int bin = (int)lroundf(dn * (STRIP_LENGTH - 1));
        bin = std::clamp(bin, 0, (int)STRIP_LENGTH - 1);

        float v = clamp01(m_ps->hist[bin] * inv);
        float veins = powf(v, 1.65f);
        float spec  = powf(veins, 2.2f) * 0.45f;

        float wave = clamp01((0.18f + 0.82f * veins + spec) * glue);
        uint8_t br = waveToBr(wave, master);

        // Colour: museum relic vibe — stable hue, slight lift on spec
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(10.0f * dn) + (int)(35.0f * spec));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
    lightwaveos::effects::cinema::apply(ctx, speedNorm);
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPIFSBioRelicEffect
uint8_t LGPIFSBioRelicEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPIFSBioRelicEffectParameters) / sizeof(kLGPIFSBioRelicEffectParameters[0]));
}

const plugins::EffectParameter* LGPIFSBioRelicEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPIFSBioRelicEffectParameters[index];
}

bool LGPIFSBioRelicEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpifsbio_relic_effect_speed_scale") == 0) {
        gLGPIFSBioRelicEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpifsbio_relic_effect_output_gain") == 0) {
        gLGPIFSBioRelicEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpifsbio_relic_effect_centre_bias") == 0) {
        gLGPIFSBioRelicEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPIFSBioRelicEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpifsbio_relic_effect_speed_scale") == 0) return gLGPIFSBioRelicEffectSpeedScale;
    if (strcmp(name, "lgpifsbio_relic_effect_output_gain") == 0) return gLGPIFSBioRelicEffectOutputGain;
    if (strcmp(name, "lgpifsbio_relic_effect_centre_bias") == 0) return gLGPIFSBioRelicEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPIFSBioRelicEffect

void LGPIFSBioRelicEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
#endif
}

const plugins::EffectMetadata& LGPIFSBioRelicEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "IFS Botanical Relic",
        "Mirrored IFS growth (fractal botany in glass)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
