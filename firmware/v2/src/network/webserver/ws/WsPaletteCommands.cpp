/**
 * @file WsPaletteCommands.cpp
 * @brief WebSocket palette command handlers implementation
 */

#include "WsPaletteCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../palettes/Palettes_Master.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::palettes;

static void handlePalettesList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t page = doc["page"] | 1;
    uint8_t limit = doc["limit"] | 20;
    
    if (page < 1) page = 1;
    if (limit < 1) limit = 1;
    if (limit > 50) limit = 50;
    
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
    
    // Set palette via NodeOrchestrator
    ctx.orchestrator.setPalette(paletteId);
    
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

