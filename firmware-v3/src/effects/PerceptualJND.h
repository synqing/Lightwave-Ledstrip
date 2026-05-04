/**
 * @file PerceptualJND.h
 * @brief K1 LGP perceptual floor constants from hardware observation.
 *
 * Phase 1 Move 1.7 substrate. Captain-observed brightness-floor test on
 * 2026-05-05 found first visible output at test level 4 in both scaler and
 * direct PWM phases: 8.0% perceptual brightness through the K1 LGP.
 *
 * This is a viewer-observed calibration, not photometer-grade production
 * photometry. Treat it as the current lower bound for subtle LGP brightness
 * or contrast claims until the C-7 photometer follow-up supersedes it.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace perceptual {

static constexpr float kK1LgpPerceptualJndFloor = 0.08f;
static constexpr float kK1LgpPerceptualJndFloorPercent = 8.0f;
static constexpr float kK1LgpPerceptualGamma = 2.2f;
static constexpr uint8_t kK1LgpMinimumVisibleChannelDelta = 1;

// Existing INF-02 ES lower cutoff, now named as the K1 perceptual lower bound.
static constexpr float kFramebufferLpfMinimumCutoffHz = 0.5f;
static constexpr float kTwoPi = 6.283185307179586f;
static constexpr float kFramebufferLpfMaximumTauSeconds =
    1.0f / (kTwoPi * kFramebufferLpfMinimumCutoffHz);

static inline float perceptualPercentToLinear(float perceptualPercent) {
    if (perceptualPercent <= 0.0f) {
        return 0.0f;
    }
    if (perceptualPercent >= 100.0f) {
        return 1.0f;
    }
    return powf(perceptualPercent / 100.0f, kK1LgpPerceptualGamma);
}

static inline uint8_t perceptualPercentToByte(float perceptualPercent) {
    const float scaled = perceptualPercentToLinear(perceptualPercent) * 255.0f;
    if (scaled <= 0.0f) {
        return 0;
    }
    if (scaled >= 255.0f) {
        return 255;
    }
    return static_cast<uint8_t>(roundf(scaled));
}

static inline bool isBelowK1LgpJnd(float perceptualDelta01) {
    return fabsf(perceptualDelta01) < kK1LgpPerceptualJndFloor;
}

}  // namespace perceptual
}  // namespace effects
}  // namespace lightwaveos
