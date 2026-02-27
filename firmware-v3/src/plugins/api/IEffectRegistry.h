/**
 * @file IEffectRegistry.h
 * @brief Interface for effect registration
 *
 * Provides a unified API for registering IEffect instances,
 * implemented by both PluginManagerActor and RendererActor.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "IEffect.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace plugins {

/**
 * @brief Interface for effect registration
 *
 * Allows effects to be registered by EffectId without knowing
 * the concrete registry implementation.
 */
class IEffectRegistry {
public:
    virtual ~IEffectRegistry() = default;

    /**
     * @brief Register an effect by stable EffectId
     * @param id Stable namespaced effect ID (from effect_ids.h)
     * @param effect Pointer to IEffect instance (must remain valid)
     * @return true if registration succeeded
     */
    virtual bool registerEffect(EffectId id, IEffect* effect) = 0;

    /**
     * @brief Unregister an effect by ID
     * @param id Effect ID to unregister
     * @return true if effect was registered and is now unregistered
     */
    virtual bool unregisterEffect(EffectId id) = 0;

    /**
     * @brief Check if an effect is registered
     * @param id Effect ID to check
     * @return true if effect is registered
     */
    virtual bool isEffectRegistered(EffectId id) const = 0;

    /**
     * @brief Get registered effect count
     * @return Number of registered effects
     */
    virtual uint16_t getRegisteredCount() const = 0;
};

} // namespace plugins
} // namespace lightwaveos
