#pragma once
// ============================================================================
// NvsStorage - Non-Volatile Storage for Tab5.encoder Parameters
// ============================================================================
// Provides persistent storage for 16 encoder parameters using ESP-IDF NVS API.
// Implements 2-second debounce to prevent flash wear from rapid encoder changes.
//
// Features:
//   - Automatic first-boot initialization with defaults
//   - Debounced saves (2-second delay after last change)
//   - Per-parameter dirty tracking (no unnecessary writes)
//   - Batch save support for shutdown/reset scenarios
//   - Graceful error handling (log, don't crash)
//
// NVS Layout:
//   Namespace: "tab5enc"
//   Keys: "p0", "p1", ..., "p15" (uint16_t values)
//
// Usage:
//   NvsStorage::init();  // Call in setup() before encoder init
//   NvsStorage::loadAllParameters(values);  // Load saved values
//   NvsStorage::requestSave(index, value);  // Queue save with debounce
//   NvsStorage::update();  // Call in loop() to process pending saves
//
// CRITICAL: All methods are static - no heap allocation required.
// ============================================================================

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>

class NvsStorage {
public:
    // Number of parameters to persist
    static constexpr uint8_t PARAM_COUNT = 16;

    // Debounce delay: wait 2 seconds after last change before committing
    static constexpr uint32_t DEBOUNCE_MS = 2000;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize NVS flash and open the namespace.
     * Handles first-boot setup (erases and reinitializes if needed).
     * @return true if NVS initialized successfully
     */
    static bool init();

    /**
     * Check if NVS is ready for operations.
     * @return true if init() succeeded
     */
    static bool isReady();

    // ========================================================================
    // Load Operations
    // ========================================================================

    /**
     * Load all 16 parameters from NVS.
     * Missing keys return default values (from Config.h).
     * @param values Array to fill (must have 16 elements)
     * @return Number of parameters successfully loaded from NVS
     */
    static uint8_t loadAllParameters(uint16_t values[PARAM_COUNT]);

    /**
     * Load a single parameter from NVS.
     * @param index Parameter index (0-15)
     * @param defaultValue Value to return if key not found
     * @return Stored value or defaultValue
     */
    static uint16_t loadParameter(uint8_t index, uint16_t defaultValue);

    // ========================================================================
    // Save Operations (Debounced)
    // ========================================================================

    /**
     * Request a parameter save with debounce.
     * The value is marked dirty and will be written to flash after
     * DEBOUNCE_MS milliseconds of no further changes to that parameter.
     * @param index Parameter index (0-15)
     * @param value New value to save
     */
    static void requestSave(uint8_t index, uint16_t value);

    /**
     * Process pending saves.
     * Call this in the main loop. Writes dirty parameters that have
     * been stable for DEBOUNCE_MS milliseconds.
     */
    static void update();

    /**
     * Force immediate save of all dirty parameters.
     * Bypasses debounce - use for shutdown scenarios.
     */
    static void flushAll();

    // ========================================================================
    // Batch Operations
    // ========================================================================

    /**
     * Save all 16 parameters immediately (no debounce).
     * Use for batch operations like factory reset or shutdown.
     * @param values Array of 16 values to save
     * @return Number of parameters successfully saved
     */
    static uint8_t saveAllParameters(const uint16_t values[PARAM_COUNT]);

    /**
     * Erase all stored parameters and reinitialize.
     * Returns NVS to first-boot state.
     * @return true if erase succeeded
     */
    static bool eraseAll();

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * Get number of pending (dirty) parameters.
     * @return Count of parameters waiting to be saved
     */
    static uint8_t getPendingCount();

    /**
     * Check if any saves are pending.
     * @return true if there are dirty parameters
     */
    static bool hasPending();

private:
    // NVS namespace (max 15 chars)
    static constexpr const char* NVS_NAMESPACE = "tab5enc";

    // Generate key for parameter index ("p0", "p1", ..., "p15")
    static void getKey(uint8_t index, char key[4]);

    // Internal state (all static - no heap allocation)
    static bool s_initialized;
    static nvs_handle_t s_handle;

    // Dirty tracking: one bit per parameter
    static uint16_t s_dirtyFlags;

    // Pending values (buffered until debounce expires)
    static uint16_t s_pendingValues[PARAM_COUNT];

    // Last change timestamp per parameter
    static uint32_t s_lastChange[PARAM_COUNT];

    // Save a single parameter immediately
    static bool saveParameter(uint8_t index, uint16_t value);
};

#endif // NVS_STORAGE_H
