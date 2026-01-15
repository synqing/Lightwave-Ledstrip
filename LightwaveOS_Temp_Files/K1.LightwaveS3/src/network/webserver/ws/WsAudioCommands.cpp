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
#include "../../../audio/contracts/AudioEffectMapping.h"
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

// ============================================================================
// FFT WebSocket Streaming (Feature C)
// ============================================================================

// FFT subscriber tracking (max 4 clients) - matches WsStreamCommands pattern
static AsyncWebSocketClient* s_fftSubscribers[4] = {nullptr, nullptr, nullptr, nullptr};
static constexpr size_t MAX_FFT_SUBSCRIBERS = 4;
static uint32_t s_lastFftBroadcast = 0;
static constexpr uint32_t FFT_BROADCAST_INTERVAL_MS = 32;  // ~31 Hz

static void handleFftSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    bool subscribed = false;
    for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
        if (s_fftSubscribers[i] == nullptr ||
            s_fftSubscribers[i]->status() != WS_CONNECTED) {
            s_fftSubscribers[i] = client;
            subscribed = true;
            break;
        }
        if (s_fftSubscribers[i] == client) {
            subscribed = true;
            break;
        }
    }

    if (subscribed) {
        String response = buildWsResponse("audio.fft.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            data["fftBins"] = 64;
            data["updateRateHz"] = 31;
            data["accepted"] = true;
        });
        client->text(response);
    } else {
        JsonDocument response;
        response["type"] = "audio.fft.rejected";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "FFT subscriber table full (max 4 concurrent clients)";

        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleFftUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
        if (s_fftSubscribers[i] == client) {
            s_fftSubscribers[i] = nullptr;
            break;
        }
    }

    String response = buildWsResponse("audio.fft.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}

// Helper functions for subscription management
bool setFftStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (subscribe) {
        // Subscribe: find empty slot or reuse existing
        for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
            if (s_fftSubscribers[i] == nullptr ||
                s_fftSubscribers[i]->status() != WS_CONNECTED) {
                s_fftSubscribers[i] = client;
                return true;
            }
            if (s_fftSubscribers[i] == client) {
                return true;  // Already subscribed
            }
        }
        return false;  // Table full
    } else {
        // Unsubscribe: remove from table
        for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
            if (s_fftSubscribers[i] == client) {
                s_fftSubscribers[i] = nullptr;
                return true;
            }
        }
        return false;  // Not found
    }
}

bool hasFftStreamSubscribers() {
    for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
        if (s_fftSubscribers[i] != nullptr && s_fftSubscribers[i]->status() == WS_CONNECTED) {
            return true;
        }
    }
    return false;
}

void broadcastFftFrame(const audio::ControlBusFrame& frame, AsyncWebSocket* ws) {
    if (!ws || !hasFftStreamSubscribers()) {
        return;
    }

    // Throttle to 31 Hz (~32ms intervals)
    uint32_t now = millis();
    if (now - s_lastFftBroadcast < FFT_BROADCAST_INTERVAL_MS) {
        return;
    }
    s_lastFftBroadcast = now;

    // Clean up disconnected clients
    for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
        if (s_fftSubscribers[i] != nullptr && s_fftSubscribers[i]->status() != WS_CONNECTED) {
            s_fftSubscribers[i] = nullptr;
        }
    }

    // Build JSON frame with FFT bins
    JsonDocument doc;
    doc["type"] = "audio.fft.frame";
    doc["hopSeq"] = frame.hop_seq;

    // Add 64 FFT bins
    JsonArray binsArray = doc["bins"].to<JsonArray>();
    for (uint8_t i = 0; i < 64; i++) {
        binsArray.add(frame.bins64[i]);
    }

    // Serialize to string
    String output;
    serializeJson(doc, output);

    // Send to all active subscribers
    for (size_t i = 0; i < MAX_FFT_SUBSCRIBERS; ++i) {
        if (s_fftSubscribers[i] != nullptr && s_fftSubscribers[i]->status() == WS_CONNECTED) {
            s_fftSubscribers[i]->text(output);
        }
    }
}

// ============================================================================
// Audio-Effect Mapping WebSocket Commands
// ============================================================================

static void handleAudioMappingListSources(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    String response = buildWsResponse("audio.mapping.sources", requestId, [](JsonObject& data) {
        JsonArray sources = data["sources"].to<JsonArray>();
        
        // Energy metrics
        sources.add("RMS");
        sources.add("FAST_RMS");
        sources.add("FLUX");
        sources.add("FAST_FLUX");
        
        // Individual frequency bands
        for (uint8_t i = 0; i < 8; i++) {
            char bandName[16];
            snprintf(bandName, sizeof(bandName), "BAND_%d", i);
            sources.add(bandName);
        }
        
        // Aggregated bands
        sources.add("BASS");
        sources.add("MID");
        sources.add("TREBLE");
        sources.add("HEAVY_BASS");
        
        // Musical timing
        sources.add("BEAT_PHASE");
        sources.add("BPM");
        sources.add("TEMPO_CONFIDENCE");
    });
    client->text(response);
}

static void handleAudioMappingListTargets(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    String response = buildWsResponse("audio.mapping.targets", requestId, [](JsonObject& data) {
        JsonArray targets = data["targets"].to<JsonArray>();
        targets.add("BRIGHTNESS");
        targets.add("SPEED");
        targets.add("INTENSITY");
        targets.add("SATURATION");
        targets.add("COMPLEXITY");
        targets.add("VARIATION");
        targets.add("HUE");
    });
    client->text(response);
}

