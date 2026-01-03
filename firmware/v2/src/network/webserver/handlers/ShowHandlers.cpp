#include "ShowHandlers.h"
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../core/actors/ShowNode.h"
#include "../../../core/shows/ShowTranslator.h"
#include "../../../core/shows/ShowTypes.h"
#include "../../../core/shows/BuiltinShows.h"
#include "../../../core/persistence/ShowStorage.h"

using namespace lightwaveos::persistence::ShowStorageConstants;
#include "../../../core/bus/MessageBus.h"
#include "../../RequestValidator.h"
#include "../../ApiResponse.h"
#include <ArduinoJson.h>
#include <cstring>

using namespace lightwaveos::nodes;
using namespace lightwaveos::shows;
using namespace lightwaveos::persistence;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// List Shows
// ============================================================================

void ShowHandlers::handleList(AsyncWebServerRequest* request,
                              NodeOrchestrator& orchestrator) {
    StaticJsonDocument<2048> doc;
    JsonObject data = doc.to<JsonObject>();

    // Built-in shows
    JsonArray builtin = data["builtin"].to<JsonArray>();
    for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
        ShowDefinition show;
        memcpy_P(&show, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));
        
        JsonObject showObj = builtin.add<JsonObject>();
        showObj["id"] = i;
        
        // Copy name from PROGMEM
        char name[32];
        strncpy_P(name, show.name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        showObj["name"] = name;
        
        showObj["durationMs"] = show.totalDurationMs;
        showObj["durationSeconds"] = show.totalDurationMs / 1000;
        showObj["chapterCount"] = show.chapterCount;
        showObj["cueCount"] = show.totalCues;
        showObj["looping"] = show.looping;
        showObj["type"] = "builtin";
    }

    // Custom shows
    JsonArray custom = data["custom"].to<JsonArray>();
    ShowInfo customShows[ShowStorageConstants::MAX_CUSTOM_SHOWS];
    uint8_t customCount = 0;
    
    if (SHOW_STORAGE.listShows(customShows, customCount, ShowStorageConstants::MAX_CUSTOM_SHOWS)) {
        for (uint8_t i = 0; i < customCount; i++) {
            JsonObject showObj = custom.add<JsonObject>();
            showObj["id"] = String("show-") + String(customShows[i].id - 100);  // Convert back to string ID
            showObj["name"] = "Custom Show";  // Name requires full load
            showObj["durationMs"] = customShows[i].durationMs;
            showObj["durationSeconds"] = customShows[i].durationMs / 1000;
            showObj["type"] = "custom";
            showObj["isSaved"] = true;
        }
    }

    sendSuccessResponseLarge(request, [&data](JsonObject& responseData) {
        responseData.set(data);
    });
}

// ============================================================================
// Get Show
// ============================================================================

