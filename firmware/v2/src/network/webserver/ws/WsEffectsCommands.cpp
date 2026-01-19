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

    const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
    const char* name = ctx.renderer->getEffectName(effectId);
    const char* familyName = "Unknown";
    uint8_t familyId = 255;
    const char* story = nullptr;
    const char* opticalIntent = nullptr;
    uint8_t tags = 0;
    
    if (meta) {
        char familyNameBuf[32];
        PatternRegistry::getFamilyName(meta->family, familyNameBuf, sizeof(familyNameBuf));
        familyName = familyNameBuf;
        familyId = static_cast<uint8_t>(meta->family);
        story = meta->story;
        opticalIntent = meta->opticalIntent;
        tags = meta->tags;
    }
    
    String response = buildWsResponse("effects.metadata", requestId, [effectId, name, familyName, familyId, story, opticalIntent, tags](JsonObject& data) {
        codec::WsEffectsCodec::encodeMetadata(effectId, name, familyName, familyId, story, opticalIntent, tags, data);
    });
    client->text(response);
}

static void handleEffectsGetCurrent(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSimpleDecodeResult decodeResult = codec::WsEffectsCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    uint8_t effectId = ctx.renderer->getCurrentEffect();
    const char* name = ctx.renderer->getEffectName(effectId);
    
    String response = buildWsResponse("effects.current", requestId, [effectId, name, &ctx](JsonObject& data) {
        codec::WsEffectsCodec::encodeGetCurrent(
            effectId,
            name,
            ctx.renderer->getBrightness(),
            ctx.renderer->getSpeed(),
            ctx.renderer->getPaletteIndex(),
            ctx.renderer->getHue(),
            ctx.renderer->getIntensity(),
            ctx.renderer->getSaturation(),
            ctx.renderer->getComplexity(),
            ctx.renderer->getVariation(),
            data
        );
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

    // Collect effect names and categories into arrays
    const char* effectNames[128];
    const char* categories[128];
    for (uint8_t i = startIdx; i < endIdx; i++) {
        effectNames[i] = ctx.renderer->getEffectName(i);
        if (details) {
            if (i <= 4) categories[i] = "Classic";
            else if (i <= 7) categories[i] = "Wave";
            else if (i <= 12) categories[i] = "Physics";
            else categories[i] = "Custom";
        } else {
            categories[i] = nullptr;
        }
    }
    
    String response = buildWsResponse("effects.list", requestId, [effectCount, startIdx, endIdx, page, limit, details, effectNames, categories](JsonObject& data) {
        codec::WsEffectsCodec::encodeList(effectCount, startIdx, endIdx, page, limit, details, effectNames, categories, data);
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

    const char* name = ctx.renderer->getEffectName(effectId);
    String response = buildWsResponse("effects.changed", requestId, [effectId, name, req](JsonObject& data) {
        codec::WsEffectsCodec::encodeChanged(effectId, name, req.hasTransition, data);
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
    const char* name = ctx.renderer->getEffectName(effectId);
    bool hasParameters = (effect != nullptr && effect->getParameterCount() > 0);
    
    // Collect parameter data into arrays
    const char* paramNames[64];
    const char* paramDisplayNames[64];
    float paramMins[64];
    float paramMaxs[64];
    float paramDefaults[64];
    float paramValues[64];
    uint8_t paramCount = 0;
    
    if (effect) {
        paramCount = effect->getParameterCount();
        for (uint8_t i = 0; i < paramCount && i < 64; ++i) {
            const plugins::EffectParameter* param = effect->getParameter(i);
            if (!param) continue;
            paramNames[i] = param->name;
            paramDisplayNames[i] = param->displayName;
            paramMins[i] = param->minValue;
            paramMaxs[i] = param->maxValue;
            paramDefaults[i] = param->defaultValue;
            paramValues[i] = effect->getParameter(param->name);
        }
    }
    
    String response = buildWsResponse("effects.parameters", requestId, [effectId, name, hasParameters, paramNames, paramDisplayNames, paramMins, paramMaxs, paramDefaults, paramValues, paramCount](JsonObject& data) {
        codec::WsEffectsCodec::encodeParametersGet(effectId, name, hasParameters, paramNames, paramDisplayNames, paramMins, paramMaxs, paramDefaults, paramValues, paramCount, data);
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

    // Iterate over JsonObjectConst (ArduinoJson v7 supports range-for on JsonObjectConst)
    const char* queuedKeys[64];
    const char* failedKeys[64];
    uint8_t queuedCount = 0;
    uint8_t failedCount = 0;
    
    for (JsonPairConst kv : req.parameters) {
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
            if (failedCount < 64) {
                failedKeys[failedCount++] = key;
            }
            continue;
        }
        if (ctx.renderer->enqueueEffectParameterUpdate(effectId, key, value)) {
            if (queuedCount < 64) {
                queuedKeys[queuedCount++] = key;
            }
        } else {
            if (failedCount < 64) {
                failedKeys[failedCount++] = key;
            }
        }
    }
    
    const char* name = ctx.renderer->getEffectName(effectId);
    String response = buildWsResponse("effects.parameters.changed", requestId, [effectId, name, queuedKeys, queuedCount, failedKeys, failedCount](JsonObject& data) {
        codec::WsEffectsCodec::encodeParametersSetChanged(effectId, name, queuedKeys, queuedCount, failedKeys, failedCount, data);
    });
    client->text(response);
}

static void handleEffectsGetCategories(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSimpleDecodeResult decodeResult = codec::WsEffectsCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    // Collect family data into arrays
    const char* familyNames[10];
    uint8_t familyCounts[10];
    for (uint8_t i = 0; i < 10; i++) {
        PatternFamily family = static_cast<PatternFamily>(i);
        familyCounts[i] = PatternRegistry::getFamilyCount(family);
        
        char familyNameBuf[32];
        PatternRegistry::getFamilyName(family, familyNameBuf, sizeof(familyNameBuf));
        familyNames[i] = familyNameBuf;
    }
    
    String response = buildWsResponse("effects.categories", requestId, [familyNames, familyCounts](JsonObject& data) {
        codec::WsEffectsCodec::encodeCategories(familyNames, familyCounts, 10, data);
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
    
    char familyNameBuf[32];
    PatternRegistry::getFamilyName(static_cast<PatternFamily>(familyId), familyNameBuf, sizeof(familyNameBuf));
    const char* familyName = familyNameBuf;
    
    String response = buildWsResponse("effects.byFamily", requestId, [familyId, familyName, patternIndices, count](JsonObject& data) {
        codec::WsEffectsCodec::encodeByFamily(familyId, familyName, patternIndices, count, data);
    });
    client->text(response);
}

static void handleParametersGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (single canonical JSON parser)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::EffectsSimpleDecodeResult decodeResult = codec::WsEffectsCodec::decodeSimple(root);
    
    const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
    
    String response = buildWsResponse("parameters", requestId, [&ctx](JsonObject& data) {
        codec::WsEffectsCodec::encodeGlobalParametersGet(
            ctx.renderer->getBrightness(),
            ctx.renderer->getSpeed(),
            ctx.renderer->getPaletteIndex(),
            ctx.renderer->getHue(),
            ctx.renderer->getIntensity(),
            ctx.renderer->getSaturation(),
            ctx.renderer->getComplexity(),
            ctx.renderer->getVariation(),
            data
        );
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

