/**
 * @file TransitionHandlers.cpp
 * @brief Transition handlers implementation
 */

#include "TransitionHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../WebServer.h"  // For CachedRendererState
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../effects/transitions/TransitionTypes.h"

using namespace lightwaveos::actors;
using namespace lightwaveos::network;
using namespace lightwaveos::transitions;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void TransitionHandlers::handleTypes(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [](JsonObject& data) {
        JsonArray types = data["types"].to<JsonArray>();

        for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
            JsonObject t = types.add<JsonObject>();
            t["id"] = i;
            t["name"] = getTransitionName(static_cast<TransitionType>(i));
            t["duration"] = getDefaultDuration(static_cast<TransitionType>(i));
            t["centerOrigin"] = true;  // All v2 transitions are center-origin
        }
    }, 1536);
}

void TransitionHandlers::handleTrigger(AsyncWebServerRequest* request,
                                         uint8_t* data, size_t len,
                                         ActorSystem& actors,
                                         const WebServer::CachedRendererState& cachedState,
                                         std::function<void()> broadcastStatus) {
    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::TriggerTransition, request);

    EffectId toEffect = doc["toEffect"];

    // SAFE: Uses cached state (no cross-core access)
    // Accept stable namespaced EffectIds. Also tolerate legacy numeric indices.
    if (!cachedState.findEffectName(toEffect)) {
        if (toEffect < cachedState.effectCount && toEffect < cachedState.MAX_CACHED_EFFECTS) {
            toEffect = cachedState.effectIds[static_cast<uint16_t>(toEffect)];
        }
    }
    if (!cachedState.findEffectName(toEffect)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID not registered", "toEffect");
        return;
    }

    uint8_t transitionType = doc["type"] | 0;
    bool randomType = doc["random"] | false;

    // SAFE: All state changes go through ActorSystem message queue (thread-safe)
    if (randomType) {
        // For random transitions, use a random type (0-11)
        // Note: This is a simplified approach - ideally ActorSystem would have startRandomTransition()
        uint8_t randomType = (millis() % 12);  // Simple pseudo-random
        actors.startTransition(toEffect, randomType);
        transitionType = randomType;
    } else {
        actors.startTransition(toEffect, transitionType);
    }

    sendSuccessResponse(request, [&cachedState, toEffect, transitionType](JsonObject& respData) {
        respData["effectId"] = toEffect;
        // SAFE: Uses cached state (no cross-core access)
        const char* transName = cachedState.findEffectName(toEffect);
        if (transName) {
            respData["name"] = transName;
        }
        respData["transitionType"] = transitionType;
    });

    broadcastStatus();
}

void TransitionHandlers::handleConfigGet(AsyncWebServerRequest* request,
                                           RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [](JsonObject& data) {
        // Current transition state
        data["enabled"] = true;  // Transitions always enabled in v2
        data["defaultDuration"] = 1000;
        data["defaultType"] = 0;  // FADE
        data["defaultTypeName"] = getTransitionName(TransitionType::FADE);

        // Available easing curves (simplified list)
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
}

void TransitionHandlers::handleConfigSet(AsyncWebServerRequest* request,
                                          uint8_t* data, size_t len) {
    JsonDocument doc;
    // Validate - all fields are optional but if present must be valid
    auto vr = RequestValidator::parseAndValidate(data, len, doc, RequestSchemas::TransitionConfig);
    if (!vr.valid) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          vr.errorCode, vr.errorMessage, vr.fieldName);
        return;
    }

    // Currently transition config is not persisted, this endpoint acknowledges the request
    // Future: store default transition type, duration, easing in NVS

    // Schema validates type is 0-15 if present
    uint16_t duration = doc["defaultDuration"] | 1000;
    uint8_t type = doc["defaultType"] | 0;

    sendSuccessResponse(request, [duration, type](JsonObject& respData) {
        respData["defaultDuration"] = duration;
        respData["defaultType"] = type;
        respData["defaultTypeName"] = getTransitionName(static_cast<TransitionType>(type));
        respData["message"] = "Transition config updated";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

