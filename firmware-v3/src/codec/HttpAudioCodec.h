// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpAudioCodec.h
 * @brief JSON codec for HTTP audio endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP audio endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from audio HTTP endpoints.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include "WsAudioCodec.h"  // Reuse POD structs (AudioPipelineTuningData, etc.)

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsAudioCodec (same namespace)

// Reuse POD structs from WsAudioCodec where applicable:
// - AudioPipelineTuningData
// - AudioContractTuningData
// - AudioDspStateData
// - AudioCapabilitiesData
// - AudioZoneAgcZoneData
// - AudioSpikeDetectionStatsData

// ============================================================================
// Decode Request Structs (for POST/PUT endpoints)
// ============================================================================

// Reuse AudioParametersSetRequest and AudioParametersSetDecodeResult from WsAudioCodec.h
// (HTTP just ignores requestId field - structs are at namespace level, not nested in class)
// These are already defined in WsAudioCodec.h, so we just use them directly

/**
 * @brief Decoded audio.control request
 */
struct AudioControlRequest {
    const char* action;  // "pause" or "resume"
    
    AudioControlRequest() : action("") {}
};

struct AudioControlDecodeResult {
    bool success;
    AudioControlRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioControlDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded audio.preset.save request
 */
struct AudioPresetSaveRequest {
    const char* name;  // Optional, defaults to "Unnamed"
    
    AudioPresetSaveRequest() : name("Unnamed") {}
};

struct AudioPresetSaveDecodeResult {
    bool success;
    AudioPresetSaveRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioPresetSaveDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// Reuse AudioZoneAgcSetRequest and AudioZoneAgcSetDecodeResult from WsAudioCodec.h
// (HTTP just ignores requestId field - structs are at namespace level, not nested in class)
// These are already defined in WsAudioCodec.h, so we just use them directly

/**
 * @brief Decoded audio.mappings.set request
 */
struct AudioMappingItem {
    const char* source;  // String name
    const char* target;  // String name
    const char* curve;   // String name
    float inputMin;
    float inputMax;
    float outputMin;
    float outputMax;
    float smoothingAlpha;
    float gain;
    bool enabled;
    bool additive;
};

struct AudioMappingsSetRequest {
    bool hasGlobalEnabled;
    bool globalEnabled;
    AudioMappingItem mappings[8];  // MAX_MAPPINGS_PER_EFFECT
    uint8_t mappingCount;
    
    AudioMappingsSetRequest() : hasGlobalEnabled(false), globalEnabled(true), mappingCount(0) {
        memset(mappings, 0, sizeof(mappings));
    }
};

struct AudioMappingsSetDecodeResult {
    bool success;
    AudioMappingsSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioMappingsSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded audio.calibrate.start request
 */
struct AudioCalibrateStartRequest {
    bool hasDurationMs;
    uint32_t durationMs;
    bool hasSafetyMultiplier;
    float safetyMultiplier;
    
