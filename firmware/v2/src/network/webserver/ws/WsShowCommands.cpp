/**
 * @file WsShowCommands.cpp
 * @brief WebSocket command handlers for show management
 */

#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../core/actors/ShowNode.h"
#include "../../../core/shows/ShowTranslator.h"
#include "../../../core/shows/ShowTypes.h"
#include "../../../core/shows/BuiltinShows.h"
#include "../../../core/persistence/ShowStorage.h"
#include "../../../core/bus/MessageBus.h"
#include "../../ApiResponse.h"
#include <ArduinoJson.h>
#include <cstring>

using namespace lightwaveos::nodes;
using namespace lightwaveos::shows;
using namespace lightwaveos::persistence;
using namespace lightwaveos::persistence::ShowStorageConstants;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// ============================================================================
// Helper Functions
// ============================================================================

static void sendWsSuccess(AsyncWebSocketClient* client, const char* type,
                          const char* requestId, std::function<void(JsonObject&)> builder) {
    JsonDocument doc;
    doc["type"] = type;
    doc["success"] = true;
    if (requestId && strlen(requestId) > 0) {
        doc["requestId"] = requestId;
    }
    JsonObject data = doc["data"].to<JsonObject>();
    builder(data);
    
    String output;
    serializeJson(doc, output);
    client->text(output);
}

static void sendWsError(AsyncWebSocketClient* client, const char* type,
                       const char* errorCode, const char* message,
                       const char* requestId = nullptr) {
    JsonDocument doc;
    doc["type"] = type;
    doc["success"] = false;
    if (requestId && strlen(requestId) > 0) {
        doc["requestId"] = requestId;
    }
    JsonObject error = doc["error"].to<JsonObject>();
    error["code"] = errorCode;
    error["message"] = message;
    
    String output;
    serializeJson(doc, output);
    client->text(output);
}

// ============================================================================
// Command Handlers
// ============================================================================

static void handleShowList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    sendWsSuccess(client, "show.list", requestId, [&ctx](JsonObject& data) {
        // Built-in shows
        JsonArray builtin = data["builtin"].to<JsonArray>();
        for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
            ShowDefinition show;
            memcpy_P(&show, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));
            
            JsonObject showObj = builtin.add<JsonObject>();
            showObj["id"] = i;
            
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
                showObj["id"] = String("show-") + String(customShows[i].id - 100);
                showObj["name"] = "Custom Show";
                showObj["durationMs"] = customShows[i].durationMs;
                showObj["durationSeconds"] = customShows[i].durationMs / 1000;
                showObj["type"] = "custom";
                showObj["isSaved"] = true;
            }
        }
    });
}

static void handleShowGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("id")) {
        sendWsError(client, "show.get", ErrorCodes::MISSING_FIELD, "Missing 'id' field", requestId);
        return;
    }
    
    String showId = doc["id"].as<String>();
    String format = doc["format"] | "scenes";
    bool useScenes = (format == "scenes" || format.length() == 0);
    
    // Check if built-in show
    int builtinId = showId.toInt();
    if (builtinId >= 0 && builtinId < BUILTIN_SHOW_COUNT) {
        ShowDefinition show;
        memcpy_P(&show, &BUILTIN_SHOWS[builtinId], sizeof(ShowDefinition));
        const int capturedBuiltinId = builtinId;
        const bool capturedUseScenes = useScenes;
        
        sendWsSuccess(client, "show.get", requestId, [&show, capturedBuiltinId, capturedUseScenes](JsonObject& data) {
            data["id"] = capturedBuiltinId;
            
            char name[32];
            strncpy_P(name, show.name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            data["name"] = name;
            
            data["durationMs"] = show.totalDurationMs;
            data["durationSeconds"] = show.totalDurationMs / 1000;
            data["type"] = "builtin";
            
            if (capturedUseScenes) {
                // Convert cues to scenes
                TimelineScene scenes[ShowTranslator::MAX_SCENES];
                uint8_t sceneCount = 0;
                
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
            }
        });
        return;
    }
    
    // Custom show
    TimelineScene scenes[ShowTranslator::MAX_SCENES];
    uint8_t sceneCount = 0;
    char name[32];
    uint32_t durationMs = 0;
    
    if (!SHOW_STORAGE.loadShow(showId.c_str(), name, sizeof(name), durationMs,
                               scenes, sceneCount, ShowTranslator::MAX_SCENES)) {
        sendWsError(client, "show.get", ErrorCodes::NOT_FOUND, "Show not found", requestId);
        return;
    }
    
    sendWsSuccess(client, "show.get", requestId, [&showId, &name, durationMs, &scenes, sceneCount](JsonObject& data) {
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
    });
}

