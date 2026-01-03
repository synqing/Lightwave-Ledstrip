/**
 * @file ZoneConfigManager.cpp
 * @brief Zone-specific persistence manager implementation
 *
 * LightwaveOS v2 - Persistence System
 */

#include "ZoneConfigManager.h"
#include <Arduino.h>
#include <cstring>
#include "../../effects/zones/ZoneDefinition.h"

using namespace lightwaveos::zones;

namespace lightwaveos {
namespace persistence {

// ==================== Version 1 Migration Structure ====================

struct ZoneConfigDataV1 {
    uint8_t version;
    ZoneLayout layout;
    bool systemEnabled;
    uint8_t zoneEffects[MAX_ZONES];
    bool zoneEnabled[MAX_ZONES];
    uint8_t zoneBrightness[MAX_ZONES];
    uint8_t zoneSpeed[MAX_ZONES];
    uint8_t zonePalette[MAX_ZONES];
    uint8_t zoneBlendMode[MAX_ZONES];
    uint32_t checksum;
};

// ==================== ZoneConfigData Implementation ====================

void ZoneConfigData::calculateChecksum() {
    // Calculate CRC32 over all fields except checksum
    const size_t dataSize = offsetof(ZoneConfigData, checksum);
    checksum = NVSManager::calculateCRC32(this, dataSize);
}

bool ZoneConfigData::isValid() const {
    // Recalculate checksum and compare
    const size_t dataSize = offsetof(ZoneConfigData, checksum);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (checksum == calculated);
}

// ==================== SystemConfigData Implementation ====================

void SystemConfigData::calculateChecksum() {
    const size_t dataSize = offsetof(SystemConfigData, checksum);
    checksum = NVSManager::calculateCRC32(this, dataSize);
}

bool SystemConfigData::isValid() const {
    const size_t dataSize = offsetof(SystemConfigData, checksum);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (checksum == calculated);
}

// ==================== Preset Definitions ====================

const ZonePreset ZONE_PRESETS[ZONE_PRESET_COUNT] = {
    // Preset 0: Unified - Single zone covering all LEDs
    {
        "Unified",
        {
            .version = 2,
            .segments = {},  // Will be set in loadPreset
            .zoneCount = 3,
            .systemEnabled = false,  // User must enable manually
            .zoneEffects = {0, 0, 0, 0},  // Fire
            .zoneEnabled = {true, false, false, false},
            .zoneBrightness = {255, 255, 255, 255},
            .zoneSpeed = {25, 25, 25, 25},
            .zonePalette = {0, 0, 0, 0},  // Global palette
            .zoneBlendMode = {0, 0, 0, 0},  // Overwrite
            .checksum = 0
        }
    },

    // Preset 1: Dual Split - 2 zone configuration
    {
        "Dual Split",
        {
            .version = 2,
            .segments = {},
            .zoneCount = 3,  // Use 3-zone layout, but only enable 2
            .systemEnabled = false,
            .zoneEffects = {0, 5, 0, 0},  // Fire + Juggle
            .zoneEnabled = {true, true, false, false},
            .zoneBrightness = {255, 200, 255, 255},
            .zoneSpeed = {25, 30, 25, 25},
            .zonePalette = {0, 0, 0, 0},
            .zoneBlendMode = {0, 0, 0, 0},
            .checksum = 0
        }
    },

    // Preset 2: Triple Rings - 3 concentric zones
    {
        "Triple Rings",
        {
            .version = 2,
            .segments = {},
            .zoneCount = 3,
            .systemEnabled = false,
            .zoneEffects = {2, 8, 10, 0},  // Plasma, Ripple, Interference
            .zoneEnabled = {true, true, true, false},
            .zoneBrightness = {255, 220, 180, 255},
            .zoneSpeed = {20, 25, 35, 25},
            .zonePalette = {0, 0, 0, 0},
            .zoneBlendMode = {0, 0, 0, 0},
            .checksum = 0
        }
    },

    // Preset 3: Quad Active - All 4 zones active
    {
        "Quad Active",
        {
            .version = 2,
            .segments = {},
            .zoneCount = 4,
            .systemEnabled = false,
            .zoneEffects = {0, 4, 8, 12},  // Fire, Sinelon, Ripple, Pulse
            .zoneEnabled = {true, true, true, true},
            .zoneBrightness = {255, 230, 200, 170},  // Gradient brightness
            .zoneSpeed = {15, 25, 35, 45},  // Gradient speed
            .zonePalette = {0, 0, 0, 0},
            .zoneBlendMode = {0, 0, 0, 0},
            .checksum = 0
        }
    },

    // Preset 4: LGP Showcase - Light Guide Plate effects
    {
        "LGP Showcase",
        {
            .version = 2,
            .segments = {},
            .zoneCount = 4,
            .systemEnabled = false,
            .zoneEffects = {10, 2, 8, 0},  // Interference, Plasma, Ripple, Fire
            .zoneEnabled = {true, true, true, true},
            .zoneBrightness = {255, 255, 255, 255},
            .zoneSpeed = {20, 25, 30, 25},
            .zonePalette = {0, 0, 0, 0},
            .zoneBlendMode = {0, 0, 0, 0},
            .checksum = 0
        }
    }
};

// ==================== ZoneConfigManager Implementation ====================

ZoneConfigManager::ZoneConfigManager(ZoneComposer* composer)
    : m_composer(composer)
    , m_lastError(NVSResult::OK) {
}

// ==================== NVS Operations ====================

bool ZoneConfigManager::saveToNVS() {
    if (!m_composer) {
        Serial.println("[ZoneConfig] ERROR: No composer reference");
        m_lastError = NVSResult::INVALID_HANDLE;
        return false;
    }

    // Ensure NVS is initialized
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            Serial.println("[ZoneConfig] ERROR: Failed to initialize NVS");
            m_lastError = NVSResult::NOT_INITIALIZED;
            return false;
        }
    }

