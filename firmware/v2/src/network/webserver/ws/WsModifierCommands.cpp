/**
 * @file WsModifierCommands.cpp
 * @brief WebSocket effect modifier command handlers implementation
 */

#include "WsModifierCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/RendererNode.h"
#include "../../../effects/modifiers/ModifierStack.h"
#include "../../../effects/modifiers/SpeedModifier.h"
#include "../../../effects/modifiers/IntensityModifier.h"
#include "../../../effects/modifiers/ColorShiftModifier.h"
#include "../../../effects/modifiers/MirrorModifier.h"
#include "../../../effects/modifiers/GlitchModifier.h"
#include "../../../effects/modifiers/BlurModifier.h"
#include "../../../effects/modifiers/TrailModifier.h"
#include "../../../effects/modifiers/SaturationModifier.h"
#include "../../../effects/modifiers/StrobeModifier.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

#define LW_LOG_TAG "WsModifierCommands"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {

// EXTERN DECLARATIONS - Reuse REST API modifier instances
// These are defined in ModifierHandlers.cpp (handlers namespace)
namespace handlers {
extern lightwaveos::effects::modifiers::SpeedModifier* g_speedModifier;
extern lightwaveos::effects::modifiers::IntensityModifier* g_intensityModifier;
extern lightwaveos::effects::modifiers::ColorShiftModifier* g_colorShiftModifier;
extern lightwaveos::effects::modifiers::MirrorModifier* g_mirrorModifier;
extern lightwaveos::effects::modifiers::GlitchModifier* g_glitchModifier;
extern lightwaveos::effects::modifiers::BlurModifier* g_blurModifier;
extern lightwaveos::effects::modifiers::TrailModifier* g_trailModifier;
extern lightwaveos::effects::modifiers::SaturationModifier* g_saturationModifier;
extern lightwaveos::effects::modifiers::StrobeModifier* g_strobeModifier;
} // namespace handlers

namespace ws {

using namespace lightwaveos::effects::modifiers;

static void handleModifiersAdd(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    const char* typeStr = doc["type"];

    if (!typeStr) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'type' field", requestId));
        return;
    }

    nodes::RendererNode* renderer = ctx.renderer;
    if (!renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Modifier stack not available", requestId));
        return;
    }

    if (stack->isFull()) {
        client->text(buildWsError(ErrorCodes::STORAGE_FULL, "Modifier stack full (max 8)", requestId));
        return;
    }

    // Create modifier based on type
    IEffectModifier* modifier = nullptr;

