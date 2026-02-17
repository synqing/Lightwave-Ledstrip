/**
 * @file BloomParityEffect.cpp
 * @brief Sensory Bridge "Bloom" parity port for LightwaveOS v2
 *
 * Transport is the brush. History is the canvas. Audio is just pigment injection.
 *
 * Parity spine (exact order matters):
 *  1) clear
 *  2) advect history by subpixel offset (draw_sprite: position = 0.250 + 1.750 * MOOD, alpha = 0.99)
 *  3) compute chroma-summed injection colour
 *  4) overwrite centre pixels with injection
 *  5) copy current -> history BEFORE any presentation shaping
 *  6) tail quadratic taper (presentation only, last 25%, edges dim to 60%)
 *  7) mirror (presentation only)
 */

#include "BloomParityEffect.h"
#include "../../utils/Log.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <FastLED.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstring>

namespace lightwaveos::effects::ieffect {

// Static tunables (defaults match user-tuned values; Sensory defaults: prism=0.25, bulb=0.00)
float   BloomParityEffect::s_prismOpacity    = 0.20f;
float   BloomParityEffect::s_bulbOpacity     = 0.40f;
float   BloomParityEffect::s_alpha           = 0.99f;   // Transport persistence (Sensory default: 0.99)
uint8_t BloomParityEffect::s_squareIter      = 1;       // Contrast shaping (Sensory default: 1)
uint8_t BloomParityEffect::s_prismIterations = 1;       // Prism passes (Sensory default: 0)
float   BloomParityEffect::s_gHueSpeed       = 1.0f;    // Palette sweep multiplier (0=frozen, -1=reverse)
float   BloomParityEffect::s_spatialSpread   = 128.0f;  // Palette spread centre→edge (0=mono, 255=full)
float   BloomParityEffect::s_intensityCoupling = 0.0f;  // 0=spatial colour, 1=intensity colour (heat map)

// -----------------------------------------------------------------------------
// Metadata
// -----------------------------------------------------------------------------
const plugins::EffectMetadata& BloomParityEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "Bloom (Parity)",
        "Sensory Bridge Bloom transport: subpixel advection + centre injection + mirror bloom",
        plugins::EffectCategory::AMBIENT,
        1,
        "LightwaveOS"
    );
    return meta;
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
bool BloomParityEffect::init(plugins::EffectContext& /*ctx*/) {
    // Allocate large transport buffers in PSRAM (19.3 KB — must not live in DRAM)
    if (!m_ps) {
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) {
            LW_LOGE("BloomParityEffect: PSRAM alloc failed (%u bytes)", (unsigned)sizeof(PsramData));
            return false;
        }
    }
    memset(m_ps, 0, sizeof(PsramData));

    // Hard reset all per-zone state.
    for (uint8_t z = 0; z < kMaxZones; z++) {
        clearBuffer(&m_ps->prev[z][0], kMaxLeds);
        clearBuffer(&m_ps->curr[z][0], kMaxLeds);

        m_chromaMaxPeak[z] = 0.01f;

        m_huePosition[z] = 0.0f;
        m_hueShiftSpeed[z] = 0.0f;
        m_huePushDirection[z] = -1.0f;
        m_hueDestination[z] = 0.0f;
        m_hueShiftingMix[z] = -0.35f;
        m_hueShiftingMixTarget[z] = 1.0f;

        m_chromaticMode[z] = true;   // Sensory default
        m_chromaVal[z] = 1.0f;       // Sensory default
    }
    return true;
}

void BloomParityEffect::cleanup() {
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
}

// -----------------------------------------------------------------------------
// Core helpers (HSV + colour forcing)
// -----------------------------------------------------------------------------
BloomParityEffect::RGBf BloomParityEffect::hsv(float h, float s, float v) {
    h = wrap01(h);
    s = clamp01(s);
    v = clamp01(v);

#ifndef NATIVE_BUILD
    // Sensory Bridge parity: uses FastLED's CHSV→CRGB (rainbow colour model)
    // with value applied as post-multiply. The rainbow hue wheel is perceptually
    // tuned and differs significantly from standard mathematical HSV.
    CRGB base = CHSV(static_cast<uint8_t>(h * 255.0f),
                      static_cast<uint8_t>(s * 255.0f),
                      255);
    return RGBf{(base.r / 255.0f) * v, (base.g / 255.0f) * v, (base.b / 255.0f) * v};
#else
    // Fallback: standard mathematical HSV for native builds without FastLED.
    const float hh = h * 6.0f;
    const int   i  = static_cast<int>(hh);
    const float f  = hh - static_cast<float>(i);

    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));

    RGBf out;
    switch (i % 6) {
        case 0: out = {v, t, p}; break;
        case 1: out = {q, v, p}; break;
        case 2: out = {p, v, t}; break;
        case 3: out = {p, q, v}; break;
        case 4: out = {t, p, v}; break;
        default: out = {v, p, q}; break;
    }
    return out;