    AudioCalibrateStartRequest() : hasDurationMs(false), durationMs(3000), hasSafetyMultiplier(false), safetyMultiplier(1.2f) {}
};

struct AudioCalibrateStartDecodeResult {
    bool success;
    AudioCalibrateStartRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioCalibrateStartDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Audio actor state data for encoder (POD)
 */
struct AudioActorStateData {
    const char* stateStr;  // "UNINITIALIZED", "RUNNING", etc.
    bool capturing;
    uint32_t hopCount;
    uint32_t sampleIndex;
    uint32_t tickCount;
    uint32_t captureSuccessCount;
    uint32_t captureFailCount;
};

/**
 * @brief Tempo data for encoder (POD)
 */
struct AudioTempoData {
    float bpm;
    float confidence;
    float beatPhase;
    float barPhase;
    uint8_t beatInBar;
    uint8_t beatsPerBar;
};

/**
 * @brief Preset summary data (POD)
 */
struct AudioPresetSummary {
    uint8_t id;
    const char* name;
};

/**
 * @brief Preset detail data (POD)
 */
struct AudioPresetDetail {
    uint8_t id;
    const char* name;
    // Pipeline and contract data (reuse existing structs)
};

/**
 * @brief Mapping source/target/curve item (POD)
 */
struct AudioMappingSourceItem {
    const char* name;
    uint8_t id;
    const char* category;
    const char* description;
    float rangeMin;
    float rangeMax;
};

struct AudioMappingTargetItem {
    const char* name;
    uint8_t id;
    uint8_t rangeMin;
    uint8_t rangeMax;
    uint8_t defaultValue;
    const char* description;
};

struct AudioMappingCurveItem {
    const char* name;
    uint8_t id;
    const char* formula;
    const char* description;
};

/**
 * @brief Mapping list effect item (POD)
 */
struct AudioMappingEffectItem {
    uint8_t id;
    const char* name;
    uint8_t mappingCount;
    bool enabled;
};

/**
 * @brief Mapping detail item (POD)
 */
struct AudioMappingDetailItem {
    const char* source;
    const char* target;
    const char* curve;
    float inputMin;
    float inputMax;
    float outputMin;
    float outputMax;
    float smoothingAlpha;
    float gain;
    bool enabled;
    bool additive;
};

/**
 * @brief Mapping stats data (POD)
 */
struct AudioMappingStatsData {
    uint32_t applyCount;
    uint32_t lastApplyMicros;
    uint32_t maxApplyMicros;
    uint8_t activeEffectsWithMappings;
    uint8_t totalMappingsConfigured;
};

/**
 * @brief Calibration state data (POD)
 */
struct AudioCalibrationStateData {
    const char* stateStr;  // "idle", "measuring", etc.
    uint32_t durationMs;
    float safetyMultiplier;
    float maxAllowedRms;
    float progress;  // 0.0-1.0 when measuring
    uint32_t samplesCollected;
    float currentAvgRms;  // When measuring
    bool hasResult;
    float resultOverallRms;
    float resultPeakRms;
    uint32_t resultSampleCount;
    float resultBandFloors[8];
    float resultChromaFloors[12];
};

/**
 * @brief Benchmark stats data (POD)
 */
struct AudioBenchmarkStatsData {
    bool streaming;
    float avgTotalUs;
    float avgGoertzelUs;
    float avgDcAgcUs;
    float avgChromaUs;
    uint16_t peakTotalUs;
    uint16_t peakGoertzelUs;
    float cpuLoadPercent;
    uint32_t hopCount;
    uint32_t goertzelCount;
    uint16_t histogramBins[8];
};

/**
 * @brief Benchmark sample data (POD)
 */
struct AudioBenchmarkSampleData {
    uint32_t timestamp_us;
    uint16_t totalProcessUs;
    uint16_t goertzelUs;
    uint16_t dcAgcLoopUs;
    uint16_t chromaUs;
};

/**
 * @brief WebSocket Audio Command JSON Codec
 *
 * Single canonical parser for audio HTTP endpoints.
 */
class HttpAudioCodec {
public:
    // ------------------------------------------------------------------------
    // Decode functions (request parsing)
    // ------------------------------------------------------------------------
    
    static AudioParametersSetDecodeResult decodeParametersSet(JsonObjectConst root);
    static AudioControlDecodeResult decodeControl(JsonObjectConst root);
    static AudioPresetSaveDecodeResult decodePresetSave(JsonObjectConst root);
    static AudioZoneAgcSetDecodeResult decodeZoneAgcSet(JsonObjectConst root);
    static AudioMappingsSetDecodeResult decodeMappingsSet(JsonObjectConst root);
    static AudioCalibrateStartDecodeResult decodeCalibrateStart(JsonObjectConst root);