    if (strcmp(typeStr, "speed") == 0) {
        float multiplier = doc["multiplier"] | 1.0f;
        if (multiplier < 0.1f || multiplier > 3.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Multiplier must be 0.1-3.0", requestId));
            return;
        }
        if (!handlers::g_speedModifier) {
            handlers::g_speedModifier = new SpeedModifier(multiplier);
        } else {
            handlers::g_speedModifier->setMultiplier(multiplier);
        }
        modifier = handlers::g_speedModifier;
    }
    else if (strcmp(typeStr, "intensity") == 0) {
        float baseIntensity = doc["baseIntensity"] | 1.0f;
        float depth = doc["depth"] | 0.5f;
        if (baseIntensity < 0.0f || baseIntensity > 2.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "baseIntensity must be 0.0-2.0", requestId));
            return;
        }
        if (!handlers::g_intensityModifier) {
            handlers::g_intensityModifier = new IntensityModifier(IntensitySource::CONSTANT, baseIntensity, depth);
        }
        modifier = handlers::g_intensityModifier;
    }
    else if (strcmp(typeStr, "color_shift") == 0) {
        uint8_t hueOffset = doc["hueOffset"] | 0;
        float rotationSpeed = doc["rotationSpeed"] | 10.0f;
        if (!handlers::g_colorShiftModifier) {
            handlers::g_colorShiftModifier = new ColorShiftModifier(ColorShiftMode::FIXED, hueOffset, rotationSpeed);
        } else {
            handlers::g_colorShiftModifier->setParameter("hueOffset", hueOffset);
        }
        modifier = handlers::g_colorShiftModifier;
    }
    else if (strcmp(typeStr, "mirror") == 0) {
        if (!handlers::g_mirrorModifier) {
            handlers::g_mirrorModifier = new MirrorModifier(MirrorMode::LEFT_TO_RIGHT);
        }
        modifier = handlers::g_mirrorModifier;
    }
    else if (strcmp(typeStr, "glitch") == 0) {
        float amount = doc["glitchAmount"] | 0.3f;
        if (amount < 0.0f || amount > 1.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "glitchAmount must be 0.0-1.0", requestId));
            return;
        }
        if (!handlers::g_glitchModifier) {
            handlers::g_glitchModifier = new GlitchModifier(GlitchMode::PIXEL_FLIP, amount);
        } else {
            handlers::g_glitchModifier->setParameter("intensity", amount);
        }
        modifier = handlers::g_glitchModifier;
    }
    else if (strcmp(typeStr, "blur") == 0) {
        uint8_t radius = doc["radius"] | 2;
        float strength = doc["strength"] | 0.8f;
        uint8_t mode = doc["mode"] | 0;  // 0=BOX, 1=GAUSSIAN, 2=MOTION
        if (radius < 1 || radius > 5) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "radius must be 1-5", requestId));
            return;
        }
        if (strength < 0.0f || strength > 1.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "strength must be 0.0-1.0", requestId));
            return;
        }
        if (!handlers::g_blurModifier) {
            handlers::g_blurModifier = new BlurModifier(static_cast<BlurMode>(mode), radius, strength);
        } else {
            handlers::g_blurModifier->setRadius(radius);
            handlers::g_blurModifier->setStrength(strength);
            handlers::g_blurModifier->setMode(static_cast<BlurMode>(mode));
        }
        modifier = handlers::g_blurModifier;
    }
    else if (strcmp(typeStr, "trail") == 0) {
        uint8_t fadeRate = doc["fadeRate"] | 20;
        uint8_t minFade = doc["minFade"] | 5;
        uint8_t maxFade = doc["maxFade"] | 50;
        uint8_t mode = doc["mode"] | 0;  // 0=CONSTANT, 1=BEAT_REACTIVE, 2=VELOCITY
        if (!handlers::g_trailModifier) {
            handlers::g_trailModifier = new TrailModifier(static_cast<TrailMode>(mode), fadeRate, minFade, maxFade);
        } else {
            handlers::g_trailModifier->setFadeRate(fadeRate);
            handlers::g_trailModifier->setMinFade(minFade);
            handlers::g_trailModifier->setMaxFade(maxFade);
            handlers::g_trailModifier->setMode(static_cast<TrailMode>(mode));
        }
        modifier = handlers::g_trailModifier;
    }
    else if (strcmp(typeStr, "saturation") == 0) {
        int16_t saturation = doc["saturation"] | 200;
        uint8_t mode = doc["mode"] | 0;  // 0=ABSOLUTE, 1=RELATIVE, 2=VIBRANCE
        bool preserveLuminance = doc["preserveLuminance"] | true;
        if (!handlers::g_saturationModifier) {
            handlers::g_saturationModifier = new SaturationModifier(static_cast<SatMode>(mode), saturation, preserveLuminance);
        } else {
            handlers::g_saturationModifier->setSaturation(saturation);
            handlers::g_saturationModifier->setMode(static_cast<SatMode>(mode));
            handlers::g_saturationModifier->setPreserveLuminance(preserveLuminance);
        }
        modifier = handlers::g_saturationModifier;
    }
    else if (strcmp(typeStr, "strobe") == 0) {
        uint8_t subdivision = doc["subdivision"] | 1;
        float dutyCycle = doc["dutyCycle"] | 0.3f;
        float intensity = doc["intensity"] | 1.0f;
        float rateHz = doc["rateHz"] | 4.0f;
        uint8_t mode = doc["mode"] | 0;  // 0=BEAT_SYNC, 1=SUBDIVISION, 2=MANUAL_RATE
        if (subdivision < 1 || subdivision > 16) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "subdivision must be 1-16", requestId));
            return;
        }
        if (dutyCycle < 0.0f || dutyCycle > 1.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "dutyCycle must be 0.0-1.0", requestId));
            return;
        }
        if (!handlers::g_strobeModifier) {
            handlers::g_strobeModifier = new StrobeModifier(static_cast<StrobeMode>(mode), subdivision, dutyCycle, intensity, rateHz);
        } else {
            handlers::g_strobeModifier->setSubdivision(subdivision);
            handlers::g_strobeModifier->setDutyCycle(dutyCycle);
            handlers::g_strobeModifier->setIntensity(intensity);
            handlers::g_strobeModifier->setRateHz(rateHz);
            handlers::g_strobeModifier->setMode(static_cast<StrobeMode>(mode));
        }
        modifier = handlers::g_strobeModifier;
    }
    else {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown modifier type", requestId));
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
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to add modifier to stack", requestId));
        return;
    }

    // Success response
    String response = buildWsResponse("modifiers.added", requestId, [stack, typeStr](JsonObject& data) {
        data["type"] = typeStr;
        data["modifierId"] = stack->getCount() - 1;
        data["stackCount"] = stack->getCount();
    });
    client->text(response);

    LW_LOGI("Added modifier via WS: %s (stack count: %d)", typeStr, stack->getCount());
}