    // Export current configuration
    ZoneConfigData config;
    exportConfig(config);
    config.calculateChecksum();

    // Save to NVS
    m_lastError = NVS_MANAGER.saveBlob(NVS_NAMESPACE, NVS_KEY_ZONES, &config, sizeof(config));

    if (m_lastError == NVSResult::OK) {
        Serial.println("[ZoneConfig] Zone configuration saved to NVS");
        return true;
    } else {
        Serial.printf("[ZoneConfig] ERROR: Save failed: %s\n",
                      NVSManager::resultToString(m_lastError));
        return false;
    }
}

bool ZoneConfigManager::loadFromNVS() {
    if (!m_composer) {
        Serial.println("[ZoneConfig] ERROR: No composer reference");
        m_lastError = NVSResult::INVALID_HANDLE;
        return false;
    }

    // Ensure NVS is initialized
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            Serial.println("[ZoneConfig] ERROR: Failed to initialize NVS");
            m_lastError = NVSResult::NOT_INITIALIZED;
            return false;
        }
    }

    // Load from NVS
    ZoneConfigData config;
    m_lastError = NVS_MANAGER.loadBlob(NVS_NAMESPACE, NVS_KEY_ZONES, &config, sizeof(config));

    if (m_lastError == NVSResult::NOT_FOUND) {
        Serial.println("[ZoneConfig] No saved configuration found (first boot)");
        return false;
    }

    if (m_lastError != NVSResult::OK) {
        Serial.printf("[ZoneConfig] ERROR: Load failed: %s\n",
                      NVSManager::resultToString(m_lastError));
        return false;
    }

    // Validate checksum
    if (!config.isValid()) {
        Serial.println("[ZoneConfig] WARNING: Saved config checksum invalid");
        m_lastError = NVSResult::CHECKSUM_ERROR;
        return false;
    }

    // Validate data ranges
    if (!validateConfig(config)) {
        Serial.println("[ZoneConfig] WARNING: Saved config contains invalid values");
        m_lastError = NVSResult::CHECKSUM_ERROR;
        return false;
    }

    // Check version compatibility and migrate if needed
    if (config.version == 1) {
        Serial.println("[ZoneConfig] Migrating from version 1 to version 3");
        // Load as V1 struct
        ZoneConfigDataV1 v1Config;
        memcpy(&v1Config, &config, sizeof(ZoneConfigDataV1));

        // Convert to V3 format
        ZoneConfigData v3Config = {};  // Zero-initialize all fields
        v3Config.version = CONFIG_VERSION;
        v3Config.systemEnabled = v1Config.systemEnabled;
        v3Config.zoneCount = (v1Config.layout == ZoneLayout::QUAD) ? 4 : 3;

        // Convert layout enum to segments
        if (v1Config.layout == ZoneLayout::QUAD) {
            memcpy(v3Config.segments, ZONE_4_CONFIG, sizeof(ZONE_4_CONFIG));
        } else {
            memcpy(v3Config.segments, ZONE_3_CONFIG, sizeof(ZONE_3_CONFIG));
        }

        // Copy zone settings
        memcpy(v3Config.zoneEffects, v1Config.zoneEffects, sizeof(v1Config.zoneEffects));
        memcpy(v3Config.zoneEnabled, v1Config.zoneEnabled, sizeof(v1Config.zoneEnabled));
        memcpy(v3Config.zoneBrightness, v1Config.zoneBrightness, sizeof(v1Config.zoneBrightness));
        memcpy(v3Config.zoneSpeed, v1Config.zoneSpeed, sizeof(v1Config.zoneSpeed));
        memcpy(v3Config.zonePalette, v1Config.zonePalette, sizeof(v1Config.zonePalette));
        memcpy(v3Config.zoneBlendMode, v1Config.zoneBlendMode, sizeof(v1Config.zoneBlendMode));

        // Audio fields stay at defaults (zero-initialized)
        for (uint8_t i = 0; i < MAX_ZONES; i++) {
            v3Config.zoneBeatTriggerInterval[i] = 4;  // Default to 4-beat interval
        }

        // Apply migrated configuration
        importConfig(v3Config);
    } else if (config.version == 2) {
        Serial.println("[ZoneConfig] Migrating from version 2 to version 3");
        // V2 has same base fields as V3, just missing audio config
        // Zero-initialize audio fields and copy the rest
        ZoneConfigData v3Config = {};
        v3Config.version = CONFIG_VERSION;
        v3Config.systemEnabled = config.systemEnabled;
        v3Config.zoneCount = config.zoneCount;
        memcpy(v3Config.segments, config.segments, sizeof(config.segments));
        memcpy(v3Config.zoneEffects, config.zoneEffects, sizeof(config.zoneEffects));
        memcpy(v3Config.zoneEnabled, config.zoneEnabled, sizeof(config.zoneEnabled));
        memcpy(v3Config.zoneBrightness, config.zoneBrightness, sizeof(config.zoneBrightness));
        memcpy(v3Config.zoneSpeed, config.zoneSpeed, sizeof(config.zoneSpeed));
        memcpy(v3Config.zonePalette, config.zonePalette, sizeof(config.zonePalette));
        memcpy(v3Config.zoneBlendMode, config.zoneBlendMode, sizeof(config.zoneBlendMode));

        // Audio fields stay at defaults (zero-initialized)
        for (uint8_t i = 0; i < MAX_ZONES; i++) {
            v3Config.zoneBeatTriggerInterval[i] = 4;  // Default to 4-beat interval
        }

        // Apply migrated configuration
        importConfig(v3Config);
    } else if (config.version == CONFIG_VERSION) {
        // Apply configuration
        importConfig(config);
    } else {
        Serial.printf("[ZoneConfig] ERROR: Unsupported config version %d (current: %d)\n",
                      config.version, CONFIG_VERSION);
        m_lastError = NVSResult::CHECKSUM_ERROR;
        return false;
    }
    Serial.println("[ZoneConfig] Zone configuration loaded from NVS");
    return true;
}

