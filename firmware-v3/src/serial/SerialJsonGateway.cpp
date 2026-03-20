/**
 * @file SerialJsonGateway.cpp
 * @brief JSON command gateway for PRISM Studio (Web Serial) control.
 *
 * Extracted from main.cpp — zero functional changes.
 * Each line beginning with '{' received over Serial is parsed and dispatched.
 * Responses are printed as JSON lines back to Serial.
 */

#include "SerialJsonGateway.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "../config/features.h"
#include "../core/actors/ActorSystem.h"
#include "../core/actors/RendererActor.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/zones/BlendMode.h"
#include "../effects/PatternRegistry.h"
#include "../effects/enhancement/EdgeMixer.h"
#include "../core/narrative/NarrativeEngine.h"
#include "../core/shows/BuiltinShows.h"
#include "../core/shows/Prim8Adapter.h"
#include "../core/shows/ShowBundleParser.h"
#include "../core/shows/DynamicShowStore.h"

#if FEATURE_TRANSITIONS
#include "../effects/transitions/TransitionEngine.h"
#include "../effects/transitions/TransitionTypes.h"
#endif

#if FEATURE_WEB_SERVER
#include "../network/WiFiManager.h"
#endif

#if FEATURE_AUDIO_SYNC
#include "../audio/AudioDebugConfig.h"
#include "../audio/contracts/AudioEffectMapping.h"
#endif

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
#if FEATURE_TRANSITIONS
using namespace lightwaveos::transitions;
#endif
using namespace lightwaveos::narrative;

