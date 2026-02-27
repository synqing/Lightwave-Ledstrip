/**
 * @file BuiltinEffectRegistry.h
 * @brief Static registry for built-in IEffect instances
 *
 * Stores compiled-in effect instances for lookup during
 * plugin manifest loading. Effects are registered during
 * registerAllEffects() and looked up by stable EffectId.
 *
 * Uses append-only linear registry (not array-indexed) to
 * support sparse namespaced EffectId values.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "api/IEffect.h"
#include "../config/effect_ids.h"
#include "../config/limits.h"
#include <stdint.h>

namespace lightwaveos {
namespace plugins {

/**
 * @brief Static registry for built-in effects
 *
 * Maps stable EffectId values to compiled IEffect pointers.
 * Used by PluginManagerActor to resolve manifest effect references.
 */
class BuiltinEffectRegistry {
public:
    static constexpr uint16_t MAX_EFFECTS = 256;
    static_assert(MAX_EFFECTS >= limits::MAX_EFFECTS,
                  "BuiltinEffectRegistry::MAX_EFFECTS must be >= limits::MAX_EFFECTS");

    /**
     * @brief Register a built-in effect
     * @param id Stable namespaced EffectId
     * @param effect Pointer to IEffect instance
     * @return true if registered successfully
     */
    static bool registerBuiltin(EffectId id, IEffect* effect);

    /**
     * @brief Get a built-in effect by ID
     * @param id Stable EffectId
     * @return Pointer to IEffect or nullptr if not registered
     */
    static IEffect* getBuiltin(EffectId id);

    /**
     * @brief Check if an effect ID is registered
     * @param id Stable EffectId
     * @return true if registered
     */
    static bool hasBuiltin(EffectId id);

    /**
     * @brief Get count of registered built-in effects
     * @return Number of registered effects
     */
    static uint16_t getBuiltinCount();

    /**
     * @brief Clear all registrations (for testing)
     */
    static void clear();

private:
    struct Entry {
        EffectId id;
        IEffect* effect;
    };
    static Entry s_entries[MAX_EFFECTS];
    static uint16_t s_count;
};

} // namespace plugins
} // namespace lightwaveos
