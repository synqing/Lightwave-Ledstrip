/**
 * @file AudioHandlers.cpp
 * @brief Audio handlers implementation
 */

#include "AudioHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../core/actors/RendererNode.h"
#include <cstring>

#if FEATURE_AUDIO_SYNC
#include "../../../audio/AudioTuning.h"
#include "../../../config/audio_config.h"
#include "../../../core/persistence/AudioTuningManager.h"
#include "../../../audio/AudioMappingRegistry.h"
#endif

#if FEATURE_AUDIO_BENCHMARK
#include "../BenchmarkStreamBroadcaster.h"
#include "../../../audio/AudioBenchmarkMetrics.h"
#endif

#define LW_LOG_TAG "AudioHandlers"
#include "../../../utils/Log.h"

using namespace lightwaveos::nodes;
using namespace lightwaveos::network;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

#if FEATURE_AUDIO_SYNC

void AudioHandlers::handleParametersGet(AsyncWebServerRequest* request,
                                         NodeOrchestrator& orchestrator,
                                         RendererNode* renderer) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Audio system not available");
        return;
    }

    audio::AudioPipelineTuning pipeline = audio->getPipelineTuning();
    audio::AudioDspState state = audio->getDspState();
    audio::AudioContractTuning contract = renderer ? renderer->getAudioContractTuning()
                                                    : audio::clampAudioContractTuning(audio::AudioContractTuning{});

    sendSuccessResponse(request, [&](JsonObject& data) {
        JsonObject pipelineObj = data["pipeline"].to<JsonObject>();
        pipelineObj["dcAlpha"] = pipeline.dcAlpha;
        pipelineObj["agcEnabled"] = pipeline.agcEnabled;
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

        JsonObject controlBus = data["controlBus"].to<JsonObject>();
        controlBus["alphaFast"] = pipeline.controlBusAlphaFast;
        controlBus["alphaSlow"] = pipeline.controlBusAlphaSlow;

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

        JsonObject caps = data["capabilities"].to<JsonObject>();
        caps["sampleRate"] = audio::SAMPLE_RATE;
        caps["hopSize"] = audio::HOP_SIZE;
        caps["fftSize"] = audio::FFT_SIZE;
        caps["goertzelWindow"] = audio::GOERTZEL_WINDOW;
        caps["bandCount"] = audio::NUM_BANDS;
        caps["chromaCount"] = audio::CONTROLBUS_NUM_CHROMA;
        caps["waveformPoints"] = audio::CONTROLBUS_WAVEFORM_N;
    });
}