#endif
}

void BloomParityEffect::rgbToHsv(const RGBf& in, float& h, float& s, float& v) {
    const float r = clamp01(in.r);
    const float g = clamp01(in.g);
    const float b = clamp01(in.b);

    const float maxv = std::fmax(r, std::fmax(g, b));
    const float minv = std::fmin(r, std::fmin(g, b));
    const float d = maxv - minv;

    v = maxv;
    s = (maxv <= 1e-6f) ? 0.0f : (d / maxv);

    if (d <= 1e-6f) {
        h = 0.0f;
        return;
    }

    float hh;
    if (maxv == r) {
        hh = (g - b) / d + (g < b ? 6.0f : 0.0f);
    } else if (maxv == g) {
        hh = (b - r) / d + 2.0f;
    } else {
        hh = (r - g) / d + 4.0f;
    }

    h = wrap01(hh / 6.0f);
}

BloomParityEffect::RGBf BloomParityEffect::forceSaturation(const RGBf& in, float sat) {
#ifndef NATIVE_BUILD
    // Sensory Bridge parity: deliberately round-trips through 8-bit CRGB and
    // FastLED's rgb2hsv_approximate(). The quantisation artefacts are intentional.
    CRGB rgb8(static_cast<uint8_t>(clamp01(in.r) * 255.0f),
              static_cast<uint8_t>(clamp01(in.g) * 255.0f),
              static_cast<uint8_t>(clamp01(in.b) * 255.0f));
    CHSV hsv8 = rgb2hsv_approximate(rgb8);
    CRGB out = CRGB(CHSV(hsv8.h, static_cast<uint8_t>(clamp01(sat) * 255.0f), hsv8.v));
    return RGBf{out.r / 255.0f, out.g / 255.0f, out.b / 255.0f};
#else
    float h, s, v;
    rgbToHsv(in, h, s, v);
    (void)s;
    return hsv(h, clamp01(sat), v);
#endif
}

BloomParityEffect::RGBf BloomParityEffect::forceHue(const RGBf& in, float hue) {
#ifndef NATIVE_BUILD
    // Sensory Bridge parity: same 8-bit round-trip as forceSaturation.
    CRGB rgb8(static_cast<uint8_t>(clamp01(in.r) * 255.0f),
              static_cast<uint8_t>(clamp01(in.g) * 255.0f),
              static_cast<uint8_t>(clamp01(in.b) * 255.0f));
    CHSV hsv8 = rgb2hsv_approximate(rgb8);
    CRGB out = CRGB(CHSV(static_cast<uint8_t>(wrap01(hue) * 255.0f), hsv8.s, hsv8.v));
    return RGBf{out.r / 255.0f, out.g / 255.0f, out.b / 255.0f};
#else
    float h, s, v;
    rgbToHsv(in, h, s, v);
    (void)h;
    return hsv(wrap01(hue), s, v);
#endif
}

