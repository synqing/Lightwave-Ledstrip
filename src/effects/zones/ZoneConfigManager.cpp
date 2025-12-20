#include "ZoneConfigManager.h"
#include "ZoneComposer.h"
#include <Arduino.h>

// ============== ZoneConfig Methods ==============

void ZoneConfig::calculateChecksum() {
    // Simple checksum: sum all bytes except checksum itself
    uint16_t sum = 0;
    sum += zoneCount;
    sum += systemEnabled ? 1 : 0;

    for (uint8_t i = 0; i < 4; i++) {
        sum += zoneEffects[i];
        sum += zoneEnabled[i] ? 1 : 0;
        sum += zoneBrightness[i];
        sum += zoneSpeed[i];
        sum += zonePalette[i];
        sum += zoneBlendMode[i];
    }

    checksum = sum;
}

bool ZoneConfig::isValid() const {
    // Recalculate checksum and compare
    uint16_t sum = 0;
    sum += zoneCount;
    sum += systemEnabled ? 1 : 0;

    for (uint8_t i = 0; i < 4; i++) {
        sum += zoneEffects[i];
        sum += zoneEnabled[i] ? 1 : 0;
        sum += zoneBrightness[i];
        sum += zoneSpeed[i];
        sum += zonePalette[i];
        sum += zoneBlendMode[i];
    }

    return (checksum == sum) && (zoneCount >= 1 && zoneCount <= 4);
}

// ============== Preset Definitions ==============

const ZonePreset ZONE_PRESETS[ZONE_PRESET_COUNT] = {
    // Preset 0: Single Zone (Unified - all LEDs one effect)
    {
        "Unified",
        {
            .zoneCount = 1,
            .zoneEffects = {0, 0, 0, 0},  // Effect 0 (Fire)
            .zoneEnabled = {true, false, false, false},
            .zoneBrightness = {255, 255, 255, 255},  // Full brightness
            .zoneSpeed = {25, 25, 25, 25},  // Mid-range speed
            .zonePalette = {0, 0, 0, 0},  // Use global palette
            .zoneBlendMode = {0, 0, 0, 0},  // Default: Overwrite
            .systemEnabled = false,
            .checksum = 0  // Will be calculated on load
        }
    },

    // Preset 1: Dual Split (2 concentric zones)
    {
        "Dual Split",
        {
            .zoneCount = 2,
            .zoneEffects = {0, 5, 0, 0},  // Fire (center) + Ocean (outer)
            .zoneEnabled = {true, true, false, false},
            .zoneBrightness = {255, 200, 255, 255},  // Slightly dimmer outer
            .zoneSpeed = {25, 30, 25, 25},  // Slightly faster outer
            .zonePalette = {0, 0, 0, 0},  // Use global palette
            .zoneBlendMode = {0, 0, 0, 0},  // Default: Overwrite
            .systemEnabled = false,
            .checksum = 0
        }
    },

    // Preset 2: Triple Rings (3 concentric zones)
    {
        "Triple Rings",
        {
            .zoneCount = 3,
            .zoneEffects = {2, 11, 12, 0},  // Wave (2), LGP Wave Collision (11), LGP Diamond Lattice (12)
            .zoneEnabled = {true, true, true, false},
            .zoneBrightness = {255, 220, 180, 255},  // Gradient brightness
            .zoneSpeed = {20, 25, 35, 25},  // Varied speeds
            .zonePalette = {0, 0, 0, 0},  // Use global palette
            .zoneBlendMode = {0, 0, 0, 0},  // Default: Overwrite
            .systemEnabled = false,
            .checksum = 0
        }
    },

    // Preset 3: Quad Zones (All 4 zones active)
    {
        "Quad Active",
        {
            .zoneCount = 4,
            .zoneEffects = {0, 12, 24, 36},  // Varied effects across all zones
            .zoneEnabled = {true, true, true, true},
            .zoneBrightness = {255, 230, 200, 170},  // Gradient: bright center to dim outer
            .zoneSpeed = {15, 25, 35, 45},  // Gradient: slow center to fast outer
            .zonePalette = {0, 0, 0, 0},  // Use global palette
            .zoneBlendMode = {0, 0, 0, 0},  // Default: Overwrite
            .systemEnabled = false,
            .checksum = 0
        }
    },

    // Preset 4: LGP Showcase (LGP physics effects)
    {
        "LGP Showcase",
        {
            .zoneCount = 4,
            .zoneEffects = {8, 15, 24, 35},  // LGP-specific effects
            .zoneEnabled = {true, true, true, true},
            .zoneBrightness = {255, 255, 255, 255},  // Full brightness for showcase
            .zoneSpeed = {20, 25, 30, 25},  // Balanced speeds
            .zonePalette = {0, 0, 0, 0},  // Use global palette
            .zoneBlendMode = {0, 0, 0, 0},  // Default: Overwrite
            .systemEnabled = false,
            .checksum = 0
        }
    }
};

// ============== ZoneConfigManager Implementation ==============

ZoneConfigManager::ZoneConfigManager(ZoneComposer* composer)
    : m_composer(composer)
{
}