void AudioHandlers::handleParametersSet(AsyncWebServerRequest* request,
                                        uint8_t* data, size_t len,
                                        NodeOrchestrator& orchestrator,
                                        RendererNode* renderer) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Audio system not available");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    bool updatedPipeline = false;
    bool updatedContract = false;
    bool resetState = false;

    audio::AudioPipelineTuning pipeline = audio->getPipelineTuning();
    audio::AudioContractTuning contract = renderer ? renderer->getAudioContractTuning()
                                                    : audio::clampAudioContractTuning(audio::AudioContractTuning{});

    auto applyFloat = [](JsonVariant source, const char* key, float& target, bool& updated) {
        if (!source.is<JsonObject>()) return;
        if (source.containsKey(key)) {
            target = source[key].as<float>();
            updated = true;
        }
    };

    auto applyUint8 = [](JsonVariant source, const char* key, uint8_t& target, bool& updated) {
        if (!source.is<JsonObject>()) return;
        if (source.containsKey(key)) {
            target = source[key].as<uint8_t>();
            updated = true;
        }
    };

    JsonVariant pipelineSrc = doc.as<JsonVariant>();
    if (doc.containsKey("pipeline")) {
        pipelineSrc = doc["pipeline"];
    }
    applyFloat(pipelineSrc, "dcAlpha", pipeline.dcAlpha, updatedPipeline);
    applyFloat(pipelineSrc, "agcTargetRms", pipeline.agcTargetRms, updatedPipeline);
    applyFloat(pipelineSrc, "agcMinGain", pipeline.agcMinGain, updatedPipeline);
    applyFloat(pipelineSrc, "agcMaxGain", pipeline.agcMaxGain, updatedPipeline);
    applyFloat(pipelineSrc, "agcAttack", pipeline.agcAttack, updatedPipeline);
    applyFloat(pipelineSrc, "agcRelease", pipeline.agcRelease, updatedPipeline);
    applyFloat(pipelineSrc, "agcClipReduce", pipeline.agcClipReduce, updatedPipeline);
    applyFloat(pipelineSrc, "agcIdleReturnRate", pipeline.agcIdleReturnRate, updatedPipeline);
    
    // AGC enable/disable toggle
    if (pipelineSrc.containsKey("agcEnabled")) {
        pipeline.agcEnabled = pipelineSrc["agcEnabled"].as<bool>();
        updatedPipeline = true;
    }
    
    applyFloat(pipelineSrc, "noiseFloorMin", pipeline.noiseFloorMin, updatedPipeline);
    applyFloat(pipelineSrc, "noiseFloorRise", pipeline.noiseFloorRise, updatedPipeline);
    applyFloat(pipelineSrc, "noiseFloorFall", pipeline.noiseFloorFall, updatedPipeline);
    applyFloat(pipelineSrc, "gateStartFactor", pipeline.gateStartFactor, updatedPipeline);
    applyFloat(pipelineSrc, "gateRangeFactor", pipeline.gateRangeFactor, updatedPipeline);
    applyFloat(pipelineSrc, "gateRangeMin", pipeline.gateRangeMin, updatedPipeline);
    applyFloat(pipelineSrc, "rmsDbFloor", pipeline.rmsDbFloor, updatedPipeline);
    applyFloat(pipelineSrc, "rmsDbCeil", pipeline.rmsDbCeil, updatedPipeline);
    applyFloat(pipelineSrc, "bandDbFloor", pipeline.bandDbFloor, updatedPipeline);
    applyFloat(pipelineSrc, "bandDbCeil", pipeline.bandDbCeil, updatedPipeline);
    applyFloat(pipelineSrc, "chromaDbFloor", pipeline.chromaDbFloor, updatedPipeline);
    applyFloat(pipelineSrc, "chromaDbCeil", pipeline.chromaDbCeil, updatedPipeline);
    applyFloat(pipelineSrc, "fluxScale", pipeline.fluxScale, updatedPipeline);

    JsonVariant controlBusSrc = doc.as<JsonVariant>();
    if (doc.containsKey("controlBus")) {
        controlBusSrc = doc["controlBus"];
    }
    applyFloat(controlBusSrc, "alphaFast", pipeline.controlBusAlphaFast, updatedPipeline);
    applyFloat(controlBusSrc, "alphaSlow", pipeline.controlBusAlphaSlow, updatedPipeline);

    JsonVariant contractSrc = doc.as<JsonVariant>();
    if (doc.containsKey("contract")) {
        contractSrc = doc["contract"];
    }
    applyFloat(contractSrc, "audioStalenessMs", contract.audioStalenessMs, updatedContract);
    applyFloat(contractSrc, "bpmMin", contract.bpmMin, updatedContract);
    applyFloat(contractSrc, "bpmMax", contract.bpmMax, updatedContract);
    applyFloat(contractSrc, "bpmTau", contract.bpmTau, updatedContract);
    applyFloat(contractSrc, "confidenceTau", contract.confidenceTau, updatedContract);
    applyFloat(contractSrc, "phaseCorrectionGain", contract.phaseCorrectionGain, updatedContract);
    applyFloat(contractSrc, "barCorrectionGain", contract.barCorrectionGain, updatedContract);
    applyUint8(contractSrc, "beatsPerBar", contract.beatsPerBar, updatedContract);
    applyUint8(contractSrc, "beatUnit", contract.beatUnit, updatedContract);

    if (doc.containsKey("resetState")) {
        resetState = doc["resetState"] | false;
    }

    if (updatedPipeline) {
        audio->setPipelineTuning(pipeline);
    }
    if (updatedContract && renderer) {
        renderer->setAudioContractTuning(contract);
    }
    if (resetState) {
        audio->resetDspState();
    }

    sendSuccessResponse(request, [updatedPipeline, updatedContract, resetState](JsonObject& resp) {
        JsonArray updated = resp["updated"].to<JsonArray>();
        if (updatedPipeline) updated.add("pipeline");
        if (updatedContract) updated.add("contract");
        if (resetState) updated.add("state");
    });
}

void AudioHandlers::handleControl(AsyncWebServerRequest* request,
                                   uint8_t* data, size_t len,
                                   NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    const char* action = doc["action"] | "";
    if (strcmp(action, "pause") == 0) {
        audio->pause();
        sendSuccessResponse(request, [](JsonObject& d) {
            d["state"] = "PAUSED";
            d["action"] = "pause";
        });
    } else if (strcmp(action, "resume") == 0) {
        audio->resume();
        sendSuccessResponse(request, [](JsonObject& d) {
            d["state"] = "RUNNING";
            d["action"] = "resume";
        });
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_ACTION, "Use action: pause or resume", "action");
    }
}

void AudioHandlers::handleAGCToggle(AsyncWebServerRequest* request,
                                     uint8_t* data, size_t len,
                                     NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("enabled")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing 'enabled' field");
        return;
    }

    bool enabled = doc["enabled"].as<bool>();
    audio::AudioPipelineTuning tuning = audio->getPipelineTuning();
    tuning.agcEnabled = enabled;
    audio->setPipelineTuning(tuning);

    sendSuccessResponse(request, [enabled](JsonObject& d) {
        d["enabled"] = enabled;
        d["agcEnabled"] = enabled;  // Alias for consistency
    });
}

void AudioHandlers::handleStateGet(AsyncWebServerRequest* request,
                                    NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    audio::AudioNodeState state = audio->getState();
    const audio::AudioNodeStats& stats = audio->getStats();

    const char* stateStr = "UNKNOWN";
    switch (state) {
        case audio::AudioNodeState::UNINITIALIZED: stateStr = "UNINITIALIZED"; break;
        case audio::AudioNodeState::INITIALIZING:  stateStr = "INITIALIZING"; break;
        case audio::AudioNodeState::RUNNING:       stateStr = "RUNNING"; break;
        case audio::AudioNodeState::PAUSED:        stateStr = "PAUSED"; break;
        case audio::AudioNodeState::ERROR:         stateStr = "ERROR"; break;
    }

    sendSuccessResponse(request, [stateStr, &stats, audio](JsonObject& d) {
        d["state"] = stateStr;
        d["capturing"] = audio->isCapturing();
        d["hopCount"] = audio->getHopCount();
        d["sampleIndex"] = (uint32_t)(audio->getSampleIndex() & 0xFFFFFFFF);
        JsonObject statsObj = d["stats"].to<JsonObject>();
        statsObj["tickCount"] = stats.tickCount;
        statsObj["captureSuccess"] = stats.captureSuccessCount;
        statsObj["captureFail"] = stats.captureFailCount;
    });
}

