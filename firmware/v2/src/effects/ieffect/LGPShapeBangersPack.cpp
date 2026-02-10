/**
 * @file LGPShapeBangersPack.cpp
 * @brief Implementation for 11 LGP shape bangers
 *
 * Pattern families: Talbot self-imaging, Airy self-accelerating beams,
 * moire interference, superformula supershapes, spirograph hypotrochoids,
 * rose curves, Lissajous/harmonograph orbits, Rule 30, Langton's ant,
 * standing waves/cymatics, shock diamonds.
 */

#include "LGPShapeBangersPack.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr float SB_PI  = 3.14159265358979323846f;
static constexpr float SB_TAU = 6.28318530717958647692f;

static inline float clamp01(float x) { return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x; }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstep(float a, float b, float x) {
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
static inline float fract(float x) { return x - floorf(x); }
static inline float tri01(float x) {
    // triangle wave in [0..1]
    float f = fract(x);
    float t = 1.0f - fabsf(2.0f * f - 1.0f);
    return clamp01(t);
}
static inline float gaussian(float x, float sigma) {
    // exp(-(x^2)/(2*sigma^2))
    float s2 = sigma * sigma;
    return expf(-(x * x) / (2.0f * s2));
}

// Dual-strip lock: no wing rivalry
static inline void writeDualLocked(plugins::EffectContext& ctx, int i, const CRGB& c) {
    ctx.leds[i] = c;
    int j = i + STRIP_LENGTH;
    if (j < (int)ctx.ledCount) ctx.leds[j] = c;
}

// Shared "premium LGP" luminance mapping: contrast without harsh clipping
static inline uint8_t luminanceToBr(float wave01, float master) {
    const float base = 0.06f;
    float out = clamp01(base + (1.0f - base) * clamp01(wave01)) * master;
    return (uint8_t)(255.0f * out);
}

// ---------------------------------------------
// 1) Talbot Carpet
// ---------------------------------------------
LGPTalbotCarpetEffect::LGPTalbotCarpetEffect() : m_t(0.0f) {}

bool LGPTalbotCarpetEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPTalbotCarpetEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    // "Propagation distance" (stylised Talbot carpet)
    m_t += (0.010f + 0.040f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Base grating pitch in pixels
    const float p = 10.0f + 8.0f * (1.0f - speedNorm);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dmid = (float)i - mid;
        float distN = fabsf(dmid) * invMid;
        float glue = 0.35f + 0.65f * expf(-(dmid * dmid) * 0.0016f);

        // Fresnel-ish harmonic sum: phase ~ k^2 * z (Talbot self-imaging motif)
        float phi = SB_TAU* ((float)i / p);
        float z = m_t;

        float sumC = 0.0f, sumS = 0.0f;
        float norm = 0.0f;
        for (int k = 1; k <= 7; k++) {
            float ak = 1.0f / (float)k;
            float phase = (float)k * phi + (float)(k * k) * z * 0.55f;
            sumC += ak * cosf(phase);
            sumS += ak * sinf(phase);
            norm += ak;
        }
        float amp = sqrtf(sumC * sumC + sumS * sumS) / (norm + 1e-6f);
        float wave = powf(clamp01(amp), 1.7f);

        // Carpet "loom" feel: edge gets slightly tighter
        wave *= lerp(1.0f, 0.85f, distN);

        uint8_t br = luminanceToBr(wave * glue, master);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(wave * 60.0f) + (int)(distN * 20.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPTalbotCarpetEffect::cleanup() {}

const plugins::EffectMetadata& LGPTalbotCarpetEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Talbot Carpet",
        "Self-imaging lattice rug (near-field diffraction vibe)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 2) Airy Comet
// ---------------------------------------------
LGPAiryCometEffect::LGPAiryCometEffect() : m_t(0.0f) {}
bool LGPAiryCometEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPAiryCometEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    m_t += (0.010f + 0.045f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Parabolic "self-accelerating" motion across the strip (bounce)
    float s = fract(m_t * 0.12f);
    float parab = (s * s);              // 0..1 with accelerating slope
    float x0 = lerp(-mid * 0.92f, mid * 0.92f, parab);

    // Alternate direction each half-cycle
    bool flip = (fract(m_t * 0.06f) > 0.5f);
    float dir = flip ? -1.0f : 1.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i - mid;
        float dmid = x;

        float glue = expf(-(dmid * dmid) * 0.0018f);

        float dx = x - x0;
        float head = gaussian(dx, 3.2f); // sharp head

        // Airy-ish tail: oscillatory lobes behind the head, decaying
        float behind = (dx * dir > 0.0f) ? (dx * dir) : 0.0f;
        float tail = expf(-behind * 0.12f) * (0.55f + 0.45f * cosf(behind * 1.25f - m_t * 0.9f));

        float wave = clamp01(head + 0.75f * tail);
        wave = powf(wave, 1.25f);

        // Melt into wings (no top/bottom fighting)
        float glued = lerp(wave, wave * (0.45f + 0.55f * glue), 0.85f);

        uint8_t br = luminanceToBr(glued, master);

        // Head warm, tail cool (subtle)
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(60.0f * (1.0f - smoothstep(0.0f, 1.0f, head))) + (int)(15.0f * glue));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPAiryCometEffect::cleanup() {}

const plugins::EffectMetadata& LGPAiryCometEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Airy Comet",
        "Self-accelerating comet with trailing lobes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 3) Moire Cathedral
// ---------------------------------------------
LGPMoireCathedralEffect::LGPMoireCathedralEffect() : m_t(0.0f) {}
bool LGPMoireCathedralEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPMoireCathedralEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.008f + 0.030f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Two close gratings -> slow giant arches
    float p1 = 7.5f;
    float p2 = 8.1f;

    float w1 = 0.65f + 0.40f * speedNorm;
    float w2 = 0.58f + 0.35f * speedNorm;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i;
        float dmid = x - mid;
        float glue = 0.40f + 0.60f * expf(-(dmid * dmid) * 0.0011f);

        float g1 = sinf(SB_TAU * (x / p1) + m_t * w1);
        float g2 = sinf(SB_TAU * (x / p2) - m_t * w2);

        float moire = fabsf(g1 - g2);           // 0..2
        float wave = clamp01(moire * 0.55f);    // ~0..1

        // Cathedral ribs: compress highlights slightly
        wave = powf(wave, 1.35f);

        uint8_t br = luminanceToBr(wave * glue, master);

        // Two-tone "stained glass" without rainbow spam
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(wave * 42.0f) + (int)(12.0f * glue));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPMoireCathedralEffect::cleanup() {}

const plugins::EffectMetadata& LGPMoireCathedralEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Moire Cathedral",
        "Interference arches from close gratings (giant beats)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 4) Superformula Living Glyph
// ---------------------------------------------
LGPSuperformulaGlyphEffect::LGPSuperformulaGlyphEffect() : m_t(0.0f) {}
bool LGPSuperformulaGlyphEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

static inline float superformula(float phi, float m, float n1, float n2, float n3, float a=1.0f, float b=1.0f) {
    float t1 = powf(fabsf(cosf(m * phi * 0.25f) / a), n2);
    float t2 = powf(fabsf(sinf(m * phi * 0.25f) / b), n3);
    float r  = powf(t1 + t2, -1.0f / n1);
    return r;
}

void LGPSuperformulaGlyphEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.030f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Morph parameters slowly (sigils)
    float morph = 0.5f + 0.5f * sinf(m_t * 0.35f);
    float sfm = lerp(3.0f, 11.0f, morph);      // "lobes"
    float n1 = lerp(0.7f, 1.6f, 1.0f - morph);
    float n2 = lerp(0.8f, 2.4f, morph);
    float n3 = lerp(0.8f, 2.4f, 1.0f - morph);

    float rot = m_t * 0.20f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float signedX = ((float)i - mid) * invMid;     // -1..1
        float phi = signedX * SB_PI + rot;                 // -pi..pi plus rotation
        float distN = fabsf((float)i - mid) * invMid;  // 0..1

        float r = superformula(phi, sfm, n1, n2, n3);
        r = clamp01(r * 0.92f);                        // keep inside

        // Distance-to-curve band
        float bandW = 0.055f;
        float wave = expf(-fabsf(distN - r) / bandW);

        // LGP glue: strengthen centre continuity
        float glue = 0.35f + 0.65f * expf(-((float)i - mid) * ((float)i - mid) * 0.0015f);
        wave *= glue;

        uint8_t br = luminanceToBr(powf(wave, 1.15f), master);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(morph * 50.0f) + (int)(wave * 90.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPSuperformulaGlyphEffect::cleanup() {}

const plugins::EffectMetadata& LGPSuperformulaGlyphEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Living Glyph",
        "Superformula sigils (morphing supershapes)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 5) Spirograph Crown (Hypotrochoid)
// ---------------------------------------------
LGPSpirographCrownEffect::LGPSpirographCrownEffect() : m_t(0.0f) {}
bool LGPSpirographCrownEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

static inline float hypotrochoid_radius(float theta, float R, float r, float d) {
    // x = (R-r)cos(theta) + d cos((R-r)/r * theta)
    // y = (R-r)sin(theta) - d sin((R-r)/r * theta)
    float k = (R - r) / r;
    float x = (R - r) * cosf(theta) + d * cosf(k * theta);
    float y = (R - r) * sinf(theta) - d * sinf(k * theta);
    return sqrtf(x * x + y * y);
}

void LGPSpirographCrownEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.030f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Crown parameters
    const float R = 1.0f;
    const float r = 0.30f + 0.05f * sinf(m_t * 0.22f);
    const float d = 0.78f;

    float rot = m_t * 0.35f;
    float maxRad = (R - r) + d; // safe normalisation

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float signedX = ((float)i - mid) * invMid;
        float theta = signedX * SB_PI + rot;

        float distN = fabsf((float)i - mid) * invMid;

        float rr = hypotrochoid_radius(theta, R, r, d) / (maxRad + 1e-6f);
        rr = clamp01(rr);

        float bandW = 0.050f;
        float wave = expf(-fabsf(distN - rr) / bandW);

        float glue = 0.30f + 0.70f * expf(-((float)i - mid) * ((float)i - mid) * 0.0014f);
        wave *= glue;

        // Facet sparkle (tiny)
        float facet = 0.85f + 0.15f * sinf(18.0f * theta + m_t * 0.6f);
        wave *= facet;

        uint8_t br = luminanceToBr(powf(wave, 1.2f), master);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(rr * 70.0f) + (int)(wave * 80.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPSpirographCrownEffect::cleanup() {}

const plugins::EffectMetadata& LGPSpirographCrownEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Spirograph Crown",
        "Hypotrochoid crown loops (gear-flower royal seal)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 6) Rose Bloom (Rhodonea)
// ---------------------------------------------
LGPRoseBloomEffect::LGPRoseBloomEffect() : m_t(0.0f) {}
bool LGPRoseBloomEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPRoseBloomEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.028f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Petal count drifts between 3 and 7 (integer-ish feel)
    float kf = 5.0f + 2.0f * sinf(m_t * 0.18f);
    float rot = m_t * 0.28f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float signedX = ((float)i - mid) * invMid;
        float theta = signedX * SB_PI + rot;

        float distN = fabsf((float)i - mid) * invMid;

        float r = fabsf(cosf(kf * theta));
        r = clamp01(r * 0.92f);

        float bandW = 0.060f;
        float wave = expf(-fabsf(distN - r) / bandW);

        // "Opening bloom" modulation
        float open = 0.55f + 0.45f * sinf(m_t * 0.35f);
        wave *= (0.75f + 0.25f * open);

        float glue = 0.30f + 0.70f * expf(-((float)i - mid) * ((float)i - mid) * 0.0016f);
        wave *= glue;

        uint8_t br = luminanceToBr(powf(wave, 1.18f), master);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(open * 40.0f) + (int)(wave * 95.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPRoseBloomEffect::cleanup() {}

const plugins::EffectMetadata& LGPRoseBloomEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Rose Bloom",
        "Rhodonea petals (geometric bloom)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 7) Harmonograph Halo (Lissajous aura)
// ---------------------------------------------
LGPHarmonographHaloEffect::LGPHarmonographHaloEffect() : m_t(0.0f) {}
bool LGPHarmonographHaloEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPHarmonographHaloEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.030f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Integer-ish frequency ratio drives Lissajous family "feel"
    float a = 3.0f;
    float b = 2.0f + (speedNorm > 0.5f ? 1.0f : 0.0f);
    float delta = 0.7f + 0.3f * sinf(m_t * 0.22f);

    float rot = m_t * 0.20f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float signedX = ((float)i - mid) * invMid;
        float phi = signedX * SB_PI;

        // "Orbit radius" derived from two perpendicular oscillations
        float x = sinf(a * (phi + rot) + delta);
        float y = sinf(b * (phi - rot));
        float r = sqrtf(x * x + y * y);     // 0..~1.414
        float rr = clamp01(r * 0.72f);

        float distN = fabsf((float)i - mid) * invMid;

        float bandW = 0.055f;
        float wave = expf(-fabsf(distN - rr) / bandW);

        // Gentle "energy pulse"
        wave *= (0.85f + 0.15f * sinf(m_t * 0.65f + phi * 2.0f));

        float glue = 0.30f + 0.70f * expf(-((float)i - mid) * ((float)i - mid) * 0.0013f);
        wave *= glue;

        uint8_t br = luminanceToBr(powf(wave, 1.15f), master);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(rr * 70.0f) + (int)(wave * 85.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPHarmonographHaloEffect::cleanup() {}

const plugins::EffectMetadata& LGPHarmonographHaloEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Harmonograph Halo",
        "Lissajous orbitals (aura loops, premium calm)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 8) Rule 30 Cathedral
// ---------------------------------------------
LGPRule30CathedralEffect::LGPRule30CathedralEffect() : m_step(0) {}

bool LGPRule30CathedralEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    for (int i = 0; i < STRIP_LENGTH; i++) { m_cells[i] = 0; m_next[i] = 0; }
    m_cells[STRIP_LENGTH / 2] = 1;
    m_step = 0;
    return true;
}

static inline uint8_t rule30(uint8_t l, uint8_t c, uint8_t r) {
    // Rule 30: new = l xor (c or r)
    return (uint8_t)(l ^ (c | r));
}

void LGPRule30CathedralEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    // Steps per frame (speed controls narrative)
    int steps = 1 + (int)(6.0f * speedNorm);

    for (int s = 0; s < steps; s++) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int im1 = (i == 0) ? (STRIP_LENGTH - 1) : (i - 1);
            int ip1 = (i == STRIP_LENGTH - 1) ? 0 : (i + 1);
            m_next[i] = rule30(m_cells[im1], m_cells[i], m_cells[ip1]);
        }
        for (int i = 0; i < STRIP_LENGTH; i++) m_cells[i] = m_next[i];
        m_step++;
    }

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dmid = (float)i - mid;
        float glue = 0.35f + 0.65f * expf(-(dmid * dmid) * 0.0019f);

        // Soften binary into "textile" (cheap blur)
        int im1 = (i == 0) ? (STRIP_LENGTH - 1) : (i - 1);
        int ip1 = (i == STRIP_LENGTH - 1) ? 0 : (i + 1);
        float cell = (float)m_cells[i];
        float blur = (cell + 0.7f * (float)m_cells[im1] + 0.7f * (float)m_cells[ip1]) / (1.0f + 0.7f + 0.7f);

        float wave = powf(clamp01(blur), 1.35f) * glue;

        uint8_t br = luminanceToBr(wave, master);

        // Neighbourhood tinting (adds "cathedral triangles" depth)
        uint8_t nb = (uint8_t)((m_cells[im1] << 2) | (m_cells[i] << 1) | (m_cells[ip1]));
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(nb * 13) + (int)(wave * 55.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPRule30CathedralEffect::cleanup() {}

const plugins::EffectMetadata& LGPRule30CathedralEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Rule 30 Cathedral",
        "Elementary CA textile (triangles + chaos + ribs)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 9) Langton Highway (2D ant projected onto 1D)
// ---------------------------------------------
LGPLangtonHighwayEffect::LGPLangtonHighwayEffect()
    : m_x(W/2), m_y(H/2), m_dir(1), m_steps(0), m_scan(0)
{
}

bool LGPLangtonHighwayEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) m_grid[y][x] = 0;
    m_x = W/2; m_y = H/2; m_dir = 1; m_steps = 0; m_scan = 0;
    return true;
}

static inline void antStep(uint8_t grid[LGPLangtonHighwayEffect::H][LGPLangtonHighwayEffect::W],
                           uint8_t& x, uint8_t& y, uint8_t& dir)
{
    // Classic Langton's ant:
    // - On white(0): turn right, flip to black(1), move forward
    // - On black(1): turn left, flip to white(0), move forward
    uint8_t cell = grid[y][x];
    if (cell == 0) {
        dir = (uint8_t)((dir + 1) & 3);
        grid[y][x] = 1;
    } else {
        dir = (uint8_t)((dir + 3) & 3);
        grid[y][x] = 0;
    }

    if (dir == 0) y = (uint8_t)((y == 0) ? (LGPLangtonHighwayEffect::H - 1) : (y - 1));
    if (dir == 1) x = (uint8_t)((x + 1) & (LGPLangtonHighwayEffect::W - 1));
    if (dir == 2) y = (uint8_t)((y + 1) & (LGPLangtonHighwayEffect::H - 1));
    if (dir == 3) x = (uint8_t)((x == 0) ? (LGPLangtonHighwayEffect::W - 1) : (x - 1));
}

void LGPLangtonHighwayEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;

    // More steps = faster emergence into "highway"
    int steps = 16 + (int)(140.0f * speedNorm);
    for (int s = 0; s < steps; s++) {
        antStep(m_grid, m_x, m_y, m_dir);
        m_steps++;
    }

    // Projection: sample a drifting diagonal slice of the 2D grid
    m_scan = (uint8_t)(m_scan + (1 + (int)(6.0f * speedNorm)));

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dmid = (float)i - mid;
        float glue = 0.35f + 0.65f * expf(-(dmid * dmid) * 0.0017f);

        uint8_t gx = (uint8_t)((i * W) / STRIP_LENGTH);
        uint8_t gy = (uint8_t)((((int)i + (int)m_scan) * H) / STRIP_LENGTH);
        uint8_t cell = m_grid[gy & (H - 1)][gx & (W - 1)];

        // Soften a bit: local neighbourhood average
        uint8_t gx1 = (uint8_t)((gx + 1) & (W - 1));
        uint8_t gy1 = (uint8_t)((gy + 1) & (H - 1));
        float blur = (cell + m_grid[gy][gx1] + m_grid[gy1][gx] + m_grid[gy1][gx1]) * 0.25f;

        float wave = powf(clamp01(blur), 1.35f) * glue;

        // Subtle "ant spark" when projection hits ant neighbourhood
        if ((gx == m_x && gy == m_y) || (gx1 == m_x && gy == m_y) || (gx == m_x && gy1 == m_y)) {
            wave = clamp01(wave + 0.35f);
        }

        uint8_t br = luminanceToBr(wave, master);
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(wave * 80.0f) + (int)((m_steps >> 7) & 31));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPLangtonHighwayEffect::cleanup() {}

const plugins::EffectMetadata& LGPLangtonHighwayEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Langton Highway",
        "Emergent order reveal (ant to chaos to highway) projected to 1D",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 10) Cymatic Ladder (standing waves)
