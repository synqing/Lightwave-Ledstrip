/**
 * @file WsAudioCommands.cpp
 * @brief WebSocket audio command handlers implementation
 */

#include "WsAudioCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../audio/AudioTuning.h"
#include "../../../config/audio_config.h"
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../core/actors/RendererNode.h"
#include "../../../audio/contracts/ControlBus.h"
#include "../../../audio/contracts/AudioTime.h"
#include "../AudioStreamBroadcaster.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#if FEATURE_AUDIO_SYNC

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::audio;
using namespace AudioStreamConfig;

static void handleAudioParametersGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    AudioPipelineTuning pipeline = audio->getPipelineTuning();
    AudioDspState state = audio->getDspState();
    AudioContractTuning contract = ctx.renderer ? ctx.renderer->getAudioContractTuning()
                                                 : clampAudioContractTuning(AudioContractTuning{});
    
    String response = buildWsResponse("audio.parameters", requestId, [&pipeline, &state, &contract](JsonObject& data) {
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
        caps["sampleRate"] = SAMPLE_RATE;
        caps["hopSize"] = HOP_SIZE;
        caps["fftSize"] = FFT_SIZE;
        caps["goertzelWindow"] = GOERTZEL_WINDOW;
        caps["bandCount"] = NUM_BANDS;
        caps["chromaCount"] = CONTROLBUS_NUM_CHROMA;
        caps["waveformPoints"] = CONTROLBUS_WAVEFORM_N;
    });
    client->text(response);
}

static void handleAudioParametersSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Audio system not available", requestId));
        return;
    }
    
    bool updatedPipeline = false;
    bool updatedContract = false;
    bool resetState = false;
    
    AudioPipelineTuning pipeline = audio->getPipelineTuning();
    AudioContractTuning contract = ctx.renderer ? ctx.renderer->getAudioContractTuning()
                                                 : clampAudioContractTuning(AudioContractTuning{});
    
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
    if (updatedContract && ctx.renderer) {
        ctx.renderer->setAudioContractTuning(contract);
    }
    if (resetState) {
        audio->resetDspState();
    }
    
    String response = buildWsResponse("audio.parameters.changed", requestId,
                                      [updatedPipeline, updatedContract, resetState](JsonObject& data) {
        JsonArray updated = data["updated"].to<JsonArray>();
        if (updatedPipeline) updated.add("pipeline");
        if (updatedContract) updated.add("contract");
        if (resetState) updated.add("state");
    });
    client->text(response);
}

static void handleAudioSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.setAudioStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Audio streaming not available", requestId));
        return;
    }
    
    bool ok = ctx.setAudioStreamSubscription(client, true);
    
    if (ok) {
        String response = buildWsResponse("audio.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            data["frameSize"] = AudioStreamConfig::FRAME_SIZE;
            data["streamVersion"] = AudioStreamConfig::STREAM_VERSION;
            data["numBands"] = AudioStreamConfig::NUM_BANDS;
            data["numChroma"] = AudioStreamConfig::NUM_CHROMA;
            data["waveformSize"] = AudioStreamConfig::WAVEFORM_SIZE;
            data["targetFps"] = AudioStreamConfig::TARGET_FPS;
            data["status"] = "ok";
        });
        client->text(response);
    } else {
        JsonDocument response;
        response["type"] = "audio.subscribed";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "Audio subscriber table full or audio system unavailable";
        
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleAudioUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";
    
    if (ctx.setAudioStreamSubscription) {
        ctx.setAudioStreamSubscription(client, false);
    }
    
    String response = buildWsResponse("audio.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
        data["status"] = "ok";
    });
    client->text(response);
}

static void handleAudioZoneAgcGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::AUDIO_UNAVAILABLE, "Audio unavailable", requestId));
        return;
    }
    const ControlBus& controlBus = audio->getControlBusRef();
    String response = buildWsResponse("audio.zone-agc.state", requestId,
        [&controlBus](JsonObject& data) {
            data["enabled"] = controlBus.getZoneAGCEnabled();
            data["lookaheadEnabled"] = controlBus.getLookaheadEnabled();
            JsonArray zones = data["zones"].to<JsonArray>();
            for (uint8_t z = 0; z < CONTROLBUS_NUM_ZONES; ++z) {
                JsonObject zone = zones.add<JsonObject>();
                zone["index"] = z;
                zone["follower"] = controlBus.getZoneFollower(z);
                zone["maxMag"] = controlBus.getZoneMaxMag(z);
            }
        });
    client->text(response);
}

static void handleAudioZoneAgcSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::AUDIO_UNAVAILABLE, "Audio unavailable", requestId));
        return;
    }
    ControlBus& controlBus = audio->getControlBusMut();
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
    String response = buildWsResponse("audio.zone-agc.updated", requestId,
        [updated](JsonObject& data) { data["updated"] = updated; });
    client->text(response);
}

static void handleAudioSpikeDetectionGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::AUDIO_UNAVAILABLE, "Audio unavailable", requestId));
        return;
    }
    const ControlBus& controlBus = audio->getControlBusRef();
    const SpikeDetectionStats& stats = controlBus.getSpikeStats();
    String response = buildWsResponse("audio.spike-detection.state", requestId,
        [&controlBus, &stats](JsonObject& data) {
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
    client->text(response);
}

static void handleAudioSpikeDetectionReset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* audio = ctx.orchestrator.getAudio();
    if (!audio) {
        client->text(buildWsError(ErrorCodes::AUDIO_UNAVAILABLE, "Audio unavailable", requestId));
        return;
    }
    audio->getControlBusMut().resetSpikeStats();
    String response = buildWsResponse("audio.spike-detection.reset", requestId,
        [](JsonObject& data) { data["reset"] = true; });
    client->text(response);
}

void registerWsAudioCommands(const WebServerContext& ctx) {
#if FEATURE_AUDIO_SYNC
    WsCommandRouter::registerCommand("audio.parameters.get", handleAudioParametersGet);
    WsCommandRouter::registerCommand("audio.parameters.set", handleAudioParametersSet);
    WsCommandRouter::registerCommand("audio.subscribe", handleAudioSubscribe);
    WsCommandRouter::registerCommand("audio.unsubscribe", handleAudioUnsubscribe);
    WsCommandRouter::registerCommand("audio.zone-agc.get", handleAudioZoneAgcGet);
    WsCommandRouter::registerCommand("audio.zone-agc.set", handleAudioZoneAgcSet);
    WsCommandRouter::registerCommand("audio.spike-detection.get", handleAudioSpikeDetectionGet);
    WsCommandRouter::registerCommand("audio.spike-detection.reset", handleAudioSpikeDetectionReset);
#endif
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC

