/**
 * @file BuiltinEffectRegistry.h
 * @brief Static registry for built-in IEffect instances
 *
 * Stores compiled-in effect instances for lookup during
 * plugin manifest loading. Effects are registered during
 * registerAllEffects() and looked up by ID.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "api/IEffect.h"
#include "../config/limits.h"
#include <stdint.h>

namespace lightwaveos {
namespace plugins {

/**
 * @brief Static registry for built-in effects
 *
 * Maps effect IDs to compiled IEffect pointers.
 * Used by PluginManagerActor to resolve manifest effect references.
 */
class BuiltinEffectRegistry {
public:
    // Larger than limits::MAX_EFFECTS to allow headroom for future effects
    static constexpr uint8_t MAX_EFFECTS = 170;
    static_assert(MAX_EFFECTS >= limits::MAX_EFFECTS,
                  "BuiltinEffectRegistry::MAX_EFFECTS must be >= limits::MAX_EFFECTS");

    /**
     * @brief Register a built-in effect
     * @param id Effect ID (0-139)
     * @param effect Pointer to IEffect instance
     * @return true if registered successfully
     */
    static bool registerBuiltin(uint8_t id, IEffect* effect);

    /**
     * @brief Get a built-in effect by ID
     * @param id Effect ID
     * @return Pointer to IEffect or nullptr if not registered
     */
    static IEffect* getBuiltin(uint8_t id);

    /**
     * @brief Check if an effect ID is registered
     * @param id Effect ID
     * @return true if registered
     */
    static bool hasBuiltin(uint8_t id);

    /**
     * @brief Get count of registered built-in effects
     * @return Number of registered effects
     */
    static uint8_t getBuiltinCount();

    /**
     * @brief Clear all registrations (for testing)
     */
    static void clear();

private:
    static IEffect* s_effects[MAX_EFFECTS];
    static uint8_t s_count;
};

} // namespace plugins
} // namespace lightwaveos
