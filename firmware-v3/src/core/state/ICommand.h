// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#include "SystemState.h"

namespace lightwaveos {
namespace state {

/**
 * Command interface for CQRS pattern
 *
 * Commands represent state mutations. Each command:
 * - Takes current state as input
 * - Returns new state as output
 * - Is immutable and replayable
 * - Has a descriptive name for logging/debugging
 *
 * Commands are the ONLY way to modify system state.
 * This ensures:
 * - All state changes are traceable
 * - State transitions are testable
 * - Changes can be logged/audited
 * - Time-travel debugging is possible
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * Apply this command to the current state
     *
     * @param current The current system state (immutable)
     * @return New system state with command applied
     *
     * Implementation MUST:
     * - Not modify 'current' state
     * - Return new state via with*() methods
     * - Increment version number
     * - Complete in < 1ms
     */
    virtual SystemState apply(const SystemState& current) const = 0;

    /**
     * Get command name for logging/debugging
     *
     * @return Human-readable command name
     *
     * Examples: "SetEffect", "EnableZone", "TriggerTransition"
     */
    virtual const char* getName() const = 0;

    /**
     * Validate command parameters (optional)
     *
     * @param current Current system state
     * @return true if command is valid, false otherwise
     *
     * Default implementation always returns true.
     * Override to add validation logic.
     */
    virtual bool validate(const SystemState& current) const {
        (void)current;  // Unused parameter
        return true;
    }
};

} // namespace state
} // namespace lightwaveos