void ShowHandlers::handleGet(AsyncWebServerRequest* request,
                             const String& showId,
                             const String& format,
                             NodeOrchestrator& orchestrator) {
    bool useScenes = (format == "scenes" || format.length() == 0);
    
    // Check if built-in show (numeric ID)
    int builtinId = showId.toInt();
    if (builtinId >= 0 && builtinId < BUILTIN_SHOW_COUNT) {
        // Built-in show
        ShowDefinition show;
        memcpy_P(&show, &BUILTIN_SHOWS[builtinId], sizeof(ShowDefinition));
        
        StaticJsonDocument<4096> doc;
        JsonObject data = doc.to<JsonObject>();
        
        data["id"] = builtinId;
        
        // Copy name from PROGMEM
        char name[32];
        strncpy_P(name, show.name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        data["name"] = name;
        
        data["durationMs"] = show.totalDurationMs;
        data["durationSeconds"] = show.totalDurationMs / 1000;
        data["type"] = "builtin";
        
        if (useScenes) {
            // Convert cues to scenes
            TimelineScene scenes[ShowTranslator::MAX_SCENES];
            uint8_t sceneCount = 0;
            
            // Read cues from PROGMEM
            ShowCue cues[100];
            uint8_t cueCount = 0;
            for (uint8_t i = 0; i < show.totalCues && cueCount < 100; i++) {
                memcpy_P(&cues[cueCount], &show.cues[i], sizeof(ShowCue));
                cueCount++;
            }
            
            if (ShowTranslator::cuesToScenes(cues, cueCount, show.totalDurationMs,
                                            scenes, sceneCount, ShowTranslator::MAX_SCENES)) {
                JsonArray scenesArr = data["scenes"].to<JsonArray>();
                for (uint8_t i = 0; i < sceneCount; i++) {
                    JsonObject scene = scenesArr.add<JsonObject>();
                    scene["id"] = scenes[i].id;
                    scene["zoneId"] = scenes[i].zoneId;
                    scene["effectName"] = scenes[i].effectName;
                    scene["startTimePercent"] = scenes[i].startTimePercent;
                    scene["durationPercent"] = scenes[i].durationPercent;
                    scene["accentColor"] = scenes[i].accentColor;
                }
            }
        } else {
            // Return cues format
            JsonArray cuesArr = data["cues"].to<JsonArray>();
            for (uint8_t i = 0; i < show.totalCues && i < 100; i++) {
                ShowCue cue;
                memcpy_P(&cue, &show.cues[i], sizeof(ShowCue));
                
                JsonObject cueObj = cuesArr.add<JsonObject>();
                cueObj["timeMs"] = cue.timeMs;
                cueObj["type"] = (int)cue.type;
                cueObj["targetZone"] = cue.targetZone;
                
                JsonObject cueData = cueObj["data"].to<JsonObject>();
                if (cue.type == CUE_EFFECT) {
                    cueData["effectId"] = cue.effectId();
                    cueData["transitionType"] = cue.effectTransition();
                } else if (cue.type == CUE_PARAMETER_SWEEP) {
                    cueData["paramId"] = cue.sweepParamId();
                    cueData["targetValue"] = cue.sweepTargetValue();
                    cueData["durationMs"] = cue.sweepDurationMs();
                }
            }
        }
        
        sendSuccessResponseLarge(request, [&data](JsonObject& responseData) {
            responseData.set(data);
        });
        return;
    }
    
    // Custom show (string ID)
    TimelineScene scenes[ShowTranslator::MAX_SCENES];
    uint8_t sceneCount = 0;
    char name[32];
    uint32_t durationMs = 0;
    
    if (!SHOW_STORAGE.loadShow(showId.c_str(), name, sizeof(name), durationMs,
                               scenes, sceneCount, ShowTranslator::MAX_SCENES)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                         ErrorCodes::NOT_FOUND, "Show not found");
        return;
    }
    
    StaticJsonDocument<4096> doc;
    JsonObject data = doc.to<JsonObject>();
    data["id"] = showId;
    data["name"] = name;
    data["durationMs"] = durationMs;
    data["durationSeconds"] = durationMs / 1000;
    data["type"] = "custom";
    
    JsonArray scenesArr = data["scenes"].to<JsonArray>();
    for (uint8_t i = 0; i < sceneCount; i++) {
        JsonObject scene = scenesArr.add<JsonObject>();
        scene["id"] = scenes[i].id;
        scene["zoneId"] = scenes[i].zoneId;
        scene["effectName"] = scenes[i].effectName;
        scene["startTimePercent"] = scenes[i].startTimePercent;
        scene["durationPercent"] = scenes[i].durationPercent;
        scene["accentColor"] = scenes[i].accentColor;
    }
    
    sendSuccessResponseLarge(request, [&data](JsonObject& responseData) {
        responseData.set(data);
    });
}

// ============================================================================
// Create Show
// ============================================================================

