// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// PresetStorage - PSRAM-Primary Implementation
// ============================================================================
// PSRAM is the source of truth. NVS is a write-behind backup.
// No code path here can silently erase user presets.
// ============================================================================

#include "PresetStorage.h"
#include <esp_heap_caps.h>
#include <cstring>

// Static member initialisation
bool PresetStorage::s_initialized = false;
bool PresetStorage::s_nvsHealthy = false;
nvs_handle_t PresetStorage::s_handle = 0;
PresetData* PresetStorage::s_presets = nullptr;

// ============================================================================
// Initialization
// ============================================================================

bool PresetStorage::init() {
    if (s_initialized) {
        return true;
    }

    // ---- Step 1: Allocate PSRAM (the actual storage) ----
    s_presets = static_cast<PresetData*>(
        heap_caps_malloc(sizeof(PresetData) * PRESET_SLOT_COUNT, MALLOC_CAP_SPIRAM));

    if (!s_presets) {
        // PSRAM alloc failed — try internal RAM as last resort (still only 512 bytes)
        s_presets = static_cast<PresetData*>(
            heap_caps_malloc(sizeof(PresetData) * PRESET_SLOT_COUNT, MALLOC_CAP_INTERNAL));
    }

    if (!s_presets) {
        Serial.println("[PresetStorage] FATAL: Cannot allocate 512 bytes for presets");
        return false;
    }

    // Clear all slots to empty defaults
    for (uint8_t i = 0; i < PRESET_SLOT_COUNT; i++) {
        s_presets[i].clear();
    }

    Serial.printf("[PresetStorage] PSRAM buffer allocated at %p (%u bytes)\n",
                  s_presets, sizeof(PresetData) * PRESET_SLOT_COUNT);

    // ---- Step 2: Attempt NVS recovery (best-effort, never fatal) ----
    s_nvsHealthy = false;

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle);

    if (err != ESP_OK) {
        // NVS not available — that's fine, PSRAM is the real storage.
        // Log it but do NOT attempt nvs_flash_init() or nvs_flash_erase().
        // NvsStorage::init() should have already called nvs_flash_init().
        // If that failed, we simply operate without NVS backing.
        Serial.printf("[PresetStorage] NVS unavailable (%s) — operating from PSRAM only\n",
                      esp_err_to_name(err));
        s_initialized = true;
        Serial.printf("[PresetStorage] Initialised (PSRAM-only mode), 0 presets recovered\n");
        return true;
    }

    s_nvsHealthy = true;

    // Recover presets from NVS into PSRAM
    uint8_t recovered = 0;
    for (uint8_t i = 0; i < PRESET_SLOT_COUNT; i++) {
        char key[8];
        getKey(i, key);

        PresetData temp;
        size_t length = sizeof(PresetData);
        err = nvs_get_blob(s_handle, key, &temp, &length);

        if (err != ESP_OK || length != sizeof(PresetData)) {
            continue;  // Slot empty or corrupt — leave PSRAM slot as empty default
        }

        // Migrate v1 presets to v2 format
        if (temp.magic == PresetData::MAGIC && temp.version == PresetData::V1_VERSION) {
            Serial.printf("[PresetStorage] Migrating slot %d from v1 to v2\n", i);
            for (uint8_t z = 0; z < 4; z++) {
                temp.setZoneEffectId(z, 0);
            }
            temp.version = PresetData::CURRENT_VERSION;
            temp.updateChecksum();
        }

        // Validate checksum before trusting NVS data
        if (temp.isValid()) {
            memcpy(&s_presets[i], &temp, sizeof(PresetData));
            recovered++;
            Serial.printf("[PresetStorage] Recovered slot %d from NVS (effect=%u)\n",
                          i, static_cast<unsigned>(temp.getEffectId16()));
        } else {
            Serial.printf("[PresetStorage] Slot %d NVS data invalid (checksum/magic) — skipped\n", i);
        }
    }

    s_initialized = true;
    Serial.printf("[PresetStorage] Initialised, %d presets recovered from NVS\n", recovered);
    return true;
}

bool PresetStorage::isReady() {
    return s_initialized && s_presets != nullptr;
}

// ============================================================================
// Key Generation
// ============================================================================

void PresetStorage::getKey(uint8_t slotIndex, char key[8]) {
    if (slotIndex >= PRESET_SLOT_COUNT) {
        slotIndex = PRESET_SLOT_COUNT - 1;
    }
    snprintf(key, 8, "slot%d", slotIndex);
}

