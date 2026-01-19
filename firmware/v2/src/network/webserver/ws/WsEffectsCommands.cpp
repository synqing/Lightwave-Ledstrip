/**
 * @file WsEffectsCommands.cpp
 * @brief WebSocket effects command handlers implementation
 *
 * NOTE: This is a partial implementation. Remaining effect commands
 * will be migrated from processWsCommand incrementally.
 */

#include "WsEffectsCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../codec/WsEffectsCodec.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../effects/PatternRegistry.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../effects/transitions/TransitionTypes.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleEffectsGetMetadata(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsGetMetadataDecodeResult decodeResult = codec::WsEffectsCodec::decodeGetMetadata(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::EffectsGetMetadataRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t effectId = req.effectId;
    
    // DEFENSIVE CHECK: Validate effectId before array access
    if (effectId != 255) {
        effectId = lightwaveos::network::validateEffectIdInRequest(effectId);
    }

    if (effectId == 255 || effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    String response = buildWsResponse("effects.metadata", requestId, [&ctx, effectId](JsonObject& data) {
        data["id"] = effectId;
        data["name"] = ctx.renderer->getEffectName(effectId);

        const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
        if (meta) {
            char familyName[32];
            PatternRegistry::getFamilyName(meta->family, familyName, sizeof(familyName));
            data["family"] = familyName;
            data["familyId"] = static_cast<uint8_t>(meta->family);
            if (meta->story) data["story"] = meta->story;
            if (meta->opticalIntent) data["opticalIntent"] = meta->opticalIntent;
            JsonArray tags = data["tags"].to<JsonArray>();
            if (meta->hasTag(PatternTags::STANDING)) tags.add("STANDING");
            if (meta->hasTag(PatternTags::TRAVELING)) tags.add("TRAVELING");
            if (meta->hasTag(PatternTags::MOIRE)) tags.add("MOIRE");
            if (meta->hasTag(PatternTags::DEPTH)) tags.add("DEPTH");
            if (meta->hasTag(PatternTags::SPECTRAL)) tags.add("SPECTRAL");
            if (meta->hasTag(PatternTags::CENTER_ORIGIN)) tags.add("CENTER_ORIGIN");
            if (meta->hasTag(PatternTags::DUAL_STRIP)) tags.add("DUAL_STRIP");
            if (meta->hasTag(PatternTags::PHYSICS)) tags.add("PHYSICS");
        } else {
            data["family"] = "Unknown";
            data["familyId"] = 255;
        }
        JsonObject properties = data["properties"].to<JsonObject>();
        properties["centerOrigin"] = true;
        properties["symmetricStrips"] = true;
        properties["paletteAware"] = true;
        properties["speedResponsive"] = true;
    });
    client->text(response);
}

static void handleEffectsGetCurrent(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("effects.current", requestId, [&ctx](JsonObject& data) {
        data["effectId"] = ctx.renderer->getCurrentEffect();
        data["name"] = ctx.renderer->getEffectName(ctx.renderer->getCurrentEffect());
        data["brightness"] = ctx.renderer->getBrightness();
        data["speed"] = ctx.renderer->getSpeed();
        data["paletteId"] = ctx.renderer->getPaletteIndex();
        data["hue"] = ctx.renderer->getHue();
        data["intensity"] = ctx.renderer->getIntensity();
        data["saturation"] = ctx.renderer->getSaturation();
        data["complexity"] = ctx.renderer->getComplexity();
        data["variation"] = ctx.renderer->getVariation();
    });
    client->text(response);
}

static void handleEffectsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsListDecodeResult decodeResult = codec::WsEffectsCodec::decodeList(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::EffectsListRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t page = req.page;
    uint8_t limit = req.limit;
    bool details = req.details;

    // Values already validated by codec (page >= 1, limit 1-50)

    uint8_t effectCount = ctx.renderer->getEffectCount();
    uint8_t startIdx = (page - 1) * limit;
    uint8_t endIdx = (startIdx + limit < effectCount) ? (startIdx + limit) : effectCount;

    String response = buildWsResponse("effects.list", requestId, [&ctx, effectCount, startIdx, endIdx, page, limit, details](JsonObject& data) {
        JsonArray effects = data["effects"].to<JsonArray>();
        for (uint8_t i = startIdx; i < endIdx; i++) {
            JsonObject effect = effects.add<JsonObject>();
            effect["id"] = i;
            effect["name"] = ctx.renderer->getEffectName(i);
            if (details) {
                if (i <= 4) effect["category"] = "Classic";
                else if (i <= 7) effect["category"] = "Wave";
                else if (i <= 12) effect["category"] = "Physics";
                else effect["category"] = "Custom";
            }
        }
        JsonObject pagination = data["pagination"].to<JsonObject>();
        pagination["page"] = page;
        pagination["limit"] = limit;
        pagination["total"] = effectCount;
        pagination["pages"] = (effectCount + limit - 1) / limit;
    });
    client->text(response);
}