static void handleShowSave(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("name") || !doc.containsKey("durationSeconds") || !doc.containsKey("scenes")) {
        sendWsError(client, "show.save", ErrorCodes::MISSING_FIELD, "Missing required fields", requestId);
        return;
    }
    
    const char* name = doc["name"].as<const char*>();
    if (!name || name[0] == '\0') {
        sendWsError(client, "show.save", ErrorCodes::INVALID_VALUE, "Invalid 'name' field", requestId);
        return;
    }
    uint32_t durationSeconds = doc["durationSeconds"];
    uint32_t durationMs = durationSeconds * 1000;
    
    if (durationSeconds < 1 || durationSeconds > 3600) {
        sendWsError(client, "show.save", ErrorCodes::INVALID_VALUE, "Duration must be 1-3600 seconds", requestId);
        return;
    }
    
    JsonArray scenesArr = doc["scenes"];
    if (scenesArr.size() == 0 || scenesArr.size() > ShowTranslator::MAX_SCENES) {
        sendWsError(client, "show.save", ErrorCodes::INVALID_VALUE, "Invalid scene count", requestId);
        return;
    }
    
    TimelineScene scenes[ShowTranslator::MAX_SCENES];
    uint8_t sceneCount = 0;
    
    for (JsonObject sceneObj : scenesArr) {
        if (sceneCount >= ShowTranslator::MAX_SCENES) break;
        
        TimelineScene& scene = scenes[sceneCount++];
        
        const char* sceneId = sceneObj["id"].as<const char*>();
        if (sceneId && sceneId[0] != '\0') {
            strncpy(scene.id, sceneId, sizeof(scene.id) - 1);
            scene.id[sizeof(scene.id) - 1] = '\0';
        } else {
            ShowTranslator::generateSceneId(sceneCount - 1, scene.id, sizeof(scene.id));
        }
        
        scene.zoneId = sceneObj["zoneId"] | 0;
        scene.startTimePercent = sceneObj["startTimePercent"] | 0.0f;
        scene.durationPercent = sceneObj["durationPercent"] | 0.0f;
        
        const char* effectName = sceneObj["effectName"].as<const char*>();
        if (effectName && effectName[0] != '\0') {
            strncpy(scene.effectName, effectName, sizeof(scene.effectName) - 1);
            scene.effectName[sizeof(scene.effectName) - 1] = '\0';
            scene.effectId = ShowTranslator::getEffectIdByName(scene.effectName);
        } else if (sceneObj.containsKey("effectId")) {
            scene.effectId = sceneObj["effectId"] | 0;
            ShowTranslator::getEffectNameById(scene.effectId, scene.effectName, sizeof(scene.effectName));
        } else {
            sendWsError(client, "show.save", ErrorCodes::MISSING_FIELD, "Scene must have effectName or effectId", requestId);
            return;
        }
        
        ShowTranslator::getZoneColor(scene.zoneId, scene.accentColor, sizeof(scene.accentColor));
    }
    
    char showId[32];
    snprintf(showId, sizeof(showId), "show-%lu", millis());
    
    if (!SHOW_STORAGE.saveShow(showId, name, durationMs, scenes, sceneCount)) {
        sendWsError(client, "show.save", ErrorCodes::STORAGE_FULL, "Failed to save show", requestId);
        return;
    }
    
    sendWsSuccess(client, "show.save", requestId, [showId, name](JsonObject& data) {
        data["id"] = showId;
        data["name"] = name;
        data["message"] = "Show saved successfully";
    });
}

