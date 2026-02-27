// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpAudioCodec.cpp
 * @brief HTTP audio codec implementation
 *
 * Single canonical JSON parser for audio HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpAudioCodec.h"
#include "WsAudioCodec.h"  // Reuse decode logic where applicable
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

AudioParametersSetDecodeResult HttpAudioCodec::decodeParametersSet(JsonObjectConst root) {
    // Reuse WS decode logic directly (HTTP just ignores requestId field)
    return WsAudioCodec::decodeParametersSet(root);
}

AudioControlDecodeResult HttpAudioCodec::decodeControl(JsonObjectConst root) {
    AudioControlDecodeResult result;
    result.request = AudioControlRequest();

    // Extract action (required)
    if (root.containsKey("action") && root["action"].is<const char*>()) {
        result.request.action = root["action"].as<const char*>();
    } else {
        result.request.action = root["action"] | "";
    }

    result.success = true;
    return result;
}

AudioPresetSaveDecodeResult HttpAudioCodec::decodePresetSave(JsonObjectConst root) {
    AudioPresetSaveDecodeResult result;
    result.request = AudioPresetSaveRequest();

    // Extract name (optional, defaults to "Unnamed")
    if (root.containsKey("name") && root["name"].is<const char*>()) {
        result.request.name = root["name"].as<const char*>();
    } else {
        result.request.name = root["name"] | "Unnamed";
    }

    result.success = true;
    return result;
}

AudioZoneAgcSetDecodeResult HttpAudioCodec::decodeZoneAgcSet(JsonObjectConst root) {
    // Reuse WS decode logic directly (HTTP just ignores requestId field)
    return WsAudioCodec::decodeZoneAgcSet(root);
}

AudioMappingsSetDecodeResult HttpAudioCodec::decodeMappingsSet(JsonObjectConst root) {
    AudioMappingsSetDecodeResult result;
    result.request = AudioMappingsSetRequest();

    // Extract globalEnabled (optional, defaults to true)
    if (root.containsKey("globalEnabled")) {
        result.request.hasGlobalEnabled = true;
        if (root["globalEnabled"].is<bool>()) {
            result.request.globalEnabled = root["globalEnabled"].as<bool>();
        } else {
            result.request.globalEnabled = root["globalEnabled"] | true;
        }
    }

    // Extract mappings array
    if (root.containsKey("mappings") && root["mappings"].is<JsonArrayConst>()) {
        JsonArrayConst mappingsArr = root["mappings"].as<JsonArrayConst>();
        
        for (JsonVariantConst m : mappingsArr) {
            if (result.request.mappingCount >= 8) {  // MAX_MAPPINGS_PER_EFFECT
                break;
            }

            AudioMappingItem& mapping = result.request.mappings[result.request.mappingCount];
            
            mapping.source = m["source"] | "NONE";
            mapping.target = m["target"] | "NONE";
            mapping.curve = m["curve"] | "LINEAR";
            mapping.inputMin = m["inputMin"] | 0.0f;
            mapping.inputMax = m["inputMax"] | 1.0f;
            mapping.outputMin = m["outputMin"] | 0.0f;
            mapping.outputMax = m["outputMax"] | 255.0f;
            mapping.smoothingAlpha = m["smoothingAlpha"] | 0.3f;
            mapping.gain = m["gain"] | 1.0f;
            mapping.enabled = m["enabled"] | true;
            mapping.additive = m["additive"] | false;

            result.request.mappingCount++;
        }
    }

    result.success = true;
    return result;
}

AudioCalibrateStartDecodeResult HttpAudioCodec::decodeCalibrateStart(JsonObjectConst root) {
    AudioCalibrateStartDecodeResult result;
    result.request = AudioCalibrateStartRequest();

    // Extract durationMs (optional, defaults to 3000)
    if (root.containsKey("durationMs")) {
        result.request.hasDurationMs = true;
        if (root["durationMs"].is<uint32_t>()) {
            uint32_t val = root["durationMs"].as<uint32_t>();
            // Clamp to reasonable range
            if (val < 1000) val = 1000;
            if (val > 10000) val = 10000;
            result.request.durationMs = val;
        } else {
            result.request.durationMs = root["durationMs"] | 3000U;
        }
    }

    // Extract safetyMultiplier (optional, defaults to 1.2f)
    if (root.containsKey("safetyMultiplier")) {
        result.request.hasSafetyMultiplier = true;
        if (root["safetyMultiplier"].is<float>()) {
            float val = root["safetyMultiplier"].as<float>();
            // Clamp to reasonable range
            if (val < 1.0f) val = 1.0f;
            if (val > 3.0f) val = 3.0f;
            result.request.safetyMultiplier = val;
        } else {
            result.request.safetyMultiplier = root["safetyMultiplier"] | 1.2f;
        }
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void HttpAudioCodec::encodeParametersGet(const AudioPipelineTuningData& pipeline,
                                        const AudioContractTuningData& contract,
                                        const AudioDspStateData& state,
                                        const AudioCapabilitiesData& caps,
                                        JsonObject& data) {
    // Reuse WS encoder (same structure)
    WsAudioCodec::encodeParametersGet(pipeline, contract, state, caps, data);
}

void HttpAudioCodec::encodeParametersChanged(bool updatedPipeline,
                                             bool updatedContract,
                                             bool resetState,
                                             JsonObject& data) {
    // Reuse WS encoder (same structure)
    WsAudioCodec::encodeParametersChanged(updatedPipeline, updatedContract, resetState, data);
}

void HttpAudioCodec::encodeControlResponse(const char* stateStr, const char* action, JsonObject& data) {
    data["state"] = stateStr;
    data["action"] = action;
}

void HttpAudioCodec::encodeStateGet(const AudioActorStateData& state, JsonObject& data) {
    data["state"] = state.stateStr;
    data["capturing"] = state.capturing;
    data["hopCount"] = state.hopCount;
    data["sampleIndex"] = state.sampleIndex;
    
    JsonObject statsObj = data["stats"].to<JsonObject>();
    statsObj["tickCount"] = state.tickCount;
    statsObj["captureSuccess"] = state.captureSuccessCount;
    statsObj["captureFail"] = state.captureFailCount;
}

void HttpAudioCodec::encodeTempoGet(const AudioTempoData& tempo, JsonObject& data) {
    data["bpm"] = tempo.bpm;
    data["confidence"] = tempo.confidence;
    data["beat_phase"] = tempo.beatPhase;
    data["bar_phase"] = tempo.barPhase;
    data["beat_in_bar"] = tempo.beatInBar;
    data["beats_per_bar"] = tempo.beatsPerBar;
}

void HttpAudioCodec::encodePresetsList(const AudioPresetSummary* presets, uint8_t count, JsonObject& data) {
    data["count"] = count;
    JsonArray presetsArr = data["presets"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
        JsonObject p = presetsArr.add<JsonObject>();
        p["id"] = presets[i].id;
        p["name"] = presets[i].name;
    }
}

void HttpAudioCodec::encodePresetGet(uint8_t id, const char* name,
                                     const AudioPipelineTuningData& pipeline,
                                     const AudioContractTuningData& contract,
                                     JsonObject& data) {
    data["id"] = id;
    data["name"] = name;
    
    // Pipeline tuning
    JsonObject p = data["pipeline"].to<JsonObject>();
    p["dcAlpha"] = pipeline.dcAlpha;
    p["agcTargetRms"] = pipeline.agcTargetRms;
    p["agcMinGain"] = pipeline.agcMinGain;
    p["agcMaxGain"] = pipeline.agcMaxGain;
    p["agcAttack"] = pipeline.agcAttack;
    p["agcRelease"] = pipeline.agcRelease;
    p["agcClipReduce"] = pipeline.agcClipReduce;
    p["agcIdleReturnRate"] = pipeline.agcIdleReturnRate;
    p["noiseFloorMin"] = pipeline.noiseFloorMin;
    p["noiseFloorRise"] = pipeline.noiseFloorRise;
    p["noiseFloorFall"] = pipeline.noiseFloorFall;
    p["gateStartFactor"] = pipeline.gateStartFactor;
    p["gateRangeFactor"] = pipeline.gateRangeFactor;
    p["gateRangeMin"] = pipeline.gateRangeMin;
    p["rmsDbFloor"] = pipeline.rmsDbFloor;
    p["rmsDbCeil"] = pipeline.rmsDbCeil;
    p["bandDbFloor"] = pipeline.bandDbFloor;
    p["bandDbCeil"] = pipeline.bandDbCeil;
    p["chromaDbFloor"] = pipeline.chromaDbFloor;
    p["chromaDbCeil"] = pipeline.chromaDbCeil;
    p["fluxScale"] = pipeline.fluxScale;
    p["controlBusAlphaFast"] = pipeline.controlBusAlphaFast;
    p["controlBusAlphaSlow"] = pipeline.controlBusAlphaSlow;
    
    // Contract tuning
    JsonObject c = data["contract"].to<JsonObject>();
    c["audioStalenessMs"] = contract.audioStalenessMs;
    c["bpmMin"] = contract.bpmMin;
    c["bpmMax"] = contract.bpmMax;
    c["bpmTau"] = contract.bpmTau;
    c["confidenceTau"] = contract.confidenceTau;
    c["phaseCorrectionGain"] = contract.phaseCorrectionGain;
    c["barCorrectionGain"] = contract.barCorrectionGain;
    c["beatsPerBar"] = contract.beatsPerBar;
    c["beatUnit"] = contract.beatUnit;
}

void HttpAudioCodec::encodePresetSave(uint8_t id, const char* name, JsonObject& data) {
    data["id"] = id;
    data["name"] = name;
    data["message"] = "Preset saved";
}

void HttpAudioCodec::encodePresetApply(uint8_t id, const char* name, JsonObject& data) {
    data["id"] = id;
    data["name"] = name;
    data["message"] = "Preset applied";
}

void HttpAudioCodec::encodePresetDelete(uint8_t id, JsonObject& data) {
    data["id"] = id;
    data["message"] = "Preset deleted";
}

void HttpAudioCodec::encodeMappingsListSources(const AudioMappingSourceItem* sources, size_t count, JsonObject& data) {
    JsonArray sourcesArr = data["sources"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject s = sourcesArr.add<JsonObject>();
        s["name"] = sources[i].name;
        s["id"] = sources[i].id;
        s["category"] = sources[i].category;
        s["description"] = sources[i].description;
        s["rangeMin"] = sources[i].rangeMin;
        s["rangeMax"] = sources[i].rangeMax;
    }
}

void HttpAudioCodec::encodeMappingsListTargets(const AudioMappingTargetItem* targets, size_t count, JsonObject& data) {
    JsonArray targetsArr = data["targets"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject t = targetsArr.add<JsonObject>();
        t["name"] = targets[i].name;
        t["id"] = targets[i].id;
        t["rangeMin"] = targets[i].rangeMin;
        t["rangeMax"] = targets[i].rangeMax;
        t["default"] = targets[i].defaultValue;
        t["description"] = targets[i].description;
    }
}

void HttpAudioCodec::encodeMappingsListCurves(const AudioMappingCurveItem* curves, size_t count, JsonObject& data) {
    JsonArray curvesArr = data["curves"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject c = curvesArr.add<JsonObject>();
        c["name"] = curves[i].name;
        c["id"] = curves[i].id;
        c["formula"] = curves[i].formula;
        c["description"] = curves[i].description;
    }
}

void HttpAudioCodec::encodeMappingsList(uint8_t activeEffects, uint8_t totalMappings,
                                       const AudioMappingEffectItem* effects, size_t effectCount,
                                       JsonObject& data) {
    data["activeEffects"] = activeEffects;
    data["totalMappings"] = totalMappings;
    
    JsonArray effectsArr = data["effects"].to<JsonArray>();
    for (size_t i = 0; i < effectCount; i++) {
        JsonObject e = effectsArr.add<JsonObject>();
        e["id"] = effects[i].id;
        e["name"] = effects[i].name;
        e["mappingCount"] = effects[i].mappingCount;
        e["enabled"] = effects[i].enabled;
    }
}

void HttpAudioCodec::encodeMappingsGet(uint8_t effectId, const char* effectName, bool globalEnabled, uint8_t mappingCount,
                                      const AudioMappingDetailItem* mappings, size_t count,
                                      JsonObject& data) {
    data["effectId"] = effectId;
    data["effectName"] = effectName;
    data["globalEnabled"] = globalEnabled;
    data["mappingCount"] = mappingCount;
    
    JsonArray mappingsArr = data["mappings"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject mapping = mappingsArr.add<JsonObject>();
        mapping["source"] = mappings[i].source;
        mapping["target"] = mappings[i].target;
        mapping["curve"] = mappings[i].curve;
        mapping["inputMin"] = mappings[i].inputMin;
        mapping["inputMax"] = mappings[i].inputMax;
        mapping["outputMin"] = mappings[i].outputMin;
        mapping["outputMax"] = mappings[i].outputMax;
        mapping["smoothingAlpha"] = mappings[i].smoothingAlpha;
        mapping["gain"] = mappings[i].gain;
        mapping["enabled"] = mappings[i].enabled;
        mapping["additive"] = mappings[i].additive;
    }
}

void HttpAudioCodec::encodeMappingsSet(uint8_t effectId, uint8_t mappingCount, bool enabled, JsonObject& data) {
    data["effectId"] = effectId;
    data["mappingCount"] = mappingCount;
    data["enabled"] = enabled;
    data["message"] = "Mapping updated";
}

void HttpAudioCodec::encodeMappingsDelete(uint8_t effectId, JsonObject& data) {
    data["effectId"] = effectId;
    data["message"] = "Mapping cleared";
}

void HttpAudioCodec::encodeMappingsEnable(uint8_t effectId, bool enabled, JsonObject& data) {
    data["effectId"] = effectId;
    data["enabled"] = enabled;
}

void HttpAudioCodec::encodeMappingsStats(const AudioMappingStatsData& stats, JsonObject& data) {
    data["applyCount"] = stats.applyCount;
    data["lastApplyMicros"] = stats.lastApplyMicros;
    data["maxApplyMicros"] = stats.maxApplyMicros;
    data["activeEffectsWithMappings"] = stats.activeEffectsWithMappings;
    data["totalMappingsConfigured"] = stats.totalMappingsConfigured;
}

void HttpAudioCodec::encodeZoneAgcGet(bool enabled, bool lookaheadEnabled,
                                     const AudioZoneAgcZoneData* zones, size_t zoneCount,
                                     JsonObject& data) {
    // Reuse WS encoder (same structure)
    WsAudioCodec::encodeZoneAgcState(enabled, lookaheadEnabled, zones, zoneCount, data);
}

void HttpAudioCodec::encodeZoneAgcSet(bool updated, JsonObject& data) {
    // Reuse WS encoder (same structure)
    WsAudioCodec::encodeZoneAgcUpdated(updated, data);
}

void HttpAudioCodec::encodeSpikeDetectionGet(bool enabled, const AudioSpikeDetectionStatsData& stats, JsonObject& data) {
    // Reuse WS encoder (same structure)
    WsAudioCodec::encodeSpikeDetectionState(enabled, stats, data);
}

void HttpAudioCodec::encodeSpikeDetectionReset(JsonObject& data) {
    // Reuse WS encoder (same structure)
    WsAudioCodec::encodeSpikeDetectionReset(data);
}

void HttpAudioCodec::encodeCalibrationStatus(const AudioCalibrationStateData& state, JsonObject& data) {
    data["state"] = state.stateStr;
    data["durationMs"] = state.durationMs;
    data["safetyMultiplier"] = state.safetyMultiplier;
    data["maxAllowedRms"] = state.maxAllowedRms;
    
    // Progress info when measuring
    if (strcmp(state.stateStr, "measuring") == 0) {
        data["progress"] = state.progress;
        data["samplesCollected"] = state.samplesCollected;
        if (state.samplesCollected > 0) {
            data["currentAvgRms"] = state.currentAvgRms;
        }
    }
    
    // Result info when complete
    if (state.hasResult) {
        JsonObject resultObj = data["result"].to<JsonObject>();
        resultObj["overallRms"] = state.resultOverallRms;
        resultObj["peakRms"] = state.resultPeakRms;
        resultObj["sampleCount"] = state.resultSampleCount;
        
        JsonArray bands = resultObj["bandFloors"].to<JsonArray>();
        for (int i = 0; i < 8; ++i) {
            bands.add(state.resultBandFloors[i]);
        }
        
        JsonArray chroma = resultObj["chromaFloors"].to<JsonArray>();
        for (int i = 0; i < 12; ++i) {
            chroma.add(state.resultChromaFloors[i]);
        }
    }
}

void HttpAudioCodec::encodeCalibrationStart(uint32_t durationMs, float safetyMultiplier, JsonObject& data) {
    data["message"] = "Calibration started - please remain silent";
    data["durationMs"] = durationMs;
    data["safetyMultiplier"] = safetyMultiplier;
}

void HttpAudioCodec::encodeCalibrationApply(float noiseFloorMin, const float* perBandNoiseFloors, size_t count, JsonObject& data) {
    data["message"] = "Calibration applied successfully";
    data["noiseFloorMin"] = noiseFloorMin;
    
    JsonArray bands = data["perBandNoiseFloors"].to<JsonArray>();
    for (size_t i = 0; i < count && i < 8; i++) {
        bands.add(perBandNoiseFloors[i]);
    }
}

void HttpAudioCodec::encodeBenchmarkGet(const AudioBenchmarkStatsData& stats, JsonObject& data) {
    data["streaming"] = stats.streaming;
    
    JsonObject timing = data["timing"].to<JsonObject>();
    timing["avgTotalUs"] = stats.avgTotalUs;
    timing["avgGoertzelUs"] = stats.avgGoertzelUs;
    timing["avgDcAgcUs"] = stats.avgDcAgcUs;
    timing["avgChromaUs"] = stats.avgChromaUs;
    timing["peakTotalUs"] = stats.peakTotalUs;
    timing["peakGoertzelUs"] = stats.peakGoertzelUs;
    
    JsonObject load = data["load"].to<JsonObject>();
    load["cpuPercent"] = stats.cpuLoadPercent;
    load["hopCount"] = stats.hopCount;
    load["goertzelCount"] = stats.goertzelCount;
    
    JsonArray histogram = data["histogram"].to<JsonArray>();
    for (int i = 0; i < 8; i++) {
        histogram.add(stats.histogramBins[i]);
    }
}

void HttpAudioCodec::encodeBenchmarkStart(JsonObject& data) {
    data["message"] = "Benchmark collection started";
    data["active"] = true;
}

void HttpAudioCodec::encodeBenchmarkStop(const AudioBenchmarkStatsData& stats, JsonObject& data) {
    data["message"] = "Benchmark collection stopped";
    data["active"] = false;
    
    JsonObject results = data["results"].to<JsonObject>();
    results["avgTotalUs"] = stats.avgTotalUs;
    results["avgGoertzelUs"] = stats.avgGoertzelUs;
    results["cpuLoadPercent"] = stats.cpuLoadPercent;
    results["hopCount"] = stats.hopCount;
    results["peakTotalUs"] = stats.peakTotalUs;
}

void HttpAudioCodec::encodeBenchmarkHistory(uint32_t available, uint32_t returned,
                                            const AudioBenchmarkSampleData* samples, size_t count,
                                            JsonObject& data) {
    data["available"] = available;
    data["returned"] = returned;
    
    JsonArray arr = data["samples"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        JsonObject s = arr.add<JsonObject>();
        s["ts"] = samples[i].timestamp_us;
        s["total"] = samples[i].totalProcessUs;
        s["goertzel"] = samples[i].goertzelUs;
        s["dcAgc"] = samples[i].dcAgcLoopUs;
        s["chroma"] = samples[i].chromaUs;
    }
}

} // namespace codec
} // namespace lightwaveos
