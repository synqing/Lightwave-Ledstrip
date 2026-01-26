/**
 * @file WsTransitionCommands.cpp
 * @brief WebSocket transition command handlers implementation
 */

#include "WsTransitionCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../codec/WsTransitionCodec.h"
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
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::TransitionTriggerDecodeResult decodeResult = codec::WsTransitionCodec::decodeTrigger(root);

    if (!decodeResult.success) {
        // Legacy command doesn't send errors, just ignore invalid requests
        return;
    }

    const codec::TransitionTriggerRequest& req = decodeResult.request;
    uint8_t toEffect = req.toEffect;
    
    if (toEffect < ctx.renderer->getEffectCount()) {
        if (req.random) {
            ctx.renderer->startRandomTransition(toEffect);
        } else {
            ctx.renderer->startTransition(toEffect, req.transitionType);
        }
        if (ctx.broadcastStatus) ctx.broadcastStatus();
    }
}

static void handleTransitionGetTypes(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::TransitionSimpleDecodeResult decodeResult = codec::WsTransitionCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    String response = buildWsResponse("transitions.types", requestId, [](JsonObject& data) {
        codec::WsTransitionCodec::encodeGetTypes(data);
    });
    client->text(response);
}

static void handleTransitionConfigGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::TransitionSimpleDecodeResult decodeResult = codec::WsTransitionCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    String response = buildWsResponse("transitions.config", requestId, [](JsonObject& data) {
        codec::WsTransitionCodec::encodeConfigGet(data);
    });
    client->text(response);
}

static void handleTransitionConfigSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::TransitionConfigSetDecodeResult decodeResult = codec::WsTransitionCodec::decodeConfigSet(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::TransitionConfigSetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint16_t duration = req.defaultDuration;
    uint8_t type = req.defaultType;
    
    if (type >= static_cast<uint8_t>(TransitionType::TYPE_COUNT)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid transition type", requestId));
        return;
    }
    
    String response = buildWsResponse("transitions.config", requestId, [duration, type](JsonObject& data) {
        codec::WsTransitionCodec::encodeConfigSet(duration, type, data);
    });
    client->text(response);
}

static void handleTransitionsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::TransitionSimpleDecodeResult decodeResult = codec::WsTransitionCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    String response = buildWsResponse("transitions.list", requestId, [](JsonObject& data) {
        codec::WsTransitionCodec::encodeList(data);
    });
    client->text(response);
}

static void handleTransitionsTrigger(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::TransitionsTriggerDecodeResult decodeResult = codec::WsTransitionCodec::decodeTransitionsTrigger(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::TransitionsTriggerRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t toEffect = req.toEffect;
    uint8_t transType = req.type;
    uint16_t duration = req.duration;
    
    if (toEffect >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid toEffect", requestId));
        return;
    }
    
    uint8_t fromEffect = ctx.renderer->getCurrentEffect();
    ctx.renderer->startTransition(toEffect, transType);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
    
    // Use variables from decode result
    const char* toEffectName = ctx.renderer ? ctx.renderer->getEffectName(toEffect) : "";
    const char* transitionName = getTransitionName(static_cast<TransitionType>(transType));
    String response = buildWsResponse("transition.started", requestId, [fromEffect, toEffect, toEffectName, transType, transitionName, duration](JsonObject& data) {
        codec::WsTransitionCodec::encodeTriggerStarted(fromEffect, toEffect, toEffectName, transType, transitionName, duration, data);
    });
    client->text(response);
}

void registerWsTransitionCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("transition.trigger", handleTransitionTrigger);
    WsCommandRouter::registerCommand("transition.getTypes", handleTransitionGetTypes);
    // Note: transition.config is handled by two separate handlers based on whether it's get or set
    // We need to register a handler that checks for the presence of defaultDuration/defaultType
    // For now, register both and handle the logic in the handler
    WsCommandRouter::registerCommand("transition.config", [](AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& context) {
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

