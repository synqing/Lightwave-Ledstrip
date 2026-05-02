#include "EffectHandlers.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../effects/PatternRegistry.h"
#include "../../../plugins/api/IEffect.h"
#include "../../RequestValidator.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"  // For CachedRendererState
#include "../../../config/persistence_trigger.h"
#include <cstring>

#undef LW_LOG_TAG
#define LW_LOG_TAG "EffectH"
#include "../../../utils/Log.h"

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using lightwaveos::network::WebServer;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void EffectHandlers::registerRoutes(HttpRouteRegistry& registry) {
    // To be implemented when routes are migrated
    (void)registry;
}

void EffectHandlers::handleList(AsyncWebServerRequest* request, RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    uint16_t effectCount = renderer->getEffectCount();

    // Parse pagination query parameters
    int page = 1;
    int limit = 20;
    int categoryFilter = -1;  // -1 = no filter
    bool details = false;

    // V2 API uses "offset" instead of "page"
    if (request->hasParam("offset")) {
        int offset = request->getParam("offset")->value().toInt();
        if (offset < 0) offset = 0;
        page = (offset / limit) + 1;
    } else if (request->hasParam("page")) {
        page = request->getParam("page")->value().toInt();
        if (page < 1) page = 1;
    }
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > 150) limit = 150;
        if (request->hasParam("offset") && !request->hasParam("page")) {
            int offset = request->getParam("offset")->value().toInt();
            if (offset < 0) offset = 0;
            page = (offset / limit) + 1;
        }
    }
    if (request->hasParam("category")) {
        categoryFilter = request->getParam("category")->value().toInt();
    }
    if (request->hasParam("details")) {
        String detailsStr = request->getParam("details")->value();
        details = (detailsStr == "true" || detailsStr == "1");
    }

    // Calculate pagination values
    int total = effectCount;
    int pages = (total + limit - 1) / limit;  // Ceiling division
    if (page > pages && pages > 0) page = pages;  // Clamp page to max
    int startIdx = (page - 1) * limit;
    int endIdx = startIdx + limit;
    if (endIdx > total) endIdx = total;

    // Calculate offset (for V2 API compatibility) - use startIdx as the actual offset
    int offset = startIdx;

    // Helper lambdas — defined once and reused for both the filtered and
    // unfiltered code paths.
    auto getCategoryId = [](EffectId eid) -> int {
        const PatternMetadata* meta = PatternRegistry::getPatternMetadata(eid);
        if (!meta) return 3;  // Custom
        switch (meta->family) {
            case PatternFamily::FLUID_PLASMA: return 0;   // Classic
            case PatternFamily::INTERFERENCE: return 1;   // Wave
            case PatternFamily::GEOMETRIC:
            case PatternFamily::PHYSICS_BASED:
            case PatternFamily::MATHEMATICAL: return 2;   // Physics
            default: return 3;                            // Custom
        }
    };

    auto getCategoryName = [](int categoryId) -> const char* {
        switch (categoryId) {
            case 0: return "Classic";
            case 1: return "Wave";
            case 2: return "Physics";
            default: return "Custom";
        }
    };

    // -----------------------------------------------------------------
    // Streamed JSON construction.
    //
    // Heap-fragmentation fix (K1 V2 internal heap shed): the previous
    // implementation built a single JsonDocument holding the entire
    // effects[] array (~22.5 KB worst case for limit=200) and serialised
    // it into a String, which doubled to ~45 KB during concat. On a
    // device with ~21 KB internal heap at boot this guaranteed
    // fragmentation and latched the heap-shed at largest-block < 8 KB.
    //
    // Now we stream the response directly: the envelope is printed to
    // the AsyncResponseStream, then each effect entry is built into a
    // small temporary JsonDocument, serialised straight to the stream,
    // and freed before the next entry. Peak heap during the loop is
    // bounded by ONE entry (~150-300 bytes) plus the underlying cbuf
    // which grows to roughly the response length but does so
    // incrementally rather than via a doubling String.
    // -----------------------------------------------------------------

    AsyncResponseStream* response =
        request->beginResponseStream("application/json", 4096);

    response->print("{\"success\":true,\"data\":{");

    // Flat pagination fields for V2 API compatibility (V2EffectsList).
    response->printf("\"total\":%d,\"offset\":%d,\"limit\":%d,",
                     total, offset, limit);

    // Pagination object for backward compatibility.
    response->printf("\"pagination\":{\"page\":%d,\"limit\":%d,\"total\":%d,\"pages\":%d},",
                     page, limit, total, pages);

    response->print("\"effects\":[");

    int writtenCount = 0;
    if (categoryFilter >= 0) {
        int matchCount = 0;
        int addedCount = 0;
        for (uint16_t i = 0; i < renderer->getEffectCount(); i++) {
            EffectId eid = renderer->getEffectIdAt(i);
            int effectCategory = getCategoryId(eid);
            if (effectCategory != categoryFilter) continue;
            if (matchCount >= startIdx && addedCount < limit) {
                if (writtenCount > 0) response->print(",");
                JsonDocument effect;
                effect["id"] = eid;
                effect["name"] = renderer->getEffectName(eid);
                effect["category"] = getCategoryName(effectCategory);
                effect["categoryId"] = effectCategory;

                if (details) {
                    JsonObject features = effect["features"].to<JsonObject>();
                    features["centerOrigin"] = true;
                    features["usesSpeed"] = true;
                    features["usesPalette"] = true;
                    features["zoneAware"] = (effectCategory != 2);
                }
                serializeJson(effect, *response);
                addedCount++;
                writtenCount++;
            }
            matchCount++;
        }
    } else {
        for (int i = startIdx; i < endIdx; i++) {
            EffectId eid = renderer->getEffectIdAt(i);
            if (writtenCount > 0) response->print(",");

            JsonDocument effect;
            effect["id"] = eid;
            effect["name"] = renderer->getEffectName(eid);
            int categoryId = getCategoryId(eid);
            effect["category"] = getCategoryName(categoryId);
            effect["categoryId"] = categoryId;
            effect["isAudioReactive"] = PatternRegistry::isAudioReactive(eid);
            effect["isExperimental"] = PatternRegistry::isExperimental(eid);

            // Query IEffect metadata if available.
            plugins::IEffect* ieffect = renderer->getEffectInstance(eid);
            if (ieffect) {
                effect["isIEffect"] = true;
                const plugins::EffectMetadata& meta = ieffect->getMetadata();
                if (meta.description) {
                    effect["description"] = meta.description;
                }
                effect["version"] = meta.version;
                if (meta.author) {
                    effect["author"] = meta.author;
                }
                // Map EffectCategory to string.
                const char* metaCategoryNames[] = {
                    "UNCATEGORIZED", "FIRE", "WATER", "NATURE", "GEOMETRIC",
                    "QUANTUM", "SHOCKWAVE", "AMBIENT", "PARTY", "CUSTOM"
                };
                if ((uint8_t)meta.category < 10) {
                    effect["ieffectCategory"] = metaCategoryNames[(uint8_t)meta.category];
                }
            } else {
                effect["isIEffect"] = false;
            }

            if (details) {
                JsonObject features = effect["features"].to<JsonObject>();
                features["centerOrigin"] = true;
                features["usesSpeed"] = true;
                features["usesPalette"] = true;
                features["zoneAware"] = (categoryId != 2);
            }
            serializeJson(effect, *response);
            writtenCount++;
        }
    }

    response->print("],");

    // Categories — fixed shape, four entries; safe to write inline.
    response->print("\"categories\":[");
    const char* categoryNames[] = {"Classic", "Wave", "Physics", "Custom"};
    for (int i = 0; i < 4; i++) {
        if (i > 0) response->print(",");
        response->printf("{\"id\":%d,\"name\":\"%s\"}", i, categoryNames[i]);
    }
    response->print("],");

    // count field (number of effects emitted in this response).
    response->printf("\"count\":%d", writtenCount);

    // Close data, add timestamp + version, close envelope.
    response->printf("},\"timestamp\":%lu,\"version\":\"%s\"}",
                     (unsigned long)millis(), API_VERSION);

    request->send(response);
}

