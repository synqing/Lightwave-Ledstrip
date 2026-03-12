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
#include "../../WebServer.h"
#include "../../../codec/WsEffectsCodec.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../effects/PatternRegistry.h"
#include "../../../config/display_order.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../effects/transitions/TransitionTypes.h"
#include "../../../effects/enhancement/ColorCorrectionEngine.h"
#include "../../../config/persistence_trigger.h"
#include "../../../config/factory_presets.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

extern uint8_t g_factoryPresetIndex;  // defined in main.cpp

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::enhancement;

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
    EffectId effectId = req.effectId;

    // DEFENSIVE CHECK: Validate effectId before registry lookup
    if (effectId != INVALID_EFFECT_ID) {
        effectId = lightwaveos::network::validateEffectIdInRequest(effectId);
    }

    if (effectId == INVALID_EFFECT_ID || !ctx.renderer->isEffectRegistered(effectId)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
    const char* name = ctx.renderer->getEffectName(effectId);
    char familyNameBuf[32] = "Unknown";
    const char* familyName = familyNameBuf;
    uint8_t familyId = 255;
    const char* story = nullptr;
    const char* opticalIntent = nullptr;
    uint8_t tags = 0;

    if (meta) {
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
    EffectId effectId = ctx.renderer->getCurrentEffect();
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

    uint16_t effectCount = ctx.renderer->getEffectCount();
    uint16_t startIdx = static_cast<uint16_t>((page - 1U) * limit);
    if (startIdx > effectCount) {
        startIdx = effectCount;
    }
    uint16_t endIdx = static_cast<uint16_t>(startIdx + limit);
    if (endIdx > effectCount) {
        endIdx = effectCount;
    }
    uint16_t windowCount = static_cast<uint16_t>(endIdx - startIdx);

    // Keep working buffers aligned with request limit (1..50) instead of full registry size.
    static constexpr uint8_t kMaxListWindow = 50;
    if (windowCount > kMaxListWindow) {
        windowCount = kMaxListWindow;
    }
    const char* effectNames[kMaxListWindow] = {};
    EffectId effectIdArr[kMaxListWindow] = {};
    const char* categories[kMaxListWindow] = {};
    static const char* categoryClassic = "Classic";
    static const char* categoryWave = "Wave";
    static const char* categoryPhysics = "Physics";
    static const char* categoryCustom = "Custom";

    for (uint16_t i = 0; i < windowCount; ++i) {
        const uint16_t effectIdx = static_cast<uint16_t>(startIdx + i);
        EffectId eid = ctx.renderer->getEffectIdAt(effectIdx);
        effectIdArr[i] = eid;
        effectNames[i] = ctx.renderer->getEffectName(eid);
        if (details) {
            if (effectIdx <= 4) categories[i] = categoryClassic;
            else if (effectIdx <= 7) categories[i] = categoryWave;
            else if (effectIdx <= 12) categories[i] = categoryPhysics;
            else categories[i] = categoryCustom;
        } else {
            categories[i] = nullptr;
        }
    }
    
    String response = buildWsResponse("effects.list", requestId, [effectCount, windowCount, page, limit, details, effectNames, effectIdArr, categories](JsonObject& data) {
        codec::WsEffectsCodec::encodeList(effectCount, 0, windowCount, page, limit, details, effectNames, effectIdArr, categories, data);
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

    // DEFENSIVE CHECK: Validate effectId against registry
    EffectId effectId = lightwaveos::network::validateEffectIdInRequest(decodeResult.request.effectId);
    if (ctx.renderer->isEffectRegistered(effectId)) {
        ctx.actorSystem.setEffect(effectId);
        if (ctx.broadcastStatus) ctx.broadcastStatus();
    }
}

static void handleNextEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    EffectId current = ctx.renderer->getCurrentEffect();
    EffectId next = lightwaveos::getNextDisplay(current);
    ctx.actorSystem.setEffect(next);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handlePrevEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    EffectId current = ctx.renderer->getCurrentEffect();
    EffectId prev = lightwaveos::getPrevDisplay(current);
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

    // DEFENSIVE CHECK: Validate effectId against registry
    EffectId effectId = lightwaveos::network::validateEffectIdInRequest(req.effectId);

    if (!ctx.renderer->isEffectRegistered(effectId)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    // Apply effect change (with or without transition)
    if (req.hasTransition && req.transitionType < static_cast<uint8_t>(lightwaveos::transitions::TransitionType::TYPE_COUNT)) {
        // Route through ActorSystem message queue for thread safety (Core 0 -> Core 1)
        ctx.actorSystem.startTransition(effectId, req.transitionType);
    } else {
        ctx.actorSystem.setEffect(effectId);
    }

    g_externalNvsSaveRequest.store(true, std::memory_order_release);
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
    EffectId effectId = (req.effectId == INVALID_EFFECT_ID) ? ctx.renderer->getCurrentEffect() : req.effectId;

    if (effectId == INVALID_EFFECT_ID) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    plugins::IEffect* effect = ctx.renderer->getEffectInstance(effectId);
    const char* name = ctx.renderer->getEffectName(effectId);
    bool hasParameters = (effect != nullptr && effect->getParameterCount() > 0);
    
    // Collect parameter data into arrays
    // Note: Effects rarely have >16 parameters, caps stack usage
    static constexpr uint8_t MAX_EFFECT_PARAMS = 16;
    const char* paramNames[MAX_EFFECT_PARAMS];
    const char* paramDisplayNames[MAX_EFFECT_PARAMS];
    float paramMins[MAX_EFFECT_PARAMS];
    float paramMaxs[MAX_EFFECT_PARAMS];
    float paramDefaults[MAX_EFFECT_PARAMS];
    float paramValues[MAX_EFFECT_PARAMS];
    uint8_t paramCount = 0;

    if (effect) {
        paramCount = effect->getParameterCount();
        for (uint8_t i = 0; i < paramCount && i < MAX_EFFECT_PARAMS; ++i) {
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
    EffectId effectId = (req.effectId == INVALID_EFFECT_ID) ? ctx.renderer->getCurrentEffect() : req.effectId;

    if (effectId == INVALID_EFFECT_ID) {
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
    // Note: Effects rarely have >16 parameters, caps stack usage
    static constexpr uint8_t MAX_PARAM_KEYS = 16;
    const char* queuedKeys[MAX_PARAM_KEYS];
    const char* failedKeys[MAX_PARAM_KEYS];
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
            if (failedCount < MAX_PARAM_KEYS) {
                failedKeys[failedCount++] = key;
            }
            continue;
        }
        if (ctx.renderer->enqueueEffectParameterUpdate(effectId, key, value)) {
            if (queuedCount < MAX_PARAM_KEYS) {
                queuedKeys[queuedCount++] = key;
            }
        } else {
            if (failedCount < MAX_PARAM_KEYS) {
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
    
    String response = buildWsResponse("effects.categories", requestId, [](JsonObject& data) {
        const char* familyNames[10];
        uint8_t familyCounts[10];
        char familyNameBufs[10][32];

        for (uint8_t i = 0; i < 10; ++i) {
            PatternFamily family = static_cast<PatternFamily>(i);
            familyCounts[i] = PatternRegistry::getFamilyCount(family);
            PatternRegistry::getFamilyName(family, familyNameBufs[i], sizeof(familyNameBufs[i]));
            familyNames[i] = familyNameBufs[i];
        }
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
    EffectId patternIndices[128];
    uint16_t count = PatternRegistry::getPatternsByFamily(family, patternIndices, 128);
    
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

    // Trigger debounced NVS save for expression param persistence (D4 § 2.4)
    if (updatedIntensity || updatedSaturation || updatedComplexity ||
        updatedVariation || updatedHue) {
        g_externalNvsSaveRequest.store(true, std::memory_order_release);
    }

    if (ctx.broadcastStatus) ctx.broadcastStatus();

    // Collect updated keys into array
    const char* updatedKeys[8];
    uint8_t updatedCount = 0;
    if (updatedBrightness && updatedCount < 8) updatedKeys[updatedCount++] = "brightness";
    if (updatedSpeed && updatedCount < 8) updatedKeys[updatedCount++] = "speed";
    if (updatedPalette && updatedCount < 8) updatedKeys[updatedCount++] = "paletteId";
    if (updatedIntensity && updatedCount < 8) updatedKeys[updatedCount++] = "intensity";
    if (updatedSaturation && updatedCount < 8) updatedKeys[updatedCount++] = "saturation";
    if (updatedComplexity && updatedCount < 8) updatedKeys[updatedCount++] = "complexity";
    if (updatedVariation && updatedCount < 8) updatedKeys[updatedCount++] = "variation";
    if (updatedHue && updatedCount < 8) updatedKeys[updatedCount++] = "hue";

    String response = buildWsResponse("parameters.changed", requestId, [&ctx, updatedKeys, updatedCount](JsonObject& data) {
        codec::WsEffectsCodec::encodeParametersChanged(
            updatedKeys,
            updatedCount,
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

// ============================================================================
// Camera Mode - Macro for recording with Sony a7s3 / cinema cameras
// ============================================================================
// Caps brightness (no blown whites), enables gamma (no crushed blacks),
// reduces speed (less flicker), bumps fade (smoother transitions).
// Toggle on/off preserves and restores previous values.

static struct {
    bool active = false;
    uint8_t prevBrightness = 128;
    uint8_t prevSpeed = 25;
    uint8_t prevFadeAmount = 128;
    bool prevGammaEnabled = true;
    float prevGammaValue = 2.2f;
} s_cameraMode;

static constexpr uint8_t CAMERA_BRIGHTNESS_CAP = 180;
static constexpr float CAMERA_SPEED_FACTOR = 0.7f;
static constexpr float CAMERA_GAMMA_VALUE = 2.2f;
static constexpr uint8_t CAMERA_FADE_BUMP = 180;  // Higher = smoother

static void handleCameraModeSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    bool enabled = doc["enabled"] | false;

    if (enabled && !s_cameraMode.active) {
        // Save current values
        const auto& cached = ctx.webServer->getCachedRendererState();
        s_cameraMode.prevBrightness = cached.brightness;
        s_cameraMode.prevSpeed = cached.speed;
        auto& ccEngine = enhancement::ColorCorrectionEngine::getInstance();
        auto& ccCfg = ccEngine.getConfig();
        s_cameraMode.prevGammaEnabled = ccCfg.gammaEnabled;
        s_cameraMode.prevGammaValue = ccCfg.gammaValue;
        // prevFadeAmount: read from renderer if available, else use sensible default
        s_cameraMode.prevFadeAmount = 128;

        // Apply camera-friendly values
        uint8_t cappedBrightness = (cached.brightness > CAMERA_BRIGHTNESS_CAP)
            ? CAMERA_BRIGHTNESS_CAP : cached.brightness;
        ctx.actorSystem.setBrightness(cappedBrightness);

        uint8_t reducedSpeed = static_cast<uint8_t>(cached.speed * CAMERA_SPEED_FACTOR);
        if (reducedSpeed < 1) reducedSpeed = 1;
        ctx.actorSystem.setSpeed(reducedSpeed);

        ctx.actorSystem.setFadeAmount(CAMERA_FADE_BUMP);

        // Enable gamma correction
        enhancement::ColorCorrectionConfig newCfg = ccCfg;
        newCfg.gammaEnabled = true;
        newCfg.gammaValue = CAMERA_GAMMA_VALUE;
        ccEngine.setConfig(newCfg);

        s_cameraMode.active = true;
    } else if (!enabled && s_cameraMode.active) {
        // Restore previous values
        ctx.actorSystem.setBrightness(s_cameraMode.prevBrightness);
        ctx.actorSystem.setSpeed(s_cameraMode.prevSpeed);
        ctx.actorSystem.setFadeAmount(s_cameraMode.prevFadeAmount);

        auto& ccEngine = enhancement::ColorCorrectionEngine::getInstance();
        auto& ccCfg = ccEngine.getConfig();
        enhancement::ColorCorrectionConfig restoreCfg = ccCfg;
        restoreCfg.gammaEnabled = s_cameraMode.prevGammaEnabled;
        restoreCfg.gammaValue = s_cameraMode.prevGammaValue;
        ccEngine.setConfig(restoreCfg);

        s_cameraMode.active = false;
    }

    // Broadcast status so all clients see the change
    if (ctx.broadcastStatus) ctx.broadcastStatus();

    // Respond with current state
    String response = buildWsResponse("cameraMode.set", requestId, [](JsonObject& data) {
        data["enabled"] = s_cameraMode.active;
    });
    client->text(response);

    // Broadcast camera mode change to all clients
    if (ctx.ws) {
        JsonDocument broadcast;
        broadcast["type"] = "cameraMode.changed";
        JsonObject bData = broadcast["data"].to<JsonObject>();
        bData["enabled"] = s_cameraMode.active;
        String broadcastStr;
        serializeJson(broadcast, broadcastStr);
        ctx.ws->textAll(broadcastStr);
    }
}

// ── Factory Preset WS Commands (D4 § 2.8) ───────────────────────────────────

static void handleFactoryPresetsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    String response = buildWsResponse("factoryPresets.list", requestId, [](JsonObject& data) {
        data["activeIndex"] = g_factoryPresetIndex;
        JsonArray arr = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < FACTORY_PRESET_COUNT; ++i) {
            const auto& p = FACTORY_PRESETS[i];
            JsonObject obj = arr.add<JsonObject>();
            obj["index"] = i;
            obj["name"] = p.name;
            obj["effectId"] = static_cast<uint16_t>(p.effectId);
            obj["paletteIndex"] = p.paletteIndex;
        }
    });
    client->text(response);
}

static void handleFactoryPresetsLoad(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    if (!doc.containsKey("index")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'index' field", requestId));
        return;
    }

    uint8_t idx = doc["index"];
    if (idx >= FACTORY_PRESET_COUNT) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Index out of range (0-7)", requestId));
        return;
    }

    const auto& p = FACTORY_PRESETS[idx];
    ctx.actorSystem.setEffect(p.effectId);
    ctx.actorSystem.setPalette(p.paletteIndex);
    ctx.actorSystem.setHue(p.hue);
    ctx.actorSystem.setSaturation(p.saturation);
    ctx.actorSystem.setMood(p.mood);
    ctx.actorSystem.setIntensity(p.intensity);
    ctx.actorSystem.setComplexity(p.complexity);
    ctx.actorSystem.setVariation(p.variation);
    ctx.actorSystem.setFadeAmount(p.trails);
    g_factoryPresetIndex = idx;
    g_externalNvsSaveRequest.store(true, std::memory_order_release);

    String response = buildWsResponse("factoryPresets.loaded", requestId, [idx, &p](JsonObject& data) {
        data["index"] = idx;
        data["name"] = p.name;
    });
    client->text(response);

    // Broadcast preset change to all connected clients
    if (ctx.ws) {
        JsonDocument broadcast;
        broadcast["type"] = "factoryPresets.changed";
        JsonObject bData = broadcast["data"].to<JsonObject>();
        bData["activeIndex"] = idx;
        bData["name"] = p.name;
        String broadcastStr;
        serializeJson(broadcast, broadcastStr);
        ctx.ws->textAll(broadcastStr);
    }
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

static void handleCameraModeGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    String response = buildWsResponse("cameraMode.get", requestId, [](JsonObject& data) {
        data["enabled"] = s_cameraMode.active;
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
    WsCommandRouter::registerCommand("cameraMode.set", handleCameraModeSet);
    WsCommandRouter::registerCommand("cameraMode.get", handleCameraModeGet);
    WsCommandRouter::registerCommand("factoryPresets.list", handleFactoryPresetsList);
    WsCommandRouter::registerCommand("factoryPresets.load", handleFactoryPresetsLoad);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