void AudioHandlers::handleTempoGet(AsyncWebServerRequest* request,
                                    NodeOrchestrator& orchestrator) {
    auto* renderer = orchestrator.getRenderer();
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Renderer not available");
        return;
    }

    const audio::MusicalGridSnapshot& grid = renderer->getLastMusicalGrid();

    sendSuccessResponse(request, [&grid](JsonObject& d) {
        d["bpm"] = grid.bpm_smoothed;
        d["confidence"] = grid.tempo_confidence;
        d["beat_phase"] = grid.beat_phase01;
        d["bar_phase"] = grid.bar_phase01;
        d["beat_in_bar"] = grid.beat_in_bar;
        d["beats_per_bar"] = grid.beats_per_bar;
    });
}

void AudioHandlers::handlePresetsList(AsyncWebServerRequest* request) {
    using namespace persistence;
    auto& mgr = AudioTuningManager::instance();

    char names[AudioTuningManager::MAX_PRESETS][AudioTuningPreset::NAME_MAX_LEN];
    uint8_t ids[AudioTuningManager::MAX_PRESETS];
    uint8_t count = mgr.listPresets(names, ids);

    sendSuccessResponse(request, [&names, &ids, count](JsonObject& d) {
        d["count"] = count;
        JsonArray presets = d["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            JsonObject p = presets.add<JsonObject>();
            p["id"] = ids[i];
            p["name"] = names[i];
        }
    });
}

void AudioHandlers::handlePresetGet(AsyncWebServerRequest* request, uint8_t presetId) {
    using namespace persistence;
    auto& mgr = AudioTuningManager::instance();

    if (presetId >= AudioTuningManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", "presetId");
        return;
    }

    audio::AudioPipelineTuning pipeline;
    audio::AudioContractTuning contract;
    char name[AudioTuningPreset::NAME_MAX_LEN];

    if (!mgr.loadPreset(presetId, pipeline, contract, name)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "presetId");
        return;
    }

    sendSuccessResponseLarge(request, [presetId, &name, &pipeline, &contract](JsonObject& d) {
        d["id"] = presetId;
        d["name"] = name;

        // Pipeline tuning (DSP parameters)
        JsonObject p = d["pipeline"].to<JsonObject>();
        p["dcAlpha"] = pipeline.dcAlpha;
        p["agcEnabled"] = pipeline.agcEnabled;
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

        // Contract tuning (tempo/beat parameters)
        JsonObject c = d["contract"].to<JsonObject>();
        c["audioStalenessMs"] = contract.audioStalenessMs;
        c["bpmMin"] = contract.bpmMin;
        c["bpmMax"] = contract.bpmMax;
        c["bpmTau"] = contract.bpmTau;
        c["confidenceTau"] = contract.confidenceTau;
        c["phaseCorrectionGain"] = contract.phaseCorrectionGain;
        c["barCorrectionGain"] = contract.barCorrectionGain;
        c["beatsPerBar"] = contract.beatsPerBar;
        c["beatUnit"] = contract.beatUnit;
    }, 2048);
}

void AudioHandlers::handlePresetSave(AsyncWebServerRequest* request,
                                      uint8_t* data, size_t len,
                                      NodeOrchestrator& orchestrator,
                                      RendererNode* renderer) {
    using namespace persistence;

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "JSON parse error");
        return;
    }

    const char* name = doc["name"] | "Unnamed";

    // Get current tuning from AudioNode
    auto* audioActor = orchestrator.getAudio();
    if (!audioActor) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    audio::AudioPipelineTuning pipeline = audioActor->getPipelineTuning();
    audio::AudioContractTuning contract = renderer ? renderer->getAudioContractTuning()
                                                      : audio::clampAudioContractTuning(audio::AudioContractTuning{});

    auto& mgr = AudioTuningManager::instance();
    int8_t slotId = mgr.savePreset(name, pipeline, contract);

    if (slotId < 0) {
        sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                          ErrorCodes::STORAGE_FULL, "No free preset slots");
        return;
    }

    sendSuccessResponse(request, [slotId, name](JsonObject& d) {
        d["id"] = (uint8_t)slotId;
        d["name"] = name;
        d["message"] = "Preset saved";
    });
}

void AudioHandlers::handlePresetApply(AsyncWebServerRequest* request, uint8_t presetId,
                                        NodeOrchestrator& orchestrator,
                                        RendererNode* renderer) {
    using namespace persistence;
    auto& mgr = AudioTuningManager::instance();

    if (presetId >= AudioTuningManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", "presetId");
        return;
    }

    audio::AudioPipelineTuning pipeline;
    audio::AudioContractTuning contract;
    char name[AudioTuningPreset::NAME_MAX_LEN];

    if (!mgr.loadPreset(presetId, pipeline, contract, name)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "presetId");
        return;
    }

    // Apply to AudioNode
    auto* audioActor = orchestrator.getAudio();
    if (!audioActor) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    audioActor->setPipelineTuning(pipeline);
    if (renderer) {
        renderer->setAudioContractTuning(contract);
    }

    sendSuccessResponse(request, [presetId, name](JsonObject& d) {
        d["id"] = presetId;
        d["name"] = name;
        d["message"] = "Preset applied";
    });
}