static void handleSetEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSetEffectDecodeResult decodeResult = codec::WsEffectsCodec::decodeSetEffect(root);

    if (!decodeResult.success) {
        // Legacy command doesn't send errors, just ignore invalid requests
        return;
    }

    // DEFENSIVE CHECK: Validate effectId against current effect count
    uint8_t effectId = lightwaveos::network::validateEffectIdInRequest(decodeResult.request.effectId);
    if (effectId < ctx.renderer->getEffectCount()) {
        ctx.actorSystem.setEffect(effectId);
        if (ctx.broadcastStatus) ctx.broadcastStatus();
    }
}

static void handleNextEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t current = ctx.renderer->getCurrentEffect();
    uint8_t next = (current + 1) % ctx.renderer->getEffectCount();
    ctx.actorSystem.setEffect(next);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handlePrevEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t current = ctx.renderer->getCurrentEffect();
    uint8_t prev = (current + ctx.renderer->getEffectCount() - 1) % ctx.renderer->getEffectCount();
    ctx.actorSystem.setEffect(prev);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleSetBrightness(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSetBrightnessDecodeResult decodeResult = codec::WsEffectsCodec::decodeSetBrightness(root);

    if (!decodeResult.success) {
        // Legacy command doesn't send errors, just ignore invalid requests
        return;
    }

    ctx.actorSystem.setBrightness(decodeResult.request.value);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleSetSpeed(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSetSpeedDecodeResult decodeResult = codec::WsEffectsCodec::decodeSetSpeed(root);

    if (!decodeResult.success) {
        // Legacy command doesn't send errors, just ignore invalid requests
        return;
    }

    // Range already validated by codec (1-50)
    ctx.actorSystem.setSpeed(decodeResult.request.value);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleSetPalette(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSetPaletteDecodeResult decodeResult = codec::WsEffectsCodec::decodeSetPalette(root);

    if (!decodeResult.success) {
        // Legacy command doesn't send errors, just ignore invalid requests
        return;
    }

    // DEFENSIVE CHECK: Validate paletteId before array access
    uint8_t paletteId = lightwaveos::network::validatePaletteIdInRequest(decodeResult.request.paletteId);
    ctx.actorSystem.setPalette(paletteId);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleEffectsSetCurrent(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSetCurrentDecodeResult decodeResult = codec::WsEffectsCodec::decodeSetCurrent(root);

    if (!decodeResult.success) {
        // Error: Cannot decode requestId from invalid JSON, use empty string
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, ""));
        return;
    }

    const codec::EffectsSetCurrentRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";

    // DEFENSIVE CHECK: Validate effectId against current effect count
    uint8_t effectId = lightwaveos::network::validateEffectIdInRequest(req.effectId);

    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    // Apply effect change (with or without transition)
    if (req.hasTransition && req.transitionType < static_cast<uint8_t>(lightwaveos::transitions::TransitionType::TYPE_COUNT)) {
        ctx.renderer->startTransition(effectId, req.transitionType);
    } else {
        ctx.actorSystem.setEffect(effectId);
    }

    if (ctx.broadcastStatus) ctx.broadcastStatus();

    String response = buildWsResponse("effects.changed", requestId, [&ctx, effectId, req](JsonObject& data) {
        data["effectId"] = effectId;
        data["name"] = ctx.renderer->getEffectName(effectId);
        data["transitionActive"] = req.hasTransition;
    });
    client->text(response);
}

static void handleEffectsParametersGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsParametersGetDecodeResult decodeResult = codec::WsEffectsCodec::decodeParametersGet(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::EffectsParametersGetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t effectId = (req.effectId == 255) ? ctx.renderer->getCurrentEffect() : req.effectId;

    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    plugins::IEffect* effect = ctx.renderer->getEffectInstance(effectId);
    String response = buildWsResponse("effects.parameters", requestId, [&ctx, effectId, effect](JsonObject& data) {
        data["effectId"] = effectId;
        data["name"] = ctx.renderer->getEffectName(effectId);
        data["hasParameters"] = (effect != nullptr && effect->getParameterCount() > 0);

        JsonArray params = data["parameters"].to<JsonArray>();
        if (!effect) {
            return;
        }

        uint8_t count = effect->getParameterCount();
        for (uint8_t i = 0; i < count; ++i) {
            const plugins::EffectParameter* param = effect->getParameter(i);
            if (!param) continue;
            JsonObject p = params.add<JsonObject>();
            p["name"] = param->name;
            p["displayName"] = param->displayName;
            p["min"] = param->minValue;
            p["max"] = param->maxValue;
            p["default"] = param->defaultValue;
            p["value"] = effect->getParameter(param->name);
        }
    });
    client->text(response);
}

static void handleEffectsParametersSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsParametersSetDecodeResult decodeResult = codec::WsEffectsCodec::decodeEffectsParametersSet(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::EffectsParametersSetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t effectId = (req.effectId == 255) ? ctx.renderer->getCurrentEffect() : req.effectId;

    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    plugins::IEffect* effect = ctx.renderer->getEffectInstance(effectId);
    if (!effect) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Effect has no parameters", requestId));
        return;
    }

    if (!req.hasParameters) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing parameters object", requestId));
        return;
    }

    // Parameters object is validated by codec, but we need JsonObject (mutable) for iteration
    // Note: JsonObjectConst can't be used in range-for, so we need to cast or handle differently
    // For now, we'll keep the original doc access for params iteration since codec validated presence
    JsonObject params = doc["parameters"].as<JsonObject>();
    String response = buildWsResponse("effects.parameters.changed", requestId,
                                      [&ctx, effectId, effect, params](JsonObject& data) {
        data["effectId"] = effectId;
        data["name"] = ctx.renderer->getEffectName(effectId);

        JsonArray queuedArr = data["queued"].to<JsonArray>();
        JsonArray failedArr = data["failed"].to<JsonArray>();

        for (JsonPair kv : params) {
            const char* key = kv.key().c_str();
            float value = kv.value().as<float>();
            bool known = false;
            uint8_t count = effect->getParameterCount();
            for (uint8_t i = 0; i < count; ++i) {
                const plugins::EffectParameter* param = effect->getParameter(i);
                if (param && strcmp(param->name, key) == 0) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                failedArr.add(key);
                continue;
            }
            if (ctx.renderer->enqueueEffectParameterUpdate(effectId, key, value)) {
                queuedArr.add(key);
            } else {
                failedArr.add(key);
            }
        }
    });
    client->text(response);
}

