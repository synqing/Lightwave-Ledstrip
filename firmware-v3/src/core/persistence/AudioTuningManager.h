// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioTuningManager.h
 * @brief Audio tuning preset persistence manager for NVS storage
 *
 * LightwaveOS v2 - Persistence System
 *
 * Manages saving and loading of audio tuning presets to NVS flash.
 * Includes checksum validation and named preset support.
 *
 * Features:
 * - Up to 10 named audio tuning presets
 * - Stores both AudioPipelineTuning and AudioContractTuning
 * - CRC32 checksum validation
 * - Thread-safe operations
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include <cstdint>
#include "NVSManager.h"
#include "../../audio/AudioTuning.h"

namespace lightwaveos {
namespace persistence {

// ==================== Audio Tuning Preset Structure ====================

/**
 * @brief Serializable audio tuning preset for NVS storage
 */
struct AudioTuningPreset {
    static constexpr uint8_t CURRENT_VERSION = 2;
    static constexpr size_t NAME_MAX_LEN = 32;

    uint8_t version = CURRENT_VERSION;
    char name[NAME_MAX_LEN] = {0};
    audio::AudioPipelineTuning pipeline;
    audio::AudioContractTuning contract;
    uint32_t checksum = 0;

    // Calculate checksum (excludes checksum field itself)
    void calculateChecksum();

    // Validate checksum
    bool isValid() const;
};

// ==================== Audio Tuning Manager Class ====================

/**
 * @brief Manager for audio tuning preset persistence
 *
 * Provides save/load/delete operations for named audio tuning presets.
 * Uses NVS blob storage with checksum validation.
 */
class AudioTuningManager {
public:
    static constexpr uint8_t MAX_PRESETS = 10;
    static constexpr const char* NVS_NAMESPACE = "audio_tune";

    /**
     * @brief Get the singleton instance
     */
    static AudioTuningManager& instance();

    // Prevent copying
    AudioTuningManager(const AudioTuningManager&) = delete;
    AudioTuningManager& operator=(const AudioTuningManager&) = delete;

    // ==================== Preset Operations ====================

    /**
     * @brief Save current tuning as a new preset
     *
     * @param name Preset name (max 31 chars)
     * @param pipeline Pipeline tuning parameters
     * @param contract Contract tuning parameters
     * @return Preset ID (0-9) on success, -1 on failure
     */
    int8_t savePreset(const char* name,
                      const audio::AudioPipelineTuning& pipeline,
                      const audio::AudioContractTuning& contract);

    /**
     * @brief Load a preset by ID
     *
     * @param id Preset ID (0-9)
     * @param pipeline Output: Pipeline tuning parameters
     * @param contract Output: Contract tuning parameters
     * @param nameOut Output: Preset name (optional, provide buffer of NAME_MAX_LEN)
     * @return true on success
     */
    bool loadPreset(uint8_t id,
                    audio::AudioPipelineTuning& pipeline,
                    audio::AudioContractTuning& contract,
                    char* nameOut = nullptr);

    /**
     * @brief Delete a preset by ID
     *
     * @param id Preset ID (0-9)
     * @return true on success
     */
    bool deletePreset(uint8_t id);

    /**
     * @brief List all saved presets
     *
     * @param names Output: Array of preset names (provide MAX_PRESETS elements)
     * @param ids Output: Array of preset IDs (provide MAX_PRESETS elements)
     * @return Number of presets found
     */
    uint8_t listPresets(char names[][AudioTuningPreset::NAME_MAX_LEN], uint8_t* ids);

    /**
     * @brief Check if a preset exists
     *
     * @param id Preset ID (0-9)
     * @return true if preset exists and is valid
     */
    bool hasPreset(uint8_t id) const;

    /**
     * @brief Get preset count
     *
     * @return Number of saved presets
     */
    uint8_t getPresetCount() const;

    /**
     * @brief Find next available preset slot
     *
     * @return Preset ID (0-9) or -1 if all slots used
     */
    int8_t findFreeSlot() const;

private:
    AudioTuningManager() = default;

    // NVS key format: "preset_0" through "preset_9"
    static void makeKey(uint8_t id, char* key);
};

} // namespace persistence
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