// ==================== System State Operations ====================

bool ZoneConfigManager::saveSystemState(uint8_t effectId, uint8_t brightness,
                                        uint8_t speed, uint8_t paletteId) {
    // Ensure NVS is initialized
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            m_lastError = NVSResult::NOT_INITIALIZED;
            return false;
        }
    }

    SystemConfigData config;
    config.version = CONFIG_VERSION;
    config.effectId = effectId;
    config.brightness = brightness;
    config.speed = speed;
    config.paletteId = paletteId;
    config.calculateChecksum();

    m_lastError = NVS_MANAGER.saveBlob(NVS_NS_SYSTEM, NVS_KEY_STATE, &config, sizeof(config));

    if (m_lastError == NVSResult::OK) {
        Serial.println("[ZoneConfig] System state saved to NVS");
        return true;
    } else {
        Serial.printf("[ZoneConfig] ERROR: Save system state failed: %s\n",
                      NVSManager::resultToString(m_lastError));
        return false;
    }
}

bool ZoneConfigManager::loadSystemState(uint8_t& effectId, uint8_t& brightness,
                                        uint8_t& speed, uint8_t& paletteId) {
    // Ensure NVS is initialized
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            m_lastError = NVSResult::NOT_INITIALIZED;
            return false;
        }
    }

    SystemConfigData config;
    m_lastError = NVS_MANAGER.loadBlob(NVS_NS_SYSTEM, NVS_KEY_STATE, &config, sizeof(config));

    if (m_lastError == NVSResult::NOT_FOUND) {
        Serial.println("[ZoneConfig] No saved system state (first boot)");
        return false;
    }

    if (m_lastError != NVSResult::OK) {
        Serial.printf("[ZoneConfig] ERROR: Load system state failed: %s\n",
                      NVSManager::resultToString(m_lastError));
        return false;
    }

    // Validate checksum
    if (!config.isValid()) {
        Serial.println("[ZoneConfig] WARNING: System state checksum invalid");
        m_lastError = NVSResult::CHECKSUM_ERROR;
        return false;
    }

    // Validate and clamp values
    effectId = (config.effectId < MAX_EFFECT_ID) ? config.effectId : 0;
    brightness = config.brightness;  // Full range 0-255 is valid
    speed = (config.speed >= MIN_SPEED && config.speed <= MAX_SPEED) ? config.speed : 25;
    paletteId = (config.paletteId <= MAX_PALETTE_ID) ? config.paletteId : 0;

    Serial.println("[ZoneConfig] System state loaded from NVS");
    return true;
}

