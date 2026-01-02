/**
 * @file AudioTuningManager.cpp
 * @brief Audio tuning preset persistence implementation
 */

#include "AudioTuningManager.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <Arduino.h>

namespace lightwaveos {
namespace persistence {

// NVS_MANAGER macro is defined in NVSManager.h

// ==================== AudioTuningPreset Implementation ====================

void AudioTuningPreset::calculateChecksum() {
    const size_t dataSize = offsetof(AudioTuningPreset, checksum);
    checksum = NVSManager::calculateCRC32(this, dataSize);
}

bool AudioTuningPreset::isValid() const {
    if (version != CURRENT_VERSION) return false;
    const size_t dataSize = offsetof(AudioTuningPreset, checksum);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (checksum == calculated);
}

// ==================== AudioTuningManager Implementation ====================

AudioTuningManager& AudioTuningManager::instance() {
    static AudioTuningManager inst;
    return inst;
}

void AudioTuningManager::makeKey(uint8_t id, char* key) {
    snprintf(key, 16, "preset_%u", id);
}

int8_t AudioTuningManager::savePreset(const char* name,
                                       const audio::AudioPipelineTuning& pipeline,
                                       const audio::AudioContractTuning& contract) {
    // Find a free slot
    int8_t slot = findFreeSlot();
    if (slot < 0) {
        Serial.println("[AudioTuning] ERROR: No free preset slots");
        return -1;
    }

    // Build preset
    AudioTuningPreset preset;
    preset.version = AudioTuningPreset::CURRENT_VERSION;
    strncpy(preset.name, name ? name : "Unnamed", AudioTuningPreset::NAME_MAX_LEN - 1);
    preset.name[AudioTuningPreset::NAME_MAX_LEN - 1] = '\0';
    preset.pipeline = audio::clampAudioPipelineTuning(pipeline);
    preset.contract = audio::clampAudioContractTuning(contract);
    preset.calculateChecksum();

    // Save to NVS
    char key[16];
    makeKey(slot, key);
    NVSResult result = NVS_MANAGER.saveBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result == NVSResult::OK) {
        Serial.printf("[AudioTuning] Preset '%s' saved to slot %d\n", preset.name, slot);
        return slot;
    } else {
        Serial.printf("[AudioTuning] ERROR: Save failed: %s\n",
                      NVSManager::resultToString(result));
        return -1;
    }
}

bool AudioTuningManager::loadPreset(uint8_t id,
                                     audio::AudioPipelineTuning& pipeline,
                                     audio::AudioContractTuning& contract,
                                     char* nameOut) {
    if (id >= MAX_PRESETS) return false;

    AudioTuningPreset preset;
    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result != NVSResult::OK) {
        return false;
    }

    if (!preset.isValid()) {
        Serial.printf("[AudioTuning] WARNING: Preset %d has invalid checksum\n", id);
        return false;
    }

    // Apply clamping for safety
    pipeline = audio::clampAudioPipelineTuning(preset.pipeline);
    contract = audio::clampAudioContractTuning(preset.contract);

    if (nameOut) {
        strncpy(nameOut, preset.name, AudioTuningPreset::NAME_MAX_LEN);
    }

    Serial.printf("[AudioTuning] Preset '%s' loaded from slot %d\n", preset.name, id);
    return true;
}

bool AudioTuningManager::deletePreset(uint8_t id) {
    if (id >= MAX_PRESETS) return false;

    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.eraseKey(NVS_NAMESPACE, key);

    if (result == NVSResult::OK || result == NVSResult::NOT_FOUND) {
        Serial.printf("[AudioTuning] Preset %d deleted\n", id);
        return true;
    }

    Serial.printf("[AudioTuning] ERROR: Delete failed: %s\n",
                  NVSManager::resultToString(result));
    return false;
}

uint8_t AudioTuningManager::listPresets(char names[][AudioTuningPreset::NAME_MAX_LEN], uint8_t* ids) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < MAX_PRESETS && count < MAX_PRESETS; i++) {
        AudioTuningPreset preset;
        char key[16];
        makeKey(i, key);

        NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

        if (result == NVSResult::OK && preset.isValid()) {
            if (names) {
                strncpy(names[count], preset.name, AudioTuningPreset::NAME_MAX_LEN);
            }
            if (ids) {
                ids[count] = i;
            }
            count++;
        }
    }

    return count;
}

bool AudioTuningManager::hasPreset(uint8_t id) const {
    if (id >= MAX_PRESETS) return false;

    AudioTuningPreset preset;
    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));
    return (result == NVSResult::OK && preset.isValid());
}

uint8_t AudioTuningManager::getPresetCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (hasPreset(i)) count++;
    }
    return count;
}

int8_t AudioTuningManager::findFreeSlot() const {
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (!hasPreset(i)) return i;
    }
    return -1;
}

} // namespace persistence
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
