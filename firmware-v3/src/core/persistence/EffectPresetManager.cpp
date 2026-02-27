/**
 * @file EffectPresetManager.cpp
 * @brief Effect preset persistence implementation
 *
 * LightwaveOS v2 - Persistence System
 */

#include "EffectPresetManager.h"
#include "../actors/RendererActor.h"

#include <cstring>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_timer.h>
#else
// Native build stubs for testing
#include <cstdio>
#define Serial_println(msg) printf("%s\n", msg)
#define Serial_printf(...) printf(__VA_ARGS__)
namespace { uint32_t millis() { return 0; } }
#endif

namespace lightwaveos {
namespace persistence {

// ==================== EffectPreset Implementation ====================

void EffectPreset::calculateChecksum() {
    // Calculate CRC32 over all fields except crc32 itself
    const size_t dataSize = offsetof(EffectPreset, crc32);
    crc32 = NVSManager::calculateCRC32(this, dataSize);
}

bool EffectPreset::isValid() const {
    // Version check
    if (version != CURRENT_VERSION) {
        return false;
    }

    // Recalculate checksum and compare
    const size_t dataSize = offsetof(EffectPreset, crc32);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (crc32 == calculated);
}

void EffectPreset::reset() {
    version = CURRENT_VERSION;
    effectId = 0;
    paletteId = 0;
    brightness = 96;
    speed = 10;
    memset(name, 0, NAME_MAX_LEN);
    mood = 128;
    trails = 128;
    hue = 0;
    saturation = 255;
    intensity = 128;
    complexity = 128;
    variation = 0;
    timestamp = 0;
    crc32 = 0;
}

// ==================== EffectPresetManager Implementation ====================

EffectPresetManager& EffectPresetManager::instance() {
    static EffectPresetManager inst;
    return inst;
}

EffectPresetManager::EffectPresetManager()
    : m_initialised(false)
    , m_lastError(NVSResult::NOT_INITIALIZED)
    , m_slotBitmap(0)
{
}

bool EffectPresetManager::init() {
    if (m_initialised) {
        return true;
    }

    // Ensure NVS is initialised
    if (!NVS_MANAGER.isInitialized()) {
#ifndef NATIVE_BUILD
        Serial.println("[EffectPreset] ERROR: NVS not initialised");
#endif
        m_lastError = NVSResult::NOT_INITIALIZED;
        return false;
    }

#ifndef NATIVE_BUILD
    Serial.println("[EffectPreset] Initialising preset manager...");
#endif

    // Scan existing presets to build slot bitmap
    scanSlots();

#ifndef NATIVE_BUILD
    Serial.printf("[EffectPreset] Found %u saved presets\n", getPresetCount());
#endif

    m_initialised = true;
    m_lastError = NVSResult::OK;
    return true;
}

void EffectPresetManager::scanSlots() {
    m_slotBitmap = 0;

    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        EffectPreset preset;
        char key[16];
        makeKey(i, key);

        NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

        if (result == NVSResult::OK && preset.isValid()) {
            m_slotBitmap |= (1 << i);
        }
    }
}

void EffectPresetManager::updateSlotBitmap(uint8_t slot, bool occupied) {
    if (slot >= MAX_PRESETS) return;

    if (occupied) {
        m_slotBitmap |= (1 << slot);
    } else {
        m_slotBitmap &= ~(1 << slot);
    }
}

void EffectPresetManager::makeKey(uint8_t slot, char* key) {
    // Format: "preset_00" to "preset_15"
    snprintf(key, 16, "preset_%02u", slot);
}

// ==================== CRUD Operations ====================

NVSResult EffectPresetManager::save(uint8_t slot, const EffectPreset& preset) {
    if (!m_initialised) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (slot >= MAX_PRESETS) {
#ifndef NATIVE_BUILD
        Serial.printf("[EffectPreset] ERROR: Invalid slot %u (max %u)\n", slot, MAX_PRESETS - 1);
#endif
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Create a mutable copy and ensure checksum is calculated
    EffectPreset presetCopy = preset;
    presetCopy.version = EffectPreset::CURRENT_VERSION;
    presetCopy.calculateChecksum();

    char key[16];
    makeKey(slot, key);

    NVSResult result = NVS_MANAGER.saveBlob(NVS_NAMESPACE, key, &presetCopy, sizeof(presetCopy));

    if (result == NVSResult::OK) {
        updateSlotBitmap(slot, true);
#ifndef NATIVE_BUILD
        Serial.printf("[EffectPreset] Saved preset '%s' to slot %u\n", presetCopy.name, slot);
#endif
    } else {
#ifndef NATIVE_BUILD
        Serial.printf("[EffectPreset] ERROR: Failed to save slot %u: %s\n",
                      slot, NVSManager::resultToString(result));
#endif
    }

    m_lastError = result;
    return result;
}

NVSResult EffectPresetManager::load(uint8_t slot, EffectPreset& preset) {
    if (!m_initialised) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (slot >= MAX_PRESETS) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    char key[16];
    makeKey(slot, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result == NVSResult::OK) {
        // Validate checksum
        if (!preset.isValid()) {
#ifndef NATIVE_BUILD
            Serial.printf("[EffectPreset] WARNING: Preset %u has invalid checksum\n", slot);
#endif
            preset.reset();
            m_lastError = NVSResult::CHECKSUM_ERROR;
            return m_lastError;
        }

#ifndef NATIVE_BUILD
        Serial.printf("[EffectPreset] Loaded preset '%s' from slot %u\n", preset.name, slot);
#endif
    } else if (result == NVSResult::NOT_FOUND) {
        // Slot is empty - not an error, just no data
        preset.reset();
    }

    m_lastError = result;
    return result;
}

NVSResult EffectPresetManager::list(EffectPresetMetadata* metadata, uint8_t& count) {
    if (!m_initialised) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        count = 0;
        return m_lastError;
    }

    if (metadata == nullptr) {
        m_lastError = NVSResult::INVALID_HANDLE;
        count = 0;
        return m_lastError;
    }

    count = 0;

    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        metadata[i].slot = i;
        metadata[i].occupied = false;
        memset(metadata[i].name, 0, EffectPreset::NAME_MAX_LEN);
        metadata[i].effectId = 0;
        metadata[i].paletteId = 0;
        metadata[i].timestamp = 0;

        // Check bitmap first (fast path)
        if (!(m_slotBitmap & (1 << i))) {
            continue;
        }

        // Load preset to get metadata
        EffectPreset preset;
        char key[16];
        makeKey(i, key);

        NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

        if (result == NVSResult::OK && preset.isValid()) {
            metadata[i].occupied = true;
            strncpy(metadata[i].name, preset.name, EffectPreset::NAME_MAX_LEN - 1);
            metadata[i].name[EffectPreset::NAME_MAX_LEN - 1] = '\0';
            metadata[i].effectId = preset.effectId;
            metadata[i].paletteId = preset.paletteId;
            metadata[i].timestamp = preset.timestamp;
            count++;
        } else {
            // Bitmap was stale, update it
            updateSlotBitmap(i, false);
        }
    }