void AudioHandlers::handlePresetDelete(AsyncWebServerRequest* request, uint8_t presetId) {
    using namespace persistence;
    auto& mgr = AudioTuningManager::instance();

    if (presetId >= AudioTuningManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", "presetId");
        return;
    }

    if (!mgr.hasPreset(presetId)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "presetId");
        return;
    }

    if (!mgr.deletePreset(presetId)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to delete preset");
        return;
    }

    sendSuccessResponse(request, [presetId](JsonObject& d) {
        d["id"] = presetId;
        d["message"] = "Preset deleted";
    });
}

void AudioHandlers::handleMappingsListSources(AsyncWebServerRequest* request) {
    using namespace audio;

    sendSuccessResponseLarge(request, [](JsonObject& data) {
        JsonArray sources = data["sources"].to<JsonArray>();

        // Energy metrics
        auto addSource = [&](const char* name, uint8_t id, const char* category,
                             const char* desc, float min, float max) {
            JsonObject s = sources.add<JsonObject>();
            s["name"] = name;
            s["id"] = id;
            s["category"] = category;
            s["description"] = desc;
            s["rangeMin"] = min;
            s["rangeMax"] = max;
        };

        addSource("RMS", 0, "energy", "Smoothed RMS level", 0.0f, 1.0f);
        addSource("FAST_RMS", 1, "energy", "Fast-attack RMS", 0.0f, 1.0f);
        addSource("FLUX", 2, "energy", "Spectral flux (onset)", 0.0f, 1.0f);
        addSource("FAST_FLUX", 3, "energy", "Fast-attack flux", 0.0f, 1.0f);

        addSource("BAND_0", 4, "frequency", "60 Hz - Sub-bass", 0.0f, 1.0f);
        addSource("BAND_1", 5, "frequency", "120 Hz - Bass", 0.0f, 1.0f);
        addSource("BAND_2", 6, "frequency", "250 Hz - Low-mid", 0.0f, 1.0f);
        addSource("BAND_3", 7, "frequency", "500 Hz - Mid", 0.0f, 1.0f);
        addSource("BAND_4", 8, "frequency", "1000 Hz - High-mid", 0.0f, 1.0f);
        addSource("BAND_5", 9, "frequency", "2000 Hz - Presence", 0.0f, 1.0f);
        addSource("BAND_6", 10, "frequency", "4000 Hz - Brilliance", 0.0f, 1.0f);
        addSource("BAND_7", 11, "frequency", "7800 Hz - Air", 0.0f, 1.0f);

        addSource("BASS", 12, "aggregate", "(band0 + band1) / 2", 0.0f, 1.0f);
        addSource("MID", 13, "aggregate", "(band2 + band3 + band4) / 3", 0.0f, 1.0f);
        addSource("TREBLE", 14, "aggregate", "(band5 + band6 + band7) / 3", 0.0f, 1.0f);
        addSource("HEAVY_BASS", 15, "aggregate", "Squared bass response", 0.0f, 1.0f);

        addSource("BEAT_PHASE", 16, "timing", "Beat phase [0,1)", 0.0f, 1.0f);
        addSource("BPM", 17, "timing", "Tempo in BPM", 30.0f, 300.0f);
        addSource("TEMPO_CONFIDENCE", 18, "timing", "Beat detection confidence", 0.0f, 1.0f);
    }, 2048);
}

void AudioHandlers::handleMappingsListTargets(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        JsonArray targets = data["targets"].to<JsonArray>();

        auto addTarget = [&](const char* name, uint8_t id, uint8_t min, uint8_t max,
                             uint8_t defVal, const char* desc) {
            JsonObject t = targets.add<JsonObject>();
            t["name"] = name;
            t["id"] = id;
            t["rangeMin"] = min;
            t["rangeMax"] = max;
            t["default"] = defVal;
            t["description"] = desc;
        };

        addTarget("BRIGHTNESS", 0, 0, 160, 96, "Master LED intensity");
        addTarget("SPEED", 1, 1, 50, 10, "Animation rate");
        addTarget("INTENSITY", 2, 0, 255, 128, "Effect amplitude");
        addTarget("SATURATION", 3, 0, 255, 255, "Color saturation");
        addTarget("COMPLEXITY", 4, 0, 255, 128, "Pattern detail");
        addTarget("VARIATION", 5, 0, 255, 0, "Mode selection");
        addTarget("HUE", 6, 0, 255, 0, "Color rotation");
    });
}

void AudioHandlers::handleMappingsListCurves(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        JsonArray curves = data["curves"].to<JsonArray>();

        auto addCurve = [&](const char* name, uint8_t id, const char* formula, const char* desc) {
            JsonObject c = curves.add<JsonObject>();
            c["name"] = name;
            c["id"] = id;
            c["formula"] = formula;
            c["description"] = desc;
        };

        addCurve("LINEAR", 0, "y = x", "Direct proportional");
        addCurve("SQUARED", 1, "y = x²", "Gentle start, aggressive end");
        addCurve("SQRT", 2, "y = √x", "Aggressive start, gentle end");
        addCurve("LOG", 3, "y = log(x+1)/log(2)", "Logarithmic compression");
        addCurve("EXP", 4, "y = (eˣ-1)/(e-1)", "Exponential expansion");
        addCurve("INVERTED", 5, "y = 1 - x", "Inverse");
    });
}

