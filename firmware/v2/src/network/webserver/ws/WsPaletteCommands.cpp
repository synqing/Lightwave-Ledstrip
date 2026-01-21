/**
 * @file WsPaletteCommands.cpp
 * @brief WebSocket palette command handlers implementation
 */

#include "WsPaletteCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../codec/WsPaletteCodec.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../palettes/Palettes_Master.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstdio>
#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::palettes;

static uint32_t debugNowMs() {
#ifndef NATIVE_BUILD
    return millis();
#else
    return 0;
#endif
}

static void handlePalettesList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::PalettesListDecodeResult decodeResult = codec::WsPaletteCodec::decodeList(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::PalettesListRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t page = req.page;
    uint8_t limit = req.limit;

    // Values already validated by codec (page >= 1, limit 1-50)
    
    uint8_t startIdx = (page - 1) * limit;
    uint8_t endIdx = (startIdx + limit < MASTER_PALETTE_COUNT) ? (startIdx + limit) : MASTER_PALETTE_COUNT;
    
    String response = buildWsResponse("palettes.list", requestId, [startIdx, endIdx, page, limit](JsonObject& data) {
        JsonArray palettes = data["palettes"].to<JsonArray>();
        
        for (uint8_t i = startIdx; i < endIdx; i++) {
            JsonObject palette = palettes.add<JsonObject>();
            palette["id"] = i;
            palette["name"] = MasterPaletteNames[i];
            palette["category"] = getPaletteCategory(i);
        }
        
        JsonObject pagination = data["pagination"].to<JsonObject>();
        pagination["page"] = page;
        pagination["limit"] = limit;
        pagination["total"] = MASTER_PALETTE_COUNT;
        pagination["pages"] = (MASTER_PALETTE_COUNT + limit - 1) / limit;
    });
    client->text(response);
}

static void handlePalettesGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t paletteId = doc["paletteId"] | 255;
    
    if (paletteId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "paletteId required", requestId));
        return;
    }
    
    if (paletteId >= MASTER_PALETTE_COUNT) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Palette ID out of range", requestId));
        return;
    }
    
    String response = buildWsResponse("palettes.get", requestId, [paletteId](JsonObject& data) {
        JsonObject palette = data["palette"].to<JsonObject>();
        palette["id"] = paletteId;
        palette["name"] = MasterPaletteNames[paletteId];
        palette["category"] = getPaletteCategory(paletteId);
        
        JsonObject flags = palette["flags"].to<JsonObject>();
        flags["warm"] = isPaletteWarm(paletteId);
        flags["cool"] = isPaletteCool(paletteId);
        flags["calm"] = isPaletteCalm(paletteId);
        flags["vivid"] = isPaletteVivid(paletteId);
        flags["cvdFriendly"] = isPaletteCVDFriendly(paletteId);
        flags["whiteHeavy"] = paletteHasFlag(paletteId, PAL_WHITE_HEAVY);
        
        palette["avgBrightness"] = getPaletteAvgBrightness(paletteId);
        palette["maxBrightness"] = getPaletteMaxBrightness(paletteId);
    });
    client->text(response);
}

static void handlePalettesSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::PalettesSetDecodeResult decodeResult = codec::WsPaletteCodec::decodeSet(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::PalettesSetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t paletteId = req.paletteId;
    
    if (paletteId >= MASTER_PALETTE_COUNT) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Palette ID out of range", requestId));
        return;
    }

    // #region agent log
    {
        FILE* f = fopen("/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/.cursor/debug.log", "a");
        if (f) {
            fprintf(f,
                    "{\"sessionId\":\"debug-session\",\"runId\":\"palette-loop-1\",\"hypothesisId\":\"H1\","
                    "\"location\":\"WsPaletteCommands.cpp:handlePalettesSet\",\"message\":\"palettes.set received\","
                    "\"data\":{\"clientId\":%lu,\"paletteId\":%u,\"requestId\":\"%s\"},\"timestamp\":%lu}\n",
                    static_cast<unsigned long>(client ? client->id() : 0UL),
                    static_cast<unsigned>(paletteId),
                    requestId ? requestId : "",
                    static_cast<unsigned long>(debugNowMs()));
            fclose(f);
        }
    }
    // #endregion

    // Set palette via ActorSystem
    ctx.actorSystem.setPalette(paletteId);
    
    String response = buildWsResponse("palettes.set", requestId, [paletteId](JsonObject& data) {
        data["paletteId"] = paletteId;
        data["name"] = MasterPaletteNames[paletteId];
        data["category"] = getPaletteCategory(paletteId);
    });
    client->text(response);
}

void registerWsPaletteCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("palettes.list", handlePalettesList);
    WsCommandRouter::registerCommand("palettes.get", handlePalettesGet);
    WsCommandRouter::registerCommand("palettes.set", handlePalettesSet);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

