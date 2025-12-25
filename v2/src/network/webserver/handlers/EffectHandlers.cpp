#include "EffectHandlers.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../effects/PatternRegistry.h"
#include "../../../plugins/api/IEffect.h"
#include "../../RequestValidator.h"
#include "../../ApiResponse.h"
#include <cstring>

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;

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

    uint8_t effectCount = renderer->getEffectCount();

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
        if (limit > 50) limit = 50;
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

    // Capture values for lambda
    const int capturedPage = page;
    const int capturedLimit = limit;
    const int capturedTotal = total;
    const int capturedPages = pages;
    const int capturedStartIdx = startIdx;
    const int capturedEndIdx = endIdx;
    const int capturedCategoryFilter = categoryFilter;
    const bool capturedDetails = details;

    sendSuccessResponseLarge(request, [capturedPage, capturedLimit, capturedTotal,
                                       capturedPages, capturedStartIdx, capturedEndIdx,
                                       capturedCategoryFilter, capturedDetails, renderer](JsonObject& data) {
        // Add pagination object
        JsonObject pagination = data["pagination"].to<JsonObject>();
        pagination["page"] = capturedPage;
        pagination["limit"] = capturedLimit;
        pagination["total"] = capturedTotal;
        pagination["pages"] = capturedPages;

        // Helper lambda to get category info
        auto getCategoryId = [](uint8_t effectId) -> int {
            if (effectId <= 4) return 0;       // Classic
            else if (effectId <= 7) return 1;  // Wave
            else if (effectId <= 12) return 2; // Physics
            else return 3;                     // Custom
        };

        auto getCategoryName = [](int categoryId) -> const char* {
            switch (categoryId) {
                case 0: return "Classic";
                case 1: return "Wave";
                case 2: return "Physics";
                default: return "Custom";
            }
        };

        // Build effects array with pagination
        JsonArray effects = data["effects"].to<JsonArray>();

        if (capturedCategoryFilter >= 0) {
            int matchCount = 0;
            int addedCount = 0;

            for (uint8_t i = 0; i < renderer->getEffectCount(); i++) {
                int effectCategory = getCategoryId(i);
                if (effectCategory == capturedCategoryFilter) {
                    if (matchCount >= capturedStartIdx && addedCount < capturedLimit) {
                        JsonObject effect = effects.add<JsonObject>();
                        effect["id"] = i;
                        effect["name"] = renderer->getEffectName(i);
                        effect["category"] = getCategoryName(effectCategory);
                        effect["categoryId"] = effectCategory;

                        if (capturedDetails) {
                            JsonObject features = effect["features"].to<JsonObject>();
                            features["centerOrigin"] = true;
                            features["usesSpeed"] = true;
                            features["usesPalette"] = true;
                            features["zoneAware"] = (effectCategory != 2);
                        }
                        addedCount++;
                    }
                    matchCount++;
                }
            }
        } else {
            for (int i = capturedStartIdx; i < capturedEndIdx; i++) {
                JsonObject effect = effects.add<JsonObject>();
                effect["id"] = i;
                effect["name"] = renderer->getEffectName(i);
                int categoryId = getCategoryId(i);
                effect["category"] = getCategoryName(categoryId);
                effect["categoryId"] = categoryId;

                // Query IEffect metadata if available
                plugins::IEffect* ieffect = renderer->getEffectInstance(i);
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
                    // Map EffectCategory to string
                    const char* categoryNames[] = {
                        "UNCATEGORIZED", "FIRE", "WATER", "NATURE", "GEOMETRIC",
                        "QUANTUM", "SHOCKWAVE", "AMBIENT", "PARTY", "CUSTOM"
                    };
                    if ((uint8_t)meta.category < 10) {
                        effect["ieffectCategory"] = categoryNames[(uint8_t)meta.category];
                    }
                } else {
                    effect["isIEffect"] = false;
                }

                if (capturedDetails) {
                    JsonObject features = effect["features"].to<JsonObject>();
                    features["centerOrigin"] = true;
                    features["usesSpeed"] = true;
                    features["usesPalette"] = true;
                    features["zoneAware"] = (categoryId != 2);
                }
            }
        }

        JsonArray categories = data["categories"].to<JsonArray>();
        const char* categoryNames[] = {"Classic", "Wave", "Physics", "Custom"};
        for (int i = 0; i < 4; i++) {
            JsonObject cat = categories.add<JsonObject>();
            cat["id"] = i;
            cat["name"] = categoryNames[i];
        }
    }, 4096);
}

void EffectHandlers::handleCurrent(AsyncWebServerRequest* request, RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    sendSuccessResponse(request, [renderer](JsonObject& data) {
        uint8_t effectId = renderer->getCurrentEffect();
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

    uint8_t effectId = renderer->getCurrentEffect();
    if (request->hasParam("id")) {
        effectId = static_cast<uint8_t>(request->getParam("id")->value().toInt());
    }

    if (effectId >= renderer->getEffectCount()) {
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

    uint8_t effectId = doc["effectId"];
    if (effectId >= renderer->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    plugins::IEffect* effect = renderer->getEffectInstance(effectId);
    if (!effect) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Effect has no parameters");
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

void EffectHandlers::handleSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, ActorSystem& actors, RendererActor* renderer, std::function<void()> broadcastStatus) {
    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetEffect, request);

    uint8_t effectId = doc["effectId"];
    if (effectId >= renderer->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    bool useTransition = doc["transition"] | false;
    uint8_t transitionType = doc["transitionType"] | 0;

    if (useTransition) {
        renderer->startTransition(effectId, transitionType);
    } else {
        actors.setEffect(effectId);
    }

    sendSuccessResponse(request, [effectId, renderer](JsonObject& respData) {
        respData["effectId"] = effectId;
        respData["name"] = renderer->getEffectName(effectId);
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

    uint8_t effectId = request->getParam("id")->value().toInt();
    uint8_t effectCount = renderer->getEffectCount();

    if (effectId >= effectCount) {
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
