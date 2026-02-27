// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#include "SystemState.h"
#include "ICommand.h"

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace lightwaveos {
namespace state {

/**
 * Callback signature for state change notifications
 *
 * @param newState The new state after a command was applied
 */
using StateChangeCallback = void(*)(const SystemState& newState);

/**
 * Central state store using CQRS pattern with double-buffering
 *
 * Architecture:
 * - Double-buffered state for lock-free reads
 * - Write mutex protects state transitions
 * - Atomic index swap after state update
 * - Publisher/subscriber pattern for state changes
 *
 * Performance:
 * - Queries: Lock-free, ~10ns latency
 * - Commands: < 1ms with mutex, includes subscriber notifications
 * - State size: ~100 bytes (cache-friendly)
 * - Thread-safe for multi-core ESP32-S3
 *
 * Usage:
 *   StateStore store;
 *
 *   // Query (lock-free)
 *   const SystemState& state = store.getState();
 *
 *   // Command (mutates state)
 *   SetEffectCommand cmd(5);
 *   store.dispatch(cmd);
 *
 *   // Subscribe to changes
 *   store.subscribe(onStateChanged);
 */
class StateStore {
public:
    /**
     * Constructor - initializes double-buffered state
     */
    StateStore();

    /**
     * Destructor - cleans up mutex
     */
    ~StateStore();

    // ==================== Query Methods (Lock-Free) ====================

    /**
     * Get current system state (lock-free read)
     *
     * This method NEVER blocks. Safe to call from:
     * - Render loop (120 FPS)
     * - Network handlers
     * - Any thread
     *
     * @return Const reference to current state
     */
    const SystemState& getState() const;

    /**
     * Get current state version (lock-free read)
     *
     * Useful for optimistic concurrency control.
     *
     * @return Current state version number
     */
    uint32_t getVersion() const;

    /**
     * Get current effect ID (convenience method)
     *
     * @return Current effect ID
     */
    uint8_t getCurrentEffect() const;

    /**
     * Get current palette ID (convenience method)
     *
     * @return Current palette ID
     */
    uint8_t getCurrentPalette() const;

    /**
     * Get current brightness (convenience method)
     *
     * @return Current brightness (0-255)
     */
    uint8_t getBrightness() const;

    /**
     * Get current speed (convenience method)
     *
     * @return Current speed (1-50)
     */
    uint8_t getSpeed() const;

    /**
     * Get zone mode enabled status (convenience method)
     *
     * @return true if zone mode is enabled
     */
    bool isZoneModeEnabled() const;

    /**
     * Get active zone count (convenience method)
     *
     * @return Number of active zones (1-4)
     */
    uint8_t getActiveZoneCount() const;

    /**
     * Get zone configuration (convenience method)
     *
     * @param zoneId Zone index (0-3)
     * @return Zone state, or default if invalid
     */
    ZoneState getZoneConfig(uint8_t zoneId) const;

    /**
     * Check if transition is active (convenience method)
     *
     * @return true if transition is in progress
     */
    bool isTransitionActive() const;

    // ==================== Command Methods (Thread-Safe) ====================

    /**
     * Dispatch a command to modify state
     *
     * Thread-safe mutation with subscriber notification.
     * Commands are validated before application.
     *
     * Performance: < 1ms including notifications
     *
     * @param command Command to apply
     * @return true if command was applied, false if validation failed
     */
    bool dispatch(const ICommand& command);

    /**
     * Dispatch multiple commands atomically
     *
     * All commands are applied in sequence within a single lock.
     * If any command fails validation, no state changes occur.
     *
     * @param commands Array of command pointers
     * @param count Number of commands
     * @return true if all commands succeeded
     */
    bool dispatchBatch(const ICommand* const* commands, uint8_t count);

    // ==================== Subscription Methods ====================

    /**
     * Subscribe to state change notifications
     *
     * Subscribers are called AFTER state is updated, within the write lock.
     * Keep subscriber callbacks FAST (< 100us recommended).
     *
     * Max subscribers: 8
     *
     * @param callback Function to call on state changes
     * @return true if subscribed, false if max subscribers reached
     */
    bool subscribe(StateChangeCallback callback);

    /**
     * Unsubscribe from state change notifications
     *
     * @param callback Function to unsubscribe
     * @return true if unsubscribed, false if not found
     */
    bool unsubscribe(StateChangeCallback callback);

    /**
     * Get subscriber count
     *
     * @return Number of active subscribers
     */
    uint8_t getSubscriberCount() const;

    // ==================== Utility Methods ====================

    /**
     * Reset state to defaults
     *
     * Dispatches commands to restore initial state.
     * Useful for factory reset scenarios.
     */
    void reset();

    /**
     * Get state store statistics
     *
     * @param outCommandCount Output: total commands dispatched
     * @param outLastCommandDuration Output: duration of last command (us)
     */
    void getStats(uint32_t& outCommandCount, uint32_t& outLastCommandDuration) const;

private:
    // ==================== Double-Buffered State ====================

    // Two state copies for lock-free reads
    SystemState m_states[2];

    // Index of active state (0 or 1)
    // Marked volatile for atomic access
    volatile uint8_t m_activeIndex;

    // ==================== Thread Safety ====================

    // Mutex for write protection
    SemaphoreHandle_t m_writeMutex;

    // ==================== Subscribers ====================

    // Maximum number of subscribers
    static constexpr uint8_t MAX_SUBSCRIBERS = 8;

    // Subscriber callbacks
    StateChangeCallback m_subscribers[MAX_SUBSCRIBERS];

    // Number of active subscribers
    uint8_t m_subscriberCount;

    // ==================== Statistics ====================

    // Total commands dispatched
    uint32_t m_commandCount;

    // Duration of last command (microseconds)
    uint32_t m_lastCommandDuration;

    // ==================== Private Methods ====================

    /**
     * Notify all subscribers of state change
     *
     * Called within write lock after state update.
     *
     * @param newState The new state after command application
     */
    void notifySubscribers(const SystemState& newState);

    /**
     * Get inactive state index for writing
     *
     * @return Index of inactive state (1 - m_activeIndex)
     */
    uint8_t getInactiveIndex() const;

    /**
     * Swap active state index atomically
     *
     * This makes the newly written state visible to readers.
     */
    void swapActiveIndex();

    /**
     * Validate and clamp active index to safe range [0, 1]
     *
     * @return Valid index (0 or 1), defaults to 0 if corrupted
     */
    uint8_t validateActiveIndex() const;
};

} // namespace state
} // namespace lightwaveos
