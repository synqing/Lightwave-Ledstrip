// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsAudioCodec.h
 * @brief JSON codec for WebSocket audio commands parsing and validation
 *
 * Single canonical location for parsing WebSocket audio command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from audio WS commands.
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

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Decode Request Structs
// ============================================================================

/**
 * @brief Decoded audio.parameters.get request (requestId only)
 */
struct AudioParametersGetRequest {
    const char* requestId;   // Optional
    
    AudioParametersGetRequest() : requestId("") {}
};

struct AudioParametersGetDecodeResult {
    bool success;
    AudioParametersGetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioParametersGetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded audio.parameters.set request (optional nested updates)
 */
struct AudioParametersSetRequest {
    const char* requestId;   // Optional
    
    // Pipeline fields (optional, presence flags)
    bool hasPipeline;
    struct {
        bool hasDcAlpha;
        float dcAlpha;
        bool hasAgcTargetRms;
        float agcTargetRms;
        bool hasAgcMinGain;
        float agcMinGain;
        bool hasAgcMaxGain;
        float agcMaxGain;
        bool hasAgcAttack;
        float agcAttack;
        bool hasAgcRelease;
        float agcRelease;
        bool hasAgcClipReduce;
        float agcClipReduce;
        bool hasAgcIdleReturnRate;
        float agcIdleReturnRate;
        bool hasNoiseFloorMin;
        float noiseFloorMin;
        bool hasNoiseFloorRise;
        float noiseFloorRise;
        bool hasNoiseFloorFall;
        float noiseFloorFall;
        bool hasGateStartFactor;
        float gateStartFactor;
        bool hasGateRangeFactor;
        float gateRangeFactor;
        bool hasGateRangeMin;
        float gateRangeMin;
        bool hasRmsDbFloor;
        float rmsDbFloor;
        bool hasRmsDbCeil;
        float rmsDbCeil;
        bool hasBandDbFloor;
        float bandDbFloor;
        bool hasBandDbCeil;
        float bandDbCeil;
        bool hasChromaDbFloor;
        float chromaDbFloor;
        bool hasChromaDbCeil;
        float chromaDbCeil;
        bool hasFluxScale;
        float fluxScale;
    } pipeline;
    
    // ControlBus fields (optional)
    bool hasControlBus;
    struct {
        bool hasAlphaFast;
        float alphaFast;
        bool hasAlphaSlow;
        float alphaSlow;
    } controlBus;
    
    // Contract fields (optional)
    bool hasContract;
    struct {
        bool hasAudioStalenessMs;
        float audioStalenessMs;
        bool hasBpmMin;
        float bpmMin;
        bool hasBpmMax;
        float bpmMax;
        bool hasBpmTau;
        float bpmTau;
        bool hasConfidenceTau;
        float confidenceTau;
        bool hasPhaseCorrectionGain;
        float phaseCorrectionGain;
        bool hasBarCorrectionGain;
        float barCorrectionGain;
        bool hasBeatsPerBar;
        uint8_t beatsPerBar;
        bool hasBeatUnit;
        uint8_t beatUnit;
    } contract;
    
    bool hasResetState;
    bool resetState;
    
    AudioParametersSetRequest() : requestId(""), hasPipeline(false), hasControlBus(false), hasContract(false), hasResetState(false), resetState(false) {
        memset(&pipeline, 0, sizeof(pipeline));
        memset(&controlBus, 0, sizeof(controlBus));
        memset(&contract, 0, sizeof(contract));
    }
};

struct AudioParametersSetDecodeResult {
    bool success;
    AudioParametersSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioParametersSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded audio.subscribe/unsubscribe request (requestId only)
 */
struct AudioSubscribeRequest {
    const char* requestId;   // Optional
    
    AudioSubscribeRequest() : requestId("") {}
};

struct AudioSubscribeDecodeResult {
    bool success;
    AudioSubscribeRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioSubscribeDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded audio.zone-agc.set request (optional fields)
 */
struct AudioZoneAgcSetRequest {
    const char* requestId;   // Optional
    
    bool hasEnabled;
    bool enabled;
    bool hasLookaheadEnabled;
    bool lookaheadEnabled;
    bool hasAttackRate;
    float attackRate;
    bool hasReleaseRate;
    float releaseRate;
    bool hasMinFloor;
    float minFloor;
    
    AudioZoneAgcSetRequest() : requestId(""), hasEnabled(false), enabled(false), hasLookaheadEnabled(false), lookaheadEnabled(false), hasAttackRate(false), attackRate(0.05f), hasReleaseRate(false), releaseRate(0.05f), hasMinFloor(false), minFloor(0.0f) {}
};

struct AudioZoneAgcSetDecodeResult {
    bool success;
    AudioZoneAgcSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioZoneAgcSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded audio.zone-agc.get / spike-detection.get/reset request (requestId only)
 */
struct AudioSimpleRequest {
    const char* requestId;   // Optional
    
