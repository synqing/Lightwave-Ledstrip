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
#include "../../../core/actors/NodeOrchestrator.h"
#include "../../../core/actors/RendererNode.h"
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
    const char* requestId = doc["requestId"] | "";
    uint8_t effectId = doc["effectId"] | 255;
    
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
    const char* requestId = doc["requestId"] | "";
    uint8_t page = doc["page"] | 1;
    uint8_t limit = doc["limit"] | 20;
    bool details = doc["details"] | false;

    if (page < 1) page = 1;
    if (limit < 1) limit = 1;
    if (limit > 50) limit = 50;

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
    uint8_t effectId = doc["effectId"] | 0;
    // DEFENSIVE CHECK: Validate effectId before array access
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);
    if (effectId < ctx.renderer->getEffectCount()) {
        ctx.orchestrator.setEffect(effectId);
        if (ctx.broadcastStatus) ctx.broadcastStatus();
    }
}

static void handleNextEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t current = ctx.renderer->getCurrentEffect();
    uint8_t next = (current + 1) % ctx.renderer->getEffectCount();
    ctx.orchestrator.setEffect(next);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handlePrevEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t current = ctx.renderer->getCurrentEffect();
    uint8_t prev = (current + ctx.renderer->getEffectCount() - 1) % ctx.renderer->getEffectCount();
    ctx.orchestrator.setEffect(prev);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleSetBrightness(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t value = doc["value"] | 128;
    ctx.orchestrator.setBrightness(value);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleSetSpeed(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t value = doc["value"] | 15;
    if (value >= 1 && value <= 50) {
        ctx.orchestrator.setSpeed(value);
        if (ctx.broadcastStatus) ctx.broadcastStatus();
    }
}

static void handleSetPalette(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint8_t paletteId = doc["paletteId"] | 0;
    // DEFENSIVE CHECK: Validate paletteId before array access
    paletteId = lightwaveos::network::validatePaletteIdInRequest(paletteId);
    ctx.orchestrator.setPalette(paletteId);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleEffectsSetCurrent(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t effectId = doc["effectId"] | 255;

    if (effectId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "effectId required", requestId));
        return;
    }
    
    // DEFENSIVE CHECK: Validate effectId before array access
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    bool useTransition = false;
    uint8_t transType = 0;
    uint16_t duration = 1000;

    if (doc.containsKey("transition")) {
        JsonObject trans = doc["transition"];
        useTransition = true;
        transType = trans["type"] | 0;
        duration = trans["duration"] | 1000;
    }

    if (useTransition && transType < static_cast<uint8_t>(lightwaveos::transitions::TransitionType::TYPE_COUNT)) {
        ctx.renderer->startTransition(effectId, transType);
    } else {
        ctx.orchestrator.setEffect(effectId);
    }

    if (ctx.broadcastStatus) ctx.broadcastStatus();

    String response = buildWsResponse("effects.changed", requestId, [&ctx, effectId, useTransition](JsonObject& data) {
        data["effectId"] = effectId;
        data["name"] = ctx.renderer->getEffectName(effectId);
        data["transitionActive"] = useTransition;
    });
    client->text(response);
}

static void handleEffectsParametersGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t effectId = doc["effectId"] | ctx.renderer->getCurrentEffect();

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
    const char* requestId = doc["requestId"] | "";
    uint8_t effectId = doc["effectId"] | ctx.renderer->getCurrentEffect();

    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    plugins::IEffect* effect = ctx.renderer->getEffectInstance(effectId);
    if (!effect) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Effect has no parameters", requestId));
        return;
    }

    if (!doc.containsKey("parameters") || !doc["parameters"].is<JsonObject>()) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing parameters object", requestId));
        return;
    }

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
    const char* requestId = doc["requestId"] | "";
    uint8_t familyId = doc["familyId"] | 255;
    
    if (familyId >= 10) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid familyId (0-9)", requestId));
        return;
    }
    
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
    const char* requestId = doc["requestId"] | "";

    bool updatedBrightness = false;
    bool updatedSpeed = false;
    bool updatedPalette = false;
    bool updatedIntensity = false;
    bool updatedSaturation = false;
    bool updatedComplexity = false;
    bool updatedVariation = false;
    bool updatedHue = false;

    if (doc.containsKey("brightness")) {
        uint8_t value = doc["brightness"] | 128;
        ctx.orchestrator.setBrightness(value);
        updatedBrightness = true;
    }

    if (doc.containsKey("speed")) {
        uint8_t value = doc["speed"] | 15;
        if (value >= 1 && value <= 50) {
            ctx.orchestrator.setSpeed(value);
            updatedSpeed = true;
        }
    }

    if (doc.containsKey("paletteId")) {
        uint8_t value = doc["paletteId"] | 0;
        ctx.orchestrator.setPalette(value);
        updatedPalette = true;
    }

    if (doc.containsKey("intensity")) {
        uint8_t value = doc["intensity"] | 128;
        ctx.orchestrator.setIntensity(value);
        updatedIntensity = true;
    }

    if (doc.containsKey("saturation")) {
        uint8_t value = doc["saturation"] | 255;
        ctx.orchestrator.setSaturation(value);
        updatedSaturation = true;
    }

    if (doc.containsKey("complexity")) {
        uint8_t value = doc["complexity"] | 128;
        ctx.orchestrator.setComplexity(value);
        updatedComplexity = true;
    }

    if (doc.containsKey("variation")) {
        uint8_t value = doc["variation"] | 0;
        ctx.orchestrator.setVariation(value);
        updatedVariation = true;
    }

    if (doc.containsKey("hue")) {
        uint8_t value = doc["hue"] | 0;
        ctx.orchestrator.setHue(value);
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