// -----------------------------------------------------------------------------
// draw_sprite parity (subpixel advection) - THE CORE TRANSPORT SPINE
// -----------------------------------------------------------------------------
void BloomParityEffect::drawSprite(RGBf* dest, const RGBf* sprite,
                                  uint16_t destLen, uint16_t spriteLen,
                                  float position, float alpha)
{
    // This is the 2-tap linear interpolation that creates the "liquid" motion.
    // Each pixel is split between its neighbours with weights (1-frac) and frac.
    const int32_t positionWhole = static_cast<int32_t>(position);
    const float   positionFract = position - static_cast<float>(positionWhole);
    const float   mixRight = positionFract;
    const float   mixLeft  = 1.0f - mixRight;

    for (uint16_t i = 0; i < spriteLen; i++) {
        int32_t posLeft  = static_cast<int32_t>(i) + positionWhole;
        int32_t posRight = static_cast<int32_t>(i) + positionWhole + 1;

        bool skipLeft = false;
        bool skipRight = false;

        if (posLeft < 0) { posLeft = 0; skipLeft = true; }
        if (posLeft > static_cast<int32_t>(destLen) - 1) { posLeft = static_cast<int32_t>(destLen) - 1; skipLeft = true; }

        if (posRight < 0) { posRight = 0; skipRight = true; }
        if (posRight > static_cast<int32_t>(destLen) - 1) { posRight = static_cast<int32_t>(destLen) - 1; skipRight = true; }

        const RGBf px = sprite[i];

        if (!skipLeft) {
            dest[posLeft].r += px.r * mixLeft  * alpha;
            dest[posLeft].g += px.g * mixLeft  * alpha;
            dest[posLeft].b += px.b * mixLeft  * alpha;
        }

        if (!skipRight) {
            dest[posRight].r += px.r * mixRight * alpha;
            dest[posRight].g += px.g * mixRight * alpha;
            dest[posRight].b += px.b * mixRight * alpha;
        }
    }
}

// -----------------------------------------------------------------------------
// Chroma acquisition (best-effort)
// -----------------------------------------------------------------------------
void BloomParityEffect::getChroma12(const plugins::EffectContext& ctx, float outChroma12[kChromaBins]) const {
    // Zero
    for (uint16_t i = 0; i < kChromaBins; i++) outChroma12[i] = 0.0f;

    if (!ctx.audio.available) {
        return;
    }

    // Prefer 12-bin chroma when populated.
    float chromaSum = 0.0f;
    for (uint16_t i = 0; i < kChromaBins; i++) {
        const float v = clamp01(ctx.audio.controlBus.chroma[i]);
        outChroma12[i] = v;
        chromaSum += v;
    }
    if (chromaSum > 0.001f) {
        return;
    }

    // Fallback: derive 12-note energy from 8-band spectrum when chroma is missing.
    static constexpr uint8_t kBandMap[kChromaBins] = {0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 6, 7};
    float maxVal = 0.0f;
    for (uint16_t i = 0; i < kChromaBins; i++) {
        const float v = clamp01(ctx.audio.getBand(kBandMap[i]));
        outChroma12[i] = v;
        if (v > maxVal) maxVal = v;
    }

    if (maxVal > 0.001f) {
        const float inv = 1.0f / maxVal;
        for (uint16_t i = 0; i < kChromaBins; i++) {
            outChroma12[i] = clamp01(outChroma12[i] * inv);
        }
    }
}

// -----------------------------------------------------------------------------
// Hue shift (parity-ish with Sensory process_color_shift())
// -----------------------------------------------------------------------------
void BloomParityEffect::processHueShift(uint8_t zone, const plugins::EffectContext& ctx, float dtVis) {
    // Sensory uses novelty_curve and a bunch of shaping. Our closest analogue is spectral flux.
    float novelty = ctx.audio.available ? clamp01(ctx.audio.flux()) : 0.0f;

    // Remove bottom 10%, stretch remaining to fill [0,1].
    novelty -= 0.10f;
    if (novelty < 0.0f) novelty = 0.0f;
    novelty *= 1.111111f;

    // Cube shaping
    novelty = novelty * novelty * novelty;

    // Cap (matches Sensory: if (novelty_now > 0.05) novelty_now = 0.05)
    if (novelty > 0.05f) novelty = 0.05f;

    // Attack/decay follower for hue shift speed
    // dt-correct: decay *= powf(0.99, dt*60) instead of per-frame *= 0.99
    if (novelty > m_hueShiftSpeed[zone]) {
        m_hueShiftSpeed[zone] = novelty * 0.75f;
    } else {
        m_hueShiftSpeed[zone] *= powf(0.99f, dtVis * 60.0f);
    }

    // Integrate hue position and wrap
    // dt-correct: scale increment by dt*60 (was once-per-frame at 60 FPS)
    m_huePosition[zone] = wrap01(m_huePosition[zone] + (m_hueShiftSpeed[zone] * m_huePushDirection[zone] * dtVis * 60.0f));

    // Destination bouncing (parity structure, deterministic but still lively)
    const float dist = std::fabs(m_huePosition[zone] - m_hueDestination[zone]);
    if (dist <= 0.01f) {
        m_huePushDirection[zone] *= -1.0f;
        m_hueShiftingMixTarget[zone] *= -1.0f;

        // Deterministic pseudo-random destination: uses frameNumber hashed into [0,1)
        const uint32_t x = ctx.frameNumber * 1664525u + 1013904223u;
        m_hueDestination[zone] = (x & 0xFFFFu) / 65535.0f;
    }

    // Mix follower (one-pole exponential approach)
    // dt-correct: alpha = 1 - (1 - 0.01)^(dt*60) instead of per-frame 0.01
    const float mixAlpha = 1.0f - powf(1.0f - 0.01f, dtVis * 60.0f);
    const float mixDist = std::fabs(m_hueShiftingMix[zone] - m_hueShiftingMixTarget[zone]);
    if (m_hueShiftingMix[zone] < m_hueShiftingMixTarget[zone]) {
        m_hueShiftingMix[zone] += mixDist * mixAlpha;
    } else if (m_hueShiftingMix[zone] > m_hueShiftingMixTarget[zone]) {
        m_hueShiftingMix[zone] -= mixDist * mixAlpha;
    }
}

// -----------------------------------------------------------------------------
// Bloom injection computation (intensity-only)
//
// Returns GRAYSCALE energy (all channels equal) so the transport buffer never
// carries colour information. Palette mapping happens at the output stage,
// guaranteeing every LED pixel displays a clean, uncorrupted palette colour.
// -----------------------------------------------------------------------------
BloomParityEffect::RGBf BloomParityEffect::computeInjection(uint8_t zone, const plugins::EffectContext& ctx, float dtRaw) {
    // 1) Pull 12-bin chroma (best effort)
    float chroma[12];
    getChroma12(ctx, chroma);

    // 2) Parity-ish max_peak follower (from make_smooth_chromagram())
    // dt-correct: decay *= powf(0.999, dt*60) instead of per-frame *= 0.999
    float max_peak = m_chromaMaxPeak[zone];
    max_peak *= powf(0.999f, dtRaw * 60.0f);
    if (max_peak < 0.01f) max_peak = 0.01f;

    // dt-correct: attack alpha = 1 - (1 - 0.05)^(dt*60) instead of per-frame 0.05
    const float attackAlpha = 1.0f - powf(1.0f - 0.05f, dtRaw * 60.0f);
    for (uint16_t i = 0; i < 12; i++) {
        if (chroma[i] > max_peak) {
            const float distance = chroma[i] - max_peak;
            max_peak += distance * attackAlpha;
        }
    }
    m_chromaMaxPeak[zone] = max_peak;

    const float multiplier = 1.0f / max_peak;
    for (uint16_t i = 0; i < 12; i++) {
        chroma[i] = clamp01(chroma[i] * multiplier);
    }

    // 3) Total chroma energy → grayscale intensity
    float totalEnergy = 0.0f;
    for (uint16_t i = 0; i < 12; i++) {
        totalEnergy += chroma[i] * chroma[i];   // Squared for contrast
    }
    float bright = clamp01(totalEnergy * (1.0f / 6.0f));

    // Square iter for contrast shaping
    for (uint8_t sq = 0; sq < s_squareIter; sq++) {
        bright *= bright;
    }

    // Return grayscale (all channels equal — no colour in transport)
    return RGBf{bright, bright, bright};
}

// -----------------------------------------------------------------------------
// PostFX: Sensory Bridge optical cheats (prism + bulb cover)
// Ported from render_bulb_cover() and apply_prism_effect() for RGBf floats.
// Call order (parity): transport pipeline → prism → bulb → output
// -----------------------------------------------------------------------------

void BloomParityEffect::scaleImageToHalf(RGBf* buf, uint16_t len, RGBf* temp) {
    if (!buf || !temp || len < 2) return;
    const uint16_t half = len >> 1;

    for (uint16_t i = 0; i < half; i++) {
        const RGBf& a = buf[i << 1];
        const RGBf& b = buf[(i << 1) + 1];
        temp[i].r = (a.r + b.r) * 0.5f;
        temp[i].g = (a.g + b.g) * 0.5f;
        temp[i].b = (a.b + b.b) * 0.5f;
        temp[half + i] = {0, 0, 0};
    }
    std::memcpy(buf, temp, sizeof(RGBf) * len);
}

void BloomParityEffect::shiftLedsUp(RGBf* buf, uint16_t len, uint16_t offset, RGBf* temp) {
    if (!buf || !temp || len == 0) return;
    if (offset >= len) {
        std::memset(buf, 0, sizeof(RGBf) * len);
        return;
    }
    std::memcpy(temp, buf, sizeof(RGBf) * len);
    std::memcpy(buf + offset, temp, sizeof(RGBf) * (len - offset));
    std::memset(buf, 0, sizeof(RGBf) * offset);
}

void BloomParityEffect::mirrorImageDownwards(RGBf* buf, uint16_t len, RGBf* temp) {
    if (!buf || !temp || len < 2) return;
    const uint16_t half = len >> 1;

    for (uint16_t i = 0; i < half; i++) {
        const RGBf& src = buf[half + i];
        temp[half + i]     = src;
        temp[half - 1 - i] = src;
    }
    std::memcpy(buf, temp, sizeof(RGBf) * len);
}

void BloomParityEffect::applyPrismEffect(RGBf* buf, uint16_t len, uint8_t iterations,
                                          float opacity, RGBf* fx, RGBf* temp) {
    if (!buf || !fx || !temp || len < 4) return;
    if ((len & 1u) != 0u) return;   // must be even
    if (iterations == 0 || opacity <= 0.0f) return;

    const uint16_t half = len >> 1;

    for (uint8_t it = 0; it < iterations; it++) {
        std::memcpy(fx, buf, sizeof(RGBf) * len);

        scaleImageToHalf(fx, len, temp);
        shiftLedsUp(fx, len, half, temp);
        mirrorImageDownwards(fx, len, temp);

        // Additive blend into buf
        for (uint16_t i = 0; i < len; i++) {
            buf[i].r = clamp01(buf[i].r + fx[i].r * opacity);
            buf[i].g = clamp01(buf[i].g + fx[i].g * opacity);
            buf[i].b = clamp01(buf[i].b + fx[i].b * opacity);
        }
    }
}

void BloomParityEffect::renderBulbCover(RGBf* buf, uint16_t len, float bulbOpacity) {
    if (!buf || len == 0) return;
    if (bulbOpacity <= 0.0f) return;
    if (bulbOpacity > 1.0f) bulbOpacity = 1.0f;

    // Sensory Bridge 4-phase occlusion mask
    const float cover[4] = {0.25f, 1.0f, 0.25f, 0.0f};

    for (uint16_t i = 0; i < len; i++) {
        // lerp(p, p*cover, opacity) = p * (1 - opacity*(1-cover))
        const float k = 1.0f - bulbOpacity * (1.0f - cover[i & 3u]);
        buf[i].r *= k;
        buf[i].g *= k;
        buf[i].b *= k;
    }
}

