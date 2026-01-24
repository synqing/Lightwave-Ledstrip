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
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>
#include <math.h>

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
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Queue saturated", ""));
        return;
    }
    
    // Success - no response needed (fire-and-forget)
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
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Queue saturated", ""));
        return;
    }
    
    // Success - no response needed (fire-and-forget)
}

static void handleTrinitySync(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    
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
    
    // Route through ActorSystem for thread safety
    bool sent = ctx.actorSystem.trinitySync(action, positionSec, bpm);
    
    if (!sent) {
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Queue saturated", ""));
        return;
    }
    
    // Success - no response needed (fire-and-forget)
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
