// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// PresetStorage - PSRAM-Primary Preset Persistence
// ============================================================================
// Presets live in PSRAM permanently. NVS is a write-behind cache for
// power-cycle recovery ONLY. If NVS fails, presets are still safe in RAM.
//
// Architecture:
//   - PSRAM: 8 x 64-byte PresetData structs (512 bytes) — source of truth
//   - NVS: Write-behind backup (namespace "tab5prst", keys "slot0".."slot7")
//   - All reads come from PSRAM (instant, cannot fail)
//   - NVS writes are best-effort — failure is logged, never fatal
//   - NVS is NEVER erased. If it's corrupt, we just stop writing to it.
//
// Guarantees:
//   - Presets survive NVS corruption, NVS init failure, heap pressure
//   - Presets survive everything except power-off (NVS handles that)
//   - No code path exists that can silently delete user presets
//
// Usage:
//   PresetStorage::init();                    // Allocate PSRAM, recover from NVS
//   PresetStorage::save(0, presetData);       // Write PSRAM + NVS backup
//   PresetStorage::load(0, presetData);       // Read from PSRAM (instant)
//   PresetStorage::isOccupied(0);             // Check PSRAM (instant)
//   PresetStorage::clear(0);                  // Clear PSRAM + NVS
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
     * Initialise PSRAM-backed preset storage.
     * Allocates PSRAM buffer, then attempts NVS recovery for power-cycle data.
     * If NVS is unavailable or corrupt, presets start empty but storage works.
     * @return true if PSRAM allocation succeeded (NVS failure is non-fatal)
     */
    static bool init();

    /**
     * Check if preset storage is ready (PSRAM allocated).
     * @return true if init() succeeded
     */
    static bool isReady();

    // ========================================================================
    // Slot Operations (all read from PSRAM, NVS is write-behind only)
    // ========================================================================

    /**
     * Save a preset to a slot.
     * Writes to PSRAM immediately (cannot fail), then attempts NVS backup.
     * @param slotIndex Slot index (0-7)
     * @param preset PresetData to save (modified: checksum/timestamp updated)
     * @return true always (PSRAM write cannot fail)
     */
    static bool save(uint8_t slotIndex, PresetData& preset);

    /**
     * Load a preset from a slot.
     * Reads directly from PSRAM — instant, zero NVS dependency.
     * @param slotIndex Slot index (0-7)
     * @param preset PresetData to fill
     * @return true if slot contains a valid occupied preset
     */
    static bool load(uint8_t slotIndex, PresetData& preset);

    /**
     * Clear a preset slot.
     * Clears PSRAM immediately, then attempts NVS cleanup.
     * @param slotIndex Slot index (0-7)
     * @return true always (PSRAM clear cannot fail)
     */
    static bool clear(uint8_t slotIndex);

    /**
     * Check if a slot contains valid preset data.
     * Reads from PSRAM — instant, no NVS access.
     * @param slotIndex Slot index (0-7)
     * @return true if slot has valid, occupied preset
     */
    static bool isOccupied(uint8_t slotIndex);

    // ========================================================================
    // Batch Operations
    // ========================================================================

    /**
     * Get occupancy status of all 8 slots (from PSRAM).
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

    // ========================================================================
    // NVS Health Query
    // ========================================================================

    /**
     * Check if NVS backing store is functional.
     * @return true if NVS writes are succeeding
     */
    static bool isNvsHealthy() { return s_nvsHealthy; }

private:
    // NVS namespace for presets (separate from parameters)
    static constexpr const char* NVS_NAMESPACE = "tab5prst";

    // Generate key for slot index ("slot0", "slot1", ..., "slot7")
    static void getKey(uint8_t slotIndex, char key[8]);

    // Attempt to write a single slot to NVS (best-effort, never fatal)
    static void nvsBackupSlot(uint8_t slotIndex);

    // Attempt to erase a single slot from NVS (best-effort, never fatal)
    static void nvsEraseSlot(uint8_t slotIndex);

    // Internal state
    static bool s_initialized;
    static bool s_nvsHealthy;
    static nvs_handle_t s_handle;

    // PSRAM-resident preset buffer — source of truth
    static PresetData* s_presets;  // heap_caps_malloc(PSRAM), 8 slots
};

#endif // PRESET_STORAGE_H