static void handleEffectsGetCategories(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("effects.categories", requestId, [](JsonObject& data) {
        JsonArray families = data["categories"].to<JsonArray>();
        
        for (uint8_t i = 0; i < 10; i++) {
            PatternFamily family = static_cast<PatternFamily>(i);
            uint8_t count = PatternRegistry::getFamilyCount(family);
            
            JsonObject familyObj = families.add<JsonObject>();
            familyObj["id"] = i;
            
            char familyName[32];
            PatternRegistry::getFamilyName(family, familyName, sizeof(familyName));
            familyObj["name"] = familyName;
            familyObj["count"] = count;
        }
        
        data["total"] = 10;
    });
    client->text(response);
}

static void handleEffectsGetByFamily(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsGetByFamilyDecodeResult decodeResult = codec::WsEffectsCodec::decodeGetByFamily(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::EffectsGetByFamilyRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    uint8_t familyId = req.familyId;

    // Range already validated by codec (0-9)
    
    PatternFamily family = static_cast<PatternFamily>(familyId);
    uint8_t patternIndices[128];
    uint8_t count = PatternRegistry::getPatternsByFamily(family, patternIndices, 128);
    
    String response = buildWsResponse("effects.byFamily", requestId, [familyId, patternIndices, count](JsonObject& data) {
        data["familyId"] = familyId;
        
        char familyName[32];
        PatternRegistry::getFamilyName(static_cast<PatternFamily>(familyId), familyName, sizeof(familyName));
        data["familyName"] = familyName;
        
        JsonArray effects = data["effects"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            effects.add(patternIndices[i]);
        }
        
        data["count"] = count;
    });
    client->text(response);
}

static void handleParametersGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("parameters", requestId, [&ctx](JsonObject& data) {
        data["brightness"] = ctx.renderer->getBrightness();
        data["speed"] = ctx.renderer->getSpeed();
        data["paletteId"] = ctx.renderer->getPaletteIndex();
        data["hue"] = ctx.renderer->getHue();
        data["intensity"] = ctx.renderer->getIntensity();
        data["saturation"] = ctx.renderer->getSaturation();
        data["complexity"] = ctx.renderer->getComplexity();
        data["variation"] = ctx.renderer->getVariation();
    });
    client->text(response);
}

