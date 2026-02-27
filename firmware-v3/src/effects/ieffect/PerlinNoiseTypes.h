// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file PerlinNoiseTypes.h
 * @brief Shared types for Emotiscope 2.0 Perlin noise port
 */

#pragma once

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Emotiscope 2.0 Perlin noise types (shared between full and quarter variants)
struct Vec2 {
    float x, y;
    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct UVec2 {
    unsigned int x, y;
    UVec2() : x(0), y(0) {}
    UVec2(unsigned int x_, unsigned int y_) : x(x_), y(y_) {}
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

