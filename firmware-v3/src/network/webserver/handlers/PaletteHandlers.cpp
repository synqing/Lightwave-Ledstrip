/**
 * @file PaletteHandlers.cpp
 * @brief Palette handlers implementation
 */

#include "PaletteHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../palettes/Palettes_Master.h"

#undef LW_LOG_TAG
#define LW_LOG_TAG "PaletteH"
#include "../../../utils/Log.h"

using namespace lightwaveos::actors;
using namespace lightwaveos::network;
using namespace lightwaveos::palettes;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void PaletteHandlers::handleList(AsyncWebServerRequest* request,
                                   RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    // Get optional filter parameters
    bool filterCategory = request->hasParam("category");
    String categoryFilter = filterCategory ? request->getParam("category")->value() : "";
    bool filterFlags = request->hasParam("warm") || request->hasParam("cool") ||
                       request->hasParam("calm") || request->hasParam("vivid") ||
                       request->hasParam("cvd");

    // Get pagination parameters (offset-based for V2 API, page-based for backward compatibility)
    uint16_t page = 1;
    uint16_t limit = 20;
    int offset = 0;
    
    // V2 API uses "offset" instead of "page"
    if (request->hasParam("offset")) {
        offset = request->getParam("offset")->value().toInt();
        if (offset < 0) offset = 0;
        page = (offset / limit) + 1;
    } else if (request->hasParam("page")) {
        page = request->getParam("page")->value().toInt();
        if (page < 1) page = 1;
    }
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > 100) limit = 100;  // Increased from 50 to 100 to allow fetching all palettes
        if (request->hasParam("offset") && !request->hasParam("page")) {
            int offsetParam = request->getParam("offset")->value().toInt();
            if (offsetParam < 0) offsetParam = 0;
            offset = offsetParam;
            page = (offset / limit) + 1;
        }
    }

    // First pass: collect all palette IDs that pass the filters
    uint8_t filteredIds[MASTER_PALETTE_COUNT];
    uint16_t filteredCount = 0;

    for (uint8_t i = 0; i < MASTER_PALETTE_COUNT; i++) {
        // Apply category filter
        if (filterCategory) {
            if (categoryFilter == "artistic" && !isCptCityPalette(i)) continue;
            if (categoryFilter == "scientific" && !isCrameriPalette(i)) continue;
            if (categoryFilter == "lgpOptimized" && !isColorspacePalette(i)) continue;
        }

        // Apply flag filters
        if (filterFlags) {
            bool match = true;
            if (request->hasParam("warm") && request->getParam("warm")->value() == "true") {
                match = match && isPaletteWarm(i);
            }
            if (request->hasParam("cool") && request->getParam("cool")->value() == "true") {
                match = match && isPaletteCool(i);
            }
            if (request->hasParam("calm") && request->getParam("calm")->value() == "true") {
                match = match && isPaletteCalm(i);
            }
            if (request->hasParam("vivid") && request->getParam("vivid")->value() == "true") {
                match = match && isPaletteVivid(i);
            }
            if (request->hasParam("cvd") && request->getParam("cvd")->value() == "true") {
                match = match && isPaletteCVDFriendly(i);
            }
            if (!match) continue;
        }

        // This palette passes all filters
        filteredIds[filteredCount++] = i;
    }

    // Calculate pagination
    uint16_t totalPages = (filteredCount + limit - 1) / limit;  // Ceiling division
    if (totalPages == 0) totalPages = 1;
    if (page > totalPages) page = totalPages;

    uint16_t startIdx = (page - 1) * limit;
    uint16_t endIdx = startIdx + limit;
    if (endIdx > filteredCount) endIdx = filteredCount;

    // Calculate actual offset (for V2 API compatibility)
    int actualOffset = startIdx;

    // -----------------------------------------------------------------
    // Streamed JSON construction.
    //
    // Heap-fragmentation fix: previously this handler built the entire
    // JsonDocument (~9 KB worst case for ~75 palettes) and serialised it
    // into a String, which fragmented the K1 V2 internal heap. We now
    // stream the response directly to the AsyncResponseStream, with one
    // small JsonDocument per palette entry — peak heap during the loop
    // is bounded by a single entry (~250 bytes) plus the cbuf which
    // grows incrementally.
    // -----------------------------------------------------------------

    AsyncResponseStream* response =
        request->beginResponseStream("application/json", 2048);

    response->print("{\"success\":true,\"data\":{");

    // Static category counts (not affected by filters).
    response->printf("\"categories\":{\"artistic\":%d,\"scientific\":%d,\"lgpOptimized\":%d},",
                     (int)(CPT_CITY_END - CPT_CITY_START + 1),
                     (int)(CRAMERI_END - CRAMERI_START + 1),
                     (int)(COLORSPACE_END - COLORSPACE_START + 1));

    response->print("\"palettes\":[");

    // Second pass: add only the paginated subset of filtered palettes.
    uint16_t writtenCount = 0;
    for (uint16_t idx = startIdx; idx < endIdx; idx++) {
        uint8_t i = filteredIds[idx];
        if (writtenCount > 0) response->print(",");

        JsonDocument palette;
        palette["id"] = i;
        palette["name"] = MasterPaletteNames[i];
        palette["category"] = getPaletteCategory(i);

        // Flags
        JsonObject flags = palette["flags"].to<JsonObject>();
        flags["warm"] = isPaletteWarm(i);
        flags["cool"] = isPaletteCool(i);
        flags["calm"] = isPaletteCalm(i);
        flags["vivid"] = isPaletteVivid(i);
        flags["cvdFriendly"] = isPaletteCVDFriendly(i);
        flags["whiteHeavy"] = paletteHasFlag(i, PAL_WHITE_HEAVY);

        // Metadata
        palette["avgBrightness"] = getPaletteAvgBrightness(i);
        palette["maxBrightness"] = getPaletteMaxBrightness(i);

        // Colour stops are NOT inlined here — iOS reads them from the
        // bundled `Resources/Palettes/Palettes_Master.json` (75 palettes,
        // ~64 KB, generated from this file's gradient tables). Embedding
        // 75 × ~7 stops × per-iteration ArduinoJson allocation in the
        // AsyncTCP request context starves loopTask on Core 1 and trips
        // the FreeRTOS task watchdog (5 s threshold) — confirmed via
        // backtrace 2026-05-03. The dedicated `/api/v1/palettes/{id}/swatch`
        // endpoint (proposed; not yet implemented) is the right home for
        // dynamic colour-stop refresh; the list endpoint stays light.

        serializeJson(palette, *response);
        writtenCount++;
    }

    response->print("],");

    // Flat pagination fields (V2PalettesList) and pagination object
    // (backward compatible). count is the number of palettes emitted.
    response->printf("\"total\":%u,\"offset\":%d,\"limit\":%u,",
                     (unsigned)filteredCount, actualOffset, (unsigned)limit);
    response->printf("\"pagination\":{\"page\":%u,\"limit\":%u,\"total\":%u,\"pages\":%u},",
                     (unsigned)page, (unsigned)limit,
                     (unsigned)filteredCount, (unsigned)totalPages);
    response->printf("\"count\":%u", (unsigned)writtenCount);

    response->printf("},\"timestamp\":%lu,\"version\":\"%s\"}",
                     (unsigned long)millis(), API_VERSION);

    request->send(response);
}

