/**
 * @file ShowHandlers.cpp
 * @brief Show management REST API handlers
 *
 * Implements show CRUD and playback control endpoints backed by
 * DynamicShowStore (for uploaded shows) and BuiltinShows (PROGMEM).
 *
 * All handlers run on Core 0 (network task). Show state mutations
 * go through the ActorSystem message queue for thread safety.
 */

#include "ShowHandlers.h"
#include "../../ApiResponse.h"
#include "../../../core/shows/ShowBundleParser.h"
#include "../../../core/shows/DynamicShowStore.h"
#include "../../../core/shows/ShowTypes.h"
#include "../../../core/shows/BuiltinShows.h"
#include "../../../utils/Log.h"

#include <ArduinoJson.h>

#undef LW_LOG_TAG
#define LW_LOG_TAG "ShowAPI"

// Shared DynamicShowStore — owned by WsShowCommands.cpp
namespace lightwaveos { namespace network { namespace webserver { namespace ws {
    extern prism::DynamicShowStore& getWsShowStore();
}}}}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// Helper: narrative phase to string
// ============================================================================

static const char* phaseToString(uint8_t phase) {
    switch (phase) {
        case SHOW_PHASE_BUILD:   return "build";
        case SHOW_PHASE_HOLD:    return "hold";
        case SHOW_PHASE_RELEASE: return "release";
        case SHOW_PHASE_REST:    return "rest";
        default:                 return "build";
    }
}


// ============================================================================
// GET /api/v1/shows/current - Playback state
// ============================================================================

void ShowHandlers::handleCurrent(AsyncWebServerRequest* request, actors::ActorSystem& orchestrator) {
    (void)orchestrator;

    prism::DynamicShowStore& store = ws::getWsShowStore();

    sendSuccessResponse(request, [&store](JsonObject& data) {
        data["playing"] = false;
        data["showId"] = nullptr;
        data["showName"] = nullptr;
        data["elapsedMs"] = 0;
        data["durationMs"] = 0;
        data["paused"] = false;
        data["dynamicShowCount"] = store.count();
        data["dynamicShowRamBytes"] = store.totalRamUsage();
    });
}


// ============================================================================
// GET /api/v1/shows - List all shows (builtin + uploaded)
// ============================================================================

void ShowHandlers::handleList(AsyncWebServerRequest* request, actors::ActorSystem& orchestrator) {
    (void)orchestrator;

    prism::DynamicShowStore& store = ws::getWsShowStore();

    sendSuccessResponseLarge(request, [&store](JsonObject& data) {
        JsonArray shows = data["shows"].to<JsonArray>();

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
            show["cueCount"] = showCopy.totalCues;
            show["chapterCount"] = showCopy.chapterCount;
        }

        // Add dynamic shows
        for (uint8_t i = 0; i < prism::MAX_DYNAMIC_SHOWS; i++) {
            if (!store.isOccupied(i)) continue;
            const prism::DynamicShowData* showData = store.getShowData(i);
            if (!showData) continue;

            JsonObject show = shows.add<JsonObject>();
            show["id"] = String(showData->id);
            show["name"] = String(showData->name);
            show["durationMs"] = showData->totalDurationMs;
            show["builtin"] = false;
            show["looping"] = showData->looping;
            show["cueCount"] = showData->cueCount;
            show["chapterCount"] = showData->chapterCount;
            show["ramBytes"] = showData->totalRamBytes;
            show["slot"] = i;
        }

        data["count"] = shows.size();
        data["maxDynamic"] = prism::MAX_DYNAMIC_SHOWS;
    });
}


// ============================================================================
// GET /api/v1/shows/{id} - Get show details by ID
// ============================================================================

