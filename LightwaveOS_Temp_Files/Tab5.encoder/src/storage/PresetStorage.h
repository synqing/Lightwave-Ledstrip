#pragma once
// ============================================================================
// PresetStorage - NVS Persistence for 8 Preset Slots
// ============================================================================
// Stores PresetData blobs in ESP-IDF NVS for non-volatile preset recall.
//
// NVS Layout:
//   Namespace: "tab5prst"
//   Keys: "slot0", "slot1", ..., "slot7" (64-byte blobs)
//
// Features:
//   - 8 independent preset slots (one per Unit-B encoder)
//   - Atomic save/load of 64-byte PresetData structs
//   - Checksum validation on load
//   - Slot occupancy tracking without full load
//
// Usage:
//   PresetStorage::init();                    // Call after NvsStorage::init()
//   PresetStorage::save(0, presetData);       // Save to slot 0
//   PresetStorage::load(0, presetData);       // Load from slot 0
//   PresetStorage::isOccupied(0);             // Check if slot 0 has data
//   PresetStorage::clear(0);                  // Clear slot 0
// ============================================================================

#ifndef PRESET_STORAGE_H
#define PRESET_STORAGE_H

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "PresetData.h"

class PresetStorage {
public:
    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize preset storage namespace.
     * Call after NvsStorage::init() to ensure NVS flash is initialized.
     * @return true if initialization succeeded
     */
    static bool init();

    /**
     * Check if preset storage is ready.
     * @return true if init() succeeded
     */
    static bool isReady();

    // ========================================================================
    // Slot Operations
    // ========================================================================

    /**
     * Save a preset to a slot.
     * Updates checksum and timestamp automatically.
     * @param slotIndex Slot index (0-7)
     * @param preset PresetData to save (modified: checksum/timestamp updated)
     * @return true if save succeeded
     */
    static bool save(uint8_t slotIndex, PresetData& preset);

    /**
     * Load a preset from a slot.
     * Validates checksum before returning.
     * @param slotIndex Slot index (0-7)
     * @param preset PresetData to fill
     * @return true if load succeeded and data is valid
     */
    static bool load(uint8_t slotIndex, PresetData& preset);

    /**
     * Clear a preset slot (mark as empty).
     * @param slotIndex Slot index (0-7)
     * @return true if clear succeeded
     */
    static bool clear(uint8_t slotIndex);

    /**
     * Check if a slot contains valid preset data.
     * More efficient than full load - only reads header.
     * @param slotIndex Slot index (0-7)
     * @return true if slot has valid, occupied preset
     */
    static bool isOccupied(uint8_t slotIndex);

    // ========================================================================
    // Batch Operations
    // ========================================================================

    /**
     * Get occupancy status of all 8 slots.
     * @return Bitmask where bit N indicates slot N is occupied
     */
    static uint8_t getOccupancyMask();

    /**
     * Clear all preset slots.
     * @return Number of slots successfully cleared
     */
    static uint8_t clearAll();

    /**
     * Count occupied slots.
     * @return Number of slots with valid presets (0-8)
     */
    static uint8_t countOccupied();

private:
    // NVS namespace for presets (separate from parameters)
    static constexpr const char* NVS_NAMESPACE = "tab5prst";

    // Generate key for slot index ("slot0", "slot1", ..., "slot7")
    static void getKey(uint8_t slotIndex, char key[8]);

    // Internal state
    static bool s_initialized;
    static nvs_handle_t s_handle;
};

#endif // PRESET_STORAGE_H