void EffectHandlers::handleCurrent(AsyncWebServerRequest* request, RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [renderer](JsonObject& data) {
        EffectId effectId = renderer->getCurrentEffect();
        data["effectId"] = effectId;
        data["name"] = renderer->getEffectName(effectId);
        data["brightness"] = renderer->getBrightness();
        data["speed"] = renderer->getSpeed();
        data["paletteId"] = renderer->getPaletteIndex();
        data["hue"] = renderer->getHue();
        data["intensity"] = renderer->getIntensity();
        data["saturation"] = renderer->getSaturation();
        data["complexity"] = renderer->getComplexity();
        data["variation"] = renderer->getVariation();

        // Include IEffect metadata if available
        plugins::IEffect* ieffect = renderer->getEffectInstance(effectId);
        if (ieffect) {
            data["isIEffect"] = true;
            const plugins::EffectMetadata& meta = ieffect->getMetadata();
            if (meta.description) {
                data["description"] = meta.description;
            }
            data["version"] = meta.version;
        } else {
            data["isIEffect"] = false;
        }
    });
}

void EffectHandlers::handleParametersGet(AsyncWebServerRequest* request, RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    EffectId effectId = renderer->getCurrentEffect();
    if (request->hasParam("id")) {
        effectId = static_cast<EffectId>(request->getParam("id")->value().toInt());
    }

    if (effectId == INVALID_EFFECT_ID) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    plugins::IEffect* effect = renderer->getEffectInstance(effectId);

    sendSuccessResponse(request, [renderer, effectId, effect](JsonObject& data) {
        data["effectId"] = effectId;
        data["name"] = renderer->getEffectName(effectId);
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
}

void EffectHandlers::handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON payload");
        return;
    }

    if (!doc.containsKey("effectId")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing effectId", "effectId");
        return;
    }

    EffectId effectId = doc["effectId"];
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    plugins::IEffect* effect = renderer->getEffectInstance(effectId);
    if (!effect) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Effect not found or has no parameters");
        return;
    }

    if (!doc.containsKey("parameters") || !doc["parameters"].is<JsonObject>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing parameters object", "parameters");
        return;
    }

    JsonObject params = doc["parameters"].as<JsonObject>();

    sendSuccessResponse(request, [effectId, renderer, effect, params](JsonObject& data) {
        data["effectId"] = effectId;
        data["name"] = renderer->getEffectName(effectId);

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
            if (renderer->enqueueEffectParameterUpdate(effectId, key, value)) {
                queuedArr.add(key);
            } else {
                failedArr.add(key);
            }
        }
    });
}