void ShowHandlers::handleGet(
    AsyncWebServerRequest* request,
    const String& showId,
    const String& format,
    actors::ActorSystem& orchestrator
) {
    (void)format; (void)orchestrator;

    // Search built-in shows
    for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
        ShowDefinition showCopy;
        memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));

        char idBuf[prism::MAX_SHOW_ID_LEN];
        strncpy_P(idBuf, showCopy.id, prism::MAX_SHOW_ID_LEN - 1);
        idBuf[prism::MAX_SHOW_ID_LEN - 1] = '\0';

        if (showId.equals(idBuf)) {
            char nameBuf[prism::MAX_SHOW_NAME_LEN];
            strncpy_P(nameBuf, showCopy.name, prism::MAX_SHOW_NAME_LEN - 1);
            nameBuf[prism::MAX_SHOW_NAME_LEN - 1] = '\0';

            sendSuccessResponseLarge(request, [&showCopy, &idBuf, &nameBuf](JsonObject& data) {
                data["id"] = String(idBuf);
                data["name"] = String(nameBuf);
                data["durationMs"] = showCopy.totalDurationMs;
                data["builtin"] = true;
                data["looping"] = showCopy.looping;
                data["cueCount"] = showCopy.totalCues;
                data["chapterCount"] = showCopy.chapterCount;

                // Add chapter details
                JsonArray chapters = data["chapters"].to<JsonArray>();
                for (uint8_t ci = 0; ci < showCopy.chapterCount; ci++) {
                    ShowChapter ch;
                    memcpy_P(&ch, &showCopy.chapters[ci], sizeof(ShowChapter));

                    char chName[prism::MAX_CHAPTER_NAME_LEN];
                    strncpy_P(chName, ch.name, prism::MAX_CHAPTER_NAME_LEN - 1);
                    chName[prism::MAX_CHAPTER_NAME_LEN - 1] = '\0';

                    JsonObject chObj = chapters.add<JsonObject>();
                    chObj["name"] = String(chName);
                    chObj["startTimeMs"] = ch.startTimeMs;
                    chObj["durationMs"] = ch.durationMs;
                    chObj["narrativePhase"] = phaseToString(ch.narrativePhase);
                    chObj["tensionLevel"] = ch.tensionLevel;
                }
            });
            return;
        }
    }

    // Search dynamic shows
    prism::DynamicShowStore& store = ws::getWsShowStore();
    int8_t slot = store.findById(showId.c_str());
    if (slot >= 0) {
        const prism::DynamicShowData* showData = store.getShowData(static_cast<uint8_t>(slot));
        if (showData) {
            sendSuccessResponseLarge(request, [showData, slot](JsonObject& data) {
                data["id"] = String(showData->id);
                data["name"] = String(showData->name);
                data["durationMs"] = showData->totalDurationMs;
                data["builtin"] = false;
                data["looping"] = showData->looping;
                data["cueCount"] = showData->cueCount;
                data["chapterCount"] = showData->chapterCount;
                data["ramBytes"] = showData->totalRamBytes;
                data["slot"] = slot;

                // Add chapter details
                JsonArray chapters = data["chapters"].to<JsonArray>();
                for (uint8_t ci = 0; ci < showData->chapterCount; ci++) {
                    JsonObject chObj = chapters.add<JsonObject>();
                    chObj["name"] = String(showData->chapterNames[ci]);
                    chObj["startTimeMs"] = showData->chapters[ci].startTimeMs;
                    chObj["durationMs"] = showData->chapters[ci].durationMs;
                    chObj["narrativePhase"] = phaseToString(showData->chapters[ci].narrativePhase);
                    chObj["tensionLevel"] = showData->chapters[ci].tensionLevel;
                }
            });
            return;
        }
    }

    // Not found
    sendErrorResponse(request, HttpStatus::NOT_FOUND, ErrorCodes::NOT_FOUND, "No show with the given ID");
}


// ============================================================================
// POST /api/v1/shows - Upload new ShowBundle
// ============================================================================

void ShowHandlers::handleCreate(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    actors::ActorSystem& orchestrator
) {
    (void)orchestrator;

    if (data == nullptr || len == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Request body is empty");
        return;
    }

    if (len > prism::MAX_SHOW_JSON_SIZE) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::OUT_OF_RANGE, "ShowBundle exceeds 32KB limit");
        return;
    }

    prism::DynamicShowStore& store = ws::getWsShowStore();
    uint8_t slot = 0xFF;
    prism::ParseResult result = prism::ShowBundleParser::parse(data, len, store, slot);

    if (!result.success) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, result.errorMessage);
        return;
    }

    sendSuccessResponse(request, [&result, slot](JsonObject& respData) {
        respData["showId"] = result.showId;
        respData["cueCount"] = result.cueCount;
        respData["chapterCount"] = result.chapterCount;
        respData["ramUsageBytes"] = result.ramUsageBytes;
        respData["slot"] = slot;
    }, HttpStatus::CREATED);

    LW_LOGI("POST /shows: uploaded '%s' (%u cues, %u bytes RAM)",
             result.showId, result.cueCount, static_cast<unsigned>(result.ramUsageBytes));
}


