/**
 * @file WsColorCommands.cpp
 * @brief WebSocket color command handlers implementation
 */

#include "WsColorCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../effects/enhancement/ColorEngine.h"
#include "../../../effects/enhancement/ColorCorrectionEngine.h"
#include "../../../palettes/Palettes_Master.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::enhancement;
using namespace lightwaveos::palettes;

static void handleColorGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = ColorEngine::getInstance();
    
    String response = buildWsResponse("color.getStatus", requestId, [&engine](JsonObject& data) {
        data["active"] = engine.isActive();
        data["blendEnabled"] = engine.isCrossBlendEnabled();
        
        JsonArray blendFactors = data["blendFactors"].to<JsonArray>();
        blendFactors.add(engine.getBlendFactor1());
        blendFactors.add(engine.getBlendFactor2());
        blendFactors.add(engine.getBlendFactor3());
        
        data["rotationEnabled"] = engine.isRotationEnabled();
        data["rotationSpeed"] = engine.getRotationSpeed();
        data["rotationPhase"] = engine.getRotationPhase();
        
        data["diffusionEnabled"] = engine.isDiffusionEnabled();
        data["diffusionAmount"] = engine.getDiffusionAmount();
    });
    client->text(response);
}

static void handleColorEnableBlend(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("enable")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enable required", requestId));
        return;
    }
    
    bool enable = doc["enable"] | false;
    auto& engine = ColorEngine::getInstance();
    engine.enableCrossBlend(enable);
    
    String response = buildWsResponse("color.enableBlend", requestId, [enable](JsonObject& data) {
        data["blendEnabled"] = enable;
    });
    client->text(response);
}

static void handleColorSetBlendPalettes(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("palette1") || !doc.containsKey("palette2")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "palette1 and palette2 required", requestId));
        return;
    }
    
    uint8_t p1 = doc["palette1"] | 0;
    uint8_t p2 = doc["palette2"] | 0;
    uint8_t p3 = doc["palette3"] | 255;
    
    if (p1 >= MASTER_PALETTE_COUNT || p2 >= MASTER_PALETTE_COUNT ||
        (p3 != 255 && p3 >= MASTER_PALETTE_COUNT)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Palette ID out of range", requestId));
        return;
    }
    
    // Validate palette IDs before array access (defensive check)
    using namespace lightwaveos::palettes;
    uint8_t safe_p1 = validatePaletteId(p1);
    uint8_t safe_p2 = validatePaletteId(p2);
    
    auto& engine = ColorEngine::getInstance();
    CRGBPalette16 pal1(gMasterPalettes[safe_p1]);
    CRGBPalette16 pal2(gMasterPalettes[safe_p2]);
    
    if (p3 != 255) {
        uint8_t safe_p3 = validatePaletteId(p3);
        CRGBPalette16 pal3(gMasterPalettes[safe_p3]);
        engine.setBlendPalettes(pal1, pal2, &pal3);
    } else {
        engine.setBlendPalettes(pal1, pal2, nullptr);
    }
    
    String response = buildWsResponse("color.setBlendPalettes", requestId, [p1, p2, p3](JsonObject& data) {
        JsonArray palettes = data["blendPalettes"].to<JsonArray>();
        palettes.add(p1);
        palettes.add(p2);
        if (p3 != 255) palettes.add(p3);
    });
    client->text(response);
}

static void handleColorSetBlendFactors(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("factor1") || !doc.containsKey("factor2")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "factor1 and factor2 required", requestId));
        return;
    }
    
    uint8_t f1 = doc["factor1"] | 0;
    uint8_t f2 = doc["factor2"] | 0;
    uint8_t f3 = doc["factor3"] | 0;
    
    auto& engine = ColorEngine::getInstance();
    engine.setBlendFactors(f1, f2, f3);
    
    String response = buildWsResponse("color.setBlendFactors", requestId, [f1, f2, f3](JsonObject& data) {
        JsonArray factors = data["blendFactors"].to<JsonArray>();
        factors.add(f1);
        factors.add(f2);
        factors.add(f3);
    });
    client->text(response);
}

static void handleColorEnableRotation(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("enable")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enable required", requestId));
        return;
    }
    
    bool enable = doc["enable"] | false;
    auto& engine = ColorEngine::getInstance();
    engine.enableTemporalRotation(enable);
    
    String response = buildWsResponse("color.enableRotation", requestId, [enable](JsonObject& data) {
        data["rotationEnabled"] = enable;
    });
    client->text(response);
}

static void handleColorSetRotationSpeed(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("degreesPerFrame")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "degreesPerFrame required", requestId));
        return;
    }
    
    float speed = doc["degreesPerFrame"] | 0.0f;
    auto& engine = ColorEngine::getInstance();
    engine.setRotationSpeed(speed);
    
    String response = buildWsResponse("color.setRotationSpeed", requestId, [speed](JsonObject& data) {
        data["rotationSpeed"] = speed;
    });
    client->text(response);
}

static void handleColorEnableDiffusion(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("enable")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enable required", requestId));
        return;
    }
    
    bool enable = doc["enable"] | false;
    auto& engine = ColorEngine::getInstance();
    engine.enableDiffusion(enable);
    
    String response = buildWsResponse("color.enableDiffusion", requestId, [enable](JsonObject& data) {
        data["diffusionEnabled"] = enable;
    });
    client->text(response);
}

static void handleColorSetDiffusionAmount(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("amount")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "amount required", requestId));
        return;
    }
    
    uint8_t amount = doc["amount"] | 0;
    auto& engine = ColorEngine::getInstance();
    engine.setDiffusionAmount(amount);
    
    String response = buildWsResponse("color.setDiffusionAmount", requestId, [amount](JsonObject& data) {
        data["diffusionAmount"] = amount;
    });
    client->text(response);
}

