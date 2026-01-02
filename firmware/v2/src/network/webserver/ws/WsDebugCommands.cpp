/**
 * @file WsDebugCommands.cpp
 * @brief WebSocket debug command handlers implementation
 */

#include "WsDebugCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "../../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "../../../audio/AudioDebugConfig.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::audio;

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

void registerWsDebugCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("debug.audio.get", handleDebugAudioGet);
    WsCommandRouter::registerCommand("debug.audio.set", handleDebugAudioSet);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#else

// Empty implementation when FEATURE_AUDIO_SYNC is disabled
namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

void registerWsDebugCommands(const WebServerContext& ctx) {
    (void)ctx;
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