void EffectHandlers::handleSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, ActorSystem& actors, const WebServer::CachedRendererState& cachedState, std::function<void()> broadcastStatus) {
    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetEffect, request);

    EffectId effectId = doc["effectId"];
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    bool useTransition = doc["transition"] | false;
    uint8_t transitionType = doc["transitionType"] | 0;

    // SAFE: All state changes go through ActorSystem message queue (thread-safe)
    bool dispatched = false;
    if (useTransition) {
        dispatched = actors.startTransition(effectId, transitionType);
    } else {
        dispatched = actors.setEffect(effectId);
    }

    if (!dispatched) {
        LW_LOGW("REST setEffect rejected - queue saturated");
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::RATE_LIMITED, "Queue saturated");
        return;
    }

    g_externalNvsSaveRequest.store(true, std::memory_order_release);

    sendSuccessResponse(request, [effectId](JsonObject& respData) {
        respData["effectId"] = effectId;
        const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
        if (meta) {
            respData["name"] = meta->name;
        }
    });

    if (broadcastStatus) {
        broadcastStatus();
    }
}

void EffectHandlers::handleMetadata(AsyncWebServerRequest* request, RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    if (!request->hasParam("id")) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
        return;
    }

    EffectId effectId = static_cast<EffectId>(request->getParam("id")->value().toInt());
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    if (effectId == INVALID_EFFECT_ID) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "id");
        return;
    }

    sendSuccessResponse(request, [effectId, renderer](JsonObject& data) {
        data["id"] = effectId;
        data["name"] = renderer->getEffectName(effectId);

        // Query IEffect metadata if available
        plugins::IEffect* ieffect = renderer->getEffectInstance(effectId);
        if (ieffect) {
            data["isIEffect"] = true;
            const plugins::EffectMetadata& ieMeta = ieffect->getMetadata();
            if (ieMeta.description) {
                data["description"] = ieMeta.description;
            }
            data["version"] = ieMeta.version;
            if (ieMeta.author) {
                data["author"] = ieMeta.author;
            }
            // Map EffectCategory to string
            const char* categoryNames[] = {
                "UNCATEGORIZED", "FIRE", "WATER", "NATURE", "GEOMETRIC",
                "QUANTUM", "SHOCKWAVE", "AMBIENT", "PARTY", "CUSTOM"
            };
            if ((uint8_t)ieMeta.category < 10) {
                data["ieffectCategory"] = categoryNames[(uint8_t)ieMeta.category];
            }
        } else {
            data["isIEffect"] = false;
        }

        const PatternMetadata* meta = PatternRegistry::getPatternMetadata(effectId);
        if (meta) {
            char familyName[32];
            PatternRegistry::getFamilyName(meta->family, familyName, sizeof(familyName));
            data["family"] = familyName;
            data["familyId"] = static_cast<uint8_t>(meta->family);
            
            if (meta->story) {
                data["story"] = meta->story;
            }
            if (meta->opticalIntent) {
                data["opticalIntent"] = meta->opticalIntent;
            }
            
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

        data["isExperimental"] = PatternRegistry::isExperimental(effectId);

        JsonObject properties = data["properties"].to<JsonObject>();
        properties["centerOrigin"] = true;
        properties["symmetricStrips"] = true;
        properties["paletteAware"] = true;
        properties["speedResponsive"] = true;

        JsonObject recommended = data["recommended"].to<JsonObject>();
        recommended["brightness"] = 180;
        recommended["speed"] = 15;
    });
}

void EffectHandlers::handleFamilies(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        JsonArray families = data["families"].to<JsonArray>();
        
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
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