static void handleColorCorrectionGetConfig(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = ColorCorrectionEngine::getInstance();
    auto& cfg = engine.getConfig();
    
    String response = buildWsResponse("colorCorrection.getConfig", requestId, [&cfg](JsonObject& data) {
        data["mode"] = (uint8_t)cfg.mode;
        data["modeNames"] = "OFF,HSV,RGB,BOTH";
        data["hsvMinSaturation"] = cfg.hsvMinSaturation;
        data["rgbWhiteThreshold"] = cfg.rgbWhiteThreshold;
        data["rgbTargetMin"] = cfg.rgbTargetMin;
        data["autoExposureEnabled"] = cfg.autoExposureEnabled;
        data["autoExposureTarget"] = cfg.autoExposureTarget;
        data["gammaEnabled"] = cfg.gammaEnabled;
        data["gammaValue"] = cfg.gammaValue;
        data["brownGuardrailEnabled"] = cfg.brownGuardrailEnabled;
        data["maxGreenPercentOfRed"] = cfg.maxGreenPercentOfRed;
        data["maxBluePercentOfRed"] = cfg.maxBluePercentOfRed;
    });
    client->text(response);
}

static void handleColorCorrectionSetMode(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("mode")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "mode required (0-3)", requestId));
        return;
    }
    
    uint8_t mode = doc["mode"] | 2;
    if (mode > 3) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "mode must be 0-3 (OFF,HSV,RGB,BOTH)", requestId));
        return;
    }
    
    auto& engine = ColorCorrectionEngine::getInstance();
    engine.setMode((CorrectionMode)mode);
    
    String response = buildWsResponse("colorCorrection.setMode", requestId, [mode](JsonObject& data) {
        data["mode"] = mode;
        const char* names[] = {"OFF", "HSV", "RGB", "BOTH"};
        data["modeName"] = names[mode];
    });
    client->text(response);
}

static void handleColorCorrectionSetConfig(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto& engine = ColorCorrectionEngine::getInstance();
    auto& cfg = engine.getConfig();
    
    if (doc.containsKey("mode")) {
        uint8_t mode = doc["mode"];
        if (mode <= 3) cfg.mode = (CorrectionMode)mode;
    }
    if (doc.containsKey("hsvMinSaturation")) {
        cfg.hsvMinSaturation = doc["hsvMinSaturation"];
    }
    if (doc.containsKey("rgbWhiteThreshold")) {
        cfg.rgbWhiteThreshold = doc["rgbWhiteThreshold"];
    }
    if (doc.containsKey("rgbTargetMin")) {
        cfg.rgbTargetMin = doc["rgbTargetMin"];
    }
    if (doc.containsKey("autoExposureEnabled")) {
        cfg.autoExposureEnabled = doc["autoExposureEnabled"];
    }
    if (doc.containsKey("autoExposureTarget")) {
        cfg.autoExposureTarget = doc["autoExposureTarget"];
    }
    if (doc.containsKey("gammaEnabled")) {
        cfg.gammaEnabled = doc["gammaEnabled"];
    }
    if (doc.containsKey("gammaValue")) {
        float val = doc["gammaValue"];
        if (val >= 1.0f && val <= 3.0f) cfg.gammaValue = val;
    }
    if (doc.containsKey("brownGuardrailEnabled")) {
        cfg.brownGuardrailEnabled = doc["brownGuardrailEnabled"];
    }
    if (doc.containsKey("maxGreenPercentOfRed")) {
        cfg.maxGreenPercentOfRed = doc["maxGreenPercentOfRed"];
    }
    if (doc.containsKey("maxBluePercentOfRed")) {
        cfg.maxBluePercentOfRed = doc["maxBluePercentOfRed"];
    }
    
    String response = buildWsResponse("colorCorrection.setConfig", requestId, [&cfg](JsonObject& data) {
        data["mode"] = (uint8_t)cfg.mode;
        data["updated"] = true;
    });
    client->text(response);
}

static void handleColorCorrectionSave(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    ColorCorrectionEngine::getInstance().saveToNVS();
    
    String response = buildWsResponse("colorCorrection.save", requestId, [](JsonObject& data) {
        data["saved"] = true;
    });
    client->text(response);
}

void registerWsColorCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("color.getStatus", handleColorGetStatus);
    WsCommandRouter::registerCommand("color.enableBlend", handleColorEnableBlend);
    WsCommandRouter::registerCommand("color.setBlendPalettes", handleColorSetBlendPalettes);
    WsCommandRouter::registerCommand("color.setBlendFactors", handleColorSetBlendFactors);
    WsCommandRouter::registerCommand("color.enableRotation", handleColorEnableRotation);
    WsCommandRouter::registerCommand("color.setRotationSpeed", handleColorSetRotationSpeed);
    WsCommandRouter::registerCommand("color.enableDiffusion", handleColorEnableDiffusion);
    WsCommandRouter::registerCommand("color.setDiffusionAmount", handleColorSetDiffusionAmount);
    WsCommandRouter::registerCommand("colorCorrection.getConfig", handleColorCorrectionGetConfig);
    WsCommandRouter::registerCommand("colorCorrection.setMode", handleColorCorrectionSetMode);
    WsCommandRouter::registerCommand("colorCorrection.setConfig", handleColorCorrectionSetConfig);
    WsCommandRouter::registerCommand("colorCorrection.save", handleColorCorrectionSave);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

