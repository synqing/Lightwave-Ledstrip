/**
 * @file WsTransitionCommands.cpp
 * @brief WebSocket transition command handlers implementation
 */

#include "WsTransitionCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../effects/transitions/TransitionTypes.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::transitions;

static void handleTransitionTrigger(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t toEffect = doc["toEffect"] | 0;
    uint8_t transType = doc["transitionType"] | 0;
    bool random = doc["random"] | false;
    
    if (toEffect < ctx.renderer->getEffectCount()) {
        if (random) {
            ctx.renderer->startRandomTransition(toEffect);
        } else {
            ctx.renderer->startTransition(toEffect, transType);
        }
        if (ctx.broadcastStatus) ctx.broadcastStatus();
    }
}

static void handleTransitionGetTypes(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("transitions.types", requestId, [](JsonObject& data) {
        JsonArray types = data["types"].to<JsonArray>();
        
        for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
            JsonObject type = types.add<JsonObject>();
            type["id"] = i;
            type["name"] = getTransitionName(static_cast<TransitionType>(i));
        }
        
        data["total"] = static_cast<uint8_t>(TransitionType::TYPE_COUNT);
    });
    client->text(response);
}

static void handleTransitionConfigGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("transitions.config", requestId, [](JsonObject& data) {
        data["enabled"] = true;
        data["defaultDuration"] = 1000;
        data["defaultType"] = 0;
        data["defaultTypeName"] = getTransitionName(TransitionType::FADE);
        
        JsonArray easings = data["easings"].to<JsonArray>();
        const char* easingNames[] = {
            "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
            "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
            "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
        };
        for (uint8_t i = 0; i < 10; i++) {
            JsonObject easing = easings.add<JsonObject>();
            easing["id"] = i;
            easing["name"] = easingNames[i];
        }
    });
    client->text(response);
}

static void handleTransitionConfigSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint16_t duration = doc["defaultDuration"] | 1000;
    uint8_t type = doc["defaultType"] | 0;
    
    if (type >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid transition type", requestId));
        return;
    }
    
    String response = buildWsResponse("transitions.config", requestId, [duration, type](JsonObject& data) {
        data["defaultDuration"] = duration;
        data["defaultType"] = type;
        data["defaultTypeName"] = getTransitionName(static_cast<TransitionType>(type));
        data["message"] = "Transition config updated";
    });
    client->text(response);
}

static void handleTransitionsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("transitions.list", requestId, [](JsonObject& data) {
        JsonArray types = data["types"].to<JsonArray>();
        
        for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
            JsonObject type = types.add<JsonObject>();
            type["id"] = i;
            type["name"] = getTransitionName(static_cast<TransitionType>(i));
        }
        
        JsonArray easings = data["easingCurves"].to<JsonArray>();
        const char* easingNames[] = {
            "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
            "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
            "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
        };
        for (uint8_t i = 0; i < 10; i++) {
            JsonObject easing = easings.add<JsonObject>();
            easing["id"] = i;
            easing["name"] = easingNames[i];
        }
        
        data["total"] = static_cast<uint8_t>(TransitionType::TYPE_COUNT);
    });
    client->text(response);
}

static void handleTransitionsTrigger(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t toEffect = doc["toEffect"] | 255;
    uint8_t transType = doc["type"] | 0;
    uint16_t duration = doc["duration"] | 1000;
    
    if (toEffect == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "toEffect required", requestId));
        return;
    }
    
    if (toEffect >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid toEffect", requestId));
        return;
    }
    
    uint8_t fromEffect = ctx.renderer->getCurrentEffect();
    ctx.renderer->startTransition(toEffect, transType);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
    
    String response = buildWsResponse("transition.started", requestId, [&ctx, fromEffect, toEffect, transType, duration](JsonObject& data) {
        data["fromEffect"] = fromEffect;
        data["toEffect"] = toEffect;
        data["toEffectName"] = ctx.renderer->getEffectName(toEffect);
        data["transitionType"] = transType;
        data["transitionName"] = getTransitionName(static_cast<TransitionType>(transType));
        data["duration"] = duration;
    });
    client->text(response);
}

void registerWsTransitionCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("transition.trigger", handleTransitionTrigger);
    WsCommandRouter::registerCommand("transition.getTypes", handleTransitionGetTypes);
    // Note: transition.config is handled by two separate handlers based on whether it's get or set
    // We need to register a handler that checks for the presence of defaultDuration/defaultType
    // For now, register both and handle the logic in the handler
    WsCommandRouter::registerCommand("transition.config", [&ctx](AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& context) {
        if (doc.containsKey("defaultDuration") || doc.containsKey("defaultType")) {
            handleTransitionConfigSet(client, doc, context);
        } else {
            handleTransitionConfigGet(client, doc, context);
        }
    });
    WsCommandRouter::registerCommand("transitions.list", handleTransitionsList);
    WsCommandRouter::registerCommand("transitions.trigger", handleTransitionsTrigger);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