void ShowHandlers::handleCreate(AsyncWebServerRequest* request,
                                uint8_t* data, size_t len,
                                NodeOrchestrator& orchestrator) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }
    
    if (!doc.containsKey("name") || !doc.containsKey("durationSeconds") || !doc.containsKey("scenes")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::MISSING_FIELD, "Missing required fields");
        return;
    }
    
    const char* name = doc["name"];
    uint32_t durationSeconds = doc["durationSeconds"];
    uint32_t durationMs = durationSeconds * 1000;
    
    // Validate duration
    if (durationSeconds < 1 || durationSeconds > 3600) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::INVALID_VALUE, "Duration must be 1-3600 seconds");
        return;
    }
    
    // Parse scenes
    JsonArray scenesArr = doc["scenes"];
    if (scenesArr.size() == 0 || scenesArr.size() > ShowTranslator::MAX_SCENES) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::INVALID_VALUE, "Invalid scene count");
        return;
    }
    
    TimelineScene scenes[ShowTranslator::MAX_SCENES];
    uint8_t sceneCount = 0;
    
    for (JsonObject sceneObj : scenesArr) {
        if (sceneCount >= ShowTranslator::MAX_SCENES) break;
        
        TimelineScene& scene = scenes[sceneCount++];
        
        // Generate ID if not provided
        if (sceneObj.containsKey("id")) {
            strncpy(scene.id, sceneObj["id"], sizeof(scene.id) - 1);
            scene.id[sizeof(scene.id) - 1] = '\0';
        } else {
            ShowTranslator::generateSceneId(sceneCount - 1, scene.id, sizeof(scene.id));
        }
        
        scene.zoneId = sceneObj["zoneId"] | 0;
        scene.startTimePercent = sceneObj["startTimePercent"] | 0.0f;
        scene.durationPercent = sceneObj["durationPercent"] | 0.0f;
        
        // Get effect name or ID
        if (sceneObj.containsKey("effectName")) {
            strncpy(scene.effectName, sceneObj["effectName"], sizeof(scene.effectName) - 1);
            scene.effectName[sizeof(scene.effectName) - 1] = '\0';
            scene.effectId = ShowTranslator::getEffectIdByName(scene.effectName);
        } else if (sceneObj.containsKey("effectId")) {
            scene.effectId = sceneObj["effectId"];
            ShowTranslator::getEffectNameById(scene.effectId, scene.effectName, sizeof(scene.effectName));
        } else {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                             ErrorCodes::MISSING_FIELD, "Scene must have effectName or effectId");
            return;
        }
        
        // Get zone color
        ShowTranslator::getZoneColor(scene.zoneId, scene.accentColor, sizeof(scene.accentColor));
    }
    
    // Generate show ID
    char showId[32];
    snprintf(showId, sizeof(showId), "show-%lu", millis());
    
    // Save to storage
    if (!SHOW_STORAGE.saveShow(showId, name, durationMs, scenes, sceneCount)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                         ErrorCodes::STORAGE_FULL, "Failed to save show");
        return;
    }
    
    // Return success
    StaticJsonDocument<256> response;
    JsonObject responseData = response.to<JsonObject>();
    responseData["id"] = showId;
    responseData["name"] = name;
    responseData["message"] = "Show saved successfully";
    
    sendSuccessResponseLarge(request, [&responseData](JsonObject& outData) {
        outData.set(responseData);
    });
}

// ============================================================================
// Update Show
// ============================================================================

void ShowHandlers::handleUpdate(AsyncWebServerRequest* request,
                                const String& showId,
                                uint8_t* data, size_t len,
                                NodeOrchestrator& orchestrator) {
    // Same as create, but with existing ID
    // For now, delete and recreate (simple implementation)
    if (!SHOW_STORAGE.deleteShow(showId.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                         ErrorCodes::NOT_FOUND, "Show not found");
        return;
    }
    
    // Reuse create logic
    handleCreate(request, data, len, orchestrator);
}

// ============================================================================
// Delete Show
// ============================================================================

void ShowHandlers::handleDelete(AsyncWebServerRequest* request,
                                const String& showId,
                                NodeOrchestrator& orchestrator) {
    if (!SHOW_STORAGE.deleteShow(showId.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                         ErrorCodes::NOT_FOUND, "Show not found");
        return;
    }
    
    StaticJsonDocument<128> doc;
    JsonObject data = doc.to<JsonObject>();
    data["id"] = showId;
    data["message"] = "Show deleted successfully";
    
    sendSuccessResponseLarge(request, [&data](JsonObject& responseData) {
        responseData.set(data);
    });
}