    // ------------------------------------------------------------------------
    // Encoder helpers (responses)
    //
    // Rule: Only codec modules are allowed to write JSON keys for audio HTTP
    // payloads. Handlers must call these functions and operate on typed values.
    // ------------------------------------------------------------------------

    // Parameters
    static void encodeParametersGet(const AudioPipelineTuningData& pipeline,
                                   const AudioContractTuningData& contract,
                                   const AudioDspStateData& state,
                                   const AudioCapabilitiesData& caps,
                                   JsonObject& data);

    static void encodeParametersChanged(bool updatedPipeline,
                                      bool updatedContract,
                                      bool resetState,
                                      JsonObject& data);

    // Control
    static void encodeControlResponse(const char* stateStr, const char* action, JsonObject& data);

    // State
    static void encodeStateGet(const AudioActorStateData& state, JsonObject& data);

    // Tempo
    static void encodeTempoGet(const AudioTempoData& tempo, JsonObject& data);

    // Presets
    static void encodePresetsList(const AudioPresetSummary* presets, uint8_t count, JsonObject& data);
    static void encodePresetGet(uint8_t id, const char* name,
                               const AudioPipelineTuningData& pipeline,
                               const AudioContractTuningData& contract,
                               JsonObject& data);
    static void encodePresetSave(uint8_t id, const char* name, JsonObject& data);
    static void encodePresetApply(uint8_t id, const char* name, JsonObject& data);
    static void encodePresetDelete(uint8_t id, JsonObject& data);

    // Mappings
    static void encodeMappingsListSources(const AudioMappingSourceItem* sources, size_t count, JsonObject& data);
    static void encodeMappingsListTargets(const AudioMappingTargetItem* targets, size_t count, JsonObject& data);
    static void encodeMappingsListCurves(const AudioMappingCurveItem* curves, size_t count, JsonObject& data);
    static void encodeMappingsList(uint8_t activeEffects, uint8_t totalMappings,
                                  const AudioMappingEffectItem* effects, size_t effectCount,
                                  JsonObject& data);
    static void encodeMappingsGet(uint8_t effectId, const char* effectName, bool globalEnabled, uint8_t mappingCount,
                                 const AudioMappingDetailItem* mappings, size_t count,
                                 JsonObject& data);
    static void encodeMappingsSet(uint8_t effectId, uint8_t mappingCount, bool enabled, JsonObject& data);
    static void encodeMappingsDelete(uint8_t effectId, JsonObject& data);
    static void encodeMappingsEnable(uint8_t effectId, bool enabled, JsonObject& data);
    static void encodeMappingsStats(const AudioMappingStatsData& stats, JsonObject& data);

    // Zone AGC
    static void encodeZoneAgcGet(bool enabled, bool lookaheadEnabled,
                                const AudioZoneAgcZoneData* zones, size_t zoneCount,
                                JsonObject& data);
    static void encodeZoneAgcSet(bool updated, JsonObject& data);

    // Spike Detection
    static void encodeSpikeDetectionGet(bool enabled, const AudioSpikeDetectionStatsData& stats, JsonObject& data);
    static void encodeSpikeDetectionReset(JsonObject& data);

    // Calibration
    static void encodeCalibrationStatus(const AudioCalibrationStateData& state, JsonObject& data);
    static void encodeCalibrationStart(uint32_t durationMs, float safetyMultiplier, JsonObject& data);
    static void encodeCalibrationApply(float noiseFloorMin, const float* perBandNoiseFloors, size_t count, JsonObject& data);

    // Benchmark
    static void encodeBenchmarkGet(const AudioBenchmarkStatsData& stats, JsonObject& data);
    static void encodeBenchmarkStart(JsonObject& data);
    static void encodeBenchmarkStop(const AudioBenchmarkStatsData& stats, JsonObject& data);
    static void encodeBenchmarkHistory(uint32_t available, uint32_t returned,
                                      const AudioBenchmarkSampleData* samples, size_t count,
                                      JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
