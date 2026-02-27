// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioTuningManager.cpp
 * @brief Audio tuning preset persistence implementation
 */

#include "AudioTuningManager.h"

#if FEATURE_AUDIO_SYNC

#include <cstddef>
#include <cstring>
#include <Arduino.h>

namespace lightwaveos {
namespace persistence {

// NVS_MANAGER macro is defined in NVSManager.h

namespace {

struct AudioPipelineTuningV1 {
    float dcAlpha = 0.001f;
    float agcTargetRms = 0.25f;
    float agcMinGain = 1.0f;
    float agcMaxGain = 40.0f;
    float agcAttack = 0.03f;
    float agcRelease = 0.015f;
    float agcClipReduce = 0.90f;
    float agcIdleReturnRate = 0.01f;
    float noiseFloorMin = 0.0004f;
    float noiseFloorRise = 0.0005f;
    float noiseFloorFall = 0.01f;
    float gateStartFactor = 1.0f;  // Updated to match AudioTuning.h default
    float gateRangeFactor = 1.5f;
    float gateRangeMin = 0.0005f;
    float rmsDbFloor = -65.0f;
    float rmsDbCeil = -12.0f;
    float bandDbFloor = -65.0f;
    float bandDbCeil = -12.0f;
    float chromaDbFloor = -65.0f;
    float chromaDbCeil = -12.0f;
    float fluxScale = 1.0f;
    float controlBusAlphaFast = 0.35f;
    float controlBusAlphaSlow = 0.12f;
    float bandAttack = 0.15f;
    float bandRelease = 0.03f;
    float heavyBandAttack = 0.08f;
    float heavyBandRelease = 0.015f;
    float perBandGains[8] = {0.8f, 0.85f, 1.0f, 1.2f, 1.5f, 1.8f, 2.0f, 2.2f};
    float perBandNoiseFloors[8] = {0.0008f, 0.0012f, 0.0006f, 0.0005f,
                                   0.0008f, 0.0010f, 0.0012f, 0.0006f};
    bool usePerBandNoiseFloor = false;
    float silenceHysteresisMs = 5000.0f;
    float silenceThreshold = 0.01f;
};

struct AudioTuningPresetV1 {
    static constexpr uint8_t VERSION = 1;
    static constexpr size_t NAME_MAX_LEN = AudioTuningPreset::NAME_MAX_LEN;

    uint8_t version = VERSION;
    char name[NAME_MAX_LEN] = {0};
    AudioPipelineTuningV1 pipeline;
    audio::AudioContractTuning contract;
    uint32_t checksum = 0;

    void calculateChecksum() {
        const size_t dataSize = offsetof(AudioTuningPresetV1, checksum);
        checksum = NVSManager::calculateCRC32(this, dataSize);
    }

    bool isValid() const {
        if (version != VERSION) return false;
        const size_t dataSize = offsetof(AudioTuningPresetV1, checksum);
        uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
        return (checksum == calculated);
    }
};

static bool loadPresetV1(uint8_t id, AudioTuningPresetV1& preset) {
    char key[16];
    snprintf(key, sizeof(key), "preset_%u", id);
    NVSResult result = NVS_MANAGER.loadBlob(AudioTuningManager::NVS_NAMESPACE, key,
                                            &preset, sizeof(preset));
    return (result == NVSResult::OK && preset.isValid());
}

} // namespace

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

    if (result == NVSResult::OK && preset.isValid()) {
        pipeline = audio::clampAudioPipelineTuning(preset.pipeline);
        contract = audio::clampAudioContractTuning(preset.contract);
        if (nameOut) {
            strncpy(nameOut, preset.name, AudioTuningPreset::NAME_MAX_LEN);
        }
        Serial.printf("[AudioTuning] Preset '%s' loaded from slot %d\n", preset.name, id);
        return true;
    }