static void handleModifiersRemove(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    const char* typeStr = doc["type"];

    if (!typeStr) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'type' field", requestId));
        return;
    }

    nodes::RendererNode* renderer = ctx.renderer;
    if (!renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Modifier stack not available", requestId));
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
    } else if (strcmp(typeStr, "blur") == 0) {
        type = ModifierType::BLUR;
    } else if (strcmp(typeStr, "trail") == 0) {
        type = ModifierType::TRAIL;
    } else if (strcmp(typeStr, "saturation") == 0) {
        type = ModifierType::SATURATION;
    } else if (strcmp(typeStr, "strobe") == 0) {
        type = ModifierType::STROBE;
    } else {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown modifier type", requestId));
        return;
    }

    if (!stack->removeByType(type)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Modifier not found", requestId));
        return;
    }

    // Success response
    String response = buildWsResponse("modifiers.removed", requestId, [typeStr, stack](JsonObject& data) {
        data["type"] = typeStr;
        data["stackCount"] = stack->getCount();
    });
    client->text(response);

    LW_LOGI("Removed modifier via WS: %s", typeStr);
}

static void handleModifiersList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    nodes::RendererNode* renderer = ctx.renderer;
    if (!renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Modifier stack not available", requestId));
        return;
    }

    String response = buildWsResponse("modifiers.list", requestId, [stack](JsonObject& data) {
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
                    case ModifierType::BLUR: typeStr = "blur"; break;
                    case ModifierType::TRAIL: typeStr = "trail"; break;
                    case ModifierType::SATURATION: typeStr = "saturation"; break;
                    case ModifierType::STROBE: typeStr = "strobe"; break;
                    default: break;
                }

                modObj["type"] = typeStr;
                modObj["name"] = meta.name;
                modObj["enabled"] = mod->isEnabled();
                modObj["preRender"] = mod->isPreRender();
                modObj["index"] = i;
            }
        }

        data["count"] = stack->getCount();
        data["maxCount"] = ModifierStack::MAX_MODIFIERS;
    });
    client->text(response);
}