// ============================================================================
// Get Current State
// ============================================================================

void ShowHandlers::handleCurrent(AsyncWebServerRequest* request,
                                 NodeOrchestrator& orchestrator) {
    ShowNode* showNode = orchestrator.getShowDirector();
    if (!showNode) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                         ErrorCodes::SYSTEM_NOT_READY, "ShowNode not available");
        return;
    }
    
    StaticJsonDocument<512> doc;
    JsonObject data = doc.to<JsonObject>();
    
    if (!showNode->hasShow()) {
        sendSuccessResponseLarge(request, [](JsonObject& responseData) {
            responseData["showId"] = nullptr;
            responseData["isPlaying"] = false;
        });
        return;
    }
    
    uint8_t showId = showNode->getCurrentShowId();
    data["showId"] = showId;
    
    // Get show name
    if (showId < BUILTIN_SHOW_COUNT) {
        ShowDefinition show;
        memcpy_P(&show, &BUILTIN_SHOWS[showId], sizeof(ShowDefinition));
        char name[32];
        strncpy_P(name, show.name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        data["showName"] = name;
    } else {
        data["showName"] = "Custom Show";
    }
    
    data["isPlaying"] = showNode->isPlaying();
    data["isPaused"] = showNode->isPaused();
    data["progress"] = showNode->getProgress();
    data["elapsedMs"] = showNode->getElapsedMs();
    data["remainingMs"] = showNode->getRemainingMs();
    data["currentChapter"] = showNode->getCurrentChapter();
    
    sendSuccessResponseLarge(request, [&data](JsonObject& responseData) {
        responseData.set(data);
    });
}

// ============================================================================
// Control Playback
// ============================================================================

void ShowHandlers::handleControl(AsyncWebServerRequest* request,
                                 uint8_t* data, size_t len,
                                 NodeOrchestrator& orchestrator) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }
    
    if (!doc.containsKey("action")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::MISSING_FIELD, "Missing 'action' field");
        return;
    }
    
    const char* action = doc["action"];
    ShowNode* showNode = orchestrator.getShowDirector();
    if (!showNode) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                         ErrorCodes::SYSTEM_NOT_READY, "ShowNode not available");
        return;
    }
    
    Message msg;
    msg.type = MessageType::HEALTH_CHECK;  // Default
    
    if (strcmp(action, "start") == 0) {
        if (!doc.containsKey("showId")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                             ErrorCodes::MISSING_FIELD, "Missing 'showId' for start action");
            return;
        }
        uint8_t showId = doc["showId"];
        msg.type = MessageType::SHOW_LOAD;
        msg.param1 = showId;
        showNode->send(msg);
        
        // Wait a bit for load, then start
        delay(10);
        msg.type = MessageType::SHOW_START;
        msg.param1 = 0;
        showNode->send(msg);
    } else if (strcmp(action, "stop") == 0) {
        msg.type = MessageType::SHOW_STOP;
        showNode->send(msg);
    } else if (strcmp(action, "pause") == 0) {
        msg.type = MessageType::SHOW_PAUSE;
        showNode->send(msg);
    } else if (strcmp(action, "resume") == 0) {
        msg.type = MessageType::SHOW_RESUME;
        showNode->send(msg);
    } else if (strcmp(action, "seek") == 0) {
        if (!doc.containsKey("timeMs")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                             ErrorCodes::MISSING_FIELD, "Missing 'timeMs' for seek action");
            return;
        }
        uint32_t timeMs = doc["timeMs"];
        msg.type = MessageType::SHOW_SEEK;
        msg.param4 = timeMs;
        showNode->send(msg);
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                         ErrorCodes::INVALID_VALUE, "Invalid action");
        return;
    }
    
    StaticJsonDocument<128> response;
    JsonObject responseData = response.to<JsonObject>();
    responseData["action"] = action;
    responseData["message"] = "Command sent";
    
    sendSuccessResponseLarge(request, [&responseData](JsonObject& outData) {
        outData.set(responseData);
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