void AudioHandlers::handleMappingsList(AsyncWebServerRequest* request,
                                        RendererNode* renderer) {
    using namespace audio;
    auto& registry = AudioMappingRegistry::instance();

    sendSuccessResponseLarge(request, [renderer, &registry](JsonObject& data) {
        data["activeEffects"] = registry.getActiveEffectCount();
        data["totalMappings"] = registry.getTotalMappingCount();

        JsonArray effects = data["effects"].to<JsonArray>();

        uint8_t effectCount = renderer->getEffectCount();
        for (uint8_t i = 0; i < effectCount && i < AudioMappingRegistry::MAX_EFFECTS; i++) {
            const EffectAudioMapping* mapping = registry.getMapping(i);
            if (mapping && mapping->globalEnabled && mapping->mappingCount > 0) {
                JsonObject e = effects.add<JsonObject>();
                e["id"] = i;
                e["name"] = renderer->getEffectName(i);
                e["mappingCount"] = mapping->mappingCount;
                e["enabled"] = mapping->globalEnabled;
            }
        }
    }, 2048);
}

void AudioHandlers::handleMappingsGet(AsyncWebServerRequest* request, uint8_t effectId,
                                       RendererNode* renderer) {
    using namespace audio;
    auto& registry = AudioMappingRegistry::instance();

    if (effectId >= AudioMappingRegistry::MAX_EFFECTS ||
        effectId >= renderer->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    const EffectAudioMapping* config = registry.getMapping(effectId);
    if (!config) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to get mapping");
        return;
    }

    sendSuccessResponseLarge(request, [effectId, renderer, config](JsonObject& data) {
        data["effectId"] = effectId;
        data["effectName"] = renderer->getEffectName(effectId);
        data["globalEnabled"] = config->globalEnabled;
        data["mappingCount"] = config->mappingCount;

        JsonArray mappings = data["mappings"].to<JsonArray>();

        for (uint8_t i = 0; i < config->mappingCount; i++) {
            const AudioParameterMapping& m = config->mappings[i];
            JsonObject mapping = mappings.add<JsonObject>();

            mapping["source"] = AudioMappingRegistry::getSourceName(m.source);
            mapping["target"] = AudioMappingRegistry::getTargetName(m.target);
            mapping["curve"] = AudioMappingRegistry::getCurveName(m.curve);
            mapping["inputMin"] = m.inputMin;
            mapping["inputMax"] = m.inputMax;
            mapping["outputMin"] = m.outputMin;
            mapping["outputMax"] = m.outputMax;
            mapping["smoothingAlpha"] = m.smoothingAlpha;
            mapping["gain"] = m.gain;
            mapping["enabled"] = m.enabled;
            mapping["additive"] = m.additive;
        }
    }, 2048);
}

void AudioHandlers::handleMappingsSet(AsyncWebServerRequest* request, uint8_t effectId,
                                       uint8_t* data, size_t len,
                                       RendererNode* renderer) {
    using namespace audio;
    auto& registry = AudioMappingRegistry::instance();

    if (effectId >= AudioMappingRegistry::MAX_EFFECTS ||
        effectId >= renderer->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, err.c_str());
        return;
    }

    EffectAudioMapping newConfig;
    newConfig.effectId = effectId;
    newConfig.globalEnabled = doc["globalEnabled"] | true;
    newConfig.mappingCount = 0;

    if (doc["mappings"].is<JsonArray>()) {
        JsonArray mappingsArr = doc["mappings"];

        for (JsonVariant m : mappingsArr) {
            if (newConfig.mappingCount >= EffectAudioMapping::MAX_MAPPINGS_PER_EFFECT) {
                break;
            }

            AudioParameterMapping mapping;
            mapping.source = AudioMappingRegistry::parseSource(m["source"] | "NONE");
            mapping.target = AudioMappingRegistry::parseTarget(m["target"] | "NONE");
            mapping.curve = AudioMappingRegistry::parseCurve(m["curve"] | "LINEAR");
            mapping.inputMin = m["inputMin"] | 0.0f;
            mapping.inputMax = m["inputMax"] | 1.0f;
            mapping.outputMin = m["outputMin"] | 0.0f;
            mapping.outputMax = m["outputMax"] | 255.0f;
            mapping.smoothingAlpha = m["smoothingAlpha"] | 0.3f;
            mapping.gain = m["gain"] | 1.0f;
            mapping.enabled = m["enabled"] | true;
            mapping.additive = m["additive"] | false;

            // Validate
            if (mapping.source == AudioSource::NONE || mapping.target == VisualTarget::NONE) {
                continue;  // Skip invalid mappings
            }

            newConfig.mappings[newConfig.mappingCount++] = mapping;
        }
    }

    if (!registry.setMapping(effectId, newConfig)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to set mapping");
        return;
    }

    sendSuccessResponse(request, [effectId, &newConfig](JsonObject& respData) {
        respData["effectId"] = effectId;
        respData["mappingCount"] = newConfig.mappingCount;
        respData["enabled"] = newConfig.globalEnabled;
        respData["message"] = "Mapping updated";
    });
}

