/**
 * @file ModifierHandlers.cpp
 * @brief REST API handlers for effect modifiers implementation
 */

#include "ModifierHandlers.h"
#include "core/actors/RendererNode.h"
#include "effects/modifiers/ModifierStack.h"
#include "effects/modifiers/SpeedModifier.h"
#include "effects/modifiers/IntensityModifier.h"
#include "effects/modifiers/ColorShiftModifier.h"
#include "effects/modifiers/MirrorModifier.h"
#include "effects/modifiers/GlitchModifier.h"
#include <ArduinoJson.h>

#define LW_LOG_TAG "ModifierHandlers"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

using namespace lightwaveos::effects::modifiers;

// Statically allocated modifier instances (owned by handlers)
// These are NOT freed - modifiers remain in memory for lifetime of firmware
// EXPORTED for WebSocket handlers via extern declarations
SpeedModifier* g_speedModifier = nullptr;
IntensityModifier* g_intensityModifier = nullptr;
ColorShiftModifier* g_colorShiftModifier = nullptr;
MirrorModifier* g_mirrorModifier = nullptr;
GlitchModifier* g_glitchModifier = nullptr;

void ModifierHandlers::handleAddModifier(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    nodes::RendererNode* renderer
) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Renderer not available");
        return;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON payload");
        return;
    }

    const char* typeStr = doc["type"];
    if (!typeStr) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "Missing 'type' field", "type");
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Modifier stack not available");
        return;
    }

    if (stack->isFull()) {
        sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                          ErrorCodes::STORAGE_FULL,
                          "Modifier stack full (max 8)");
        return;
    }

    // Create modifier based on type
    IEffectModifier* modifier = nullptr;

    if (strcmp(typeStr, "speed") == 0) {
        float multiplier = doc["multiplier"] | 1.0f;
        if (!g_speedModifier) {
            g_speedModifier = new SpeedModifier(multiplier);
        } else {
            g_speedModifier->setMultiplier(multiplier);
        }
        modifier = g_speedModifier;
    }
    else if (strcmp(typeStr, "intensity") == 0) {
        float baseIntensity = doc["baseIntensity"] | 1.0f;
        float depth = doc["depth"] | 0.5f;
        if (!g_intensityModifier) {
            // Use CONSTANT source by default (audio sources require integration)
            g_intensityModifier = new IntensityModifier(IntensitySource::CONSTANT, baseIntensity, depth);
        }
        modifier = g_intensityModifier;
    }
    else if (strcmp(typeStr, "color_shift") == 0) {
        uint8_t hueOffset = doc["hueOffset"] | 0;
        float rotationSpeed = doc["rotationSpeed"] | 10.0f;
        if (!g_colorShiftModifier) {
            g_colorShiftModifier = new ColorShiftModifier(ColorShiftMode::FIXED, hueOffset, rotationSpeed);
        } else {
            g_colorShiftModifier->setParameter("hueOffset", hueOffset);
        }
        modifier = g_colorShiftModifier;
    }
    else if (strcmp(typeStr, "mirror") == 0) {
        if (!g_mirrorModifier) {
            g_mirrorModifier = new MirrorModifier(MirrorMode::LEFT_TO_RIGHT);
        }
        modifier = g_mirrorModifier;
    }
    else if (strcmp(typeStr, "glitch") == 0) {
        float amount = doc["glitchAmount"] | 0.3f;
        if (!g_glitchModifier) {
            g_glitchModifier = new GlitchModifier(GlitchMode::PIXEL_FLIP, amount);
        } else {
            g_glitchModifier->setParameter("intensity", amount);
        }
        modifier = g_glitchModifier;
    }
    else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_TYPE,
                          "Unknown modifier type", "type");
        return;
    }

    // Add to stack
    plugins::EffectContext dummyCtx;
    dummyCtx.leds = renderer->getLedBuffer();
    dummyCtx.ledCount = 320;
    dummyCtx.centerPoint = 79;
    dummyCtx.brightness = renderer->getBrightness();
    dummyCtx.speed = renderer->getSpeed();

    if (!stack->add(modifier, dummyCtx)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          "Failed to add modifier to stack");
        return;
    }

    // Success response
    sendSuccessResponse(request, [stack, typeStr](JsonObject& data) {
        data["modifierId"] = stack->getCount() - 1;
        data["type"] = typeStr;
    });

    LW_LOGI("Added modifier: %s (stack count: %d)", typeStr, stack->getCount());
}