// ==================== Preset Management ====================

bool ZoneConfigManager::loadPreset(uint8_t presetId) {
    if (presetId >= ZONE_PRESET_COUNT) {
        Serial.printf("[ZoneConfig] ERROR: Invalid preset ID %d (valid: 0-%d)\n",
                      presetId, ZONE_PRESET_COUNT - 1);
        return false;
    }

    const ZonePreset& preset = ZONE_PRESETS[presetId];
    
    // Create a config with segments filled in
    ZoneConfigData config = preset.config;
    
    // Set segments based on zoneCount
    if (config.zoneCount == 4) {
        memcpy(config.segments, ZONE_4_CONFIG, sizeof(ZONE_4_CONFIG));
    } else {
        memcpy(config.segments, ZONE_3_CONFIG, sizeof(ZONE_3_CONFIG));
    }

    // Validate preset config
    if (!validateConfig(config)) {
        Serial.printf("[ZoneConfig] ERROR: Preset %d contains invalid values\n", presetId);
        return false;
    }

    // Apply preset
    importConfig(config);
    Serial.printf("[ZoneConfig] Loaded preset %d: %s\n", presetId, preset.name);
    return true;
}

const char* ZoneConfigManager::getPresetName(uint8_t presetId) {
    if (presetId >= ZONE_PRESET_COUNT) {
        return "Invalid";
    }
    return ZONE_PRESETS[presetId].name;
}

// ==================== Config Export/Import ====================

void ZoneConfigManager::exportConfig(ZoneConfigData& config) {
    if (!m_composer) return;

    config.version = CONFIG_VERSION;
    config.systemEnabled = m_composer->isEnabled();
    config.zoneCount = m_composer->getZoneCount();
    
    // Copy zone segments
    const ZoneSegment* segments = m_composer->getZoneConfig();
    memcpy(config.segments, segments, config.zoneCount * sizeof(ZoneSegment));

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        config.zoneEffects[i] = m_composer->getZoneEffect(i);
        config.zoneEnabled[i] = m_composer->isZoneEnabled(i);
        config.zoneBrightness[i] = m_composer->getZoneBrightness(i);
        config.zoneSpeed[i] = m_composer->getZoneSpeed(i);
        config.zonePalette[i] = m_composer->getZonePalette(i);
        config.zoneBlendMode[i] = static_cast<uint8_t>(m_composer->getZoneBlendMode(i));

        // Export audio config (v3)
        ZoneAudioConfig audioConfig = m_composer->getZoneAudioConfig(i);
        config.zoneTempoSync[i] = audioConfig.tempoSync;
        config.zoneBeatModulation[i] = audioConfig.beatModulation;
        config.zoneTempoSpeedScale[i] = audioConfig.tempoSpeedScale;
        config.zoneBeatDecay[i] = audioConfig.beatDecay;
        config.zoneAudioBand[i] = audioConfig.audioBand;

        // Export beat trigger config (v3)
        bool beatEnabled = false;
        uint8_t beatInterval = 4;
        uint8_t effectList[8] = {0};
        uint8_t effectCount = 0;
        uint8_t currentIndex = 0;  // Not persisted (runtime state)
        m_composer->getZoneBeatTriggerConfig(i, beatEnabled, beatInterval, effectList, effectCount, currentIndex);
        config.zoneBeatTriggerEnabled[i] = beatEnabled;
        config.zoneBeatTriggerInterval[i] = beatInterval;
        config.zoneEffectListSize[i] = effectCount;
        for (uint8_t j = 0; j < 8; j++) {
            config.zoneEffectList[i][j] = (j < effectCount) ? effectList[j] : 0;
        }
    }
}

void ZoneConfigManager::importConfig(const ZoneConfigData& config) {
    if (!m_composer) return;

    // Set layout first (affects zone count) - use segments array
    if (!m_composer->setLayout(config.segments, config.zoneCount)) {
        Serial.println("[ZoneConfig] ERROR: Failed to set layout from config");
        return;
    }

    // Apply per-zone settings
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        m_composer->setZoneEffect(i, config.zoneEffects[i]);
        m_composer->setZoneEnabled(i, config.zoneEnabled[i]);
        m_composer->setZoneBrightness(i, config.zoneBrightness[i]);
        m_composer->setZoneSpeed(i, config.zoneSpeed[i]);
        m_composer->setZonePalette(i, config.zonePalette[i]);
        m_composer->setZoneBlendMode(i, static_cast<BlendMode>(config.zoneBlendMode[i]));

        // Import audio config (v3)
        m_composer->setZoneTempoSync(i, config.zoneTempoSync[i]);
        m_composer->setZoneBeatModulation(i, config.zoneBeatModulation[i]);
        m_composer->setZoneTempoSpeedScale(i, config.zoneTempoSpeedScale[i]);
        m_composer->setZoneBeatDecay(i, config.zoneBeatDecay[i]);
        m_composer->setZoneAudioBand(i, config.zoneAudioBand[i]);

        // Import beat trigger config (v3)
        m_composer->setZoneBeatTriggerEnabled(i, config.zoneBeatTriggerEnabled[i]);
        m_composer->setZoneBeatTriggerInterval(i, config.zoneBeatTriggerInterval[i]);
        if (config.zoneEffectListSize[i] > 0) {
            m_composer->setZoneBeatTriggerEffectList(i, config.zoneEffectList[i],
                                                     config.zoneEffectListSize[i]);
        }
    }

    // Apply system enabled state
    m_composer->setEnabled(config.systemEnabled);
}

// ==================== Validation ====================

bool ZoneConfigManager::validateConfig(const ZoneConfigData& config) const {
    // Validate zone count
    if (config.zoneCount == 0 || config.zoneCount > MAX_ZONES) {
        return false;
    }
    
    // Validate segments (basic check - full validation done by ZoneComposer)
    for (uint8_t i = 0; i < config.zoneCount; i++) {
        const ZoneSegment& seg = config.segments[i];
        if (seg.s1LeftStart > seg.s1LeftEnd || seg.s1RightStart > seg.s1RightEnd) {
            return false;
        }
        if (seg.s1LeftEnd >= 80 || seg.s1RightStart < 80) {
            return false;
        }
    }

    // Validate per-zone settings
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        // Effect ID range
        if (config.zoneEffects[i] >= MAX_EFFECT_ID) {
            return false;
        }

        // Speed range
        if (config.zoneSpeed[i] < MIN_SPEED || config.zoneSpeed[i] > MAX_SPEED) {
            return false;
        }

        // Palette range
        if (config.zonePalette[i] > MAX_PALETTE_ID) {
            return false;
        }

        // Blend mode range
        if (config.zoneBlendMode[i] >= static_cast<uint8_t>(BlendMode::MODE_COUNT)) {
            return false;
        }

        // Brightness 0-255 is always valid

        // Audio config validation (v3)
        if (config.zoneTempoSpeedScale[i] > 200) {
            return false;
        }
        if (config.zoneAudioBand[i] > 3) {
            return false;
        }

        // Beat trigger validation (v3)
        uint8_t interval = config.zoneBeatTriggerInterval[i];
        if (interval != 1 && interval != 2 && interval != 4 && interval != 8) {
            return false;
        }
        if (config.zoneEffectListSize[i] > 8) {
            return false;
        }
    }

    return true;
}

} // namespace persistence
} // namespace lightwaveos