void AudioHandlers::handleMappingsDelete(AsyncWebServerRequest* request, uint8_t effectId) {
    using namespace audio;
    auto& registry = AudioMappingRegistry::instance();

    if (effectId >= AudioMappingRegistry::MAX_EFFECTS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    EffectAudioMapping* config = registry.getMapping(effectId);
    if (config) {
        config->clearMappings();
        config->globalEnabled = false;
    }

    sendSuccessResponse(request, [effectId](JsonObject& d) {
        d["effectId"] = effectId;
        d["message"] = "Mapping cleared";
    });
}

void AudioHandlers::handleMappingsEnable(AsyncWebServerRequest* request, uint8_t effectId, bool enable) {
    using namespace audio;
    auto& registry = AudioMappingRegistry::instance();

    if (effectId >= AudioMappingRegistry::MAX_EFFECTS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    registry.setEffectMappingEnabled(effectId, enable);

    sendSuccessResponse(request, [effectId, enable](JsonObject& d) {
        d["effectId"] = effectId;
        d["enabled"] = enable;
    });
}

void AudioHandlers::handleMappingsStats(AsyncWebServerRequest* request) {
    using namespace audio;
    auto& registry = AudioMappingRegistry::instance();

    sendSuccessResponse(request, [&registry](JsonObject& data) {
        data["applyCount"] = registry.getApplyCount();
        data["lastApplyMicros"] = registry.getLastApplyMicros();
        data["maxApplyMicros"] = registry.getMaxApplyMicros();
        data["activeEffectsWithMappings"] = registry.getActiveEffectCount();
        data["totalMappingsConfigured"] = registry.getTotalMappingCount();
    });
}

void AudioHandlers::handleZoneAGCGet(AsyncWebServerRequest* request,
                                      NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    const audio::ControlBus& controlBus = audio->getControlBusRef();

    sendSuccessResponse(request, [&controlBus](JsonObject& data) {
        data["enabled"] = controlBus.getZoneAGCEnabled();
        data["lookaheadEnabled"] = controlBus.getLookaheadEnabled();

        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t z = 0; z < audio::CONTROLBUS_NUM_ZONES; ++z) {
            JsonObject zone = zones.add<JsonObject>();
            zone["index"] = z;
            zone["follower"] = controlBus.getZoneFollower(z);
            zone["maxMag"] = controlBus.getZoneMaxMag(z);
        }
    });
}

void AudioHandlers::handleZoneAGCSet(AsyncWebServerRequest* request,
                                       uint8_t* data, size_t len,
                                       NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    audio::ControlBus& controlBus = audio->getControlBusMut();
    bool updated = false;

    if (doc.containsKey("enabled")) {
        controlBus.setZoneAGCEnabled(doc["enabled"].as<bool>());
        updated = true;
    }

    if (doc.containsKey("lookaheadEnabled")) {
        controlBus.setLookaheadEnabled(doc["lookaheadEnabled"].as<bool>());
        updated = true;
    }

    if (doc.containsKey("attackRate") || doc.containsKey("releaseRate")) {
        float attack = doc["attackRate"] | 0.05f;
        float release = doc["releaseRate"] | 0.05f;
        controlBus.setZoneAGCRates(attack, release);
        updated = true;
    }

    if (doc.containsKey("minFloor")) {
        controlBus.setZoneMinFloor(doc["minFloor"].as<float>());
        updated = true;
    }

    sendSuccessResponse(request, [updated](JsonObject& resp) {
        resp["updated"] = updated;
    });
}

void AudioHandlers::handleSpikeDetectionGet(AsyncWebServerRequest* request,
                                               NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    const audio::ControlBus& controlBus = audio->getControlBusRef();
    const audio::SpikeDetectionStats& stats = controlBus.getSpikeStats();

    sendSuccessResponse(request, [&controlBus, &stats](JsonObject& data) {
        data["enabled"] = controlBus.getLookaheadEnabled();

        JsonObject statsObj = data["stats"].to<JsonObject>();
        statsObj["totalFrames"] = stats.totalFrames;
        statsObj["spikesDetectedBands"] = stats.spikesDetectedBands;
        statsObj["spikesDetectedChroma"] = stats.spikesDetectedChroma;
        statsObj["spikesCorrected"] = stats.spikesCorrected;
        statsObj["totalEnergyRemoved"] = stats.totalEnergyRemoved;
        statsObj["avgSpikesPerFrame"] = stats.avgSpikesPerFrame;
        statsObj["avgCorrectionMagnitude"] = stats.avgCorrectionMagnitude;
    });
}

void AudioHandlers::handleSpikeDetectionReset(AsyncWebServerRequest* request,
                                                NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    audio->getControlBusMut().resetSpikeStats();
    sendSuccessResponse(request);
}

void AudioHandlers::handleCalibrateStatus(AsyncWebServerRequest* request,
                                             NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    const auto& calState = audio->getNoiseCalibrationState();
    const auto& result = audio->getCalibrationResult();

    // Capture values for lambda
    uint32_t elapsed = 0;
    if (calState.state == audio::CalibrationState::MEASURING) {
        elapsed = millis() - calState.startTimeMs;
    }

    sendSuccessResponse(request, [&calState, &result, elapsed](JsonObject& data) {
        // Map state enum to string
        const char* stateStr = "unknown";
        switch (calState.state) {
            case audio::CalibrationState::IDLE:      stateStr = "idle"; break;
            case audio::CalibrationState::REQUESTED: stateStr = "requested"; break;
            case audio::CalibrationState::MEASURING: stateStr = "measuring"; break;
            case audio::CalibrationState::COMPLETE:  stateStr = "complete"; break;
            case audio::CalibrationState::FAILED:    stateStr = "failed"; break;
        }
        data["state"] = stateStr;
        data["durationMs"] = calState.durationMs;
        data["safetyMultiplier"] = calState.safetyMultiplier;
        data["maxAllowedRms"] = calState.maxAllowedRms;

        // Progress info when measuring
        if (calState.state == audio::CalibrationState::MEASURING) {
            float progress = (float)elapsed / (float)calState.durationMs;
            if (progress > 1.0f) progress = 1.0f;
            data["progress"] = progress;
            data["samplesCollected"] = calState.sampleCount;
            if (calState.sampleCount > 0) {
                data["currentAvgRms"] = calState.rmsSum / calState.sampleCount;
            }
        }

        // Result info when complete
        if (calState.state == audio::CalibrationState::COMPLETE && result.valid) {
            JsonObject resultObj = data["result"].to<JsonObject>();
            resultObj["overallRms"] = result.overallRms;
            resultObj["peakRms"] = result.peakRms;
            resultObj["sampleCount"] = result.sampleCount;

            JsonArray bands = resultObj["bandFloors"].to<JsonArray>();
            for (int i = 0; i < 8; ++i) {
                bands.add(result.bandFloors[i]);
            }

            JsonArray chroma = resultObj["chromaFloors"].to<JsonArray>();
            for (int i = 0; i < 12; ++i) {
                chroma.add(result.chromaFloors[i]);
            }
        }
    });
}

void AudioHandlers::handleCalibrateStart(AsyncWebServerRequest* request,
                                           uint8_t* data, size_t len,
                                           NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    // Parse optional parameters
    uint32_t durationMs = 3000;
    float safetyMultiplier = 1.2f;

    if (len > 0) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (!err) {
            if (doc.containsKey("durationMs")) {
                durationMs = doc["durationMs"].as<uint32_t>();
                // Clamp to reasonable range
                if (durationMs < 1000) durationMs = 1000;
                if (durationMs > 10000) durationMs = 10000;
            }
            if (doc.containsKey("safetyMultiplier")) {
                safetyMultiplier = doc["safetyMultiplier"].as<float>();
                // Clamp to reasonable range
                if (safetyMultiplier < 1.0f) safetyMultiplier = 1.0f;
                if (safetyMultiplier > 3.0f) safetyMultiplier = 3.0f;
            }
        }
    }

    bool started = audio->startNoiseCalibration(durationMs, safetyMultiplier);
    if (started) {
        sendSuccessResponse(request, [durationMs, safetyMultiplier](JsonObject& data) {
            data["message"] = "Calibration started - please remain silent";
            data["durationMs"] = durationMs;
            data["safetyMultiplier"] = safetyMultiplier;
        });
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::BUSY, "Calibration already in progress");
    }
}

void AudioHandlers::handleCalibrateCancel(AsyncWebServerRequest* request,
                                            NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    audio->cancelNoiseCalibration();
    sendSuccessResponse(request);
}

void AudioHandlers::handleCalibrateApply(AsyncWebServerRequest* request,
                                           NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    bool applied = audio->applyCalibrationResults();
    if (applied) {
        const auto& result = audio->getCalibrationResult();
        const auto& calState = audio->getNoiseCalibrationState();
        float noiseFloorMin = result.overallRms * calState.safetyMultiplier;

        sendSuccessResponse(request, [&result, noiseFloorMin](JsonObject& data) {
            data["message"] = "Calibration applied successfully";
            data["noiseFloorMin"] = noiseFloorMin;

            JsonArray bands = data["perBandNoiseFloors"].to<JsonArray>();
            for (int i = 0; i < 8; ++i) {
                bands.add(result.bandFloors[i]);
            }
        });
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "No valid calibration results to apply");
    }
}

#if FEATURE_AUDIO_BENCHMARK
void AudioHandlers::handleBenchmarkGet(AsyncWebServerRequest* request,
                                        NodeOrchestrator& orchestrator,
                                        webserver::BenchmarkStreamBroadcaster* broadcaster) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();
    bool hasSubs = broadcaster && broadcaster->hasSubscribers();

    sendSuccessResponse(request, [&stats, hasSubs](JsonObject& d) {
        d["streaming"] = hasSubs;

        JsonObject timing = d["timing"].to<JsonObject>();
        timing["avgTotalUs"] = stats.avgTotalUs;
        timing["avgGoertzelUs"] = stats.avgGoertzelUs;
        timing["avgDcAgcUs"] = stats.avgDcAgcUs;
        timing["avgChromaUs"] = stats.avgChromaUs;
        timing["peakTotalUs"] = stats.peakTotalUs;
        timing["peakGoertzelUs"] = stats.peakGoertzelUs;

        JsonObject load = d["load"].to<JsonObject>();
        load["cpuPercent"] = stats.cpuLoadPercent;
        load["hopCount"] = stats.hopCount;
        load["goertzelCount"] = stats.goertzelCount;

        JsonArray histogram = d["histogram"].to<JsonArray>();
        for (int i = 0; i < 8; i++) {
            histogram.add(stats.histogramBins[i]);
        }
    });
}

