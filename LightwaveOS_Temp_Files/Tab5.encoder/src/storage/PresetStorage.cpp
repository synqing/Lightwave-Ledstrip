// ============================================================================
// PresetStorage - NVS Persistence Implementation
// ============================================================================

#include "PresetStorage.h"
#include "../config/AnsiColors.h"
#include <cstring>
#include "../config/AnsiColors.h"

// Static member initialization
bool PresetStorage::s_initialized = false;
nvs_handle_t PresetStorage::s_handle = 0;

// ============================================================================
// Initialization
// ============================================================================

bool PresetStorage::init() {
    if (s_initialized) {
        return true;
    }

    // Open NVS namespace for presets
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND || err == ESP_ERR_NVS_NOT_INITIALIZED) {
        // NVS flash not initialized - try to initialize it
        Serial.println("[PresetStorage] NVS not initialized, initializing...");
        err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            // WARNING: This will erase ALL NVS data including presets!
            // Only do this if absolutely necessary (NVS format changed)
            Serial.println("[PresetStorage] WARNING: NVS version mismatch - erasing entire NVS partition!");
            Serial.println("[PresetStorage] WARNING: This will DELETE ALL PRESETS and other stored data!");
            Serial.println("[PresetStorage] This should only happen after ESP-IDF version upgrade.");
            // Close any existing handle first
            if (s_handle != 0) {
                nvs_close(s_handle);
                s_handle = 0;
            }
            // Erase entire NVS partition (unavoidable for version mismatch)
            err = nvs_flash_erase();
            if (err == ESP_OK) {
                err = nvs_flash_init();
            }
        }

        if (err != ESP_OK) {
            Serial.printf("[PresetStorage] NVS flash init failed: %s\n", esp_err_to_name(err));
            return false;
        }

        // Retry namespace open
        err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    }

    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] Failed to open NVS namespace: %s\n", esp_err_to_name(err));
        return false;
    }

    s_initialized = true;
    Serial.printf("[PresetStorage] Initialized, %d slots occupied\n", countOccupied());
    return true;
}

bool PresetStorage::isReady() {
    return s_initialized;
}

// ============================================================================
// Key Generation
// ============================================================================

void PresetStorage::getKey(uint8_t slotIndex, char key[8]) {
    // Clamp to valid range
    if (slotIndex >= PRESET_SLOT_COUNT) {
        slotIndex = PRESET_SLOT_COUNT - 1;
    }

    snprintf(key, 8, "slot%d", slotIndex);
}

// ============================================================================
// Slot Operations
// ============================================================================

bool PresetStorage::save(uint8_t slotIndex, PresetData& preset) {
    if (!s_initialized || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    // Ensure preset is properly marked
    preset.magic = PresetData::MAGIC;
    preset.version = PresetData::CURRENT_VERSION;
    preset.markOccupied();  // Sets occupied, timestamp, and checksum

    char key[8];
    getKey(slotIndex, key);

    esp_err_t err = nvs_set_blob(s_handle, key, &preset, sizeof(PresetData));
    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] Save slot %d failed: %s\n", slotIndex, esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] Commit slot %d failed: %s\n", slotIndex, esp_err_to_name(err));
        return false;
    }

    Serial.printf("[PresetStorage] Saved slot %d (effect=%d, brightness=%d, palette=%d)\n",
                  slotIndex, preset.effectId, preset.brightness, preset.paletteId);
    return true;
}

bool PresetStorage::load(uint8_t slotIndex, PresetData& preset) {
    if (!s_initialized || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    char key[8];
    getKey(slotIndex, key);

    size_t length = sizeof(PresetData);
    esp_err_t err = nvs_get_blob(s_handle, key, &preset, &length);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Slot is empty - not an error, just unoccupied
        preset.clear();
        return false;
    }

    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] Load slot %d failed: %s\n", slotIndex, esp_err_to_name(err));
        preset.clear();
        return false;
    }

    if (length != sizeof(PresetData)) {
        Serial.printf("[PresetStorage] Slot %d size mismatch: got %u, expected %u\n",
                      slotIndex, length, sizeof(PresetData));
        preset.clear();
        return false;
    }

    // Validate loaded data
    if (!preset.isValid()) {
        Serial.printf("[PresetStorage] Slot %d checksum/magic invalid\n", slotIndex);
        preset.clear();
        return false;
    }

    Serial.printf("[PresetStorage] Loaded slot %d (effect=%d, brightness=%d, palette=%d)\n",
                  slotIndex, preset.effectId, preset.brightness, preset.paletteId);
    return true;
}

bool PresetStorage::clear(uint8_t slotIndex) {
    if (!s_initialized || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    char key[8];
    getKey(slotIndex, key);

    esp_err_t err = nvs_erase_key(s_handle, key);

    // ESP_ERR_NVS_NOT_FOUND means key doesn't exist - that's fine for clear
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        Serial.printf("[PresetStorage] Clear slot %d failed: %s\n", slotIndex, esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[PresetStorage] Commit clear slot %d failed: %s\n", slotIndex, esp_err_to_name(err));
        return false;
    }

    Serial.printf("[PresetStorage] Cleared slot %d\n", slotIndex);
    return true;
}

bool PresetStorage::isOccupied(uint8_t slotIndex) {
    if (!s_initialized || slotIndex >= PRESET_SLOT_COUNT) {
        return false;
    }

    char key[8];
    getKey(slotIndex, key);

    // Just check if the key exists and has valid header
    // Read only the first 4 bytes (magic + version + first byte of data)
    struct {
        uint16_t magic;
        uint8_t version;
        uint8_t occupied;
    } header;

    // Actually we need to read the full blob to check occupied flag position
    // But we can use a simpler approach: try to get blob length
    size_t length = 0;
    esp_err_t err = nvs_get_blob(s_handle, key, nullptr, &length);

    if (err == ESP_ERR_NVS_NOT_FOUND || length != sizeof(PresetData)) {
        return false;
    }

    // Key exists with correct size - load and validate
    PresetData preset;
    if (!load(slotIndex, preset)) {
        return false;
    }

    return preset.occupied && preset.isValid();
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