namespace lightwaveos {
namespace serial {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief Send a successful JSON response over Serial.
 *
 * @param type     Response type string (echoed from request or descriptive)
 * @param reqId    Request ID to echo back (may be empty)
 * @param dataJson Pre-serialised JSON string for the "data" field
 */
static void serialJsonResponse(const char* type, const char* reqId, const char* dataJson) {
    Serial.printf("{\"type\":\"%s\",\"requestId\":\"%s\",\"success\":true,\"data\":%s}\n",
                  type, reqId, dataJson);
}

/**
 * @brief Send an error JSON response over Serial.
 */
static void serialJsonError(const char* reqId, const char* error) {
    Serial.printf("{\"type\":\"error\",\"requestId\":\"%s\",\"success\":false,\"error\":\"%s\"}\n",
                  reqId, error);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void processSerialJsonCommand(const String& json, const SerialJsonGatewayDeps& deps) {
    // Unpack deps for readability (matches original main.cpp variable names).
    ActorSystem&   actors       = deps.actors;
    RendererActor* renderer     = deps.renderer;
    ZoneComposer&  zoneComposer = deps.zoneComposer;
    ::prism::DynamicShowStore& serialShowStore = deps.showStore;

    // Stack-allocated document -- 1 KB is sufficient for inbound commands
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        serialJsonError("", err.c_str());
        return;
    }

    const char* type = doc["type"] | "";
    const char* reqId = doc["requestId"] | "";

    // ------------------------------------------------------------------
    // device.getStatus
    // ------------------------------------------------------------------
    if (strcmp(type, "device.getStatus") == 0) {
        char buf[256];
#if FEATURE_WEB_SERVER
        bool wifiConnected = WIFI_MANAGER.isConnected();
        bool apMode = WIFI_MANAGER.isAPMode();
        const char* wifiStatus = wifiConnected ? "connected" : (apMode ? "ap" : "disconnected");
#else
        const char* wifiStatus = "disabled";
#endif
        snprintf(buf, sizeof(buf),
            "{\"freeHeap\":%lu,\"uptimeMs\":%lu,\"wifi\":\"%s\",\"fps\":%u,\"effectCount\":%u}",
            (unsigned long)ESP.getFreeHeap(),
            (unsigned long)millis(),
            wifiStatus,
            renderer ? (unsigned)renderer->getStats().currentFPS : 0u,
            renderer ? (unsigned)renderer->getEffectCount() : 0u);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // effects.getCurrent
    // ------------------------------------------------------------------
    else if (strcmp(type, "effects.getCurrent") == 0) {
        if (!renderer) { serialJsonError(reqId, "renderer unavailable"); return; }
        EffectId id = renderer->getCurrentEffect();
        const char* name = renderer->getEffectName(id);
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"effectId\":%u,\"name\":\"%s\"}", (unsigned)id, name ? name : "");
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // effects.list  (supports optional offset / limit for pagination)
    // ------------------------------------------------------------------
    else if (strcmp(type, "effects.list") == 0) {
        if (!renderer) { serialJsonError(reqId, "renderer unavailable"); return; }

        uint16_t total = renderer->getEffectCount();
        uint16_t offset = doc["offset"] | 0;
        uint16_t limit  = doc["limit"]  | total;
        if (offset >= total) { offset = 0; limit = 0; }
        if (offset + limit > total) limit = total - offset;

        // Build response with ArduinoJson to handle variable-length array safely
        JsonDocument respDoc;
        respDoc["total"] = total;
        respDoc["offset"] = offset;
        respDoc["limit"] = limit;
        JsonArray arr = respDoc["effects"].to<JsonArray>();
        for (uint16_t i = offset; i < offset + limit; i++) {
            EffectId eid = renderer->getEffectIdAt(i);
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = eid;
            const char* name = renderer->getEffectName(eid);
            obj["name"] = name ? name : "";
        }

        // Serialise directly to Serial
        Serial.printf("{\"type\":\"%s\",\"requestId\":\"%s\",\"success\":true,\"data\":", type, reqId);
        serializeJson(respDoc, Serial);
        Serial.println("}");
    }
    // ------------------------------------------------------------------
    // effects.getCategories
    // ------------------------------------------------------------------
    else if (strcmp(type, "effects.getCategories") == 0) {
        // Return reactive vs ambient categorisation counts
        uint8_t reactiveCount = PatternRegistry::getReactiveEffectCount();
        uint16_t totalCount = renderer ? renderer->getEffectCount() : 0;
        char buf[192];
        snprintf(buf, sizeof(buf),
            "{\"categories\":[{\"name\":\"reactive\",\"count\":%u},{\"name\":\"ambient\",\"count\":%u},{\"name\":\"all\",\"count\":%u}]}",
            (unsigned)reactiveCount,
            (unsigned)(totalCount > reactiveCount ? totalCount - reactiveCount : 0),
            (unsigned)totalCount);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // parameters.get
    // ------------------------------------------------------------------
    else if (strcmp(type, "parameters.get") == 0) {
        if (!renderer) { serialJsonError(reqId, "renderer unavailable"); return; }
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"brightness\":%u,\"speed\":%u,\"paletteId\":%u,\"paletteName\":\"%s\","
            "\"effectId\":%u,\"intensity\":%u,\"saturation\":%u,\"complexity\":%u,"
            "\"variation\":%u,\"mood\":%u,\"hue\":%u,\"fadeAmount\":%u}",
            (unsigned)renderer->getBrightness(),
            (unsigned)renderer->getSpeed(),
            (unsigned)renderer->getPaletteIndex(),
            renderer->getPaletteName(renderer->getPaletteIndex()),
            (unsigned)renderer->getCurrentEffect(),
            (unsigned)renderer->getIntensity(),
            (unsigned)renderer->getSaturation(),
            (unsigned)renderer->getComplexity(),
            (unsigned)renderer->getVariation(),
            (unsigned)renderer->getMood(),
            (unsigned)renderer->getHue(),
            (unsigned)renderer->getFadeAmount());
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setEffect
    // ------------------------------------------------------------------
    else if (strcmp(type, "setEffect") == 0) {
        if (!renderer) { serialJsonError(reqId, "renderer unavailable"); return; }
        if (!doc["effectId"].is<int>()) { serialJsonError(reqId, "missing effectId"); return; }
        EffectId effectId = doc["effectId"];
        if (!renderer->isEffectRegistered(effectId)) { serialJsonError(reqId, "effectId not registered"); return; }
        actors.setEffect(effectId);
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"effectId\":%u,\"name\":\"%s\"}",
                 (unsigned)effectId, renderer->getEffectName(effectId));
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setBrightness
    // ------------------------------------------------------------------
    else if (strcmp(type, "setBrightness") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setBrightness(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"brightness\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setSpeed
    // ------------------------------------------------------------------
    else if (strcmp(type, "setSpeed") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setSpeed(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"speed\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setPalette
    // ------------------------------------------------------------------
    else if (strcmp(type, "setPalette") == 0) {
        if (!renderer) { serialJsonError(reqId, "renderer unavailable"); return; }
        if (!doc["paletteId"].is<int>()) { serialJsonError(reqId, "missing paletteId"); return; }
        uint8_t pal = doc["paletteId"];
        uint8_t paletteCount = renderer->getPaletteCount();
        if (pal >= paletteCount) { serialJsonError(reqId, "paletteId out of range"); return; }
        actors.setPalette(pal);
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"paletteId\":%u,\"name\":\"%s\"}",
                 (unsigned)pal, renderer->getPaletteName(pal));
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zones.list
    // ------------------------------------------------------------------
    else if (strcmp(type, "zones.list") == 0) {
        bool enabled = zoneComposer.isEnabled();
        uint8_t count = zoneComposer.getZoneCount();
        JsonDocument respDoc;
        respDoc["enabled"] = enabled;
        respDoc["zoneCount"] = count;
        JsonArray arr = respDoc["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = i;
            obj["effectId"] = zoneComposer.getZoneEffect(i);
            obj["brightness"] = zoneComposer.getZoneBrightness(i);
            obj["speed"] = zoneComposer.getZoneSpeed(i);
            obj["palette"] = zoneComposer.getZonePalette(i);
            obj["enabled"] = zoneComposer.isZoneEnabled(i);
        }
        Serial.printf("{\"type\":\"%s\",\"requestId\":\"%s\",\"success\":true,\"data\":", type, reqId);
        serializeJson(respDoc, Serial);
        Serial.println("}");
    }
    // ------------------------------------------------------------------
    // transition.getTypes
    // ------------------------------------------------------------------
    else if (strcmp(type, "transition.getTypes") == 0) {
#if FEATURE_TRANSITIONS
        JsonDocument respDoc;
        JsonArray arr = respDoc["types"].to<JsonArray>();
        for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = i;
            obj["name"] = getTransitionName(static_cast<TransitionType>(i));
            obj["durationMs"] = getDefaultDuration(static_cast<TransitionType>(i));
        }
        Serial.printf("{\"type\":\"%s\",\"requestId\":\"%s\",\"success\":true,\"data\":", type, reqId);
        serializeJson(respDoc, Serial);
        Serial.println("}");
#else
        serialJsonResponse(type, reqId, "{\"types\":[]}");
#endif
    }
    // ------------------------------------------------------------------
    // transition.trigger
    // ------------------------------------------------------------------
    else if (strcmp(type, "transition.trigger") == 0) {
#if FEATURE_TRANSITIONS
        if (!renderer) { serialJsonError(reqId, "renderer unavailable"); return; }
        if (!doc["toEffect"].is<int>()) { serialJsonError(reqId, "missing toEffect"); return; }
        EffectId toEffect = doc["toEffect"];
        if (!renderer->isEffectRegistered(toEffect)) { serialJsonError(reqId, "toEffect not registered"); return; }

        if (doc["transitionType"].is<int>()) {
            uint8_t tt = doc["transitionType"];
            // Route through ActorSystem message queue for thread safety (Core 0 -> Core 1)
            actors.startTransition(toEffect, tt);
        } else {
            uint8_t randomType = static_cast<uint8_t>(lightwaveos::transitions::TransitionEngine::getRandomTransition());
            actors.startTransition(toEffect, randomType);
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"toEffect\":%u,\"name\":\"%s\"}",
                 (unsigned)toEffect, renderer->getEffectName(toEffect));
        serialJsonResponse(type, reqId, buf);
#else
        serialJsonError(reqId, "transitions disabled");
#endif
    }
    // ------------------------------------------------------------------
    // setHue
    // ------------------------------------------------------------------
    else if (strcmp(type, "setHue") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setHue(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"hue\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setIntensity
    // ------------------------------------------------------------------
    else if (strcmp(type, "setIntensity") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setIntensity(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"intensity\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setSaturation
    // ------------------------------------------------------------------
    else if (strcmp(type, "setSaturation") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setSaturation(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"saturation\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setComplexity
    // ------------------------------------------------------------------
    else if (strcmp(type, "setComplexity") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setComplexity(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"complexity\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setVariation
    // ------------------------------------------------------------------
    else if (strcmp(type, "setVariation") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setVariation(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"variation\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setMood
    // ------------------------------------------------------------------
    else if (strcmp(type, "setMood") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setMood(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"mood\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setFadeAmount
    // ------------------------------------------------------------------
    else if (strcmp(type, "setFadeAmount") == 0) {
        if (!doc["value"].is<int>()) { serialJsonError(reqId, "missing value"); return; }
        uint8_t val = doc["value"];
        actors.setFadeAmount(val);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"fadeAmount\":%u}", (unsigned)val);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // getEdgeMixer
    // ------------------------------------------------------------------
    else if (strcmp(type, "getEdgeMixer") == 0) {
        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"mode\":%u,\"modeName\":\"%s\",\"spread\":%u,\"strength\":%u,"
            "\"spatial\":%u,\"spatialName\":\"%s\",\"temporal\":%u,\"temporalName\":\"%s\"}",
            (unsigned)static_cast<uint8_t>(mixer.getMode()),
            mixer.modeName(),
            (unsigned)mixer.getSpread(),
            (unsigned)mixer.getStrength(),
            (unsigned)static_cast<uint8_t>(mixer.getSpatial()),
            mixer.spatialName(),
            (unsigned)static_cast<uint8_t>(mixer.getTemporal()),
            mixer.temporalName());
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // setEdgeMixer
    // ------------------------------------------------------------------
    else if (strcmp(type, "setEdgeMixer") == 0) {
        // Snapshot current state; override with requested values for response.
        // Actor messages are queued (async), so the singleton may not reflect
        // the new state by the time we build the response. Responding with the
        // REQUESTED values is correct: "here is what will take effect."
        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
        uint8_t rMode     = static_cast<uint8_t>(mixer.getMode());
        uint8_t rSpread   = mixer.getSpread();
        uint8_t rStrength = mixer.getStrength();
        uint8_t rSpatial  = static_cast<uint8_t>(mixer.getSpatial());
        uint8_t rTemporal = static_cast<uint8_t>(mixer.getTemporal());

        bool anySet = false;
        if (doc.containsKey("mode")) {
            uint8_t mode = doc["mode"] | 0;
            if (mode > 4) { serialJsonError(reqId, "mode must be 0-4"); return; }
            actors.setEdgeMixerMode(mode);
            rMode = mode;
            anySet = true;
        }
        if (doc.containsKey("spread")) {
            uint8_t spread = doc["spread"] | 30;
            if (spread > 60) { serialJsonError(reqId, "spread must be 0-60"); return; }
            actors.setEdgeMixerSpread(spread);
            rSpread = spread;
            anySet = true;
        }
        if (doc.containsKey("strength")) {
            uint8_t s = doc["strength"] | 255;
            actors.setEdgeMixerStrength(s);
            rStrength = s;
            anySet = true;
        }
        if (doc.containsKey("spatial")) {
            uint8_t spatial = doc["spatial"] | 0;
            if (spatial > 1) { serialJsonError(reqId, "spatial must be 0-1"); return; }
            actors.setEdgeMixerSpatial(spatial);
            rSpatial = spatial;
            anySet = true;
        }
        if (doc.containsKey("temporal")) {
            uint8_t temporal = doc["temporal"] | 0;
            if (temporal > 1) { serialJsonError(reqId, "temporal must be 0-1"); return; }
            actors.setEdgeMixerTemporal(temporal);
            rTemporal = temporal;
            anySet = true;
        }
        if (!anySet) { serialJsonError(reqId, "provide mode, spread, strength, spatial, or temporal"); return; }
        // Respond with the values that will take effect (not stale singleton).
        using EM = lightwaveos::enhancement::EdgeMixerMode;
        using ES = lightwaveos::enhancement::EdgeMixerSpatial;
        using ET = lightwaveos::enhancement::EdgeMixerTemporal;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"mode\":%u,\"modeName\":\"%s\",\"spread\":%u,\"strength\":%u,"
            "\"spatial\":%u,\"spatialName\":\"%s\",\"temporal\":%u,\"temporalName\":\"%s\"}",
            (unsigned)rMode,
            lightwaveos::enhancement::EdgeMixer::modeName(static_cast<EM>(rMode)),
            (unsigned)rSpread,
            (unsigned)rStrength,
            (unsigned)rSpatial,
            lightwaveos::enhancement::EdgeMixer::spatialName(static_cast<ES>(rSpatial)),
            (unsigned)rTemporal,
            lightwaveos::enhancement::EdgeMixer::temporalName(static_cast<ET>(rTemporal)));
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // saveEdgeMixer
    // ------------------------------------------------------------------
    else if (strcmp(type, "saveEdgeMixer") == 0) {
        actors.saveEdgeMixerToNVS();
        serialJsonResponse(type, reqId, "{\"saved\":true}");
    }
    // ------------------------------------------------------------------
    // prim8.set  (Prim8 8-dimensional semantic vector)
    // ------------------------------------------------------------------
    else if (strcmp(type, "prim8.set") == 0) {
        uint8_t zone = doc["zone"] | (uint8_t)ZONE_GLOBAL;

        ::prism::Prim8Vector prim8;
        prim8.pressure = doc["pressure"] | 0.5f;
        prim8.impact   = doc["impact"]   | 0.5f;
        prim8.mass     = doc["mass"]     | 0.5f;
        prim8.momentum = doc["momentum"] | 0.5f;
        prim8.heat     = doc["heat"]     | 0.5f;
        prim8.space    = doc["space"]    | 0.5f;
        prim8.texture  = doc["texture"]  | 0.5f;
        prim8.gravity  = doc["gravity"]  | 0.5f;

        uint8_t paletteId = doc["paletteId"] | 0u;
        ::prism::FirmwareParams params = ::prism::mapPrim8ToParams(prim8, paletteId);

        // Apply mapped parameters to the renderer
        actors.setBrightness(params.brightness);
        actors.setSpeed(params.speed);
        actors.setHue(params.hue);
        actors.setIntensity(params.intensity);
        actors.setSaturation(params.saturation);
        actors.setComplexity(params.complexity);
        actors.setVariation(params.variation);
        actors.setMood(params.mood);
        actors.setFadeAmount(params.fadeAmount);

        char buf[192];
        snprintf(buf, sizeof(buf),
            "{\"zone\":%u,\"mapped\":{\"brightness\":%u,\"speed\":%u,\"hue\":%u,"
            "\"intensity\":%u,\"saturation\":%u,\"complexity\":%u,\"variation\":%u,"
            "\"mood\":%u,\"fadeAmount\":%u}}",
            (unsigned)zone, (unsigned)params.brightness, (unsigned)params.speed,
            (unsigned)params.hue, (unsigned)params.intensity, (unsigned)params.saturation,
            (unsigned)params.complexity, (unsigned)params.variation,
            (unsigned)params.mood, (unsigned)params.fadeAmount);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.enable / zones.enabled  (enable/disable zone system)
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.enable") == 0 || strcmp(type, "zones.enabled") == 0) {
        bool enable = doc["enable"] | doc["enabled"] | false;
        zoneComposer.setEnabled(enable);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"enabled\":%s}", enable ? "true" : "false");
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.setEffect  (set effect on a specific zone)
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.setEffect") == 0) {
        if (!doc["zoneId"].is<int>()) { serialJsonError(reqId, "missing zoneId"); return; }
        if (!doc["effectId"].is<int>()) { serialJsonError(reqId, "missing effectId"); return; }
        uint8_t zoneId = doc["zoneId"];
        EffectId effectId = doc["effectId"];

        if (zoneId >= zoneComposer.getZoneCount()) { serialJsonError(reqId, "zoneId out of range"); return; }
        if (renderer && !renderer->isEffectRegistered(effectId)) { serialJsonError(reqId, "effectId not registered"); return; }

        zoneComposer.setZoneEffect(zoneId, effectId);
        char buf[96];
        snprintf(buf, sizeof(buf), "{\"zoneId\":%u,\"effectId\":%u}", (unsigned)zoneId, (unsigned)effectId);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.setBrightness
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.setBrightness") == 0) {
        if (!doc["zoneId"].is<int>()) { serialJsonError(reqId, "missing zoneId"); return; }
        if (!doc["brightness"].is<int>()) { serialJsonError(reqId, "missing brightness"); return; }
        uint8_t zoneId = doc["zoneId"];
        uint8_t brightness = doc["brightness"];

        if (zoneId >= zoneComposer.getZoneCount()) { serialJsonError(reqId, "zoneId out of range"); return; }

        zoneComposer.setZoneBrightness(zoneId, brightness);
        char buf[96];
        snprintf(buf, sizeof(buf), "{\"zoneId\":%u,\"brightness\":%u}", (unsigned)zoneId, (unsigned)brightness);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.setSpeed
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.setSpeed") == 0) {
        if (!doc["zoneId"].is<int>()) { serialJsonError(reqId, "missing zoneId"); return; }
        if (!doc["speed"].is<int>()) { serialJsonError(reqId, "missing speed"); return; }
        uint8_t zoneId = doc["zoneId"];
        uint8_t speed = doc["speed"];

        if (zoneId >= zoneComposer.getZoneCount()) { serialJsonError(reqId, "zoneId out of range"); return; }

        zoneComposer.setZoneSpeed(zoneId, speed);
        char buf[96];
        snprintf(buf, sizeof(buf), "{\"zoneId\":%u,\"speed\":%u}", (unsigned)zoneId, (unsigned)speed);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.setPalette
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.setPalette") == 0) {
        if (!doc["zoneId"].is<int>()) { serialJsonError(reqId, "missing zoneId"); return; }
        if (!doc["paletteId"].is<int>()) { serialJsonError(reqId, "missing paletteId"); return; }
        uint8_t zoneId = doc["zoneId"];
        uint8_t paletteId = doc["paletteId"];

        if (zoneId >= zoneComposer.getZoneCount()) { serialJsonError(reqId, "zoneId out of range"); return; }

        zoneComposer.setZonePalette(zoneId, paletteId);
        char buf[96];
        snprintf(buf, sizeof(buf), "{\"zoneId\":%u,\"paletteId\":%u}", (unsigned)zoneId, (unsigned)paletteId);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.setBlend  (set blend mode on a specific zone)
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.setBlend") == 0) {
        if (!doc["zoneId"].is<int>()) { serialJsonError(reqId, "missing zoneId"); return; }
        if (!doc["blendMode"].is<int>()) { serialJsonError(reqId, "missing blendMode"); return; }
        uint8_t zoneId = doc["zoneId"];
        uint8_t blendModeVal = doc["blendMode"];

        if (zoneId >= zoneComposer.getZoneCount()) { serialJsonError(reqId, "zoneId out of range"); return; }

        lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(blendModeVal);
        zoneComposer.setZoneBlendMode(zoneId, blendMode);
        char buf[96];
        snprintf(buf, sizeof(buf), "{\"zoneId\":%u,\"blendMode\":%u}", (unsigned)zoneId, (unsigned)blendModeVal);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zones.update  (batch update zone properties)
    // ------------------------------------------------------------------
    else if (strcmp(type, "zones.update") == 0) {
        if (!doc["zoneId"].is<int>()) { serialJsonError(reqId, "missing zoneId"); return; }
        uint8_t zoneId = doc["zoneId"];

        if (zoneId >= zoneComposer.getZoneCount()) { serialJsonError(reqId, "zoneId out of range"); return; }

        if (doc.containsKey("effectId") && renderer) {
            EffectId eid = doc["effectId"];
            if (renderer->isEffectRegistered(eid)) {
                zoneComposer.setZoneEffect(zoneId, eid);
            }
        }
        if (doc.containsKey("brightness")) {
            zoneComposer.setZoneBrightness(zoneId, doc["brightness"].as<uint8_t>());
        }
        if (doc.containsKey("speed")) {
            zoneComposer.setZoneSpeed(zoneId, doc["speed"].as<uint8_t>());
        }
        if (doc.containsKey("paletteId")) {
            zoneComposer.setZonePalette(zoneId, doc["paletteId"].as<uint8_t>());
        }
        if (doc.containsKey("blendMode")) {
            uint8_t bm = doc["blendMode"];
            zoneComposer.setZoneBlendMode(zoneId, static_cast<lightwaveos::zones::BlendMode>(bm));
        }

        char buf[48];
        snprintf(buf, sizeof(buf), "{\"zoneId\":%u,\"updated\":true}", (unsigned)zoneId);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // zone.loadPreset / zones.setPreset  (load a zone preset)
    // ------------------------------------------------------------------
    else if (strcmp(type, "zone.loadPreset") == 0 || strcmp(type, "zones.setPreset") == 0) {
        if (!doc["presetId"].is<int>()) { serialJsonError(reqId, "missing presetId"); return; }
        uint8_t presetId = doc["presetId"];
        zoneComposer.loadPreset(presetId);
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"presetId\":%u}", (unsigned)presetId);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // transition.config  (get/set transition config)
    // ------------------------------------------------------------------
    else if (strcmp(type, "transition.config") == 0) {
#if FEATURE_TRANSITIONS
        if (doc.containsKey("enabled") || doc.containsKey("defaultDuration") || doc.containsKey("defaultType")) {
            // Set config -- acknowledge with current values
            bool enabled = doc["enabled"] | true;
            uint16_t duration = doc["defaultDuration"] | 800;
            uint8_t ttype = doc["defaultType"] | 0;
            char buf[128];
            snprintf(buf, sizeof(buf),
                "{\"enabled\":%s,\"defaultDuration\":%u,\"defaultType\":%u}",
                enabled ? "true" : "false", (unsigned)duration, (unsigned)ttype);
            serialJsonResponse(type, reqId, buf);
        } else {
            // Get config
            char buf[128];
            snprintf(buf, sizeof(buf),
                "{\"enabled\":true,\"defaultDuration\":800,\"defaultType\":0,"
                "\"typeCount\":%u}",
                (unsigned)static_cast<uint8_t>(TransitionType::TYPE_COUNT));
            serialJsonResponse(type, reqId, buf);
        }
#else
        serialJsonResponse(type, reqId, "{\"enabled\":false}");
#endif
    }
    // ------------------------------------------------------------------
    // show.list  (list all available shows)
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.list") == 0) {
        JsonDocument respDoc;
        JsonArray shows = respDoc["shows"].to<JsonArray>();

        // Built-in shows
        for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
            ShowDefinition showCopy;
            memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));

            char nameBuf[::prism::MAX_SHOW_NAME_LEN];
            strncpy_P(nameBuf, showCopy.name, ::prism::MAX_SHOW_NAME_LEN - 1);
            nameBuf[::prism::MAX_SHOW_NAME_LEN - 1] = '\0';

            char idBuf[::prism::MAX_SHOW_ID_LEN];
            strncpy_P(idBuf, showCopy.id, ::prism::MAX_SHOW_ID_LEN - 1);
            idBuf[::prism::MAX_SHOW_ID_LEN - 1] = '\0';

            JsonObject show = shows.add<JsonObject>();
            show["id"] = String(idBuf);
            show["name"] = String(nameBuf);
            show["durationMs"] = showCopy.totalDurationMs;
            show["builtin"] = true;
            show["looping"] = showCopy.looping;
        }

        // Dynamic shows
        for (uint8_t i = 0; i < ::prism::MAX_DYNAMIC_SHOWS; i++) {
            if (!serialShowStore.isOccupied(i)) continue;
            const ::prism::DynamicShowData* data = serialShowStore.getShowData(i);
            if (!data) continue;

            JsonObject show = shows.add<JsonObject>();
            show["id"] = String(data->id);
            show["name"] = String(data->name);
            show["durationMs"] = data->totalDurationMs;
            show["builtin"] = false;
            show["looping"] = data->looping;
            show["cueCount"] = data->cueCount;
            show["slot"] = i;
        }

        Serial.printf("{\"type\":\"%s\",\"requestId\":\"%s\",\"success\":true,\"data\":", type, reqId);
        serializeJson(respDoc, Serial);
        Serial.println("}");
    }
    // ------------------------------------------------------------------
    // show.upload  (upload ShowBundle JSON)
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.upload") == 0) {
        // Re-serialise the doc to pass to ShowBundleParser
        String jsonStr;
        serializeJson(doc, jsonStr);

        uint8_t slot = 0xFF;
        ::prism::ParseResult result = ::prism::ShowBundleParser::parse(
            reinterpret_cast<const uint8_t*>(jsonStr.c_str()),
            jsonStr.length(),
            serialShowStore,
            slot
        );

        if (!result.success) {
            serialJsonError(reqId, result.errorMessage);
            return;
        }

        char buf[192];
        snprintf(buf, sizeof(buf),
            "{\"showId\":\"%s\",\"cueCount\":%u,\"chapterCount\":%u,\"ramUsageBytes\":%u,\"slot\":%u}",
            result.showId, (unsigned)result.cueCount, (unsigned)result.chapterCount,
            (unsigned)result.ramUsageBytes, (unsigned)slot);
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // show.play  (start show playback)
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.play") == 0) {
        const char* showId = doc["showId"] | "";
        if (strlen(showId) == 0) { serialJsonError(reqId, "missing showId"); return; }

        // Check dynamic shows
        int8_t dynamicSlot = serialShowStore.findById(showId);
        if (dynamicSlot >= 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "{\"showId\":\"%s\",\"source\":\"dynamic\",\"slot\":%d}",
                     showId, (int)dynamicSlot);
            serialJsonResponse(type, reqId, buf);
            return;
        }

        // Check built-in shows
        for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
            ShowDefinition showCopy;
            memcpy_P(&showCopy, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));
            char idBuf[::prism::MAX_SHOW_ID_LEN];
            strncpy_P(idBuf, showCopy.id, ::prism::MAX_SHOW_ID_LEN - 1);
            idBuf[::prism::MAX_SHOW_ID_LEN - 1] = '\0';

            if (strcmp(idBuf, showId) == 0) {
                char buf[128];
                snprintf(buf, sizeof(buf), "{\"showId\":\"%s\",\"source\":\"builtin\",\"builtinIndex\":%u}",
                         showId, (unsigned)i);
                serialJsonResponse(type, reqId, buf);
                return;
            }
        }

        serialJsonError(reqId, "show not found");
    }
    // ------------------------------------------------------------------
    // show.pause
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.pause") == 0) {
        serialJsonResponse(type, reqId, "{\"success\":true}");
    }
    // ------------------------------------------------------------------
    // show.resume
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.resume") == 0) {
        serialJsonResponse(type, reqId, "{\"success\":true}");
    }
    // ------------------------------------------------------------------
    // show.stop
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.stop") == 0) {
        serialJsonResponse(type, reqId, "{\"success\":true}");
    }
    // ------------------------------------------------------------------
    // show.delete  (delete an uploaded show)
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.delete") == 0) {
        const char* showId = doc["showId"] | "";
        if (strlen(showId) == 0) { serialJsonError(reqId, "missing showId"); return; }

        if (serialShowStore.deleteShowById(showId)) {
            serialJsonResponse(type, reqId, "{\"deleted\":true}");
        } else {
            serialJsonError(reqId, "show not found or is built-in");
        }
    }
    // ------------------------------------------------------------------
    // show.status  (get current playback state)
    // ------------------------------------------------------------------
    else if (strcmp(type, "show.status") == 0) {
        char buf[192];
        snprintf(buf, sizeof(buf),
            "{\"playing\":false,\"showId\":null,\"paused\":false,"
            "\"dynamicShowCount\":%u,\"dynamicShowRamBytes\":%u}",
            (unsigned)serialShowStore.count(),
            (unsigned)serialShowStore.totalRamUsage());
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // narrative.setPhase  (set narrative phase, tension, duration)
    // ------------------------------------------------------------------
    else if (strcmp(type, "narrative.setPhase") == 0 || strcmp(type, "narrative.config") == 0) {
        NarrativeEngine& narrative = NarrativeEngine::getInstance();

        if (doc.containsKey("enabled")) {
            bool enabled = doc["enabled"];
            if (enabled) narrative.enable(); else narrative.disable();
        }

        if (doc.containsKey("phase")) {
            uint8_t phase = doc["phase"];
            uint32_t phaseDuration = doc["phaseDuration"] | 0u;
            // Phase values: 0=BUILD, 1=HOLD, 2=RELEASE, 3=REST
            if (phase <= 3) {
                narrative.setPhase(static_cast<NarrativePhase>(phase), phaseDuration);
            }
        }

        if (doc.containsKey("tension")) {
            float tension = doc["tension"].as<float>();
            narrative.setTensionOverride(tension);
        }

        if (doc.containsKey("buildDuration")) {
            narrative.setBuildDuration(doc["buildDuration"].as<uint32_t>());
        }
        if (doc.containsKey("holdDuration")) {
            narrative.setHoldDuration(doc["holdDuration"].as<uint32_t>());
        }
        if (doc.containsKey("releaseDuration")) {
            narrative.setReleaseDuration(doc["releaseDuration"].as<uint32_t>());
        }
        if (doc.containsKey("restDuration")) {
            narrative.setRestDuration(doc["restDuration"].as<uint32_t>());
        }

        NarrativePhase phase = narrative.getPhase();
        const char* phaseName = "BUILD";
        switch (phase) {
            case PHASE_BUILD:   phaseName = "BUILD"; break;
            case PHASE_HOLD:    phaseName = "HOLD"; break;
            case PHASE_RELEASE: phaseName = "RELEASE"; break;
            case PHASE_REST:    phaseName = "REST"; break;
        }

        char buf[192];
        snprintf(buf, sizeof(buf),
            "{\"enabled\":%s,\"phase\":\"%s\",\"tension\":%.2f,\"totalDuration\":%lu}",
            narrative.isEnabled() ? "true" : "false",
            phaseName,
            narrative.getTension(),
            (unsigned long)narrative.getTotalDuration());
        serialJsonResponse(type, reqId, buf);
    }
#if FEATURE_AUDIO_SYNC
    // ------------------------------------------------------------------
    // trinity.sync  (transport sync: play/pause/seek)
    // ------------------------------------------------------------------
    else if (strcmp(type, "trinity.sync") == 0) {
        const char* actionStr = doc["action"] | "";
        float positionSec = doc["position_sec"] | 0.0f;
        float bpm = doc["bpm"] | 120.0f;

        uint8_t action = 255;
        if (strcmp(actionStr, "start") == 0) action = 0;
        else if (strcmp(actionStr, "stop") == 0) action = 1;
        else if (strcmp(actionStr, "pause") == 0) action = 2;
        else if (strcmp(actionStr, "resume") == 0) action = 3;
        else if (strcmp(actionStr, "seek") == 0) action = 4;

        if (action == 255) { serialJsonError(reqId, "invalid action"); return; }
        if (positionSec < 0.0f) { serialJsonError(reqId, "position_sec must be >= 0"); return; }

        bool sent = actors.trinitySync(action, positionSec, bpm);
        char buf[96];
        snprintf(buf, sizeof(buf), "{\"action\":\"%s\",\"dispatched\":%s}",
                 actionStr, sent ? "true" : "false");
        serialJsonResponse(type, reqId, buf);
    }
    // ------------------------------------------------------------------
    // trinity.macro  (macro parameter updates at 30Hz)
    // ------------------------------------------------------------------
    else if (strcmp(type, "trinity.macro") == 0) {
        float energy = doc["energy"] | 0.0f;
        float vocal = doc["vocal_presence"] | 0.0f;
        float bass = doc["bass_weight"] | 0.0f;
        float perc = doc["percussiveness"] | 0.0f;
        float bright = doc["brightness"] | 0.0f;

        // Clamp to [0,1]
        energy = fmaxf(0.0f, fminf(1.0f, energy));
        vocal  = fmaxf(0.0f, fminf(1.0f, vocal));
        bass   = fmaxf(0.0f, fminf(1.0f, bass));
        perc   = fmaxf(0.0f, fminf(1.0f, perc));
        bright = fmaxf(0.0f, fminf(1.0f, bright));

        actors.trinityMacro(energy, vocal, bass, perc, bright);
        // No response for high-frequency macro updates (fire-and-forget)
    }
    // ------------------------------------------------------------------
    // trinity.beat  (beat clock updates)
    // ------------------------------------------------------------------
    else if (strcmp(type, "trinity.beat") == 0) {
        float bpm = doc["bpm"] | 120.0f;
        float beatPhase = doc["beat_phase"] | 0.0f;
        bool tick = doc["tick"] | false;
        bool downbeat = doc["downbeat"] | false;
        int beatInBar = doc["beat_in_bar"] | 0;

        if (bpm >= 30.0f && bpm <= 300.0f && beatPhase >= 0.0f && beatPhase < 1.0f) {
            actors.trinityBeat(bpm, beatPhase, tick, downbeat, beatInBar);
        }
        // No response for high-frequency beat updates (fire-and-forget)
    }
#endif // FEATURE_AUDIO_SYNC
    // ------------------------------------------------------------------
    // Unknown command
    // ------------------------------------------------------------------
    else {
        serialJsonError(reqId, "unknown command type");
    }
}

} // namespace serial
} // namespace lightwaveos
