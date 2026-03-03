#include "StimulusHandlers.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../audio/contracts/ControlBus.h"
#ifndef NATIVE_BUILD
#include <esp_timer.h>
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

static bool parseJson(AsyncWebServerRequest* request, uint8_t* data, size_t len, JsonDocument& doc) {
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Invalid JSON body");
        return false;
    }
    return true;
}

void StimulusHandlers::handleMode(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len,
                                  lightwaveos::actors::ActorSystem& actorSystem,
                                  lightwaveos::actors::RendererActor* renderer) {
    (void)renderer;
    JsonDocument doc;
    if (!parseJson(request, data, len, doc)) {
        return;
    }
    const char* modeStr = doc["mode"] | "";
    uint8_t mode = 0;
    if (strcmp(modeStr, "live") == 0 || strcmp(modeStr, "off") == 0) {
        mode = 0;
    } else if (strcmp(modeStr, "trinity") == 0) {
        mode = 1;
    } else if (strcmp(modeStr, "stimulus") == 0) {
        mode = 2;
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_VALUE, "Invalid mode");
        return;
    }
    if (!actorSystem.setStimulusMode(mode)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR, ErrorCodes::OPERATION_FAILED, "Failed to set stimulus mode");
        return;
    }
    sendSuccessResponse(request, [mode](JsonObject& dataObj) {
        const char* modeName = "live";
        if (mode == 1) modeName = "trinity";
        else if (mode == 2) modeName = "stimulus";
        dataObj["mode"] = modeName;
    });
}

void StimulusHandlers::handlePatch(AsyncWebServerRequest* request,
                                   uint8_t* data, size_t len,
                                   lightwaveos::actors::ActorSystem& actorSystem,
                                   lightwaveos::actors::RendererActor* renderer) {
    (void)renderer;
    JsonDocument doc;
    if (!parseJson(request, data, len, doc)) {
        return;
    }
    lightwaveos::audio::ControlBusFrame frame = actorSystem.getStimulusFrame();
    frame.tempoBeatTick = false;
    if (doc.containsKey("rms")) frame.rms = doc["rms"].as<float>();
    if (doc.containsKey("flux")) frame.flux = doc["flux"].as<float>();
    if (doc.containsKey("bands")) {
        JsonArray arr = doc["bands"].as<JsonArray>();
        uint8_t i = 0;
        for (JsonVariant v : arr) {
            if (i >= lightwaveos::audio::CONTROLBUS_NUM_BANDS) break;
            frame.bands[i] = v.as<float>();
            ++i;
        }
    }
    if (doc.containsKey("chroma")) {
        JsonArray arr = doc["chroma"].as<JsonArray>();
        uint8_t i = 0;
        for (JsonVariant v : arr) {
            if (i >= lightwaveos::audio::CONTROLBUS_NUM_CHROMA) break;
            frame.chroma[i] = v.as<float>();
            ++i;
        }
    }
    if (doc.containsKey("tempo_bpm")) frame.tempoBpm = doc["tempo_bpm"].as<float>();
    if (doc.containsKey("beat_tick")) frame.tempoBeatTick = doc["beat_tick"].as<bool>();
    if (doc.containsKey("beat_strength")) frame.tempoBeatStrength = doc["beat_strength"].as<float>();
    frame.hop_seq += 1;
#ifndef NATIVE_BUILD
    uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
#else
    uint64_t now_us = static_cast<uint64_t>(micros());
#endif
    frame.t.monotonic_us = now_us;
    if (!actorSystem.publishStimulusFrame(frame)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR, ErrorCodes::OPERATION_FAILED, "Failed to publish stimulus frame");
        return;
    }
    sendSuccessResponse(request, [&frame](JsonObject& dataObj) {
        dataObj["hop_seq"] = frame.hop_seq;
    });
}

void StimulusHandlers::handleClear(AsyncWebServerRequest* request,
                                   lightwaveos::actors::ActorSystem& actorSystem,
                                   lightwaveos::actors::RendererActor* renderer) {
    (void)renderer;
    if (!actorSystem.clearStimulus()) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR, ErrorCodes::OPERATION_FAILED, "Failed to clear stimulus");
        return;
    }
    sendSuccessResponse(request);
}

void StimulusHandlers::handleStatus(AsyncWebServerRequest* request,
                                    lightwaveos::actors::ActorSystem& actorSystem,
                                    lightwaveos::actors::RendererActor* renderer) {
    uint8_t mode = actorSystem.getStimulusMode();
    uint32_t seq = actorSystem.getStimulusControlBusBuffer().Sequence();
    bool usingStimulus = (mode == 2);
    bool rendererAvailable = renderer != nullptr && renderer->isRunning();
    sendSuccessResponse(request, [mode, seq, usingStimulus, rendererAvailable](JsonObject& dataObj) {
        const char* modeName = "live";
        if (mode == 1) modeName = "trinity";
        else if (mode == 2) modeName = "stimulus";
        dataObj["mode"] = modeName;
        dataObj["sequence"] = seq;
        dataObj["usingStimulus"] = usingStimulus && rendererAvailable;
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