// ---------------------------------------------
LGPCymaticLadderEffect::LGPCymaticLadderEffect() : m_t(0.0f) {}
bool LGPCymaticLadderEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPCymaticLadderEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.020f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;

    // Mode number (standing wave harmonic)
    int n = 2 + (int)(6.0f * speedNorm); // 2..8

    float phase = m_t * (0.8f + 0.5f * speedNorm);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i / (float)(STRIP_LENGTH - 1); // 0..1 along length

        // Standing wave shape: sin(n*pi*x + phase) with nodes/antinodes
        float s = fabsf(sinf((float)n * SB_PI * x + phase));

        // Sharpen nodes and anti-nodes to "sculpture"
        float wave = powf(s, 1.8f);

        // LGP glue: centre continuity
        float dmid = (float)i - mid;
        float glue = 0.30f + 0.70f * expf(-(dmid * dmid) * 0.0016f);
        wave *= glue;

        uint8_t br = luminanceToBr(wave, master);

        // Cymatic vibe: hue shifts mainly with harmonic, not position
        uint8_t hue = (uint8_t)(ctx.gHue + n * 9 + (int)(wave * 60.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPCymaticLadderEffect::cleanup() {}

const plugins::EffectMetadata& LGPCymaticLadderEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Cymatic Ladder",
        "Standing-wave nodes/antinodes sculpted into LGP glass",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------
// 11) Mach Diamonds (shock diamonds)
// ---------------------------------------------
LGPMachDiamondsEffect::LGPMachDiamondsEffect() : m_t(0.0f) {}
bool LGPMachDiamondsEffect::init(plugins::EffectContext& ctx) { (void)ctx; m_t = 0.0f; return true; }

void LGPMachDiamondsEffect::render(plugins::EffectContext& ctx) {
    const float speedNorm = ctx.speed / 50.0f;
    const float master = ctx.brightness / 255.0f;
    m_t += (0.010f + 0.040f * speedNorm);

    const float mid = (STRIP_LENGTH - 1) * 0.5f;
    const float invMid = 1.0f / mid;

    // Shock-cell spacing in centre-distance space
    float spacing = 0.13f;      // in distN units
    float drift = m_t * (0.20f + 0.35f * speedNorm);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dmid = (float)i - mid;
        float distN = fabsf(dmid) * invMid;     // 0..1

        // "Shock diamonds" as standing-wave-ish pulses along distance
        float cellPhase = (distN / spacing) - drift;
        float tri = tri01(cellPhase);           // 0..1 triangular peaks
        float wave = powf(tri, 2.2f);           // jewel highlights

        // Diamond "breathing" (very subtle)
        wave *= (0.85f + 0.15f * cosf(SB_TAU * cellPhase + m_t * 0.6f));

        // Glue so the middle melts into wings
        float glue = 0.35f + 0.65f * expf(-(dmid * dmid) * 0.0013f);
        wave *= glue;

        uint8_t br = luminanceToBr(wave, master);

        // Deep jewel tones: hue primarily tracks shock-cell rhythm
        uint8_t hue = (uint8_t)(ctx.gHue + (int)(tri * 80.0f) + (int)(distN * 15.0f));

        writeDualLocked(ctx, i, ctx.palette.getColor(hue, br));
    }
}

void LGPMachDiamondsEffect::cleanup() {}

const plugins::EffectMetadata& LGPMachDiamondsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Mach Diamonds",
        "Shock-diamond jewellery (standing shock-cell pulses)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
