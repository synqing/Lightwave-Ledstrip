// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FadeOverride.h
 * @brief Bench-gated fade-to-black helper for opt-in effect A/B tests.
 *
 * This header intentionally does not migrate existing effects by itself.
 * Effects opt in by replacing direct fade-to-black calls with this helper when
 * Captain asks to isolate persistence behaviour for that specific effect.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "PersistenceHelpers.h"
#include "utils/BenchRegistry.h"

namespace lightwaveos {
namespace effects {
namespace persistence {

/**
 * @brief Apply dt-correct fade-to-black only when the bench toggle is enabled.
 *
 * Hot-path contract: one volatile bool read per helper call, then the existing
 * dt-correct fade loop. No heap allocation, string lookup, registry traversal,
 * or frame-count timing.
 */
static inline void fadeToBlackByGated(CRGB* leds,
                                      size_t n,
                                      uint8_t fadeBy,
                                      float dt) {
    const bool fadeEnabled = ::lightwaveos::bench::isToggleEnabled(
        &::lightwaveos::bench::g_bench_effect_fade_to_black);
    if (!fadeEnabled) {
        return;
    }
    fadeToBlackByDt(leds, n, fadeBy, dt);
}

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