static void handleShowDelete(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("id")) {
        sendWsError(client, "show.delete", ErrorCodes::MISSING_FIELD, "Missing 'id' field", requestId);
        return;
    }
    
    String showId = doc["id"].as<String>();
    
    if (!SHOW_STORAGE.deleteShow(showId.c_str())) {
        sendWsError(client, "show.delete", ErrorCodes::NOT_FOUND, "Show not found", requestId);
        return;
    }
    
    sendWsSuccess(client, "show.delete", requestId, [&showId](JsonObject& data) {
        data["id"] = showId;
        data["message"] = "Show deleted successfully";
    });
}

static void handleShowControl(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("action")) {
        sendWsError(client, "show.control", ErrorCodes::MISSING_FIELD, "Missing 'action' field", requestId);
        return;
    }
    
    ShowNode* showNode = ctx.orchestrator.getShowDirector();
    if (!showNode) {
        sendWsError(client, "show.control", ErrorCodes::SYSTEM_NOT_READY, "ShowNode not available", requestId);
        return;
    }
    
    const char* action = doc["action"].as<const char*>();
    if (!action || action[0] == '\0') {
        sendWsError(client, "show.control", ErrorCodes::INVALID_VALUE, "Invalid 'action' field", requestId);
        return;
    }
    Message msg;
    
    if (strcmp(action, "start") == 0) {
        if (!doc.containsKey("showId")) {
            sendWsError(client, "show.control", ErrorCodes::MISSING_FIELD, "Missing 'showId' for start action", requestId);
            return;
        }
        uint8_t showId = doc["showId"];
        msg.type = MessageType::SHOW_LOAD;
        msg.param1 = showId;
        showNode->send(msg);
        
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
            sendWsError(client, "show.control", ErrorCodes::MISSING_FIELD, "Missing 'timeMs' for seek action", requestId);
            return;
        }
        uint32_t timeMs = doc["timeMs"];
        msg.type = MessageType::SHOW_SEEK;
        msg.param4 = timeMs;
        showNode->send(msg);
    } else {
        sendWsError(client, "show.control", ErrorCodes::INVALID_VALUE, "Invalid action", requestId);
        return;
    }
    
    sendWsSuccess(client, "show.control", requestId, [action](JsonObject& data) {
        data["action"] = action;
        data["message"] = "Command sent";
    });
}

static void handleShowState(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    ShowNode* showNode = ctx.orchestrator.getShowDirector();
    if (!showNode) {
        sendWsError(client, "show.state", ErrorCodes::SYSTEM_NOT_READY, "ShowNode not available", requestId);
        return;
    }
    
    sendWsSuccess(client, "show.state", requestId, [showNode](JsonObject& data) {
        if (!showNode->hasShow()) {
            data["showId"] = nullptr;
            data["isPlaying"] = false;
            return;
        }
        
        uint8_t showId = showNode->getCurrentShowId();
        data["showId"] = showId;
        
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
    });
}

// ============================================================================
// Registration
// ============================================================================

// Static initializer to register commands
struct ShowCommandsRegistrar {
    ShowCommandsRegistrar() {
        WsCommandRouter::registerCommand("show.list", handleShowList);
        WsCommandRouter::registerCommand("show.get", handleShowGet);
        WsCommandRouter::registerCommand("show.save", handleShowSave);
        WsCommandRouter::registerCommand("show.delete", handleShowDelete);
        WsCommandRouter::registerCommand("show.control", handleShowControl);
        WsCommandRouter::registerCommand("show.state", handleShowState);
    }
};

// Global instance to trigger registration
static ShowCommandsRegistrar s_registrar;

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