static void handleParametersSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ParametersSetDecodeResult decodeResult = codec::WsEffectsCodec::decodeParametersSet(root);

    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, decodeResult.errorMsg, requestId));
        return;
    }

    const codec::ParametersSetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";

    bool updatedBrightness = false;
    bool updatedSpeed = false;
    bool updatedPalette = false;
    bool updatedIntensity = false;
    bool updatedSaturation = false;
    bool updatedComplexity = false;
    bool updatedVariation = false;
    bool updatedHue = false;

    if (req.hasBrightness) {
        ctx.actorSystem.setBrightness(req.brightness);
        updatedBrightness = true;
    }

    if (req.hasSpeed) {
        ctx.actorSystem.setSpeed(req.speed);
        updatedSpeed = true;
    }

    if (req.hasPaletteId) {
        ctx.actorSystem.setPalette(req.paletteId);
        updatedPalette = true;
    }

    if (req.hasIntensity) {
        ctx.actorSystem.setIntensity(req.intensity);
        updatedIntensity = true;
    }

    if (req.hasSaturation) {
        ctx.actorSystem.setSaturation(req.saturation);
        updatedSaturation = true;
    }

    if (req.hasComplexity) {
        ctx.actorSystem.setComplexity(req.complexity);
        updatedComplexity = true;
    }

    if (req.hasVariation) {
        ctx.actorSystem.setVariation(req.variation);
        updatedVariation = true;
    }

    if (req.hasHue) {
        ctx.actorSystem.setHue(req.hue);
        updatedHue = true;
    }

    if (ctx.broadcastStatus) ctx.broadcastStatus();

    String response = buildWsResponse("parameters.changed", requestId, [&ctx, updatedBrightness, updatedSpeed, updatedPalette, updatedIntensity, updatedSaturation, updatedComplexity, updatedVariation, updatedHue](JsonObject& data) {
        JsonArray updated = data["updated"].to<JsonArray>();
        if (updatedBrightness) updated.add("brightness");
        if (updatedSpeed) updated.add("speed");
        if (updatedPalette) updated.add("paletteId");
        if (updatedIntensity) updated.add("intensity");
        if (updatedSaturation) updated.add("saturation");
        if (updatedComplexity) updated.add("complexity");
        if (updatedVariation) updated.add("variation");
        if (updatedHue) updated.add("hue");

        JsonObject current = data["current"].to<JsonObject>();
        current["brightness"] = ctx.renderer->getBrightness();
        current["speed"] = ctx.renderer->getSpeed();
        current["paletteId"] = ctx.renderer->getPaletteIndex();
        current["hue"] = ctx.renderer->getHue();
        current["intensity"] = ctx.renderer->getIntensity();
        current["saturation"] = ctx.renderer->getSaturation();
        current["complexity"] = ctx.renderer->getComplexity();
        current["variation"] = ctx.renderer->getVariation();
    });
    client->text(response);
}

void registerWsEffectsCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("effects.getMetadata", handleEffectsGetMetadata);
    WsCommandRouter::registerCommand("effects.getCurrent", handleEffectsGetCurrent);
    WsCommandRouter::registerCommand("effects.list", handleEffectsList);
    WsCommandRouter::registerCommand("setEffect", handleSetEffect);
    WsCommandRouter::registerCommand("nextEffect", handleNextEffect);
    WsCommandRouter::registerCommand("prevEffect", handlePrevEffect);
    WsCommandRouter::registerCommand("setBrightness", handleSetBrightness);
    WsCommandRouter::registerCommand("setSpeed", handleSetSpeed);
    WsCommandRouter::registerCommand("setPalette", handleSetPalette);
    WsCommandRouter::registerCommand("effects.setCurrent", handleEffectsSetCurrent);
    WsCommandRouter::registerCommand("effects.parameters.get", handleEffectsParametersGet);
    WsCommandRouter::registerCommand("effects.parameters.set", handleEffectsParametersSet);
    WsCommandRouter::registerCommand("effects.getCategories", handleEffectsGetCategories);
    WsCommandRouter::registerCommand("effects.getByFamily", handleEffectsGetByFamily);
    WsCommandRouter::registerCommand("parameters.get", handleParametersGet);
    WsCommandRouter::registerCommand("parameters.set", handleParametersSet);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

