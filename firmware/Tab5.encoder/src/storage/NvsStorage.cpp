// ============================================================================
// NvsStorage Implementation
// ============================================================================
// ESP-IDF NVS API for persistent parameter storage with debounced writes.
// ============================================================================

#include "NvsStorage.h"
#include "../config/Config.h"

// ============================================================================
// Static Member Initialization
// ============================================================================

bool NvsStorage::s_initialized = false;
nvs_handle_t NvsStorage::s_handle = 0;
uint16_t NvsStorage::s_dirtyFlags = 0;
uint16_t NvsStorage::s_pendingValues[PARAM_COUNT] = {0};
uint32_t NvsStorage::s_lastChange[PARAM_COUNT] = {0};

// ============================================================================
// Initialization
// ============================================================================

bool NvsStorage::init() {
    if (s_initialized) {
        return true;
    }

    Serial.println("[NVS] Initialising NVS flash...");

    // Initialize NVS flash partition
    esp_err_t err = nvs_flash_init();

    // Handle corrupt or version-mismatch NVS
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("[NVS] NVS partition needs erase - performing first-boot init");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            Serial.printf("[NVS] ERROR: nvs_flash_erase failed: %s\n", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        Serial.printf("[NVS] ERROR: nvs_flash_init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Open namespace in read-write mode
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) {
        Serial.printf("[NVS] ERROR: nvs_open('%s') failed: %s\n", NVS_NAMESPACE, esp_err_to_name(err));
        return false;
    }

    s_initialized = true;
    Serial.printf("[NVS] Initialized successfully (namespace: '%s')\n", NVS_NAMESPACE);

    // Clear dirty flags and pending state
    s_dirtyFlags = 0;
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        s_pendingValues[i] = 0;
        s_lastChange[i] = 0;
    }

    return true;
}

bool NvsStorage::isReady() {
    return s_initialized;
}

// ============================================================================
// Key Generation
// ============================================================================

void NvsStorage::getKey(uint8_t index, char key[4]) {
    // Generate key: "p0", "p1", ..., "p15"
    key[0] = 'p';
    if (index < 10) {
        key[1] = '0' + index;
        key[2] = '\0';
    } else {
        key[1] = '1';
        key[2] = '0' + (index - 10);
        key[3] = '\0';
    }
}

// ============================================================================
// Load Operations
// ============================================================================

uint16_t NvsStorage::loadParameter(uint8_t index, uint16_t defaultValue) {
    if (!s_initialized || index >= PARAM_COUNT) {
        return defaultValue;
    }

    char key[4];
    getKey(index, key);

    uint16_t value = 0;
    esp_err_t err = nvs_get_u16(s_handle, key, &value);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Key not found - return default (normal for first boot)
        return defaultValue;
    } else if (err != ESP_OK) {
        Serial.printf("[NVS] WARN: Failed to load %s: %s\n", key, esp_err_to_name(err));
        return defaultValue;
    }

    return value;
}

uint8_t NvsStorage::loadAllParameters(uint16_t values[PARAM_COUNT]) {
    if (!s_initialized) {
        Serial.println("[NVS] ERROR: loadAllParameters called before init");
        // Fill with defaults
        for (uint8_t i = 0; i < PARAM_COUNT; i++) {
            values[i] = getParameterDefault(static_cast<Parameter>(i));
        }
        return 0;
    }

    uint8_t loadedCount = 0;

    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        // Get default value for this parameter
        uint16_t defaultVal = getParameterDefault(static_cast<Parameter>(i));

        char key[4];
        getKey(i, key);

        uint16_t value = 0;
        esp_err_t err = nvs_get_u16(s_handle, key, &value);

        if (err == ESP_OK) {
            values[i] = value;
            loadedCount++;
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            // First boot - use default
            values[i] = defaultVal;
        } else {
            Serial.printf("[NVS] WARN: Failed to load %s: %s\n", key, esp_err_to_name(err));
            values[i] = defaultVal;
        }
    }

    if (loadedCount == 0) {
        Serial.println("[NVS] First boot - using default values");
    } else {
        Serial.printf("[NVS] Loaded %d/%d parameters from flash\n", loadedCount, PARAM_COUNT);
    }

    return loadedCount;
}

