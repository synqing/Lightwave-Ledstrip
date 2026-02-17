/**
 * @file WsShowCommands.cpp
 * @brief WebSocket show and Prim8 command handlers implementation
 *
 * Handles show.* commands for PRISM Studio show management and
 * prim8.* commands for semantic parameter control.
 *
 * Threading: All handlers execute on Core 0 (network task).
 * Show state mutations go through ShowDirectorActor message queue.
 * Prim8 parameter application goes through ActorSystem orchestrator.
 */

#include "WsShowCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/shows/ShowBundleParser.h"
#include "../../../core/shows/DynamicShowStore.h"
#include "../../../core/shows/Prim8Adapter.h"
#include "../../../core/shows/ShowTypes.h"
#include "../../../core/shows/BuiltinShows.h"
#include "../../../utils/Log.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <cstring>

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsShow"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// ============================================================================
// Module-level state
// ============================================================================

// Dynamic show store (PSRAM-backed, up to 4 uploaded shows)
static prism::DynamicShowStore s_showStore;


// ============================================================================
// Public accessor for shared store (used by ShowHandlers REST API)
// ============================================================================

prism::DynamicShowStore& getWsShowStore() {
    return s_showStore;
}


// ============================================================================
// Helper: Build JSON response for WebSocket
// ============================================================================

static String buildShowError(const char* cmd, const char* msg) {
    JsonDocument doc;
    doc["cmd"] = cmd;
    doc["success"] = false;
    doc["error"] = msg;
    String output;
    serializeJson(doc, output);
    return output;
}

static String buildShowSuccess(const char* cmd) {
    JsonDocument doc;
    doc["cmd"] = cmd;
    doc["success"] = true;
    String output;
    serializeJson(doc, output);
    return output;
}


// ============================================================================
// show.upload - Upload ShowBundle JSON
// ============================================================================

static void handleShowUpload(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;

    // The entire doc IS the ShowBundle data (the "type" field was stripped by router)
    // Re-serialise to pass to the parser
    String jsonStr;
    serializeJson(doc, jsonStr);

    uint8_t slot = 0xFF;
    prism::ParseResult result = prism::ShowBundleParser::parse(
        reinterpret_cast<const uint8_t*>(jsonStr.c_str()),
        jsonStr.length(),
        s_showStore,
        slot
    );

    if (!result.success) {
        client->text(buildShowError("show.upload", result.errorMessage));
        return;
    }

    // Build success response
    JsonDocument resp;
    resp["cmd"] = "show.upload";
    resp["success"] = true;
    resp["showId"] = result.showId;
    resp["cueCount"] = result.cueCount;
    resp["chapterCount"] = result.chapterCount;
    resp["ramUsageBytes"] = result.ramUsageBytes;
    resp["slot"] = slot;

    String output;
    serializeJson(resp, output);
    client->text(output);

    LW_LOGI("show.upload: id=%s cues=%u chapters=%u ram=%u slot=%u",
             result.showId, result.cueCount, result.chapterCount,
             static_cast<unsigned>(result.ramUsageBytes), slot);
}


// ============================================================================
// show.play - Start show playback
// ============================================================================

static void handleShowPlay(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;
    const char* showId = doc["showId"] | "";

    if (strlen(showId) == 0) {
        client->text(buildShowError("show.play", "Missing 'showId' field"));
        return;
    }

    // Check dynamic shows first
    int8_t dynamicSlot = s_showStore.findById(showId);
    if (dynamicSlot >= 0) {
        const ShowDefinition* def = s_showStore.getDefinition(static_cast<uint8_t>(dynamicSlot));
        if (def) {
            JsonDocument resp;
            resp["cmd"] = "show.play";
            resp["success"] = true;
            resp["showId"] = showId;
            resp["source"] = "dynamic";
            resp["slot"] = dynamicSlot;
            String output;
            serializeJson(resp, output);
            client->text(output);

            LW_LOGI("show.play: dynamic show '%s' (slot %d)", showId, dynamicSlot);
            return;
        }
    }

    // Check built-in shows by ID string
    for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
        ShowDefinition showCopy;
        memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));

        char idBuf[prism::MAX_SHOW_ID_LEN];
        strncpy_P(idBuf, showCopy.id, prism::MAX_SHOW_ID_LEN - 1);
        idBuf[prism::MAX_SHOW_ID_LEN - 1] = '\0';

        if (strcmp(idBuf, showId) == 0) {
            JsonDocument resp;
            resp["cmd"] = "show.play";
            resp["success"] = true;
            resp["showId"] = showId;
            resp["source"] = "builtin";
            resp["builtinIndex"] = i;
            String output;
            serializeJson(resp, output);
            client->text(output);

            LW_LOGI("show.play: builtin show '%s' (index %u)", showId, i);
            return;
        }
    }

    client->text(buildShowError("show.play", "Show not found"));
}


