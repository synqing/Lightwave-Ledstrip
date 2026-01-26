/**
 * @file WsTrinityCommands.cpp
 * @brief WebSocket Trinity command handlers implementation
 */

#include "WsTrinityCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../config/features.h"
#include "../../../utils/Log.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>
#include <math.h>

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsTrinity"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

#if FEATURE_AUDIO_SYNC

static void handleTrinityBeat(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    
    // Extract fields from trinity.beat message
    if (!root.containsKey("bpm") || !root.containsKey("beat_phase")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing required fields: bpm, beat_phase", ""));
        return;
    }
    
    float bpm = root["bpm"].as<float>();
    float beatPhase = root["beat_phase"].as<float>();
    bool tick = root["tick"] | false;
    bool downbeat = root["downbeat"] | false;
    int beatInBar = root["beat_in_bar"] | 0;
    
    // Validate ranges
    if (bpm < 30.0f || bpm > 300.0f) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "BPM out of range (30-300)", ""));
        return;
    }
    
    if (beatPhase < 0.0f || beatPhase >= 1.0f) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "beat_phase must be [0,1)", ""));
        return;
    }
    
    // Route through ActorSystem for thread safety
    bool sent = ctx.actorSystem.trinityBeat(bpm, beatPhase, tick, downbeat, beatInBar);

    if (!sent) {
        LW_LOGW("trinity.beat rejected - queue saturated");
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Queue saturated", ""));
        return;
    }

    // Success - log for debugging
    LW_LOGD("trinity.beat: bpm=%.1f phase=%.3f tick=%d downbeat=%d bar=%d",
            bpm, beatPhase, tick, downbeat, beatInBar);
}

static void handleTrinityMacro(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    
    // Extract macro values (all optional, default to 0)
    float energy = root["energy"] | 0.0f;
    float vocal = root["vocal_presence"] | 0.0f;
    float bass = root["bass_weight"] | 0.0f;
    float perc = root["percussiveness"] | 0.0f;
    float bright = root["brightness"] | 0.0f;
    
    // Clamp to [0,1] range
    energy = fmaxf(0.0f, fminf(1.0f, energy));
    vocal = fmaxf(0.0f, fminf(1.0f, vocal));
    bass = fmaxf(0.0f, fminf(1.0f, bass));
    perc = fmaxf(0.0f, fminf(1.0f, perc));
    bright = fmaxf(0.0f, fminf(1.0f, bright));
    
    // Route through ActorSystem for thread safety
    bool sent = ctx.actorSystem.trinityMacro(energy, vocal, bass, perc, bright);

    if (!sent) {
        LW_LOGW("trinity.macro rejected - queue saturated");
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Queue saturated", ""));
        return;
    }

    // Success - log for debugging (throttled to avoid flood)
    static uint32_t lastMacroLog = 0;
    uint32_t now = millis();
    if (now - lastMacroLog >= 1000) {  // Log once per second max
        LW_LOGD("trinity.macro: energy=%.2f vocal=%.2f bass=%.2f perc=%.2f bright=%.2f",
                energy, vocal, bass, perc, bright);
        lastMacroLog = now;
    }
}

static void handleTrinitySync(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    
    int64_t commandId = 0;
    bool hasCommandId = false;
    if (root.containsKey("_id")) {
        commandId = root["_id"].as<int64_t>();
        if (commandId > 0) {
            hasCommandId = true;
        }
    }
    
    if (!root.containsKey("action") || !root.containsKey("position_sec")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing required fields: action, position_sec", ""));
        return;
    }
    
    const char* actionStr = root["action"].as<const char*>();
    float positionSec = root["position_sec"].as<float>();
    float bpm = root["bpm"] | 120.0f;
    
    // Map action string to enum
    uint8_t action = 255;  // Invalid
    if (strcmp(actionStr, "start") == 0) {
        action = 0;
    } else if (strcmp(actionStr, "stop") == 0) {
        action = 1;
    } else if (strcmp(actionStr, "pause") == 0) {
        action = 2;
    } else if (strcmp(actionStr, "resume") == 0) {
        action = 3;
    } else if (strcmp(actionStr, "seek") == 0) {
        action = 4;
    } else {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Invalid action (must be: start, stop, pause, resume, seek)", ""));
        return;
    }
    
    // Validate position
    if (positionSec < 0.0f) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "position_sec must be >= 0", ""));
        return;
    }
    
    // Log trinity.sync reception (CRITICAL for debugging)
    LW_LOGI("trinity.sync: action=%s pos=%.2fs bpm=%.1f _id=%lld",
            actionStr, positionSec, bpm, commandId);

    // Route through ActorSystem for thread safety
    bool sent = ctx.actorSystem.trinitySync(action, positionSec, bpm);

    if (!sent) {
        LW_LOGW("trinity.sync rejected - queue saturated");
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Queue saturated", ""));
        return;
    }

    LW_LOGI("trinity.sync: action=%s dispatched to RendererActor", actionStr);

    if (hasCommandId) {
        StaticJsonDocument<64> response;
        response["_type"] = "ack";
        response["_id"] = commandId;
        String output;
        serializeJson(response, output);
        client->text(output);
        LW_LOGD("trinity.sync: ACK sent for _id=%lld", commandId);
    }
}

#endif // FEATURE_AUDIO_SYNC

void registerWsTrinityCommands(const WebServerContext& ctx) {
#if FEATURE_AUDIO_SYNC
    WsCommandRouter::registerCommand("trinity.beat", handleTrinityBeat);
    WsCommandRouter::registerCommand("trinity.macro", handleTrinityMacro);
    WsCommandRouter::registerCommand("trinity.sync", handleTrinitySync);
#endif
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
