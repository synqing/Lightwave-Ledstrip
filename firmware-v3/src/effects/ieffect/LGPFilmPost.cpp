/**
 * @file LGPFilmPost.cpp
 * @brief Cinema-grade post chain: spatial soften, filmic tone map, gamma, temporal EMA, dither.
 * No heap allocation in apply() — static buffers only.
 */

#include "LGPFilmPost.h"
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace effects {
namespace cinema {

static inline float clamp01(float x) { return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x; }

static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

static constexpr int MAX_N = 256;

static bool s_inited = false;
static uint8_t s_gamma[256];
static CRGB s_prev[MAX_N];
static CRGB s_src[MAX_N];
static uint32_t s_frame = 0;

static inline void buildGammaLUT(float gammaEncode) {
    for (int i = 0; i < 256; i++) {
        float x = (float)i / 255.0f;
        float y = powf(x, gammaEncode);
        int v = (int)lroundf(y * 255.0f);
        s_gamma[i] = (uint8_t)std::clamp(v, 0, 255);
    }
}

/// ACES fitted tone map (Narkowicz fit). Filmic shoulder/toe for highlight rolloff.
static inline float acesFilm(float x) {
    x = x * 0.85f;
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    float y = (x * (a * x + b)) / (x * (c * x + d) + e);
    return clamp01(y);
}

void reset() {
    for (int i = 0; i < MAX_N; i++) s_prev[i] = CRGB::Black;
    s_frame = 0;
    s_inited = false;
}

void apply(plugins::EffectContext& ctx, float speedNorm) {
    int N = (int)ctx.ledCount;
    if (N <= 0) return;
    if (N >= 2) N = N / 2;
    N = std::min(N, MAX_N);

    if (!s_inited) {
        buildGammaLUT(1.0f / 2.2f);
        s_inited = true;
    }

    speedNorm = clamp01(speedNorm);
    uint8_t alpha8 = (uint8_t)(40 + (int)(120.0f * speedNorm));

    for (int i = 0; i < N; i++) s_src[i] = ctx.leds[i];

    for (int i = 0; i < N; i++) {
        int im1 = (i == 0) ? 0 : (i - 1);
        int ip1 = (i == N - 1) ? (N - 1) : (i + 1);

        uint16_t r = (uint16_t)(2 * s_src[i].r + s_src[im1].r + s_src[ip1].r);
        uint16_t g = (uint16_t)(2 * s_src[i].g + s_src[im1].g + s_src[ip1].g);
        uint16_t b = (uint16_t)(2 * s_src[i].b + s_src[im1].b + s_src[ip1].b);

        float rf = (r * 0.25f) / 255.0f;
        float gf = (g * 0.25f) / 255.0f;
        float bf = (b * 0.25f) / 255.0f;

        rf = acesFilm(rf);
        gf = acesFilm(gf);
        bf = acesFilm(bf);

        uint8_t rt = (uint8_t)lroundf(rf * 255.0f);
        uint8_t gt = (uint8_t)lroundf(gf * 255.0f);
        uint8_t bt = (uint8_t)lroundf(bf * 255.0f);

        rt = s_gamma[rt];
        gt = s_gamma[gt];
        bt = s_gamma[bt];

        uint32_t h = hash32((uint32_t)(i + 1) * 2654435761U ^ (s_frame * 1013904223U));
        int8_t dn = (int8_t)((h & 0x3U) - 1);
        auto addDither = [&](uint8_t v) -> uint8_t {
            int vv = (int)v + (int)dn;
            return (uint8_t)std::clamp(vv, 0, 255);
        };
        rt = addDither(rt);
        gt = addDither(gt);
        bt = addDither(bt);

        CRGB prev = s_prev[i];
        auto ema = [&](uint8_t p, uint8_t t) -> uint8_t {
            int16_t dp = (int16_t)t - (int16_t)p;
            int16_t step = (int16_t)((dp * (int16_t)alpha8) >> 8);
            int16_t out = (int16_t)p + step;
            return (uint8_t)std::clamp((int)out, 0, 255);
        };

        CRGB out;
        out.r = ema(prev.r, rt);
        out.g = ema(prev.g, gt);
        out.b = ema(prev.b, bt);

        s_prev[i] = out;

        ctx.leds[i] = out;
        int j = i + N;
        if (j < (int)ctx.ledCount) ctx.leds[j] = out;
    }

    s_frame++;
}

} // namespace cinema
} // namespace effects
} // namespace lightwaveos