// ============================================================================
// Slot Operations (PSRAM-primary, NVS write-behind)
// ============================================================================

bool PresetStorage::save(uint8_t slotIndex, PresetData& preset) {
    if (!s_initialized || !s_presets || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    // Ensure preset is properly marked
    preset.magic = PresetData::MAGIC;
    preset.version = PresetData::CURRENT_VERSION;
    preset.markOccupied();  // Sets occupied, timestamp, and checksum

    // Write to PSRAM — instant, cannot fail
    memcpy(&s_presets[slotIndex], &preset, sizeof(PresetData));

    Serial.printf("[PresetStorage] Saved slot %d to PSRAM (effect=%u, brightness=%d, palette=%d)\n",
                  slotIndex, static_cast<unsigned>(preset.getEffectId16()),
                  preset.brightness, preset.paletteId);

    // Best-effort NVS backup
    nvsBackupSlot(slotIndex);

    return true;
}

bool PresetStorage::load(uint8_t slotIndex, PresetData& preset) {
    if (!s_initialized || !s_presets || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    // Read directly from PSRAM — zero NVS dependency
    const PresetData& stored = s_presets[slotIndex];

    if (!stored.occupied || stored.magic != PresetData::MAGIC) {
        preset.clear();
        return false;
    }

    memcpy(&preset, &stored, sizeof(PresetData));
    return true;
}

bool PresetStorage::clear(uint8_t slotIndex) {
    if (!s_initialized || !s_presets || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    // Clear in PSRAM
    s_presets[slotIndex].clear();

    Serial.printf("[PresetStorage] Cleared slot %d\n", slotIndex);

    // Best-effort NVS cleanup
    nvsEraseSlot(slotIndex);

    return true;
}

bool PresetStorage::isOccupied(uint8_t slotIndex) {
    if (!s_initialized || !s_presets || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    // Direct PSRAM check — instant, no NVS access
    const PresetData& stored = s_presets[slotIndex];
    return stored.occupied && stored.magic == PresetData::MAGIC;
}

// ============================================================================
// Batch Operations
// ============================================================================

uint8_t PresetStorage::getOccupancyMask() {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < PRESET_SLOT_COUNT; i++) {
        if (isOccupied(i)) {
            mask |= (1 << i);
        }
    }
    return mask;
}

uint8_t PresetStorage::clearAll() {
    uint8_t cleared = 0;
    for (uint8_t i = 0; i < PRESET_SLOT_COUNT; i++) {
        if (clear(i)) {
            cleared++;
        }
    }
    return cleared;
}

uint8_t PresetStorage::countOccupied() {
    uint8_t mask = getOccupancyMask();
    uint8_t count = 0;
    while (mask) {
        count += (mask & 1);
        mask >>= 1;
    }
    return count;
}

// ============================================================================
// NVS Write-Behind (best-effort, never fatal)
// ============================================================================

void PresetStorage::nvsBackupSlot(uint8_t slotIndex) {
    if (!s_nvsHealthy || !s_presets || slotIndex >= PRESET_SLOT_COUNT) {
        return;
    }

    char key[8];
    getKey(slotIndex, key);

    esp_err_t err = nvs_set_blob(s_handle, key, &s_presets[slotIndex], sizeof(PresetData));
    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] NVS backup slot %d failed: %s (PSRAM copy is safe)\n",
                      slotIndex, esp_err_to_name(err));
        // If NVS write fails, mark as unhealthy — stop trying until next boot
        s_nvsHealthy = false;
        return;
    }

    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] NVS commit slot %d failed: %s (PSRAM copy is safe)\n",
                      slotIndex, esp_err_to_name(err));
        s_nvsHealthy = false;
        return;
    }
}

void PresetStorage::nvsEraseSlot(uint8_t slotIndex) {
    if (!s_nvsHealthy || slotIndex >= PRESET_SLOT_COUNT) {
        return;
    }

    char key[8];
    getKey(slotIndex, key);

    esp_err_t err = nvs_erase_key(s_handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        Serial.printf("[PresetStorage] NVS erase slot %d failed: %s\n",
                      slotIndex, esp_err_to_name(err));
        return;
    }

    nvs_commit(s_handle);  // Best-effort, don't care if this fails
}
