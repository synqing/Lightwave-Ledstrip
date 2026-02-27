// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
#include <stdint.h>

namespace lightwaveos {
namespace plugins {

/**
 * @brief Interface for effect registration
 *
 * Allows effects to be registered by ID without knowing
 * the concrete registry implementation.
 */
class IEffectRegistry {
public:
    virtual ~IEffectRegistry() = default;

    /**
     * @brief Register an effect by ID
     * @param id Effect ID (0-127)
     * @param effect Pointer to IEffect instance (must remain valid)
     * @return true if registration succeeded
     */
    virtual bool registerEffect(uint8_t id, IEffect* effect) = 0;

    /**
     * @brief Unregister an effect by ID
     * @param id Effect ID to unregister
     * @return true if effect was registered and is now unregistered
     */
    virtual bool unregisterEffect(uint8_t id) = 0;

    /**
     * @brief Check if an effect is registered
     * @param id Effect ID to check
     * @return true if effect is registered
     */
    virtual bool isEffectRegistered(uint8_t id) const = 0;

    /**
     * @brief Get registered effect count
     * @return Number of registered effects
     */
    virtual uint8_t getRegisteredCount() const = 0;
};

} // namespace plugins
} // namespace lightwaveos