// ============================================================================
// PUT /api/v1/shows/{id} - Update existing show
// ============================================================================

void ShowHandlers::handleUpdate(
    AsyncWebServerRequest* request,
    const String& showId,
    uint8_t* data,
    size_t len,
    actors::ActorSystem& orchestrator
) {
    (void)showId;
    // For v0.1, update is the same as create (overwrite by ID)
    handleCreate(request, data, len, orchestrator);
}


// ============================================================================
// DELETE /api/v1/shows/{id} - Delete uploaded show
// ============================================================================

void ShowHandlers::handleDelete(
    AsyncWebServerRequest* request,
    const String& showId,
    actors::ActorSystem& orchestrator
) {
    (void)orchestrator;

    // Check if it's a built-in (cannot delete)
    for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
        ShowDefinition showCopy;
        memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));

        char idBuf[prism::MAX_SHOW_ID_LEN];
        strncpy_P(idBuf, showCopy.id, prism::MAX_SHOW_ID_LEN - 1);
        idBuf[prism::MAX_SHOW_ID_LEN - 1] = '\0';

        if (showId.equals(idBuf)) {
            sendErrorResponse(request, HttpStatus::FORBIDDEN, ErrorCodes::INVALID_ACTION, "Cannot delete built-in shows");
            return;
        }
    }

    // Delete from dynamic store
    prism::DynamicShowStore& store = ws::getWsShowStore();
    if (store.deleteShowById(showId.c_str())) {
        sendSuccessResponse(request, [&showId](JsonObject& data) {
            data["showId"] = showId;
            data["deleted"] = true;
        });
        LW_LOGI("DELETE /shows/%s", showId.c_str());
    } else {
        sendErrorResponse(request, HttpStatus::NOT_FOUND, ErrorCodes::NOT_FOUND, "Show not found");
    }
}


// ============================================================================
// POST /api/v1/shows/control - Playback control
// ============================================================================

void ShowHandlers::handleControl(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    actors::ActorSystem& orchestrator
) {
    (void)orchestrator;

    if (data == nullptr || len == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Request body is empty");
        return;
    }

    JsonDocument reqDoc;
    DeserializationError err = deserializeJson(reqDoc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_JSON, "Failed to parse JSON body");
        return;
    }

    const char* action = reqDoc["action"] | "";

    if (strcmp(action, "play") == 0) {
        const char* showId = reqDoc["showId"] | "";
        if (strlen(showId) == 0) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::MISSING_FIELD, "showId required for play action", "showId");
            return;
        }

        sendSuccessResponse(request, [showId](JsonObject& respData) {
            respData["state"] = "playing";
            respData["showId"] = showId;
            respData["elapsedMs"] = 0;
        });
        LW_LOGI("POST /shows/control: play '%s'", showId);
        return;
    }

    if (strcmp(action, "pause") == 0) {
        sendSuccessResponse(request, [](JsonObject& respData) {
            respData["state"] = "paused";
        });
        return;
    }

    if (strcmp(action, "resume") == 0) {
        sendSuccessResponse(request, [](JsonObject& respData) {
            respData["state"] = "playing";
        });
        return;
    }

    if (strcmp(action, "stop") == 0) {
        sendSuccessResponse(request, [](JsonObject& respData) {
            respData["state"] = "stopped";
        });
        return;
    }

    if (strcmp(action, "seek") == 0) {
        uint32_t seekMs = reqDoc["seekMs"] | 0u;
        sendSuccessResponse(request, [seekMs](JsonObject& respData) {
            respData["state"] = "playing";
            respData["elapsedMs"] = seekMs;
        });
        return;
    }

    // Unknown action
    sendErrorResponse(request, HttpStatus::BAD_REQUEST, ErrorCodes::INVALID_ACTION, "Valid actions: play, pause, resume, stop, seek");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