    AudioSimpleRequest() : requestId("") {}
};

struct AudioSimpleDecodeResult {
    bool success;
    AudioSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    AudioSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Pipeline tuning data for encoder (POD, no audio dependencies)
 */
struct AudioPipelineTuningData {
    float dcAlpha;
    float agcTargetRms;
    float agcMinGain;
    float agcMaxGain;
    float agcAttack;
    float agcRelease;
    float agcClipReduce;
    float agcIdleReturnRate;
    float noiseFloorMin;
    float noiseFloorRise;
    float noiseFloorFall;
    float gateStartFactor;
    float gateRangeFactor;
    float gateRangeMin;
    float rmsDbFloor;
    float rmsDbCeil;
    float bandDbFloor;
    float bandDbCeil;
    float chromaDbFloor;
    float chromaDbCeil;
    float fluxScale;
    float controlBusAlphaFast;
    float controlBusAlphaSlow;
};

/**
 * @brief Contract tuning data for encoder (POD)
 */
struct AudioContractTuningData {
    float audioStalenessMs;
    float bpmMin;
    float bpmMax;
    float bpmTau;
    float confidenceTau;
    float phaseCorrectionGain;
    float barCorrectionGain;
    uint8_t beatsPerBar;
    uint8_t beatUnit;
};

/**
 * @brief DSP state data for encoder (POD)
 */
struct AudioDspStateData {
    float rmsRaw;
    float rmsMapped;
    float rmsPreGain;
    float fluxMapped;
    float agcGain;
    float dcEstimate;
    float noiseFloor;
    int16_t minSample;
    int16_t maxSample;
    int16_t peakCentered;
    float meanSample;
    uint16_t clipCount;
};

/**
 * @brief Capabilities data for encoder (POD)
 */
struct AudioCapabilitiesData {
    uint16_t sampleRate;
    uint16_t hopSize;
    uint16_t fftSize;
    uint16_t goertzelWindow;
    uint8_t bandCount;
    uint8_t chromaCount;
    uint16_t waveformPoints;
};

/**
 * @brief Zone AGC zone data (POD)
 */
struct AudioZoneAgcZoneData {
    uint8_t index;
    float follower;
    float maxMag;
};

/**
 * @brief Spike detection stats data (POD)
 */
struct AudioSpikeDetectionStatsData {
    uint32_t totalFrames;
    uint32_t spikesDetectedBands;
    uint32_t spikesDetectedChroma;
    uint32_t spikesCorrected;
    float totalEnergyRemoved;
    float avgSpikesPerFrame;
    float avgCorrectionMagnitude;
};

/**
 * @brief WebSocket Audio Command JSON Codec
 *
 * Single canonical parser for audio WebSocket commands.
 */
class WsAudioCodec {
public:
    // ------------------------------------------------------------------------
    // Decode functions (request parsing)
    // ------------------------------------------------------------------------
    
    static AudioParametersGetDecodeResult decodeParametersGet(JsonObjectConst root);
    static AudioParametersSetDecodeResult decodeParametersSet(JsonObjectConst root);
    static AudioSubscribeDecodeResult decodeSubscribe(JsonObjectConst root);
    static AudioSimpleDecodeResult decodeSimple(JsonObjectConst root);  // For unsubscribe, zone-agc.get, spike-detection.get/reset
    static AudioZoneAgcSetDecodeResult decodeZoneAgcSet(JsonObjectConst root);

    // ------------------------------------------------------------------------
    // Encoder helpers (responses)
    //
    // Rule: Only codec modules are allowed to write JSON keys for audio WS
    // payloads. Handlers must call these functions and operate on typed values.
    // ------------------------------------------------------------------------

    static void encodeParametersGet(const AudioPipelineTuningData& pipeline,
                                   const AudioContractTuningData& contract,
                                   const AudioDspStateData& state,
                                   const AudioCapabilitiesData& caps,
                                   JsonObject& data);

    static void encodeParametersChanged(bool updatedPipeline,
                                      bool updatedContract,
                                      bool resetState,
                                      JsonObject& data);

    static void encodeSubscribed(uint32_t clientId,
                               uint16_t frameSize,
                               uint8_t streamVersion,
                               uint8_t numBands,
                               uint8_t numChroma,
                               uint16_t waveformSize,
                               uint8_t targetFps,
                               JsonObject& data);

    static void encodeUnsubscribed(uint32_t clientId, JsonObject& data);

    static void encodeZoneAgcState(bool enabled,
                                   bool lookaheadEnabled,
                                   const AudioZoneAgcZoneData* zones,
                                   size_t zoneCount,
                                   JsonObject& data);

    static void encodeZoneAgcUpdated(bool updated, JsonObject& data);

    static void encodeSpikeDetectionState(bool enabled,
                                         const AudioSpikeDetectionStatsData& stats,
                                         JsonObject& data);

    static void encodeSpikeDetectionReset(JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