bool ZoneConfigManager::saveToNVS() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Serial.printf("❌ NVS open failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Export current configuration
    ZoneConfig config;
    exportConfig(config);
    config.calculateChecksum();

    // Save as blob
    err = nvs_set_blob(handle, NVS_KEY_CONFIG, &config, sizeof(ZoneConfig));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        Serial.println("✅ Zone configuration saved to NVS");
        return true;
    } else {
        Serial.printf("❌ NVS save failed: %s\n", esp_err_to_name(err));
        return false;
    }
}

bool ZoneConfigManager::loadFromNVS() {
    // Ensure NVS is initialized
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        Serial.println("[LOAD] No saved zone configuration found");
        return false;
    } else if (err != ESP_OK) {
        Serial.printf("❌ NVS open failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Load blob
    ZoneConfig config;
    size_t required_size = sizeof(ZoneConfig);
    err = nvs_get_blob(handle, NVS_KEY_CONFIG, &config, &required_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        Serial.println("[LOAD] No saved configuration found");
        return false;
    } else if (err != ESP_OK) {
        Serial.printf("❌ NVS load failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Validate configuration
    if (!config.isValid()) {
        Serial.println("⚠️  Saved configuration invalid (checksum mismatch)");
        return false;
    }

    if (!validateConfig(config)) {
        Serial.println("⚠️  Saved configuration contains invalid values");
        return false;
    }

    // Import valid configuration
    importConfig(config);
    Serial.println("✅ Zone configuration loaded from NVS");
    return true;
}

bool ZoneConfigManager::loadPreset(uint8_t presetId) {
    if (presetId >= ZONE_PRESET_COUNT) {
        Serial.printf("❌ Invalid preset ID %d (valid: 0-%d)\n", presetId, ZONE_PRESET_COUNT - 1);
        return false;
    }

    const ZonePreset& preset = ZONE_PRESETS[presetId];

    // Validate preset config
    if (!validateConfig(preset.config)) {
        Serial.printf("❌ Preset %d contains invalid configuration\n", presetId);
        return false;
    }

    // Import preset configuration
    importConfig(preset.config);
    Serial.printf("✅ Loaded preset %d: %s\n", presetId, preset.name);
    return true;
}

const char* ZoneConfigManager::getPresetName(uint8_t presetId) const {
    if (presetId >= ZONE_PRESET_COUNT) {
        return "Invalid";
    }
    return ZONE_PRESETS[presetId].name;
}

void ZoneConfigManager::exportConfig(ZoneConfig& config) {
    if (!m_composer) return;

    config.zoneCount = m_composer->getZoneCount();
    config.systemEnabled = m_composer->isEnabled();

    for (uint8_t i = 0; i < 4; i++) {
        config.zoneEffects[i] = m_composer->getZoneEffect(i);
        config.zoneEnabled[i] = m_composer->isZoneEnabled(i);
        config.zoneBrightness[i] = m_composer->getZoneBrightness(i);
        config.zoneSpeed[i] = m_composer->getZoneSpeed(i);
        config.zonePalette[i] = m_composer->getZonePalette(i);
        config.zoneBlendMode[i] = (uint8_t)m_composer->getZoneBlendMode(i);
    }
}

void ZoneConfigManager::importConfig(const ZoneConfig& config) {
    if (!m_composer) return;

    // Apply configuration to composer
    m_composer->setZoneCount(config.zoneCount);

    for (uint8_t i = 0; i < 4; i++) {
        m_composer->setZoneEffect(i, config.zoneEffects[i]);
        m_composer->enableZone(i, config.zoneEnabled[i]);
        m_composer->setZoneBrightness(i, config.zoneBrightness[i]);
        m_composer->setZoneSpeed(i, config.zoneSpeed[i]);
        m_composer->setZonePalette(i, config.zonePalette[i]);
        m_composer->setZoneBlendMode(i, (BlendMode)config.zoneBlendMode[i]);
    }

    // Note: We don't auto-enable the system - user must use "zone on"
    if (config.systemEnabled) {
        m_composer->enable();
    }
}

bool ZoneConfigManager::validateConfig(const ZoneConfig& config) const {
    // Validate zone count
    if (config.zoneCount < 1 || config.zoneCount > 4) {
        return false;
    }

    // Validate effect IDs (0-46 for 47 total effects)
    extern const uint8_t NUM_EFFECTS;
    extern const uint8_t gMasterPaletteCount;
    for (uint8_t i = 0; i < 4; i++) {
        if (config.zoneEffects[i] >= NUM_EFFECTS) {
            return false;
        }
        // Validate speed (1-50)
        if (config.zoneSpeed[i] < 1 || config.zoneSpeed[i] > 50) {
            return false;
        }
        // Validate palette (0=global, 1-N=specific palette)
        if (config.zonePalette[i] > gMasterPaletteCount) {
            return false;
        }
        // Validate blend mode
        if (config.zoneBlendMode[i] >= BLEND_MODE_COUNT) {
            return false;
        }
        // Brightness 0-255 is always valid, no need to check
    }

    return true;
}