// ============================================================================
// Save Operations (Debounced)
// ============================================================================

void NvsStorage::requestSave(uint8_t index, uint16_t value) {
    if (!s_initialized || index >= PARAM_COUNT) {
        return;
    }

    // Mark parameter as dirty
    s_dirtyFlags |= (1 << index);

    // Store pending value and update timestamp
    s_pendingValues[index] = value;
    s_lastChange[index] = millis();
}

void NvsStorage::update() {
    if (!s_initialized || s_dirtyFlags == 0) {
        return;
    }

    uint32_t now = millis();
    bool didCommit = false;

    // Check each dirty parameter
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        if (s_dirtyFlags & (1 << i)) {
            // Check if debounce period has elapsed
            if (now - s_lastChange[i] >= DEBOUNCE_MS) {
                // Save to NVS
                if (saveParameter(i, s_pendingValues[i])) {
                    // Clear dirty flag
                    s_dirtyFlags &= ~(1 << i);
                    didCommit = true;
                }
            }
        }
    }

    // Commit changes to flash if any were saved
    if (didCommit) {
        esp_err_t err = nvs_commit(s_handle);
        if (err != ESP_OK) {
            Serial.printf("[NVS] WARN: nvs_commit failed: %s\n", esp_err_to_name(err));
        }
    }
}

void NvsStorage::flushAll() {
    if (!s_initialized || s_dirtyFlags == 0) {
        return;
    }

    Serial.println("[NVS] Flushing all pending saves...");

    // Save all dirty parameters immediately
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        if (s_dirtyFlags & (1 << i)) {
            saveParameter(i, s_pendingValues[i]);
            s_dirtyFlags &= ~(1 << i);
        }
    }

    // Commit to flash
    esp_err_t err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[NVS] WARN: nvs_commit failed: %s\n", esp_err_to_name(err));
    } else {
        Serial.println("[NVS] Flush complete");
    }
}

bool NvsStorage::saveParameter(uint8_t index, uint16_t value) {
    if (!s_initialized || index >= PARAM_COUNT) {
        return false;
    }

    char key[4];
    getKey(index, key);

    esp_err_t err = nvs_set_u16(s_handle, key, value);
    if (err != ESP_OK) {
        Serial.printf("[NVS] ERROR: Failed to save %s=%d: %s\n", key, value, esp_err_to_name(err));
        return false;
    }

    // Note: nvs_commit is called by update() after batch of saves
    Serial.printf("[NVS] Saved %s=%d\n", key, value);
    return true;
}

// ============================================================================
// Batch Operations
// ============================================================================

uint8_t NvsStorage::saveAllParameters(const uint16_t values[PARAM_COUNT]) {
    if (!s_initialized) {
        Serial.println("[NVS] ERROR: saveAllParameters called before init");
        return 0;
    }

    uint8_t savedCount = 0;

    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        if (saveParameter(i, values[i])) {
            savedCount++;
        }
    }

    // Commit all changes
    esp_err_t err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[NVS] WARN: nvs_commit failed: %s\n", esp_err_to_name(err));
    }

    Serial.printf("[NVS] Batch saved %d/%d parameters\n", savedCount, PARAM_COUNT);

    // Clear dirty flags since we just saved everything
    s_dirtyFlags = 0;

    return savedCount;
}

bool NvsStorage::eraseAll() {
    if (!s_initialized) {
        Serial.println("[NVS] ERROR: eraseAll called before init");
        return false;
    }

    Serial.println("[NVS] Erasing all parameters...");

    // Erase all keys in our namespace
    esp_err_t err = nvs_erase_all(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[NVS] ERROR: nvs_erase_all failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Commit the erase
    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        Serial.printf("[NVS] ERROR: nvs_commit failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Clear dirty flags
    s_dirtyFlags = 0;

    Serial.println("[NVS] All parameters erased");
    return true;
}

// ============================================================================
// Statistics
// ============================================================================

uint8_t NvsStorage::getPendingCount() {
    uint8_t count = 0;
    uint16_t flags = s_dirtyFlags;

    while (flags) {
        count += flags & 1;
        flags >>= 1;
    }

    return count;
}

bool NvsStorage::hasPending() {
    return s_dirtyFlags != 0;
}
