// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsDebugCommands.cpp
 * @brief WebSocket debug command handlers implementation
 */

#include "WsDebugCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../UdpStreamer.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "../../../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "../../../audio/AudioDebugConfig.h"
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

#if FEATURE_AUDIO_SYNC
using namespace lightwaveos::audio;
#endif

#if FEATURE_AUDIO_SYNC
static void handleDebugAudioGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";
    AudioDebugConfig& config = getAudioDebugConfig();

    String response = buildWsResponse("debug.audio.state", requestId,
        [&config](JsonObject& data) {
            data["verbosity"] = config.verbosity;
            data["baseInterval"] = config.baseInterval;

            JsonObject intervals = data["intervals"].to<JsonObject>();
            intervals["8band"] = config.interval8Band();
            intervals["64bin"] = config.interval64Bin();
            intervals["dma"] = config.intervalDMA();

            JsonArray levels = data["levels"].to<JsonArray>();
            levels.add("Off - No debug output");
            levels.add("Minimal - Errors only");
            levels.add("Status - 10s health reports");
            levels.add("Low - + DMA diagnostics (~5s)");
            levels.add("Medium - + 64-bin Goertzel (~2s)");
            levels.add("High - + 8-band Goertzel (~1s)");
        });
    client->text(response);
}
#endif

#if FEATURE_AUDIO_SYNC
static void handleDebugAudioSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";

    if (!doc.containsKey("verbosity") && !doc.containsKey("baseInterval")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
            "At least one of 'verbosity' or 'baseInterval' required", requestId));
        return;
    }

    AudioDebugConfig& config = getAudioDebugConfig();

    if (doc.containsKey("verbosity")) {
        int verbosity = doc["verbosity"].as<int>();
        if (verbosity < 0 || verbosity > 5) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                "verbosity must be 0-5", requestId));
            return;
        }
        config.verbosity = static_cast<uint8_t>(verbosity);
    }

    if (doc.containsKey("baseInterval")) {
        int interval = doc["baseInterval"].as<int>();
        if (interval < 1 || interval > 1000) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                "baseInterval must be 1-1000", requestId));
            return;
        }
        config.baseInterval = static_cast<uint16_t>(interval);
    }

    String response = buildWsResponse("debug.audio.updated", requestId,
        [&config](JsonObject& data) {
            data["verbosity"] = config.verbosity;
            data["baseInterval"] = config.baseInterval;

            JsonObject intervals = data["intervals"].to<JsonObject>();
            intervals["8band"] = config.interval8Band();
            intervals["64bin"] = config.interval64Bin();
            intervals["dma"] = config.intervalDMA();
        });
    client->text(response);
}
#endif

static void handleDebugUdpGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    if (!ctx.udpStreamer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "UDP streamer unavailable", requestId));
        return;
    }

    webserver::UdpStreamer::UdpStats stats;
    ctx.udpStreamer->getStats(stats);
    uint32_t nowMs = millis();

    String response = buildWsResponse("debug.udp.stats", requestId,
        [stats, nowMs](JsonObject& data) {
            data["started"] = stats.started;
            data["subscribers"] = stats.subscriberCount;
            data["suppressed"] = stats.suppressedCount;
            data["consecutiveFailures"] = stats.consecutiveFailures;
            data["lastFailureMs"] = stats.lastFailureMs;
            data["lastFailureAgoMs"] = stats.lastFailureMs > 0 ? (nowMs - stats.lastFailureMs) : 0;
            data["cooldownRemainingMs"] = stats.cooldownUntilMs > nowMs ? (stats.cooldownUntilMs - nowMs) : 0;
            data["socketResets"] = stats.socketResets;
            data["lastSocketResetMs"] = stats.lastSocketResetMs;

            JsonObject led = data["led"].to<JsonObject>();
            led["attempts"] = stats.ledAttempts;
            led["success"] = stats.ledSuccess;
            led["failures"] = stats.ledFailures;

            JsonObject audio = data["audio"].to<JsonObject>();
            audio["attempts"] = stats.audioAttempts;
            audio["success"] = stats.audioSuccess;
            audio["failures"] = stats.audioFailures;
        });
    client->text(response);
}

void registerWsDebugCommands(const WebServerContext& ctx) {
#if FEATURE_AUDIO_SYNC
    WsCommandRouter::registerCommand("debug.audio.get", handleDebugAudioGet);
    WsCommandRouter::registerCommand("debug.audio.set", handleDebugAudioSet);
#endif
    WsCommandRouter::registerCommand("debug.udp.get", handleDebugUdpGet);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