void AudioHandlers::handleBenchmarkStart(AsyncWebServerRequest* request,
                                          NodeOrchestrator& orchestrator,
                                          webserver::BenchmarkStreamBroadcaster* broadcaster) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    // Reset stats to start fresh collection
    audio->resetBenchmarkStats();

    if (broadcaster) broadcaster->setStreamingActive(true);

    LW_LOGI("Benchmark collection started");

    sendSuccessResponse(request, [](JsonObject& d) {
        d["message"] = "Benchmark collection started";
        d["active"] = true;
    });
}

void AudioHandlers::handleBenchmarkStop(AsyncWebServerRequest* request,
                                         NodeOrchestrator& orchestrator,
                                         webserver::BenchmarkStreamBroadcaster* broadcaster) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    if (broadcaster) broadcaster->setStreamingActive(false);

    // Return final stats
    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();

    LW_LOGI("Benchmark collection stopped");

    sendSuccessResponse(request, [&stats](JsonObject& d) {
        d["message"] = "Benchmark collection stopped";
        d["active"] = false;

        JsonObject results = d["results"].to<JsonObject>();
        results["avgTotalUs"] = stats.avgTotalUs;
        results["avgGoertzelUs"] = stats.avgGoertzelUs;
        results["cpuLoadPercent"] = stats.cpuLoadPercent;
        results["hopCount"] = stats.hopCount;
        results["peakTotalUs"] = stats.peakTotalUs;
    });
}

void AudioHandlers::handleBenchmarkHistory(AsyncWebServerRequest* request,
                                            NodeOrchestrator& orchestrator) {
    auto* audio = orchestrator.getAudio();
    if (!audio) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::AUDIO_UNAVAILABLE, "Audio system not available");
        return;
    }

    // Get ring buffer reference (read-only peek, no destructive pop)
    const auto& ring = audio->getBenchmarkRing();
    size_t available = ring.available();

    // Limit to last 32 samples to avoid response bloat
    constexpr size_t MAX_HISTORY = 32;
    size_t count = (available < MAX_HISTORY) ? available : MAX_HISTORY;

    // Collect samples via peekLast (non-destructive read, most recent first)
    audio::AudioBenchmarkSample samples[MAX_HISTORY];
    size_t peekedCount = ring.peekLast(samples, count);

    sendSuccessResponse(request, [available, peekedCount, &samples](JsonObject& d) {
        d["available"] = available;
        d["returned"] = peekedCount;

        JsonArray arr = d["samples"].to<JsonArray>();

        for (size_t i = 0; i < peekedCount; i++) {
            JsonObject s = arr.add<JsonObject>();
            s["ts"] = samples[i].timestamp_us;
            s["total"] = samples[i].totalProcessUs;
            s["goertzel"] = samples[i].goertzelUs;
            s["dcAgc"] = samples[i].dcAgcLoopUs;
            s["chroma"] = samples[i].chromaUs;
        }
    });
}
#endif

#else // !FEATURE_AUDIO_SYNC

// Stub implementations when audio sync is disabled
void AudioHandlers::handleParametersGet(AsyncWebServerRequest* request,
                                         NodeOrchestrator&, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleParametersSet(AsyncWebServerRequest* request,
                                         uint8_t*, size_t,
                                         NodeOrchestrator&, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleControl(AsyncWebServerRequest* request,
                                    uint8_t*, size_t, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleStateGet(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleTempoGet(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handlePresetsList(AsyncWebServerRequest* request) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handlePresetGet(AsyncWebServerRequest* request, uint8_t) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handlePresetSave(AsyncWebServerRequest* request,
                                       uint8_t*, size_t, NodeOrchestrator&, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handlePresetApply(AsyncWebServerRequest* request, uint8_t,
                                        NodeOrchestrator&, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handlePresetDelete(AsyncWebServerRequest* request, uint8_t) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsListSources(AsyncWebServerRequest* request) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsListTargets(AsyncWebServerRequest* request) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsListCurves(AsyncWebServerRequest* request) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsList(AsyncWebServerRequest* request, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsGet(AsyncWebServerRequest* request, uint8_t, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsSet(AsyncWebServerRequest* request, uint8_t,
                                        uint8_t*, size_t, RendererNode*) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsDelete(AsyncWebServerRequest* request, uint8_t) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsEnable(AsyncWebServerRequest* request, uint8_t, bool) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleMappingsStats(AsyncWebServerRequest* request) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleZoneAGCGet(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleZoneAGCSet(AsyncWebServerRequest* request,
                                       uint8_t*, size_t, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleSpikeDetectionGet(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleSpikeDetectionReset(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleCalibrateStatus(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleCalibrateStart(AsyncWebServerRequest* request,
                                           uint8_t*, size_t, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleCalibrateCancel(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

void AudioHandlers::handleCalibrateApply(AsyncWebServerRequest* request, NodeOrchestrator&) {
    sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                      ErrorCodes::FEATURE_DISABLED, "Audio sync disabled");
}

#endif // FEATURE_AUDIO_SYNC

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
