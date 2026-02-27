/**
 * @file ZonePresetManager.cpp
 * @brief User-saveable zone preset persistence implementation
 *
 * LightwaveOS v2 - Persistence System
 */

#include "ZonePresetManager.h"
#include "../../effects/zones/ZoneComposer.h"
#include "../../effects/zones/ZoneDefinition.h"
#include "../../effects/zones/BlendMode.h"
#include <Arduino.h>
#include <cstring>

namespace lightwaveos {
namespace persistence {

using namespace lightwaveos::zones;

// ==================== ZonePreset Implementation ====================

void ZonePreset::calculateChecksum() {
    // Calculate CRC32 over all fields except the crc32 field itself
    const size_t dataSize = offsetof(ZonePreset, crc32);
    crc32 = NVSManager::calculateCRC32(this, dataSize);
}

bool ZonePreset::isValid() const {
    // Recalculate checksum and compare
    const size_t dataSize = offsetof(ZonePreset, crc32);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (crc32 == calculated);
}

// ==================== Singleton ====================

ZonePresetManager& ZonePresetManager::instance() {
    static ZonePresetManager instance;
    return instance;
}

ZonePresetManager::ZonePresetManager()
    : m_initialized(false)
    , m_lastError(NVSResult::OK) {
    // Initialise slot occupancy cache
    for (uint8_t i = 0; i < ZONE_PRESET_MAX_SLOTS; i++) {
        m_slotOccupied[i] = false;
    }
}

// ==================== Initialisation ====================

bool ZonePresetManager::init() {
    if (m_initialized) {
        return true;
    }

    // Ensure NVS is ready
    if (!NVS_MANAGER.isInitialized()) {
        if (!NVS_MANAGER.init()) {
            Serial.println("[ZonePreset] ERROR: Failed to initialise NVS");
            m_lastError = NVSResult::NOT_INITIALIZED;
            return false;
        }
    }

    // Scan all slots to build occupancy cache
    char keyBuffer[12];
    size_t blobSize = 0;

    for (uint8_t slot = 0; slot < ZONE_PRESET_MAX_SLOTS; slot++) {
        slotToKey(slot, keyBuffer);
        NVSResult result = NVS_MANAGER.getBlobSize(NVS_NAMESPACE, keyBuffer, &blobSize);
        m_slotOccupied[slot] = (result == NVSResult::OK && blobSize == sizeof(ZonePreset));
    }

    m_initialized = true;
    Serial.println("[ZonePreset] Zone preset manager initialised");

    // Count occupied slots for debug output
    uint8_t occupiedCount = 0;
    for (uint8_t i = 0; i < ZONE_PRESET_MAX_SLOTS; i++) {
        if (m_slotOccupied[i]) occupiedCount++;
    }
    Serial.printf("[ZonePreset] Found %d saved preset(s)\n", occupiedCount);

    return true;
}

// ==================== Helper Methods ====================

void ZonePresetManager::slotToKey(uint8_t slot, char* buffer) const {
    // Generate key like "preset_00", "preset_01", etc.
    snprintf(buffer, 12, "preset_%02d", slot);
}

bool ZonePresetManager::validatePreset(const ZonePreset& preset) const {
    // Validate version
    if (preset.version == 0 || preset.version > PRESET_VERSION) {
        return false;
    }

    // Validate zone count
    if (preset.zoneCount == 0 || preset.zoneCount > ZONE_PRESET_MAX_ZONES) {
        return false;
    }

    // Validate per-zone settings
    for (uint8_t i = 0; i < ZONE_PRESET_MAX_ZONES; i++) {
        const ZonePresetEntry& entry = preset.zones[i];

        // Effect ID range
        if (entry.effectId > MAX_EFFECT_ID) {
            return false;
        }

        // Palette ID range
        if (entry.paletteId > MAX_PALETTE_ID) {
            return false;
        }

        // Speed range
        if (entry.speed < MIN_SPEED || entry.speed > MAX_SPEED) {
            return false;
        }

        // Blend mode range
        if (entry.blendMode > MAX_BLEND_MODE) {
            return false;
        }

        // Segment validation (only for active zones)
        if (i < preset.zoneCount) {
            // Left segment must be in left half (0-79)
            if (entry.s1LeftEnd >= 80) {
                return false;
            }
            if (entry.s1LeftStart > entry.s1LeftEnd) {
                return false;
            }

            // Right segment must be in right half (80-159)
            if (entry.s1RightStart < 80 || entry.s1RightEnd > 159) {
                return false;
            }
            if (entry.s1RightStart > entry.s1RightEnd) {
                return false;
            }
        }

        // Brightness 0-255 is always valid
    }

    // Validate name is null-terminated
    bool hasNull = false;
    for (uint8_t i = 0; i < ZONE_PRESET_NAME_LENGTH; i++) {
        if (preset.name[i] == '\0') {
            hasNull = true;
            break;
        }
    }
    if (!hasNull) {
        return false;
    }

    return true;
}

// ==================== CRUD Operations ====================

NVSResult ZonePresetManager::save(uint8_t slot, const ZonePreset& preset) {
    if (!m_initialized) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (slot >= ZONE_PRESET_MAX_SLOTS) {
        Serial.printf("[ZonePreset] ERROR: Invalid slot %d (valid: 0-%d)\n",
                      slot, ZONE_PRESET_MAX_SLOTS - 1);
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Create a mutable copy for checksum calculation
    ZonePreset presetCopy = preset;
    presetCopy.calculateChecksum();

    // Generate NVS key
    char keyBuffer[12];
    slotToKey(slot, keyBuffer);

    // Save to NVS
    m_lastError = NVS_MANAGER.saveBlob(NVS_NAMESPACE, keyBuffer, &presetCopy, sizeof(ZonePreset));

    if (m_lastError == NVSResult::OK) {
        m_slotOccupied[slot] = true;
        Serial.printf("[ZonePreset] Saved preset to slot %d: '%s'\n", slot, preset.name);
    } else {
        Serial.printf("[ZonePreset] ERROR: Failed to save slot %d: %s\n",
                      slot, NVSManager::resultToString(m_lastError));
    }

    return m_lastError;
}

NVSResult ZonePresetManager::load(uint8_t slot, ZonePreset& preset) {
    if (!m_initialized) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (slot >= ZONE_PRESET_MAX_SLOTS) {
        Serial.printf("[ZonePreset] ERROR: Invalid slot %d (valid: 0-%d)\n",
                      slot, ZONE_PRESET_MAX_SLOTS - 1);
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Generate NVS key
    char keyBuffer[12];
    slotToKey(slot, keyBuffer);

    // Load from NVS
    m_lastError = NVS_MANAGER.loadBlob(NVS_NAMESPACE, keyBuffer, &preset, sizeof(ZonePreset));

    if (m_lastError == NVSResult::NOT_FOUND) {
        Serial.printf("[ZonePreset] Slot %d is empty\n", slot);
        return m_lastError;
    }

    if (m_lastError != NVSResult::OK) {
        Serial.printf("[ZonePreset] ERROR: Failed to load slot %d: %s\n",
                      slot, NVSManager::resultToString(m_lastError));
        return m_lastError;
    }

    // Validate checksum
    if (!preset.isValid()) {
        Serial.printf("[ZonePreset] ERROR: Slot %d checksum invalid\n", slot);
        m_lastError = NVSResult::CHECKSUM_ERROR;
        return m_lastError;
    }

    // Validate data ranges
    if (!validatePreset(preset)) {
        Serial.printf("[ZonePreset] ERROR: Slot %d contains invalid data\n", slot);
        m_lastError = NVSResult::CHECKSUM_ERROR;
        return m_lastError;
    }

    Serial.printf("[ZonePreset] Loaded preset from slot %d: '%s'\n", slot, preset.name);
    return NVSResult::OK;
}

NVSResult ZonePresetManager::list(ZonePresetMetadata* metadata, uint8_t& count) {
    if (!m_initialized) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        count = 0;
        return m_lastError;
    }

    if (metadata == nullptr) {
        m_lastError = NVSResult::INVALID_HANDLE;
        count = 0;
        return m_lastError;
    }

    uint8_t maxCount = count;
    count = 0;

    ZonePreset tempPreset;
    char keyBuffer[12];

    for (uint8_t slot = 0; slot < ZONE_PRESET_MAX_SLOTS && count < maxCount; slot++) {
        // Check cached occupancy first
        if (!m_slotOccupied[slot]) {
            continue;
        }

        // Load preset to get metadata
        slotToKey(slot, keyBuffer);
        NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, keyBuffer, &tempPreset, sizeof(ZonePreset));

        if (result == NVSResult::OK && tempPreset.isValid() && validatePreset(tempPreset)) {
            metadata[count].slot = slot;
            strncpy(metadata[count].name, tempPreset.name, ZONE_PRESET_NAME_LENGTH - 1);
            metadata[count].name[ZONE_PRESET_NAME_LENGTH - 1] = '\0';
            metadata[count].zoneCount = tempPreset.zoneCount;
            metadata[count].timestamp = tempPreset.timestamp;
            metadata[count].occupied = true;
            count++;
        } else {
            // Cache was stale or preset is corrupt
            m_slotOccupied[slot] = false;
        }
    }

    m_lastError = NVSResult::OK;
    return m_lastError;
}

NVSResult ZonePresetManager::remove(uint8_t slot) {
    if (!m_initialized) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (slot >= ZONE_PRESET_MAX_SLOTS) {
        Serial.printf("[ZonePreset] ERROR: Invalid slot %d (valid: 0-%d)\n",
                      slot, ZONE_PRESET_MAX_SLOTS - 1);
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Generate NVS key
    char keyBuffer[12];
    slotToKey(slot, keyBuffer);

    // Erase from NVS
    m_lastError = NVS_MANAGER.eraseKey(NVS_NAMESPACE, keyBuffer);

    if (m_lastError == NVSResult::OK || m_lastError == NVSResult::NOT_FOUND) {
        m_slotOccupied[slot] = false;
        Serial.printf("[ZonePreset] Removed preset from slot %d\n", slot);
        m_lastError = NVSResult::OK;
    } else {
        Serial.printf("[ZonePreset] ERROR: Failed to remove slot %d: %s\n",
                      slot, NVSManager::resultToString(m_lastError));
    }

    return m_lastError;
}

bool ZonePresetManager::isSlotOccupied(uint8_t slot) {
    if (!m_initialized || slot >= ZONE_PRESET_MAX_SLOTS) {
        return false;
    }
    return m_slotOccupied[slot];
}

// ==================== ZoneComposer Integration ====================

void ZonePresetManager::populateFromComposer(ZonePreset& preset, const char* name,
                                              zones::ZoneComposer* composer) {
    // Clear preset structure
    memset(&preset, 0, sizeof(ZonePreset));

    preset.version = PRESET_VERSION;
    preset.zoneCount = composer->getZoneCount();

    // Copy name with null termination
    if (name != nullptr) {
        strncpy(preset.name, name, ZONE_PRESET_NAME_LENGTH - 1);
        preset.name[ZONE_PRESET_NAME_LENGTH - 1] = '\0';
    } else {
        strncpy(preset.name, "Untitled", ZONE_PRESET_NAME_LENGTH - 1);
    }

    // Get current zone segments
    const ZoneSegment* segments = composer->getZoneConfig();

    // Populate per-zone settings
    for (uint8_t i = 0; i < ZONE_PRESET_MAX_ZONES; i++) {
        ZonePresetEntry& entry = preset.zones[i];

        entry.effectId = composer->getZoneEffect(i);
        entry.paletteId = composer->getZonePalette(i);
        entry.brightness = composer->getZoneBrightness(i);
        entry.speed = composer->getZoneSpeed(i);
        entry.blendMode = static_cast<uint8_t>(composer->getZoneBlendMode(i));

        // Copy segment definitions (only for active zones)
        if (i < preset.zoneCount && segments != nullptr) {
            entry.s1LeftStart = segments[i].s1LeftStart;
            entry.s1LeftEnd = segments[i].s1LeftEnd;
            entry.s1RightStart = segments[i].s1RightStart;
            entry.s1RightEnd = segments[i].s1RightEnd;
        } else {
            // Default segment values for inactive zones
            entry.s1LeftStart = 0;
            entry.s1LeftEnd = 79;
            entry.s1RightStart = 80;
            entry.s1RightEnd = 159;
        }
    }

    // Set timestamp (Unix epoch seconds)
    // Use millis()/1000 as a relative timestamp if RTC not available
    // In production, this should come from NTP or RTC
    preset.timestamp = millis() / 1000;
}

void ZonePresetManager::applyToComposer(const ZonePreset& preset,
                                         zones::ZoneComposer* composer) {
    // Build segment array for setLayout
    ZoneSegment segments[ZONE_PRESET_MAX_ZONES];

    for (uint8_t i = 0; i < preset.zoneCount; i++) {
        const ZonePresetEntry& entry = preset.zones[i];

        segments[i].zoneId = i;
        segments[i].s1LeftStart = static_cast<uint8_t>(entry.s1LeftStart);
        segments[i].s1LeftEnd = static_cast<uint8_t>(entry.s1LeftEnd);
        segments[i].s1RightStart = static_cast<uint8_t>(entry.s1RightStart);
        segments[i].s1RightEnd = static_cast<uint8_t>(entry.s1RightEnd);

        // Calculate total LEDs for this zone
        uint8_t leftCount = (entry.s1LeftEnd - entry.s1LeftStart + 1);
        uint8_t rightCount = (entry.s1RightEnd - entry.s1RightStart + 1);
        segments[i].totalLeds = leftCount + rightCount;
    }

    // Set zone layout first (this determines zone count)
    if (!composer->setLayout(segments, preset.zoneCount)) {
        Serial.println("[ZonePreset] ERROR: Failed to set zone layout");
        return;
    }

    // Apply per-zone settings
    for (uint8_t i = 0; i < ZONE_PRESET_MAX_ZONES; i++) {
        const ZonePresetEntry& entry = preset.zones[i];

        composer->setZoneEffect(i, entry.effectId);
        composer->setZonePalette(i, entry.paletteId);
        composer->setZoneBrightness(i, entry.brightness);
        composer->setZoneSpeed(i, entry.speed);
        composer->setZoneBlendMode(i, static_cast<BlendMode>(entry.blendMode));

        // Enable zone if it's within the active zone count
        composer->setZoneEnabled(i, i < preset.zoneCount);
    }
}

NVSResult ZonePresetManager::saveCurrentZones(uint8_t slot, const char* name,
                                               zones::ZoneComposer* composer) {
    if (!m_initialized) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (composer == nullptr) {
        Serial.println("[ZonePreset] ERROR: No ZoneComposer reference");
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    if (slot >= ZONE_PRESET_MAX_SLOTS) {
        Serial.printf("[ZonePreset] ERROR: Invalid slot %d (valid: 0-%d)\n",
                      slot, ZONE_PRESET_MAX_SLOTS - 1);
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Populate preset from current ZoneComposer state
    ZonePreset preset;
    populateFromComposer(preset, name, composer);

    // Save to NVS
    return save(slot, preset);
}

NVSResult ZonePresetManager::applyToZones(uint8_t slot, zones::ZoneComposer* composer) {
    if (!m_initialized) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (composer == nullptr) {
        Serial.println("[ZonePreset] ERROR: No ZoneComposer reference");
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    if (slot >= ZONE_PRESET_MAX_SLOTS) {
        Serial.printf("[ZonePreset] ERROR: Invalid slot %d (valid: 0-%d)\n",
                      slot, ZONE_PRESET_MAX_SLOTS - 1);
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Load preset from NVS
    ZonePreset preset;
    m_lastError = load(slot, preset);

    if (m_lastError != NVSResult::OK) {
        return m_lastError;
    }

    // Apply to ZoneComposer
    applyToComposer(preset, composer);

    Serial.printf("[ZonePreset] Applied preset '%s' to ZoneComposer\n", preset.name);
    return NVSResult::OK;
}

} // namespace persistence
} // namespace lightwaveos
