/**
 * @file LGPGradientFieldEffect.cpp
 * @brief LGP Gradient Field — operator-surfaced gradient proof effect
 *
 * All three coordinate bases, all three repeat modes, all three interpolation
 * modes, dual-edge colour differentiation, dirty-flag ramp rebuild.
 */

#include "LGPGradientFieldEffect.h"
#include "../CoreEffects.h"
#include "../gradient/GradientCoord.h"

#include <FastLED.h>
#include <cstring>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ============================================================================
// Static metadata
// ============================================================================

const plugins::EffectMetadata LGPGradientFieldEffect::s_meta{
    "LGP Gradient Field",
    "Operator-controlled gradient with basis, repeat, and edge controls",
    plugins::EffectCategory::GEOMETRIC,
    1,
    "LightwaveOS"
};

const plugins::EffectParameter LGPGradientFieldEffect::s_params[6] = {
    // name, displayName, min, max, default, type, step, group, unit, advanced
    {"basis",          "Basis",           0.0f, 2.0f, 0.0f,
     plugins::EffectParameterType::ENUM, 1.0f, "gradient", "", false},
    {"repeatMode",     "Repeat Mode",     0.0f, 2.0f, 0.0f,
     plugins::EffectParameterType::ENUM, 1.0f, "gradient", "", false},
    {"interpolation",  "Interpolation",   0.0f, 2.0f, 1.0f,
     plugins::EffectParameterType::ENUM, 1.0f, "gradient", "", false},
    {"spread",         "Spread",          0.1f, 2.0f, 1.0f,
     plugins::EffectParameterType::FLOAT, 0.05f, "gradient", "x", false},
    {"phaseOffset",    "Phase Offset",    0.0f, 1.0f, 0.0f,
     plugins::EffectParameterType::FLOAT, 0.01f, "gradient", "", false},
    {"edgeAsymmetry",  "Edge Asymmetry",  0.0f, 1.0f, 0.0f,
     plugins::EffectParameterType::FLOAT, 0.05f, "gradient", "", false},
};

// ============================================================================
// Lifecycle
// ============================================================================

LGPGradientFieldEffect::LGPGradientFieldEffect()
    : m_basis(0.0f)
    , m_repeatMode(0.0f)
    , m_interpolation(1.0f)
    , m_spread(1.0f)
    , m_phaseOffset(0.0f)
    , m_edgeAsymmetry(0.0f)
    , m_dirty(true)
    , m_phase(0.0f)
{
}

bool LGPGradientFieldEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_dirty = true;
    m_phase = 0.0f;
    return true;
}

void LGPGradientFieldEffect::cleanup() {
    // No heap resources to free
}

const plugins::EffectMetadata& LGPGradientFieldEffect::getMetadata() const {
    return s_meta;
}

// ============================================================================
// Parameters
// ============================================================================

const plugins::EffectParameter* LGPGradientFieldEffect::getParameter(uint8_t index) const {
    if (index >= 6) return nullptr;
    return &s_params[index];
}

bool LGPGradientFieldEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    bool changed = false;
    if (strcmp(name, "basis") == 0) {
        float v = (value < 0.0f) ? 0.0f : (value > 2.0f) ? 2.0f : value;
        v = roundf(v);
        changed = (v != m_basis); m_basis = v;
    } else if (strcmp(name, "repeatMode") == 0) {
        float v = (value < 0.0f) ? 0.0f : (value > 2.0f) ? 2.0f : value;
        v = roundf(v);
        changed = (v != m_repeatMode); m_repeatMode = v;
    } else if (strcmp(name, "interpolation") == 0) {
        float v = (value < 0.0f) ? 0.0f : (value > 2.0f) ? 2.0f : value;
        v = roundf(v);
        changed = (v != m_interpolation); m_interpolation = v;
    } else if (strcmp(name, "spread") == 0) {
        float v = (value < 0.1f) ? 0.1f : (value > 2.0f) ? 2.0f : value;
        changed = (v != m_spread); m_spread = v;
    } else if (strcmp(name, "phaseOffset") == 0) {
        float v = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
        changed = (v != m_phaseOffset); m_phaseOffset = v;
    } else if (strcmp(name, "edgeAsymmetry") == 0) {
        float v = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
        changed = (v != m_edgeAsymmetry); m_edgeAsymmetry = v;
    } else {
        return false;
    }

    if (changed) m_dirty = true;
    return true;
}

float LGPGradientFieldEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "basis") == 0)          return m_basis;
    if (strcmp(name, "repeatMode") == 0)     return m_repeatMode;
    if (strcmp(name, "interpolation") == 0)  return m_interpolation;
    if (strcmp(name, "spread") == 0)         return m_spread;
    if (strcmp(name, "phaseOffset") == 0)    return m_phaseOffset;
    if (strcmp(name, "edgeAsymmetry") == 0)  return m_edgeAsymmetry;
    return 0.0f;
}

// ============================================================================
// Ramp rebuild (only on parameter change)
// ============================================================================