void PaletteHandlers::handleCurrent(AsyncWebServerRequest* request,
                                      RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [renderer](JsonObject& data) {
        uint8_t id = renderer->getPaletteIndex();
        data["paletteId"] = id;
        data["name"] = MasterPaletteNames[id];
        data["category"] = getPaletteCategory(id);

        JsonObject flags = data["flags"].to<JsonObject>();
        flags["warm"] = isPaletteWarm(id);
        flags["cool"] = isPaletteCool(id);
        flags["calm"] = isPaletteCalm(id);
        flags["vivid"] = isPaletteVivid(id);
        flags["cvdFriendly"] = isPaletteCVDFriendly(id);

        data["avgBrightness"] = getPaletteAvgBrightness(id);
        data["maxBrightness"] = getPaletteMaxBrightness(id);
    });
}

void PaletteHandlers::handleSet(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len,
                                  ActorSystem& actorSystem,
                                  std::function<void()> broadcastStatus) {
    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetPalette, request);

    uint8_t paletteId = doc["paletteId"];
    if (paletteId >= MASTER_PALETTE_COUNT) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Palette ID out of range (0-74)", "paletteId");
        return;
    }

    if (!actorSystem.setPalette(paletteId)) {
        LW_LOGW("REST setPalette rejected - queue saturated");
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::RATE_LIMITED, "Queue saturated");
        return;
    }

    sendSuccessResponse(request, [paletteId](JsonObject& respData) {
        respData["paletteId"] = paletteId;
        respData["name"] = MasterPaletteNames[paletteId];
        respData["category"] = getPaletteCategory(paletteId);
    });

    broadcastStatus();
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

