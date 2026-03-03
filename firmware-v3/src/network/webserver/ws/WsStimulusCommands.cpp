#include "WsStimulusCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../audio/contracts/ControlBus.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#ifndef NATIVE_BUILD
#include <esp_timer.h>
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using lightwaveos::actors::ActorSystem;
using lightwaveos::actors::RendererActor;
using lightwaveos::audio::ControlBusFrame;

static void handleStimulusMode(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* op = doc["op"] | "";
    int id = doc["id"] | 0;
    const char* modeStr = doc["mode"] | "";

    uint8_t mode = 0;
    if (strcmp(modeStr, "live") == 0 || strcmp(modeStr, "off") == 0) {
        mode = 0;
    } else if (strcmp(modeStr, "trinity") == 0) {
        mode = 1;
    } else if (strcmp(modeStr, "stimulus") == 0) {
        mode = 2;
    } else {
        String response = buildWsResponse("stimulus.mode.ack", nullptr, [id](JsonObject& data) {
            data["success"] = false;
            data["id"] = id;
            data["error"] = "Invalid mode";
        });
        client->text(response);
        return;
    }

    bool ok = ctx.actorSystem.setStimulusMode(mode);
    String response = buildWsResponse("stimulus.mode.ack", nullptr, [mode, ok, id, &ctx](JsonObject& data) {
        const char* m = "live";
        if (mode == 1) m = "trinity";
        else if (mode == 2) m = "stimulus";
        bool rendererAvailable = ctx.renderer != nullptr && ctx.renderer->isRunning();
        data["op"] = "stimulus.mode.ack";
        data["id"] = id;
        data["success"] = ok;
        data["mode"] = m;
        data["usingStimulus"] = (mode == 2) && rendererAvailable;
    });
    client->text(response);
    (void)op;
}

static void handleStimulusPatch(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    int id = doc["id"] | 0;
    JsonVariant patch = doc["patch"];
    if (!patch.is<JsonObject>()) {
        String response = buildWsResponse("stimulus.patch.ack", nullptr, [id](JsonObject& data) {
            data["op"] = "stimulus.patch.ack";
            data["id"] = id;
            data["success"] = false;
            data["error"] = "Missing patch object";
        });
        client->text(response);
        return;
    }

    ControlBusFrame frame = ctx.actorSystem.getStimulusFrame();
    frame.tempoBeatTick = false;

    JsonObject p = patch.as<JsonObject>();
    if (p.containsKey("rms")) frame.rms = p["rms"].as<float>();
    if (p.containsKey("flux")) frame.flux = p["flux"].as<float>();

    if (p.containsKey("bands")) {
        JsonArray arr = p["bands"].as<JsonArray>();
        uint8_t i = 0;
        for (JsonVariant v : arr) {
            if (i >= lightwaveos::audio::CONTROLBUS_NUM_BANDS) break;
            frame.bands[i] = v.as<float>();
            ++i;
        }
    }

    if (p.containsKey("chroma")) {
        JsonArray arr = p["chroma"].as<JsonArray>();
        uint8_t i = 0;
        for (JsonVariant v : arr) {
            if (i >= lightwaveos::audio::CONTROLBUS_NUM_CHROMA) break;
            frame.chroma[i] = v.as<float>();
            ++i;
        }
    }

    if (p.containsKey("tempo_bpm")) frame.tempoBpm = p["tempo_bpm"].as<float>();
    if (p.containsKey("beat_tick")) frame.tempoBeatTick = p["beat_tick"].as<bool>();
    if (p.containsKey("beat_strength")) frame.tempoBeatStrength = p["beat_strength"].as<float>();

    frame.hop_seq += 1;
#ifndef NATIVE_BUILD
    uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
#else
    uint64_t now_us = static_cast<uint64_t>(micros());
#endif
    frame.t.monotonic_us = now_us;

    bool ok = ctx.actorSystem.publishStimulusFrame(frame);
    uint32_t seq = ctx.actorSystem.getStimulusControlBusBuffer().Sequence();

    String response = buildWsResponse("stimulus.patch.ack", nullptr, [ok, id, &frame, seq](JsonObject& data) {
        data["op"] = "stimulus.patch.ack";
        data["id"] = id;
        data["success"] = ok;
        data["hop_seq"] = frame.hop_seq;
        data["sequence"] = seq;
    });
    client->text(response);
}

static void handleStimulusClear(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    int id = doc["id"] | 0;
    bool ok = ctx.actorSystem.clearStimulus();
    String response = buildWsResponse("stimulus.clear.ack", nullptr, [ok, id](JsonObject& data) {
        data["op"] = "stimulus.clear.ack";
        data["id"] = id;
        data["success"] = ok;
    });
    client->text(response);
}

static void handleStimulusStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    int id = doc["id"] | 0;
    uint8_t mode = ctx.actorSystem.getStimulusMode();
    uint32_t seq = ctx.actorSystem.getStimulusControlBusBuffer().Sequence();
    bool rendererAvailable = ctx.renderer != nullptr && ctx.renderer->isRunning();
    bool usingStimulus = (mode == 2) && rendererAvailable;

    String response = buildWsResponse("stimulus.status.rsp", nullptr, [id, mode, seq, usingStimulus](JsonObject& data) {
        const char* m = "live";
        if (mode == 1) m = "trinity";
        else if (mode == 2) m = "stimulus";
        data["op"] = "stimulus.status.rsp";
        data["id"] = id;
        data["success"] = true;
        data["mode"] = m;
        data["usingStimulus"] = usingStimulus;
        data["sequence"] = seq;
    });
    client->text(response);
}

void registerWsStimulusCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("stimulus.mode", handleStimulusMode);
    WsCommandRouter::registerCommand("stimulus.patch", handleStimulusPatch);
    WsCommandRouter::registerCommand("stimulus.clear", handleStimulusClear);
    WsCommandRouter::registerCommand("stimulus.status", handleStimulusStatus);
}

} 
} 
} 
} 