void LGPGradientFieldEffect::rebuildRamp(const plugins::EffectContext& ctx) {
    // Map enum parameters to gradient types
    gradient::RepeatMode repeat = gradient::RepeatMode::CLAMP;
    if ((int)m_repeatMode == 1) repeat = gradient::RepeatMode::REPEAT;
    if ((int)m_repeatMode == 2) repeat = gradient::RepeatMode::MIRROR;

    gradient::InterpolationMode interp = gradient::InterpolationMode::LINEAR;
    if ((int)m_interpolation == 1) interp = gradient::InterpolationMode::EASED;
    if ((int)m_interpolation == 2) interp = gradient::InterpolationMode::HARD_STOP;

    // Build ramps at FULL brightness (255) — brightness is applied AFTER
    // sampling to preserve interpolation fidelity at low brightness levels.
    // Without this, brightness=15 produces near-zero RGB values that quantise
    // badly through lerp8, causing visible flicker and colour corruption.
    m_rampA.clear();
    m_rampA.addStop(0,   ctx.palette.getColor(ctx.gHue,                   255));
    m_rampA.addStop(85,  ctx.palette.getColor((uint8_t)(ctx.gHue + 64),   255));
    m_rampA.addStop(170, ctx.palette.getColor((uint8_t)(ctx.gHue + 128),  255));
    m_rampA.addStop(255, ctx.palette.getColor((uint8_t)(ctx.gHue + 192),  255));
    m_rampA.setRepeatMode(repeat);
    m_rampA.setInterpolationMode(interp);

    // Secondary ramp for strip 2 (edge asymmetry)
    // edgeAsymmetry=0 → rampB ≈ rampA (symmetric)
    // edgeAsymmetry=1 → rampB is colour-rotated 128° complement
    uint8_t hueShift = (uint8_t)(m_edgeAsymmetry * 128.0f);

    m_rampB.clear();
    m_rampB.addStop(0,   ctx.palette.getColor((uint8_t)(ctx.gHue + hueShift),               255));
    m_rampB.addStop(85,  ctx.palette.getColor((uint8_t)(ctx.gHue + 64 + hueShift),          255));
    m_rampB.addStop(170, ctx.palette.getColor((uint8_t)(ctx.gHue + 128 + hueShift),         255));
    m_rampB.addStop(255, ctx.palette.getColor((uint8_t)(ctx.gHue + 192 + hueShift),         255));
    m_rampB.setRepeatMode(repeat);
    m_rampB.setInterpolationMode(interp);

    m_dirty = false;
}

// ============================================================================
// Render (lookup-only — no allocation, no ramp rebuild)
// ============================================================================

void LGPGradientFieldEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;

    // Slow phase animation for visual interest
    m_phase += speed * 0.005f * 60.0f * dt;
    if (m_phase > 1.0f) m_phase -= 1.0f;

    // Rebuild ramps on parameter change or meaningful gHue drift (≥2 steps).
    // Avoids per-frame rebuilds that cause flicker at low brightness.
    static uint8_t lastGHue = 0;
    uint8_t hueDelta = (ctx.gHue >= lastGHue) ? (ctx.gHue - lastGHue) : (lastGHue - ctx.gHue);
    if (m_dirty || hueDelta >= 2) {
        rebuildRamp(ctx);
        lastGHue = ctx.gHue;
    }

    // Determine coordinate basis
    Basis basis = Basis::CENTER;
    if ((int)m_basis == 1) basis = Basis::LOCAL;
    if ((int)m_basis == 2) basis = Basis::SIGNED;

    // Phase offset + spread
    float offset = m_phaseOffset + m_phase;
    float spread = m_spread;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Compute position in chosen coordinate basis
        float u = 0.0f;
        switch (basis) {
            case Basis::CENTER:
                u = gradient::uCenter(i);
                break;
            case Basis::LOCAL:
                u = gradient::uLocal(i);
                break;
            case Basis::SIGNED:
                // Map [-1,+1] to [0,1] for ramp sampling
                u = (gradient::uSigned(i) + 1.0f) * 0.5f;
                break;
        }

        // Apply spread and offset
        u = u * spread + offset;

        // Sample ramps at full brightness, then scale down.
        // This preserves interpolation fidelity at low brightness.
        CRGB colourA = m_rampA.samplef(u);
        CRGB colourB = m_rampB.samplef(u);
        colourA.nscale8(ctx.brightness);
        colourB.nscale8(ctx.brightness);

        // Dither-safe floor: FastLED temporal dithering toggles LSBs
        // across frames, causing visible flicker on channels in the 1-3
        // range (33-100% brightness oscillation). Snap sub-dither values:
        // 0 stays 0 (true black), 1-3 snap to 4 (stable minimum).
        auto ditherFloor = [](CRGB& c) {
            if (c.r > 0 && c.r < 4) c.r = 4;
            if (c.g > 0 && c.g < 4) c.g = 4;
            if (c.b > 0 && c.b < 4) c.b = 4;
        };
        ditherFloor(colourA);
        ditherFloor(colourB);

        // Write strip 1
        ctx.leds[i] = colourA;

        // Write strip 2
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = colourB;
        }
    }

}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
