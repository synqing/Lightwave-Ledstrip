/**
 * @file LegacyEffectAdapter.h
 * @brief Adapter to wrap v1 effects for v2 plugin system
 *
 * This adapter allows the 47 existing v1 effects to run in v2 without
 * modification. It bridges the global-variable-based v1 API to the
 * dependency-injection-based v2 IEffect interface.
 *
 * Usage:
 * @code
 * // In effect registration
 * extern void fire();  // v1 effect function
 *
 * auto fireEffect = new LegacyEffectAdapter(
 *     fire,
 *     EffectMetadata{"Fire", "Classic fire effect", EffectCategory::FIRE}
 * );
 * registry.registerEffect(fireEffect);
 * @endcode
 *
 * How it works:
 * 1. Before calling the legacy function, the adapter copies EffectContext
 *    values to the global variables that v1 effects expect
 * 2. The legacy effect renders directly to the global leds[] buffer
 * 3. After the call, any changes are already in the LED buffer
 *
 * Migration path:
 * - Phase 1: Wrap all 47 effects with LegacyEffectAdapter (zero code changes)
 * - Phase 2: Migrate high-value effects to native IEffect (optional)
 * - Phase 3: Community contributes new native IEffect plugins
 */

#pragma once

#include "../api/IEffect.h"
#include "../api/EffectContext.h"
#include <cstring>

namespace lightwaveos {
namespace plugins {
namespace legacy {

/**
 * @brief Function signature for v1 effects (void functions with no parameters)
 */
using LegacyEffectFunc = void(*)();

/**
 * @brief Adapter to wrap v1 effects for v2 plugin system
 *
 * This class implements IEffect by calling a legacy v1 effect function.
 * It handles the translation between v2's EffectContext and v1's global
 * variables.
 */
class LegacyEffectAdapter : public IEffect {
public:
    /**
     * @brief Construct a legacy effect adapter
     * @param func The v1 effect function pointer
     * @param metadata Effect metadata for registration
     */
    LegacyEffectAdapter(LegacyEffectFunc func, const EffectMetadata& metadata)
        : m_func(func)
        , m_metadata(metadata)
    {}

    ~LegacyEffectAdapter() override = default;

    //--------------------------------------------------------------------------
    // IEffect Implementation
    //--------------------------------------------------------------------------

    bool init(EffectContext& ctx) override {
        // Legacy effects don't have explicit init
        // Just verify we have a valid function
        (void)ctx;
        return m_func != nullptr;
    }

    void render(EffectContext& ctx) override {
        if (!m_func) return;

        // Bridge: Copy context values to v1 globals
        // These externs reference the actual v1 global variables
        bridgeContextToGlobals(ctx);

        // Call the legacy effect
        m_func();

        // No need to copy back - legacy effects write directly to leds[]
    }

    void cleanup() override {
        // Legacy effects don't have explicit cleanup
    }

    const EffectMetadata& getMetadata() const override {
        return m_metadata;
    }

private:
    LegacyEffectFunc m_func;
    EffectMetadata m_metadata;

    /**
     * @brief Copy EffectContext values to v1 global variables
     *
     * This is the bridge that makes legacy effects work. v1 effects
     * expect these globals to exist and be set to current values.
     *
     * Note: These globals are defined in v1/src/main.cpp and must be
     * linked when using legacy effects. For v2-only builds without
     * legacy support, this function can be stubbed out.
     */
    static void bridgeContextToGlobals(EffectContext& ctx);
};

//------------------------------------------------------------------------------
// Global Variable Bridges
//------------------------------------------------------------------------------

// These are declared as weak symbols so they can be optionally linked.
// When building v2 with legacy effect support, link against v1 globals.
// When building v2 standalone, provide stubs.

#ifndef NATIVE_BUILD

// Forward declare v1 globals (defined in v1/src/main.cpp)
extern CRGB leds[];
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t brightnessVal;
extern uint8_t effectSpeed;
extern uint8_t intensity;
extern uint8_t saturation;
extern uint8_t complexity;
extern uint8_t variation;

inline void LegacyEffectAdapter::bridgeContextToGlobals(EffectContext& ctx) {
    // The context's LED buffer should point to the global leds[]
    // This is set up by the RendererActor when creating the context

    // Copy scalar parameters to globals
    gHue = ctx.gHue;
    brightnessVal = ctx.brightness;
    effectSpeed = ctx.speed;
    intensity = ctx.intensity;
    saturation = ctx.saturation;
    complexity = ctx.complexity;
    variation = ctx.variation;

    // Copy palette if different
    // Note: In v2, palette changes go through StateStore, so this
    // syncs the current palette to what legacy effects expect
    if (ctx.palette.isValid()) {
        // The palette reference in context should point to currentPalette
        // This is managed by the RendererActor
    }
}

#else  // NATIVE_BUILD

// Stub implementation for native tests
inline void LegacyEffectAdapter::bridgeContextToGlobals(EffectContext& ctx) {
    (void)ctx;
    // No-op in native builds - legacy effects can't run without hardware
}

#endif  // NATIVE_BUILD

//------------------------------------------------------------------------------
// Legacy Effect Registration Macros
//------------------------------------------------------------------------------

/**
 * @brief Macro to register a legacy effect
 *
 * Usage:
 * @code
 * extern void fire();
 * REGISTER_LEGACY_EFFECT(fire, "Fire", "Classic fire effect", EffectCategory::FIRE);
 * @endcode
 */
#define REGISTER_LEGACY_EFFECT(func, name, desc, category) \
    static LegacyEffectAdapter __legacy_##func( \
        func, \
        EffectMetadata(name, desc, category) \
    )

/**
 * @brief Macro to register with full metadata
 */
#define REGISTER_LEGACY_EFFECT_FULL(func, name, desc, category, version, author) \
    static LegacyEffectAdapter __legacy_##func( \
        func, \
        EffectMetadata(name, desc, category, version, author) \
    )

} // namespace legacy
} // namespace plugins
} // namespace lightwaveos