static void handleModifiersUpdate(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    const char* typeStr = doc["type"];

    if (!typeStr) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'type' field", requestId));
        return;
    }

    nodes::RendererNode* renderer = ctx.renderer;
    if (!renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Modifier stack not available", requestId));
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
    } else if (strcmp(typeStr, "blur") == 0) {
        type = ModifierType::BLUR;
    } else if (strcmp(typeStr, "trail") == 0) {
        type = ModifierType::TRAIL;
    } else if (strcmp(typeStr, "saturation") == 0) {
        type = ModifierType::SATURATION;
    } else if (strcmp(typeStr, "strobe") == 0) {
        type = ModifierType::STROBE;
    } else {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown modifier type", requestId));
        return;
    }

    IEffectModifier* modifier = stack->findByType(type);
    if (!modifier) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Modifier not found", requestId));
        return;
    }

    // Update parameters based on type
    bool updated = false;
    if (type == ModifierType::SPEED && doc.containsKey("multiplier")) {
        float multiplier = doc["multiplier"];
        if (multiplier < 0.1f || multiplier > 3.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Multiplier must be 0.1-3.0", requestId));
            return;
        }
        updated = modifier->setParameter("multiplier", multiplier);
    } else if (type == ModifierType::INTENSITY && doc.containsKey("baseIntensity")) {
        float baseIntensity = doc["baseIntensity"];
        if (baseIntensity < 0.0f || baseIntensity > 2.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "baseIntensity must be 0.0-2.0", requestId));
            return;
        }
        updated = modifier->setParameter("baseIntensity", baseIntensity);
    } else if (type == ModifierType::COLOR_SHIFT && doc.containsKey("hueOffset")) {
        uint8_t hueOffset = doc["hueOffset"];
        updated = modifier->setParameter("hueOffset", hueOffset);
    } else if (type == ModifierType::GLITCH && doc.containsKey("glitchAmount")) {
        float amount = doc["glitchAmount"];
        if (amount < 0.0f || amount > 1.0f) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "glitchAmount must be 0.0-1.0", requestId));
            return;
        }
        updated = modifier->setParameter("intensity", amount);
    } else if (type == ModifierType::BLUR) {
        if (doc.containsKey("radius")) updated = modifier->setParameter("radius", doc["radius"]);
        if (doc.containsKey("strength")) updated = modifier->setParameter("strength", doc["strength"]) || updated;
        if (doc.containsKey("mode")) updated = modifier->setParameter("mode", doc["mode"]) || updated;
    } else if (type == ModifierType::TRAIL) {
        if (doc.containsKey("fadeRate")) updated = modifier->setParameter("fadeRate", doc["fadeRate"]);
        if (doc.containsKey("minFade")) updated = modifier->setParameter("minFade", doc["minFade"]) || updated;
        if (doc.containsKey("maxFade")) updated = modifier->setParameter("maxFade", doc["maxFade"]) || updated;
        if (doc.containsKey("mode")) updated = modifier->setParameter("mode", doc["mode"]) || updated;
    } else if (type == ModifierType::SATURATION) {
        if (doc.containsKey("saturation")) updated = modifier->setParameter("saturation", doc["saturation"]);
        if (doc.containsKey("mode")) updated = modifier->setParameter("mode", doc["mode"]) || updated;
        if (doc.containsKey("preserveLuminance")) updated = modifier->setParameter("preserveLuminance", doc["preserveLuminance"] ? 1.0f : 0.0f) || updated;
    } else if (type == ModifierType::STROBE) {
        if (doc.containsKey("subdivision")) updated = modifier->setParameter("subdivision", doc["subdivision"]);
        if (doc.containsKey("dutyCycle")) updated = modifier->setParameter("dutyCycle", doc["dutyCycle"]) || updated;
        if (doc.containsKey("intensity")) updated = modifier->setParameter("intensity", doc["intensity"]) || updated;
        if (doc.containsKey("rateHz")) updated = modifier->setParameter("rateHz", doc["rateHz"]) || updated;
        if (doc.containsKey("mode")) updated = modifier->setParameter("mode", doc["mode"]) || updated;
    }

    if (!updated) {
        client->text(buildWsError(ErrorCodes::INVALID_PARAMETER, "No valid parameters to update", requestId));
        return;
    }

    // Success response
    String response = buildWsResponse("modifiers.updated", requestId, [typeStr](JsonObject& data) {
        data["type"] = typeStr;
        data["updated"] = true;
    });
    client->text(response);

    LW_LOGI("Updated modifier via WS: %s", typeStr);
}

static void handleModifiersClear(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    nodes::RendererNode* renderer = ctx.renderer;
    if (!renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    ModifierStack* stack = renderer->getModifierStack();
    if (!stack) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Modifier stack not available", requestId));
        return;
    }

    stack->clear();

    // Success response
    String response = buildWsResponse("modifiers.cleared", requestId, [](JsonObject& data) {
        data["cleared"] = true;
    });
    client->text(response);

    LW_LOGI("Cleared all modifiers via WS");
}

void registerWsModifierCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("modifiers.add", handleModifiersAdd);
    WsCommandRouter::registerCommand("modifiers.remove", handleModifiersRemove);
    WsCommandRouter::registerCommand("modifiers.list", handleModifiersList);
    WsCommandRouter::registerCommand("modifiers.update", handleModifiersUpdate);
    WsCommandRouter::registerCommand("modifiers.clear", handleModifiersClear);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
