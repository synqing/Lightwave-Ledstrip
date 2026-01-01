/**
 * @file DebugHandlers.cpp
 * @brief Debug handlers implementation
 */

#include "DebugHandlers.h"
#include "../../ApiResponse.h"
#include <ArduinoJson.h>

#if FEATURE_AUDIO_SYNC
#include "../../../audio/AudioDebugConfig.h"
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

#if FEATURE_AUDIO_SYNC

void DebugHandlers::handleAudioDebugGet(AsyncWebServerRequest* request) {
    auto& cfg = audio::getAudioDebugConfig();

    sendSuccessResponse(request, [&cfg](JsonObject& data) {
        data["verbosity"] = cfg.verbosity;
        data["baseInterval"] = cfg.baseInterval;

        // Include derived intervals for client reference
        JsonObject intervals = data["intervals"].to<JsonObject>();
        intervals["8band"] = cfg.interval8Band();
        intervals["64bin"] = cfg.interval64Bin();
        intervals["dma"] = cfg.intervalDMA();

        // Include level descriptions for UI
        JsonArray levels = data["levels"].to<JsonArray>();
        levels.add("Off - No debug output");
        levels.add("Minimal - Errors only");
        levels.add("Status - 10s health reports");
        levels.add("Low - + DMA diagnostics (~5s)");
        levels.add("Medium - + 64-bin Goertzel (~2s)");
        levels.add("High - + 8-band Goertzel (~1s)");
    });
}

void DebugHandlers::handleAudioDebugSet(AsyncWebServerRequest* request,
                                         uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    auto& cfg = audio::getAudioDebugConfig();
    bool updated = false;

    // Validate and update verbosity
    if (doc.containsKey("verbosity")) {
        int verbosity = doc["verbosity"].as<int>();
        if (verbosity < 0 || verbosity > 5) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "verbosity must be 0-5", "verbosity");
            return;
        }
        cfg.verbosity = static_cast<uint8_t>(verbosity);
        updated = true;
    }

    // Validate and update baseInterval
    if (doc.containsKey("baseInterval")) {
        int interval = doc["baseInterval"].as<int>();
        if (interval < 1 || interval > 1000) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "baseInterval must be 1-1000", "baseInterval");
            return;
        }
        cfg.baseInterval = static_cast<uint16_t>(interval);
        updated = true;
    }

    if (!updated) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "At least one of 'verbosity' or 'baseInterval' required");
        return;
    }

    // Return updated config
    sendSuccessResponse(request, [&cfg](JsonObject& data) {
        data["verbosity"] = cfg.verbosity;
        data["baseInterval"] = cfg.baseInterval;

        JsonObject intervals = data["intervals"].to<JsonObject>();
        intervals["8band"] = cfg.interval8Band();
        intervals["64bin"] = cfg.interval64Bin();
        intervals["dma"] = cfg.intervalDMA();
    });
}

#endif // FEATURE_AUDIO_SYNC

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