    m_lastError = NVSResult::OK;
    return m_lastError;
}

NVSResult EffectPresetManager::remove(uint8_t slot) {
    if (!m_initialised) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (slot >= MAX_PRESETS) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    char key[16];
    makeKey(slot, key);

    NVSResult result = NVS_MANAGER.eraseKey(NVS_NAMESPACE, key);

    if (result == NVSResult::OK || result == NVSResult::NOT_FOUND) {
        updateSlotBitmap(slot, false);
#ifndef NATIVE_BUILD
        Serial.printf("[EffectPreset] Removed preset from slot %u\n", slot);
#endif
        m_lastError = NVSResult::OK;
    } else {
#ifndef NATIVE_BUILD
        Serial.printf("[EffectPreset] ERROR: Failed to remove slot %u: %s\n",
                      slot, NVSManager::resultToString(result));
#endif
        m_lastError = result;
    }

    return m_lastError;
}

// ==================== Convenience Methods ====================

NVSResult EffectPresetManager::saveCurrentEffect(uint8_t slot, const char* name,
                                                  actors::RendererActor* renderer) {
    if (!m_initialised) {
        m_lastError = NVSResult::NOT_INITIALIZED;
        return m_lastError;
    }

    if (renderer == nullptr) {
#ifndef NATIVE_BUILD
        Serial.println("[EffectPreset] ERROR: Renderer pointer is null");
#endif
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    if (slot >= MAX_PRESETS) {
        m_lastError = NVSResult::INVALID_HANDLE;
        return m_lastError;
    }

    // Build preset from current renderer state
    EffectPreset preset;
    preset.version = EffectPreset::CURRENT_VERSION;

    // Core effect settings
    preset.effectId = renderer->getCurrentEffect();
    preset.paletteId = renderer->getPaletteIndex();
    preset.brightness = renderer->getBrightness();
    preset.speed = renderer->getSpeed();

    // Expression parameters
    preset.hue = renderer->getHue();
    preset.intensity = renderer->getIntensity();
    preset.saturation = renderer->getSaturation();
    preset.complexity = renderer->getComplexity();
    preset.variation = renderer->getVariation();
    preset.mood = renderer->getMood();
    preset.trails = renderer->getFadeAmount();

    // Set name
    if (name != nullptr && name[0] != '\0') {
        strncpy(preset.name, name, EffectPreset::NAME_MAX_LEN - 1);
        preset.name[EffectPreset::NAME_MAX_LEN - 1] = '\0';
    } else {
        snprintf(preset.name, EffectPreset::NAME_MAX_LEN, "Preset %u", slot + 1);
    }

    // Set timestamp (seconds since boot as a proxy - real Unix timestamp would
    // require RTC or NTP sync which may not be available)
#ifndef NATIVE_BUILD
    preset.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
#else
    preset.timestamp = 0;
#endif

    // Calculate checksum and save
    preset.calculateChecksum();

    return save(slot, preset);
}

bool EffectPresetManager::isSlotOccupied(uint8_t slot) const {
    if (slot >= MAX_PRESETS) {
        return false;
    }
    return (m_slotBitmap & (1 << slot)) != 0;
}

uint8_t EffectPresetManager::getPresetCount() const {
    uint8_t count = 0;
    uint16_t bitmap = m_slotBitmap;

    // Count set bits (Brian Kernighan's algorithm)
    while (bitmap) {
        bitmap &= (bitmap - 1);
        count++;
    }

    return count;
}

int8_t EffectPresetManager::findFreeSlot() const {
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (!(m_slotBitmap & (1 << i))) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // All slots occupied
}

} // namespace persistence
} // namespace lightwaveos