// -----------------------------------------------------------------------------
// Render (THE PARITY PIPELINE - order matters!)
// -----------------------------------------------------------------------------
void BloomParityEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;   // PSRAM not allocated (init failed or not called)

    // Zone handling (ZoneComposer uses zoneId 0-3)
    uint8_t zone = (ctx.zoneId == 0xFF) ? 0 : ctx.zoneId;
    if (zone >= kMaxZones) zone = 0;

    const uint16_t len = (ctx.ledCount > kMaxLeds) ? kMaxLeds : ctx.ledCount;
    if (len < 4 || ctx.leds == nullptr) return;

    // dt values: visual for animation, raw for audio signal processing
    const float dtVis = ctx.getSafeDeltaSeconds();
    const float dtRaw = ctx.getSafeRawDeltaSeconds();

    RGBf* curr = &m_ps->curr[zone][0];
    RGBf* prev = &m_ps->prev[zone][0];

    // ----- (1) Clear output
    clearBuffer(curr, len);

    // ----- (2) Advect previous frame (subpixel)
    // THE SPINE: position = 0.250 + 1.750 * MOOD, alpha = 0.99 (at 60 FPS)
    // dt-correct: persistence alpha = powf(0.99, dt*60) instead of fixed 0.99 per frame
    const float mood = clamp01(static_cast<float>(ctx.mood) / 255.0f);
    const float position = 0.250f + 1.750f * mood;
    const float dtAlpha = powf(s_alpha, dtVis * 60.0f);
    drawSprite(curr, prev, len, len, position, dtAlpha);

    // ----- (Parity) Update hue shift state (kept invisible unless chromatic mode off)
    processHueShift(zone, ctx, dtVis);

    // ----- (3) Compute injection colour
    const RGBf inject = computeInjection(zone, ctx, dtRaw);

    // ----- (4) Overwrite centre pixels (parity: leds_16[63] and [64])
    const uint16_t centerL = (len / 2) - 1;
    const uint16_t centerR = (len / 2);

    curr[centerL] = inject;
    curr[centerR] = inject;

    // ----- (5) Copy current to history BEFORE any presentation shaping
    // This is critical: history stores pre-taper/pre-mirror state.
    // Transport is grayscale (all channels equal), so no colour distortion to worry about.
    std::memcpy(prev, curr, sizeof(RGBf) * len);

    // ----- (6) Tail quadratic taper (presentation only, last 25% of strip)
    // Edges dim to 60% min (was 0%) to avoid ~10–15 unused LEDs at strip ends.
    constexpr float kEdgeMinBrightness = 0.60f;
    const uint16_t tailLen = len / 4;
    if (tailLen >= 2) {
        for (uint16_t i = 0; i < tailLen; i++) {
            const float prog = static_cast<float>(i) / static_cast<float>(tailLen - 1);
            const float k = kEdgeMinBrightness + (1.0f - kEdgeMinBrightness) * (prog * prog);

            const uint16_t idx = (len - 1) - i;
            curr[idx].r *= k;
            curr[idx].g *= k;
            curr[idx].b *= k;
        }
    }

    // ----- (7) Mirror for symmetry (left half = mirror of right half)
    for (uint16_t i = 0; i < (len / 2); i++) {
        curr[i] = curr[(len - 1) - i];
    }

    // ----- PostFX A: Prism (glassy layered glow — Sensory: apply_prism_effect)
    applyPrismEffect(curr, len, s_prismIterations, s_prismOpacity, m_ps->fx, m_ps->tmp);

    // ----- PostFX B: Bulb cover (micro-occlusion mask — Sensory: render_bulb_cover)
    renderBulbCover(curr, len, s_bulbOpacity);

    // ----- Clamp accumulation to [0,1] per channel before lum extraction
    // Preserves r:g:b ratio and restores dynamic range in hot spots (splat sum > 1).
    for (uint16_t i = 0; i < len; i++) {
        curr[i].r = clamp01(curr[i].r);
        curr[i].g = clamp01(curr[i].g);
        curr[i].b = clamp01(curr[i].b);
    }

    // ----- Write to LED buffer (palette-mapped from grayscale intensity)
    // Transport carries intensity only (all channels equal). Palette colour is
    // assigned here so every pixel is ONE clean palette.getColor() call.
    //
    // Two palette-position sources blended by s_intensityCoupling:
    //   Spatial:   distance from centre × spread   → palette gradient across strip
    //   Intensity: pixel brightness × 255          → heat-map / fire mode
    // Both are offset by audio novelty drift + gHue time rotation.
    const float hueOffset = m_huePosition[zone];
    const uint16_t half = len / 2;
    const float hueRotation = static_cast<float>(ctx.gHue) * s_gHueSpeed;
    const float coupling = s_intensityCoupling;

    for (uint16_t i = 0; i < len; i++) {
        // Extract intensity (average of channels — should be equal, but safe)
        float lum = (curr[i].r + curr[i].g + curr[i].b) * (1.0f / 3.0f);
        if (lum > 1.0f) lum = 1.0f;
        if (lum < 0.0f) lum = 0.0f;

        // Spatial palette position: distance from centre × spread
        const float dist = static_cast<float>(std::abs(static_cast<int>(i) - static_cast<int>(half)))
                         / static_cast<float>(half);
        const float spatialPal = dist * s_spatialSpread;

        // Intensity palette position: brightness → palette (heat map)
        const float intensityPal = lum * 255.0f;

        // Blend spatial ↔ intensity, add audio drift + time rotation
        const float palFloat = spatialPal * (1.0f - coupling) + intensityPal * coupling
                             + hueOffset * 255.0f + hueRotation;
        const uint8_t palIdx = static_cast<uint8_t>(static_cast<uint16_t>(palFloat) & 0xFFu);

        ctx.leds[i] = ctx.palette.getColor(palIdx, static_cast<uint8_t>(lum * 255.0f));
    }

    // If ctx.ledCount > len, clear remainder to avoid stale pixels in odd configs.
    for (uint16_t i = len; i < ctx.ledCount; i++) {
        ctx.leds[i] = CRGB::Black;
    }
}

} // namespace lightwaveos::effects::ieffect
