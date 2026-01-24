/**
 * @file WsAudioCodec.cpp
 * @brief WebSocket audio codec implementation
 *
 * Single canonical JSON parser for audio WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsAudioCodec.h"
#include "WsCommonCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

AudioParametersGetDecodeResult WsAudioCodec::decodeParametersGet(JsonObjectConst root) {
    AudioParametersGetDecodeResult result;
    result.request = AudioParametersGetRequest();

    // Extract requestId (optional, canonical)
    result.request.requestId = WsCommonCodec::decodeRequestId(root).requestId;

    result.success = true;
    return result;
}

AudioParametersSetDecodeResult WsAudioCodec::decodeParametersSet(JsonObjectConst root) {
    AudioParametersSetDecodeResult result;
    result.request = AudioParametersSetRequest();

    // Extract requestId (optional, canonical)
    result.request.requestId = WsCommonCodec::decodeRequestId(root).requestId;

    // Extract resetState (optional, top-level)
    if (root.containsKey("resetState")) {
        result.request.hasResetState = true;
        if (root["resetState"].is<bool>()) {
            result.request.resetState = root["resetState"].as<bool>();
        } else {
            result.request.resetState = root["resetState"] | false;
        }
    }

    // Extract pipeline fields (optional nested object)
    if (root.containsKey("pipeline") && root["pipeline"].is<JsonObjectConst>()) {
        JsonObjectConst pipeline = root["pipeline"].as<JsonObjectConst>();
        result.request.hasPipeline = true;
        
        if (pipeline.containsKey("dcAlpha")) {
            result.request.pipeline.hasDcAlpha = true;
            result.request.pipeline.dcAlpha = pipeline["dcAlpha"] | 0.001f;
        }
        if (pipeline.containsKey("agcTargetRms")) {
            result.request.pipeline.hasAgcTargetRms = true;
            result.request.pipeline.agcTargetRms = pipeline["agcTargetRms"] | 0.25f;
        }
        if (pipeline.containsKey("agcMinGain")) {
            result.request.pipeline.hasAgcMinGain = true;
            result.request.pipeline.agcMinGain = pipeline["agcMinGain"] | 1.0f;
        }
        if (pipeline.containsKey("agcMaxGain")) {
            result.request.pipeline.hasAgcMaxGain = true;
            result.request.pipeline.agcMaxGain = pipeline["agcMaxGain"] | 40.0f;
        }
        if (pipeline.containsKey("agcAttack")) {
            result.request.pipeline.hasAgcAttack = true;
            result.request.pipeline.agcAttack = pipeline["agcAttack"] | 0.03f;
        }
        if (pipeline.containsKey("agcRelease")) {
            result.request.pipeline.hasAgcRelease = true;
            result.request.pipeline.agcRelease = pipeline["agcRelease"] | 0.015f;
        }
        if (pipeline.containsKey("agcClipReduce")) {
            result.request.pipeline.hasAgcClipReduce = true;
            result.request.pipeline.agcClipReduce = pipeline["agcClipReduce"] | 0.90f;
        }
        if (pipeline.containsKey("agcIdleReturnRate")) {
            result.request.pipeline.hasAgcIdleReturnRate = true;
            result.request.pipeline.agcIdleReturnRate = pipeline["agcIdleReturnRate"] | 0.01f;
        }
        if (pipeline.containsKey("noiseFloorMin")) {
            result.request.pipeline.hasNoiseFloorMin = true;
            result.request.pipeline.noiseFloorMin = pipeline["noiseFloorMin"] | 0.0004f;
        }
        if (pipeline.containsKey("noiseFloorRise")) {
            result.request.pipeline.hasNoiseFloorRise = true;
            result.request.pipeline.noiseFloorRise = pipeline["noiseFloorRise"] | 0.0005f;
        }
        if (pipeline.containsKey("noiseFloorFall")) {
            result.request.pipeline.hasNoiseFloorFall = true;
            result.request.pipeline.noiseFloorFall = pipeline["noiseFloorFall"] | 0.01f;
        }
        if (pipeline.containsKey("gateStartFactor")) {
            result.request.pipeline.hasGateStartFactor = true;
            result.request.pipeline.gateStartFactor = pipeline["gateStartFactor"] | 1.0f;  // Updated default
        }
        if (pipeline.containsKey("gateRangeFactor")) {
            result.request.pipeline.hasGateRangeFactor = true;
            result.request.pipeline.gateRangeFactor = pipeline["gateRangeFactor"] | 1.5f;
        }
        if (pipeline.containsKey("gateRangeMin")) {
            result.request.pipeline.hasGateRangeMin = true;
            result.request.pipeline.gateRangeMin = pipeline["gateRangeMin"] | 0.0005f;
        }
        if (pipeline.containsKey("rmsDbFloor")) {
            result.request.pipeline.hasRmsDbFloor = true;
            result.request.pipeline.rmsDbFloor = pipeline["rmsDbFloor"] | -65.0f;
        }
        if (pipeline.containsKey("rmsDbCeil")) {
            result.request.pipeline.hasRmsDbCeil = true;
            result.request.pipeline.rmsDbCeil = pipeline["rmsDbCeil"] | -12.0f;
        }
        if (pipeline.containsKey("bandDbFloor")) {
            result.request.pipeline.hasBandDbFloor = true;
            result.request.pipeline.bandDbFloor = pipeline["bandDbFloor"] | -65.0f;
        }
        if (pipeline.containsKey("bandDbCeil")) {
            result.request.pipeline.hasBandDbCeil = true;
            result.request.pipeline.bandDbCeil = pipeline["bandDbCeil"] | -12.0f;
        }
        if (pipeline.containsKey("chromaDbFloor")) {
            result.request.pipeline.hasChromaDbFloor = true;
            result.request.pipeline.chromaDbFloor = pipeline["chromaDbFloor"] | -65.0f;
        }
        if (pipeline.containsKey("chromaDbCeil")) {
            result.request.pipeline.hasChromaDbCeil = true;
            result.request.pipeline.chromaDbCeil = pipeline["chromaDbCeil"] | -12.0f;
        }
        if (pipeline.containsKey("fluxScale")) {
            result.request.pipeline.hasFluxScale = true;
            result.request.pipeline.fluxScale = pipeline["fluxScale"] | 1.0f;
        }
    }

    // Extract controlBus fields (optional nested object)
    if (root.containsKey("controlBus") && root["controlBus"].is<JsonObjectConst>()) {
        JsonObjectConst controlBus = root["controlBus"].as<JsonObjectConst>();
        result.request.hasControlBus = true;
        
        if (controlBus.containsKey("alphaFast")) {
            result.request.controlBus.hasAlphaFast = true;
            result.request.controlBus.alphaFast = controlBus["alphaFast"] | 0.35f;
        }
        if (controlBus.containsKey("alphaSlow")) {
            result.request.controlBus.hasAlphaSlow = true;
            result.request.controlBus.alphaSlow = controlBus["alphaSlow"] | 0.12f;
        }
    }

    // Extract contract fields (optional nested object)
    if (root.containsKey("contract") && root["contract"].is<JsonObjectConst>()) {
        JsonObjectConst contract = root["contract"].as<JsonObjectConst>();
        result.request.hasContract = true;
        
        if (contract.containsKey("audioStalenessMs")) {
            result.request.contract.hasAudioStalenessMs = true;
            result.request.contract.audioStalenessMs = contract["audioStalenessMs"] | 100.0f;
        }
        if (contract.containsKey("bpmMin")) {
            result.request.contract.hasBpmMin = true;
            result.request.contract.bpmMin = contract["bpmMin"] | 30.0f;
        }
        if (contract.containsKey("bpmMax")) {
            result.request.contract.hasBpmMax = true;
            result.request.contract.bpmMax = contract["bpmMax"] | 300.0f;
        }
        if (contract.containsKey("bpmTau")) {
            result.request.contract.hasBpmTau = true;
            result.request.contract.bpmTau = contract["bpmTau"] | 0.50f;
        }
        if (contract.containsKey("confidenceTau")) {
            result.request.contract.hasConfidenceTau = true;
            result.request.contract.confidenceTau = contract["confidenceTau"] | 1.00f;
        }
        if (contract.containsKey("phaseCorrectionGain")) {
            result.request.contract.hasPhaseCorrectionGain = true;
            result.request.contract.phaseCorrectionGain = contract["phaseCorrectionGain"] | 0.35f;
        }
        if (contract.containsKey("barCorrectionGain")) {
            result.request.contract.hasBarCorrectionGain = true;
            result.request.contract.barCorrectionGain = contract["barCorrectionGain"] | 0.20f;
        }
        if (contract.containsKey("beatsPerBar")) {
            result.request.contract.hasBeatsPerBar = true;
            result.request.contract.beatsPerBar = contract["beatsPerBar"] | 4;
        }
        if (contract.containsKey("beatUnit")) {
            result.request.contract.hasBeatUnit = true;
            result.request.contract.beatUnit = contract["beatUnit"] | 4;
        }
    }

    result.success = true;
    return result;
}

AudioSubscribeDecodeResult WsAudioCodec::decodeSubscribe(JsonObjectConst root) {
    AudioSubscribeDecodeResult result;
    result.request = AudioSubscribeRequest();

    // Extract requestId (optional, canonical)
    result.request.requestId = WsCommonCodec::decodeRequestId(root).requestId;

    result.success = true;
    return result;
}

AudioSimpleDecodeResult WsAudioCodec::decodeSimple(JsonObjectConst root) {
    AudioSimpleDecodeResult result;
    result.request = AudioSimpleRequest();

    // Extract requestId (optional, canonical)
    result.request.requestId = WsCommonCodec::decodeRequestId(root).requestId;

    result.success = true;
    return result;
}

AudioZoneAgcSetDecodeResult WsAudioCodec::decodeZoneAgcSet(JsonObjectConst root) {
    AudioZoneAgcSetDecodeResult result;
    result.request = AudioZoneAgcSetRequest();

    // Extract requestId (optional, canonical)
    result.request.requestId = WsCommonCodec::decodeRequestId(root).requestId;

    // Extract enabled (optional)
    if (root.containsKey("enabled")) {
        result.request.hasEnabled = true;
        if (root["enabled"].is<bool>()) {
            result.request.enabled = root["enabled"].as<bool>();
        } else {
            result.request.enabled = root["enabled"] | false;
        }
    }

    // Extract lookaheadEnabled (optional)
    if (root.containsKey("lookaheadEnabled")) {
        result.request.hasLookaheadEnabled = true;
        if (root["lookaheadEnabled"].is<bool>()) {
            result.request.lookaheadEnabled = root["lookaheadEnabled"].as<bool>();
        } else {
            result.request.lookaheadEnabled = root["lookaheadEnabled"] | false;
        }
    }

    // Extract attackRate (optional, default 0.05f)
    if (root.containsKey("attackRate")) {
        result.request.hasAttackRate = true;
        result.request.attackRate = root["attackRate"] | 0.05f;
    }

    // Extract releaseRate (optional, default 0.05f)
    if (root.containsKey("releaseRate")) {
        result.request.hasReleaseRate = true;
        result.request.releaseRate = root["releaseRate"] | 0.05f;
    }

    // Extract minFloor (optional)
    if (root.containsKey("minFloor")) {
        result.request.hasMinFloor = true;
        result.request.minFloor = root["minFloor"] | 0.0f;
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsAudioCodec::encodeParametersGet(const AudioPipelineTuningData& pipeline,
                                       const AudioContractTuningData& contract,
                                       const AudioDspStateData& state,
                                       const AudioCapabilitiesData& caps,
                                       JsonObject& data) {
    // Pipeline object
    JsonObject pipelineObj = data["pipeline"].to<JsonObject>();
    pipelineObj["dcAlpha"] = pipeline.dcAlpha;
    pipelineObj["agcTargetRms"] = pipeline.agcTargetRms;
    pipelineObj["agcMinGain"] = pipeline.agcMinGain;
    pipelineObj["agcMaxGain"] = pipeline.agcMaxGain;
    pipelineObj["agcAttack"] = pipeline.agcAttack;
    pipelineObj["agcRelease"] = pipeline.agcRelease;
    pipelineObj["agcClipReduce"] = pipeline.agcClipReduce;
    pipelineObj["agcIdleReturnRate"] = pipeline.agcIdleReturnRate;
    pipelineObj["noiseFloorMin"] = pipeline.noiseFloorMin;
    pipelineObj["noiseFloorRise"] = pipeline.noiseFloorRise;
    pipelineObj["noiseFloorFall"] = pipeline.noiseFloorFall;
    pipelineObj["gateStartFactor"] = pipeline.gateStartFactor;
    pipelineObj["gateRangeFactor"] = pipeline.gateRangeFactor;
    pipelineObj["gateRangeMin"] = pipeline.gateRangeMin;
    pipelineObj["rmsDbFloor"] = pipeline.rmsDbFloor;
    pipelineObj["rmsDbCeil"] = pipeline.rmsDbCeil;
    pipelineObj["bandDbFloor"] = pipeline.bandDbFloor;
    pipelineObj["bandDbCeil"] = pipeline.bandDbCeil;
    pipelineObj["chromaDbFloor"] = pipeline.chromaDbFloor;
    pipelineObj["chromaDbCeil"] = pipeline.chromaDbCeil;
    pipelineObj["fluxScale"] = pipeline.fluxScale;

    // ControlBus object
    JsonObject controlBusObj = data["controlBus"].to<JsonObject>();
    controlBusObj["alphaFast"] = pipeline.controlBusAlphaFast;
    controlBusObj["alphaSlow"] = pipeline.controlBusAlphaSlow;

    // Contract object
    JsonObject contractObj = data["contract"].to<JsonObject>();
    contractObj["audioStalenessMs"] = contract.audioStalenessMs;
    contractObj["bpmMin"] = contract.bpmMin;
    contractObj["bpmMax"] = contract.bpmMax;
    contractObj["bpmTau"] = contract.bpmTau;
    contractObj["confidenceTau"] = contract.confidenceTau;
    contractObj["phaseCorrectionGain"] = contract.phaseCorrectionGain;
    contractObj["barCorrectionGain"] = contract.barCorrectionGain;
    contractObj["beatsPerBar"] = contract.beatsPerBar;
    contractObj["beatUnit"] = contract.beatUnit;

    // State object
    JsonObject stateObj = data["state"].to<JsonObject>();
    stateObj["rmsRaw"] = state.rmsRaw;
    stateObj["rmsMapped"] = state.rmsMapped;
    stateObj["rmsPreGain"] = state.rmsPreGain;
    stateObj["fluxMapped"] = state.fluxMapped;
    stateObj["agcGain"] = state.agcGain;
    stateObj["dcEstimate"] = state.dcEstimate;
    stateObj["noiseFloor"] = state.noiseFloor;
    stateObj["minSample"] = state.minSample;
    stateObj["maxSample"] = state.maxSample;
    stateObj["peakCentered"] = state.peakCentered;
    stateObj["meanSample"] = state.meanSample;
    stateObj["clipCount"] = state.clipCount;

    // Capabilities object
    JsonObject capsObj = data["capabilities"].to<JsonObject>();
    capsObj["sampleRate"] = caps.sampleRate;
    capsObj["hopSize"] = caps.hopSize;
    capsObj["fftSize"] = caps.fftSize;
    capsObj["goertzelWindow"] = caps.goertzelWindow;
    capsObj["bandCount"] = caps.bandCount;
    capsObj["chromaCount"] = caps.chromaCount;
    capsObj["waveformPoints"] = caps.waveformPoints;
}

void WsAudioCodec::encodeParametersChanged(bool updatedPipeline,
                                           bool updatedContract,
                                           bool resetState,
                                           JsonObject& data) {
    JsonArray updated = data["updated"].to<JsonArray>();
    if (updatedPipeline) updated.add("pipeline");
    if (updatedContract) updated.add("contract");
    if (resetState) updated.add("state");
}

void WsAudioCodec::encodeSubscribed(uint32_t clientId,
                                   uint16_t frameSize,
                                   uint8_t streamVersion,
                                   uint8_t numBands,
                                   uint8_t numChroma,
                                   uint16_t waveformSize,
                                   uint8_t targetFps,
                                   JsonObject& data) {
    data["clientId"] = clientId;
    data["frameSize"] = frameSize;
    data["streamVersion"] = streamVersion;
    data["numBands"] = numBands;
    data["numChroma"] = numChroma;
    data["waveformSize"] = waveformSize;
    data["targetFps"] = targetFps;
    data["status"] = "ok";
}

void WsAudioCodec::encodeUnsubscribed(uint32_t clientId, JsonObject& data) {
    data["clientId"] = clientId;
    data["status"] = "ok";
}

void WsAudioCodec::encodeZoneAgcState(bool enabled,
                                     bool lookaheadEnabled,
                                     const AudioZoneAgcZoneData* zones,
                                     size_t zoneCount,
                                     JsonObject& data) {
    data["enabled"] = enabled;
    data["lookaheadEnabled"] = lookaheadEnabled;
    
    JsonArray zonesArray = data["zones"].to<JsonArray>();
    for (size_t i = 0; i < zoneCount; i++) {
        JsonObject zone = zonesArray.add<JsonObject>();
        zone["index"] = zones[i].index;
        zone["follower"] = zones[i].follower;
        zone["maxMag"] = zones[i].maxMag;
    }
}

void WsAudioCodec::encodeZoneAgcUpdated(bool updated, JsonObject& data) {
    data["updated"] = updated;
}

void WsAudioCodec::encodeSpikeDetectionState(bool enabled,
                                            const AudioSpikeDetectionStatsData& stats,
                                            JsonObject& data) {
    data["enabled"] = enabled;
    
    JsonObject statsObj = data["stats"].to<JsonObject>();
    statsObj["totalFrames"] = stats.totalFrames;
    statsObj["spikesDetectedBands"] = stats.spikesDetectedBands;
    statsObj["spikesDetectedChroma"] = stats.spikesDetectedChroma;
    statsObj["spikesCorrected"] = stats.spikesCorrected;
    statsObj["totalEnergyRemoved"] = stats.totalEnergyRemoved;
    statsObj["avgSpikesPerFrame"] = stats.avgSpikesPerFrame;
    statsObj["avgCorrectionMagnitude"] = stats.avgCorrectionMagnitude;
}

void WsAudioCodec::encodeSpikeDetectionReset(JsonObject& data) {
    data["reset"] = true;
}

} // namespace codec
} // namespace lightwaveos
