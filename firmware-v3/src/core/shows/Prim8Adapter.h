/**
 * @file Prim8Adapter.h
 * @brief Maps Prim8 semantic vector (8 floats) to firmware parameters (10 uint8s)
 *
 * PRISM Studio uses 8 semantic float primitives (Prim8). The firmware
 * understands 10 flat uint8 parameters. This adapter provides the
 * v0.1 mapping layer as specified in PRIM8_PARAMETER_MAP.md.
 *
 * The mapping is intentionally lossy -- translating expressive intent
 * into the limited vocabulary the firmware currently understands.
 *
 * No heap allocation. Pure function with static inline helpers.
 * Safe to call from any context including render paths (though
 * typically called from WS/REST handlers on Core 0).
 */

#pragma once

#include <Arduino.h>
#include <cmath>

namespace prism {

/**
 * @brief Clamp a float to uint8_t range [0, 255]
 */
static inline uint8_t clamp8(float value) {
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return static_cast<uint8_t>(value + 0.5f);  // round
}


/**
 * @brief 8-dimensional semantic vector for creative expression
 *
 * All values are normalised to [0.0, 1.0].
 */
struct Prim8Vector {
    float pressure;   // 0.0 - 1.0  (overall energy/brightness)
    float impact;     // 0.0 - 1.0  (speed, sharpness)
    float mass;       // 0.0 - 1.0  (weight, complexity)
    float momentum;   // 0.0 - 1.0  (continuation, mood)
    float heat;       // 0.0 - 1.0  (colour warmth, saturation)
    float space;      // 0.0 - 1.0  (spatial variation)
    float texture;    // 0.0 - 1.0  (roughness, detail)
    float gravity;    // 0.0 - 1.0  (drag, trails)

    /// Clamp all values to [0, 1]
    void clamp() {
        pressure = fmaxf(0.0f, fminf(1.0f, pressure));
        impact   = fmaxf(0.0f, fminf(1.0f, impact));
        mass     = fmaxf(0.0f, fminf(1.0f, mass));
        momentum = fmaxf(0.0f, fminf(1.0f, momentum));
        heat     = fmaxf(0.0f, fminf(1.0f, heat));
        space    = fmaxf(0.0f, fminf(1.0f, space));
        texture  = fmaxf(0.0f, fminf(1.0f, texture));
        gravity  = fmaxf(0.0f, fminf(1.0f, gravity));
    }

    /// Create a default neutral vector
    static Prim8Vector neutral() {
        return { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    }
};


/**
 * @brief Firmware parameter set (mirrors the 10 existing parameters)
 */
struct FirmwareParams {
    uint8_t brightness;    // 0-255
    uint8_t speed;         // 0-255
    uint8_t paletteId;     // 0-74 (not mapped by Prim8 -- set separately)
    uint8_t hue;           // 0-255
    uint8_t intensity;     // 0-255
    uint8_t saturation;    // 0-255
    uint8_t complexity;    // 0-255
    uint8_t variation;     // 0-255
    uint8_t mood;          // 0-255
    uint8_t fadeAmount;    // 0-255
};


/**
 * @brief Map Prim8 semantic vector to firmware parameter space
 *
 * This is the v0.1 direct/lossy mapping. Each Prim8 dimension maps to one
 * or more firmware parameters with simple linear relationships.
 *
 * @param prim8 Input semantic vector (will be clamped internally)
 * @param paletteId Palette ID to pass through (not derived from Prim8)
 * @return Mapped firmware parameters
 */
inline FirmwareParams mapPrim8ToParams(const Prim8Vector& prim8, uint8_t paletteId = 0) {
    // Work with clamped copy
    Prim8Vector p = prim8;
    p.clamp();

    FirmwareParams out;
    out.paletteId = paletteId;

    // --- pressure -> brightness + intensity ---
    out.brightness = clamp8(p.pressure * 255.0f);
    out.intensity  = clamp8(p.pressure * 200.0f + 55.0f);

    // --- impact -> speed + fadeAmount ---
    float rawSpeed   = p.impact * 255.0f;
    float rawFade    = (1.0f - p.impact) * 200.0f;

    // --- momentum -> speed (additive) + mood ---
    rawSpeed += p.momentum * 50.0f;
    out.mood = clamp8(p.momentum * 255.0f);

    // --- gravity -> speed (subtractive) + fadeAmount (additive) ---
    rawSpeed -= p.gravity * 30.0f;
    rawFade  += p.gravity * 50.0f;

    out.speed      = clamp8(rawSpeed);
    out.fadeAmount = clamp8(rawFade);

    // --- heat -> saturation + hue ---
    out.saturation = clamp8(p.heat * 255.0f);
    out.hue        = clamp8(p.heat * 40.0f);  // warm shift only

    // --- mass -> complexity ---
    float rawComplexity = p.mass * 255.0f;

    // --- texture -> complexity (additive) ---
    rawComplexity += p.texture * 50.0f;

    out.complexity = clamp8(rawComplexity);

    // --- space -> variation ---
    out.variation = clamp8(p.space * 255.0f);

    return out;
}

} // namespace prism