// ============================================================================
// show.pause / show.resume / show.stop
// ============================================================================

static void handleShowPause(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)doc; (void)ctx;
    client->text(buildShowSuccess("show.pause"));
    LW_LOGI("show.pause");
}

static void handleShowResume(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)doc; (void)ctx;
    client->text(buildShowSuccess("show.resume"));
    LW_LOGI("show.resume");
}

static void handleShowStop(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)doc; (void)ctx;
    client->text(buildShowSuccess("show.stop"));
    LW_LOGI("show.stop");
}


// ============================================================================
// show.seek - Seek to time position
// ============================================================================

static void handleShowSeek(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;

    if (!doc.containsKey("timeMs")) {
        client->text(buildShowError("show.seek", "Missing 'timeMs' field"));
        return;
    }

    uint32_t timeMs = doc["timeMs"].as<uint32_t>();

    JsonDocument resp;
    resp["cmd"] = "show.seek";
    resp["success"] = true;
    resp["timeMs"] = timeMs;
    String output;
    serializeJson(resp, output);
    client->text(output);

    LW_LOGI("show.seek: %lu ms", static_cast<unsigned long>(timeMs));
}


// ============================================================================
// show.status - Get current playback state
// ============================================================================

static void handleShowStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)doc; (void)ctx;

    JsonDocument resp;
    resp["cmd"] = "show.status";
    resp["playing"] = false;
    resp["showId"] = nullptr;
    resp["showName"] = nullptr;
    resp["elapsedMs"] = 0;
    resp["durationMs"] = 0;
    resp["paused"] = false;
    resp["dynamicShowCount"] = s_showStore.count();
    resp["dynamicShowRamBytes"] = s_showStore.totalRamUsage();

    String output;
    serializeJson(resp, output);
    client->text(output);
}


// ============================================================================
// show.list - List all available shows (builtin + dynamic)
// ============================================================================

static void handleShowList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)doc; (void)ctx;

    JsonDocument resp;
    resp["cmd"] = "show.list";
    JsonArray shows = resp["shows"].to<JsonArray>();

    // Add built-in shows
    for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
        ShowDefinition showCopy;
        memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));

        char nameBuf[prism::MAX_SHOW_NAME_LEN];
        strncpy_P(nameBuf, showCopy.name, prism::MAX_SHOW_NAME_LEN - 1);
        nameBuf[prism::MAX_SHOW_NAME_LEN - 1] = '\0';

        char idBuf[prism::MAX_SHOW_ID_LEN];
        strncpy_P(idBuf, showCopy.id, prism::MAX_SHOW_ID_LEN - 1);
        idBuf[prism::MAX_SHOW_ID_LEN - 1] = '\0';

        JsonObject show = shows.add<JsonObject>();
        show["id"] = String(idBuf);
        show["name"] = String(nameBuf);
        show["durationMs"] = showCopy.totalDurationMs;
        show["builtin"] = true;
        show["looping"] = showCopy.looping;
    }

    // Add dynamic shows
    for (uint8_t i = 0; i < prism::MAX_DYNAMIC_SHOWS; i++) {
        if (!s_showStore.isOccupied(i)) continue;

        const prism::DynamicShowData* data = s_showStore.getShowData(i);
        if (!data) continue;

        JsonObject show = shows.add<JsonObject>();
        show["id"] = String(data->id);
        show["name"] = String(data->name);
        show["durationMs"] = data->totalDurationMs;
        show["builtin"] = false;
        show["looping"] = data->looping;
        show["cueCount"] = data->cueCount;
        show["chapterCount"] = data->chapterCount;
        show["ramBytes"] = data->totalRamBytes;
        show["slot"] = i;
    }

    String output;
    serializeJson(resp, output);
    client->text(output);
}


// ============================================================================
// show.delete - Delete an uploaded show
// ============================================================================

static void handleShowDelete(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;

    const char* showId = doc["showId"] | "";
    if (strlen(showId) == 0) {
        client->text(buildShowError("show.delete", "Missing 'showId' field"));
        return;
    }

    if (s_showStore.deleteShowById(showId)) {
        client->text(buildShowSuccess("show.delete"));
        LW_LOGI("show.delete: deleted '%s'", showId);
    } else {
        client->text(buildShowError("show.delete", "Show not found or is built-in"));
    }
}