void ModifierHandlers::handleRemoveModifier(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    nodes::RendererNode* renderer
) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Renderer not available");
        return;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON payload");
        return;
    }

    const char* typeStr = doc["type"];
    if (!typeStr) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "Missing 'type' field", "type");
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Modifier stack not available");
        return;
    }

    // Map type string to enum
    ModifierType type;
    if (strcmp(typeStr, "speed") == 0) {
        type = ModifierType::SPEED;
    } else if (strcmp(typeStr, "intensity") == 0) {
        type = ModifierType::INTENSITY;
    } else if (strcmp(typeStr, "color_shift") == 0) {
        type = ModifierType::COLOR_SHIFT;
    } else if (strcmp(typeStr, "mirror") == 0) {
        type = ModifierType::MIRROR;
    } else if (strcmp(typeStr, "glitch") == 0) {
        type = ModifierType::GLITCH;
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_TYPE,
                          "Unknown modifier type", "type");
        return;
    }

    if (!stack->removeByType(type)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND,
                          "Modifier not found");
        return;
    }

    // Success response
    sendSuccessResponse(request, [typeStr](JsonObject& data) {
        data["type"] = typeStr;
    });

    LW_LOGI("Removed modifier: %s", typeStr);
}

void ModifierHandlers::handleListModifiers(
    AsyncWebServerRequest* request,
    nodes::RendererNode* renderer
) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Renderer not available");
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Modifier stack not available");
        return;
    }

    // Build response
    sendSuccessResponse(request, [stack](JsonObject& data) {
        JsonArray modifiers = data["modifiers"].to<JsonArray>();

        for (uint8_t i = 0; i < stack->getCount(); i++) {
            IEffectModifier* mod = stack->getModifier(i);
            if (mod) {
                JsonObject modObj = modifiers.add<JsonObject>();
                const ModifierMetadata& meta = mod->getMetadata();

                // Map ModifierType to string
                const char* typeStr = "unknown";
                switch (meta.type) {
                    case ModifierType::SPEED: typeStr = "speed"; break;
                    case ModifierType::INTENSITY: typeStr = "intensity"; break;
                    case ModifierType::COLOR_SHIFT: typeStr = "color_shift"; break;
                    case ModifierType::MIRROR: typeStr = "mirror"; break;
                    case ModifierType::GLITCH: typeStr = "glitch"; break;
                    default: break;
                }

                modObj["type"] = typeStr;
                modObj["name"] = meta.name;
                modObj["enabled"] = mod->isEnabled();
                modObj["preRender"] = mod->isPreRender();
            }
        }

        data["count"] = stack->getCount();
        data["maxCount"] = ModifierStack::MAX_MODIFIERS;
    });
}

void ModifierHandlers::handleClearModifiers(
    AsyncWebServerRequest* request,
    nodes::RendererNode* renderer
) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Renderer not available");
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Modifier stack not available");
        return;
    }

    stack->clear();

    // Success response
    sendSuccessResponse(request, [](JsonObject& data) {
        data["cleared"] = true;
    });

    LW_LOGI("Cleared all modifiers");
}

void ModifierHandlers::handleUpdateModifier(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    nodes::RendererNode* renderer
) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Renderer not available");
        return;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "Invalid JSON payload");
        return;
    }

    const char* typeStr = doc["type"];
    if (!typeStr) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "Missing 'type' field", "type");
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Modifier stack not available");
        return;
    }

    // Map type string to enum
    ModifierType type;
    if (strcmp(typeStr, "speed") == 0) {
        type = ModifierType::SPEED;
    } else if (strcmp(typeStr, "intensity") == 0) {
        type = ModifierType::INTENSITY;
    } else if (strcmp(typeStr, "color_shift") == 0) {
        type = ModifierType::COLOR_SHIFT;
    } else if (strcmp(typeStr, "mirror") == 0) {
        type = ModifierType::MIRROR;
    } else if (strcmp(typeStr, "glitch") == 0) {
        type = ModifierType::GLITCH;
    } else {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_TYPE,
                          "Unknown modifier type", "type");
        return;
    }

    IEffectModifier* modifier = stack->findByType(type);
    if (!modifier) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND,
                          "Modifier not found");
        return;
    }

    // Update parameters based on type
    bool updated = false;
    if (type == ModifierType::SPEED && doc.containsKey("multiplier")) {
        updated = modifier->setParameter("multiplier", doc["multiplier"]);
    } else if (type == ModifierType::INTENSITY && doc.containsKey("baseIntensity")) {
        updated = modifier->setParameter("baseIntensity", doc["baseIntensity"]);
    } else if (type == ModifierType::COLOR_SHIFT && doc.containsKey("hueOffset")) {
        updated = modifier->setParameter("hueOffset", doc["hueOffset"]);
    } else if (type == ModifierType::GLITCH && doc.containsKey("glitchAmount")) {
        updated = modifier->setParameter("intensity", doc["glitchAmount"]);
    }

    if (!updated) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_PARAMETER,
                          "No valid parameters to update");
        return;
    }

    // Success response
    sendSuccessResponse(request, [typeStr](JsonObject& data) {
        data["type"] = typeStr;
        data["updated"] = true;
    });

    LW_LOGI("Updated modifier: %s", typeStr);
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