    AudioTuningPresetV1 presetV1;
    if (result == NVSResult::SIZE_MISMATCH || (result == NVSResult::OK && !preset.isValid())) {
        if (loadPresetV1(id, presetV1)) {
            audio::AudioPipelineTuning pipelineOut{};
            pipelineOut.dcAlpha = presetV1.pipeline.dcAlpha;
            pipelineOut.agcTargetRms = presetV1.pipeline.agcTargetRms;
            pipelineOut.agcMinGain = presetV1.pipeline.agcMinGain;
            pipelineOut.agcMaxGain = presetV1.pipeline.agcMaxGain;
            pipelineOut.agcAttack = presetV1.pipeline.agcAttack;
            pipelineOut.agcRelease = presetV1.pipeline.agcRelease;
            pipelineOut.agcClipReduce = presetV1.pipeline.agcClipReduce;
            pipelineOut.agcIdleReturnRate = presetV1.pipeline.agcIdleReturnRate;
            pipelineOut.noiseFloorMin = presetV1.pipeline.noiseFloorMin;
            pipelineOut.noiseFloorRise = presetV1.pipeline.noiseFloorRise;
            pipelineOut.noiseFloorFall = presetV1.pipeline.noiseFloorFall;
            pipelineOut.gateStartFactor = presetV1.pipeline.gateStartFactor;
            pipelineOut.gateRangeFactor = presetV1.pipeline.gateRangeFactor;
            pipelineOut.gateRangeMin = presetV1.pipeline.gateRangeMin;
            pipelineOut.rmsDbFloor = presetV1.pipeline.rmsDbFloor;
            pipelineOut.rmsDbCeil = presetV1.pipeline.rmsDbCeil;
            pipelineOut.bandDbFloor = presetV1.pipeline.bandDbFloor;
            pipelineOut.bandDbCeil = presetV1.pipeline.bandDbCeil;
            pipelineOut.chromaDbFloor = presetV1.pipeline.chromaDbFloor;
            pipelineOut.chromaDbCeil = presetV1.pipeline.chromaDbCeil;
            pipelineOut.fluxScale = presetV1.pipeline.fluxScale;
            pipelineOut.controlBusAlphaFast = presetV1.pipeline.controlBusAlphaFast;
            pipelineOut.controlBusAlphaSlow = presetV1.pipeline.controlBusAlphaSlow;
            pipelineOut.bandAttack = presetV1.pipeline.bandAttack;
            pipelineOut.bandRelease = presetV1.pipeline.bandRelease;
            pipelineOut.heavyBandAttack = presetV1.pipeline.heavyBandAttack;
            pipelineOut.heavyBandRelease = presetV1.pipeline.heavyBandRelease;
            for (size_t i = 0; i < 8; ++i) {
                pipelineOut.perBandGains[i] = presetV1.pipeline.perBandGains[i];
                pipelineOut.perBandNoiseFloors[i] = presetV1.pipeline.perBandNoiseFloors[i];
            }
            pipelineOut.usePerBandNoiseFloor = presetV1.pipeline.usePerBandNoiseFloor;
            pipelineOut.silenceHysteresisMs = presetV1.pipeline.silenceHysteresisMs;
            pipelineOut.silenceThreshold = presetV1.pipeline.silenceThreshold;
            pipeline = audio::clampAudioPipelineTuning(pipelineOut);
            contract = audio::clampAudioContractTuning(presetV1.contract);
            if (nameOut) {
                strncpy(nameOut, presetV1.name, AudioTuningPreset::NAME_MAX_LEN);
            }
            Serial.printf("[AudioTuning] Preset '%s' loaded from slot %d (v1)\n", presetV1.name, id);
            return true;
        }
    }

    if (result == NVSResult::OK) {
        Serial.printf("[AudioTuning] WARNING: Preset %d has invalid checksum\n", id);
    }
    return false;
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
            continue;
        }

        if (result == NVSResult::SIZE_MISMATCH || (result == NVSResult::OK && !preset.isValid())) {
            AudioTuningPresetV1 presetV1;
            if (loadPresetV1(i, presetV1)) {
                if (names) {
                    strncpy(names[count], presetV1.name, AudioTuningPreset::NAME_MAX_LEN);
                }
                if (ids) {
                    ids[count] = i;
                }
                count++;
            }
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
    if (result == NVSResult::OK && preset.isValid()) {
        return true;
    }
    if (result == NVSResult::SIZE_MISMATCH || (result == NVSResult::OK && !preset.isValid())) {
        AudioTuningPresetV1 presetV1;
        return loadPresetV1(id, presetV1);
    }
    return false;
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