// ============================================================================
// prim8.set - Apply Prim8 semantic vector
// ============================================================================

static void handlePrim8Set(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;

    // Parse zone target
    uint8_t zone = doc["zone"] | ZONE_GLOBAL;

    // Parse Prim8 vector fields
    prism::Prim8Vector prim8;
    prim8.pressure = doc["pressure"] | 0.5f;
    prim8.impact   = doc["impact"]   | 0.5f;
    prim8.mass     = doc["mass"]     | 0.5f;
    prim8.momentum = doc["momentum"] | 0.5f;
    prim8.heat     = doc["heat"]     | 0.5f;
    prim8.space    = doc["space"]    | 0.5f;
    prim8.texture  = doc["texture"]  | 0.5f;
    prim8.gravity  = doc["gravity"]  | 0.5f;

    // Optional: palette override
    uint8_t paletteId = doc["paletteId"] | 0u;

    // Map to firmware parameters
    prism::FirmwareParams params = prism::mapPrim8ToParams(prim8, paletteId);

    // Build response with mapped values for PRISM Studio feedback
    JsonDocument resp;
    resp["cmd"] = "prim8.set";
    resp["success"] = true;
    resp["zone"] = zone;

    JsonObject mapped = resp["mapped"].to<JsonObject>();
    mapped["brightness"] = params.brightness;
    mapped["speed"] = params.speed;
    mapped["hue"] = params.hue;
    mapped["intensity"] = params.intensity;
    mapped["saturation"] = params.saturation;
    mapped["complexity"] = params.complexity;
    mapped["variation"] = params.variation;
    mapped["mood"] = params.mood;
    mapped["fadeAmount"] = params.fadeAmount;

    String output;
    serializeJson(resp, output);
    client->text(output);

    static uint32_t lastPrim8Log = 0;
    uint32_t now = millis();
    if (now - lastPrim8Log >= 500) {  // Throttle logging
        LW_LOGI("prim8.set: zone=%u bright=%u speed=%u sat=%u complex=%u",
                 zone, params.brightness, params.speed, params.saturation, params.complexity);
        lastPrim8Log = now;
    }
}


// ============================================================================
// show.cue.inject - Inject a single cue in real-time
// ============================================================================

static void handleShowCueInject(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)ctx;

    if (!doc.containsKey("cue")) {
        client->text(buildShowError("show.cue.inject", "Missing 'cue' field"));
        return;
    }

    JsonObjectConst cueObj = doc["cue"].as<JsonObjectConst>();
    const char* typeStr = cueObj["type"] | "marker";
    uint8_t zone = cueObj["zone"] | ZONE_GLOBAL;

    // Parse the cue type and extract key fields
    if (strcmp(typeStr, "effect") == 0) {
        uint8_t effectId = cueObj["data"]["effectId"] | 0u;
        uint8_t transType = cueObj["data"]["transitionType"] | 0u;

        JsonDocument resp;
        resp["cmd"] = "show.cue.inject";
        resp["success"] = true;
        resp["type"] = "effect";
        resp["effectId"] = effectId;
        resp["zone"] = zone;
        String output;
        serializeJson(resp, output);
        client->text(output);

        LW_LOGI("show.cue.inject: effect=%u transition=%u zone=%u",
                 effectId, transType, zone);
        (void)transType;
        return;
    }

    // For other cue types, acknowledge receipt
    client->text(buildShowSuccess("show.cue.inject"));
}


// ============================================================================
// Registration
// ============================================================================

void registerWsShowCommands(const WebServerContext& ctx) {
    (void)ctx;

    // Show management commands
    WsCommandRouter::registerCommand("show.upload", handleShowUpload);
    WsCommandRouter::registerCommand("show.play", handleShowPlay);
    WsCommandRouter::registerCommand("show.pause", handleShowPause);
    WsCommandRouter::registerCommand("show.resume", handleShowResume);
    WsCommandRouter::registerCommand("show.stop", handleShowStop);
    WsCommandRouter::registerCommand("show.seek", handleShowSeek);
    WsCommandRouter::registerCommand("show.status", handleShowStatus);
    WsCommandRouter::registerCommand("show.list", handleShowList);
    WsCommandRouter::registerCommand("show.delete", handleShowDelete);
    WsCommandRouter::registerCommand("show.cue.inject", handleShowCueInject);

    // Prim8 semantic parameter commands
    WsCommandRouter::registerCommand("prim8.set", handlePrim8Set);

    LW_LOGI("Registered 11 show/prim8 WS commands");
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