static void handleAudioMappingListCurves(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    String response = buildWsResponse("audio.mapping.curves", requestId, [](JsonObject& data) {
        JsonArray curves = data["curves"].to<JsonArray>();
        curves.add("LINEAR");
        curves.add("SQUARED");
        curves.add("SQRT");
        curves.add("LOG");
        curves.add("EXP");
        curves.add("INVERTED");
    });
    client->text(response);
}

static void handleAudioMappingList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }
    
    auto& registry = AudioMappingRegistry::instance();
    uint8_t effectCount = ctx.renderer->getEffectCount();
    
    String response = buildWsResponse("audio.mapping.list", requestId, [&registry, effectCount, &ctx](JsonObject& data) {
        data["activeEffects"] = registry.getActiveEffectCount();
        data["totalMappings"] = registry.getTotalMappingCount();
        
        JsonArray effects = data["effects"].to<JsonArray>();
        
        for (uint8_t i = 0; i < effectCount && i < AudioMappingRegistry::MAX_EFFECTS; i++) {
            const EffectAudioMapping* mapping = registry.getMapping(i);
            if (mapping && mapping->globalEnabled && mapping->mappingCount > 0) {
                JsonObject e = effects.add<JsonObject>();
                e["id"] = i;
                e["name"] = ctx.renderer->getEffectName(i);
                e["mappingCount"] = mapping->mappingCount;
                e["enabled"] = mapping->globalEnabled;
            }
        }
    });
    client->text(response);
}

static void handleAudioMappingGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }
    
    uint8_t effectId = doc["effectId"] | 255;
    
    if (effectId >= AudioMappingRegistry::MAX_EFFECTS ||
        effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", requestId));
        return;
    }
    
    auto& registry = AudioMappingRegistry::instance();
    const EffectAudioMapping* config = registry.getMapping(effectId);
    
    if (!config) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to get mapping", requestId));
        return;
    }
    
    String response = buildWsResponse("audio.mapping.get", requestId, [effectId, &ctx, config](JsonObject& data) {
        data["effectId"] = effectId;
        data["effectName"] = ctx.renderer->getEffectName(effectId);
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
    });
    client->text(response);
}

static void handleAudioMappingSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }
    
    uint8_t effectId = doc["effectId"] | 255;
    
    if (effectId >= AudioMappingRegistry::MAX_EFFECTS ||
        effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", requestId));
        return;
    }
    
    auto& registry = AudioMappingRegistry::instance();
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
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to set mapping", requestId));
        return;
    }
    
    String response = buildWsResponse("audio.mapping.set", requestId, [effectId, &newConfig](JsonObject& data) {
        data["effectId"] = effectId;
        data["mappingCount"] = newConfig.mappingCount;
        data["enabled"] = newConfig.globalEnabled;
        data["message"] = "Mapping updated";
    });
    client->text(response);
}

static void handleAudioMappingDelete(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    uint8_t effectId = doc["effectId"] | 255;
    
    if (effectId >= AudioMappingRegistry::MAX_EFFECTS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", requestId));
        return;
    }
    
    auto& registry = AudioMappingRegistry::instance();
    EffectAudioMapping* config = registry.getMapping(effectId);
    
    if (config) {
        config->clearMappings();
        config->globalEnabled = false;
    }
    
    String response = buildWsResponse("audio.mapping.delete", requestId, [effectId](JsonObject& data) {
        data["effectId"] = effectId;
        data["message"] = "Mapping cleared";
    });
    client->text(response);
}

static void handleAudioMappingEnable(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    uint8_t effectId = doc["effectId"] | 255;
    bool enable = doc["enable"] | true;
    
    if (effectId >= AudioMappingRegistry::MAX_EFFECTS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", requestId));
        return;
    }
    
    auto& registry = AudioMappingRegistry::instance();
    registry.setEffectMappingEnabled(effectId, enable);
    
    String response = buildWsResponse("audio.mapping.enable", requestId, [effectId, enable](JsonObject& data) {
        data["effectId"] = effectId;
        data["enabled"] = enable;
    });
    client->text(response);
}

static void handleAudioMappingStats(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    using namespace lightwaveos::audio;
    
    auto& registry = AudioMappingRegistry::instance();
    
    String response = buildWsResponse("audio.mapping.stats", requestId, [&registry](JsonObject& data) {
        data["applyCount"] = registry.getApplyCount();
        data["lastApplyMicros"] = registry.getLastApplyMicros();
        data["maxApplyMicros"] = registry.getMaxApplyMicros();
        data["activeEffectsWithMappings"] = registry.getActiveEffectCount();
        data["totalMappingsConfigured"] = registry.getTotalMappingCount();
    });
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
    WsCommandRouter::registerCommand("audio.fft.subscribe", handleFftSubscribe);
    WsCommandRouter::registerCommand("audio.fft.unsubscribe", handleFftUnsubscribe);
    
    // Audio-Effect Mapping commands
    WsCommandRouter::registerCommand("audio.mapping.sources", handleAudioMappingListSources);
    WsCommandRouter::registerCommand("audio.mapping.targets", handleAudioMappingListTargets);
    WsCommandRouter::registerCommand("audio.mapping.curves", handleAudioMappingListCurves);
    WsCommandRouter::registerCommand("audio.mapping.list", handleAudioMappingList);
    WsCommandRouter::registerCommand("audio.mapping.get", handleAudioMappingGet);
    WsCommandRouter::registerCommand("audio.mapping.set", handleAudioMappingSet);
    WsCommandRouter::registerCommand("audio.mapping.delete", handleAudioMappingDelete);
    WsCommandRouter::registerCommand("audio.mapping.enable", handleAudioMappingEnable);
    WsCommandRouter::registerCommand("audio.mapping.stats", handleAudioMappingStats);
#endif
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC

