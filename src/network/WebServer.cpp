#include "WebServer.h"

#if FEATURE_WEB_SERVER

#include <ESPmDNS.h>
#include <FastLED.h>

#include "OpenApiSpec.h"
#include "OpenApiSpecV2.h"
#include "WebServerV2.h"
#include "../effects.h"
#include "../effects/EffectMetadata.h"
#include "../effects/transitions/TransitionEngine.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/engines/ColorEngine.h"
#include "../effects/engines/MotionEngine.h"
#include "../config/hardware_config.h"
#include "../hardware/PerformanceMonitor.h"

// Palette externs (avoid including full PROGMEM headers)
extern const TProgmemRGBGradientPaletteRef gMasterPalettes[];
extern const uint8_t gMasterPaletteCount;
extern const char* MasterPaletteNames[];

// External references from main.cpp (keep these minimal and explicit)
extern uint8_t currentEffect;
extern uint8_t effectSpeed;
extern uint8_t effectIntensity, effectSaturation, effectComplexity, effectVariation;
extern uint8_t currentPaletteIndex;
extern uint8_t fadeAmount;
extern CRGB leds[];
extern CRGB transitionSourceBuffer[];
extern TransitionEngine transitionEngine;
extern ZoneComposer zoneComposer;
extern PerformanceMonitor perfMon;

// Global instance
LightwaveWebServer webServer;

LightwaveWebServer::LightwaveWebServer() = default;

LightwaveWebServer::~LightwaveWebServer() {
    stop();
}

bool LightwaveWebServer::begin() {
    Serial.println("[WebServer] Starting ESP-IDF httpd web server (robust backend)");
    
    if (!SPIFFS.begin(true)) {
        Serial.println("[WebServer] Failed to mount SPIFFS");
        return false;
    }
    
    IdfHttpServer::Config cfg{};
    cfg.port = NetworkConfig::WEB_SERVER_PORT;
    cfg.maxOpenSockets = 8;
    cfg.maxReqBodyBytes = 2048;
    cfg.maxWsFrameBytes = 2048;

    if (!m_http.begin(cfg)) {
        Serial.println("[WebServer] httpd_start failed");
        return false;
    }

    m_http.setWsHandlers(&LightwaveWebServer::onWsClientEvent,
                         &LightwaveWebServer::onWsMessage,
                         this);

    registerRoutes();

    m_running = true;
    startMDNS();
    
    Serial.printf("[WebServer] httpd started on port %u\n", (unsigned)NetworkConfig::WEB_SERVER_PORT);
    return true;
}

void LightwaveWebServer::stop() {
    if (!m_running) return;
    stopMDNS();
    m_http.stop();
    m_running = false;
}

void LightwaveWebServer::update() {
    // httpd is event-driven; nothing required per-loop for basic operation.
    // Keep a heartbeat hook for future WS status broadcasts.
    const uint32_t now = millis();
    if (now - m_lastHeartbeatMs > 1000) {
        m_lastHeartbeatMs = now;
    }
}

void LightwaveWebServer::sendLEDUpdate() {
    // Future: optional binary streaming channel; left intentionally blank in Phase 2 skeleton.
}

void LightwaveWebServer::notifyEffectChange(uint8_t effectId) {
    // API v2 docs event: effects.changed
    cJSON* ev2 = cJSON_CreateObject();
    cJSON_AddStringToObject(ev2, "type", "effects.changed");
    cJSON_AddNumberToObject(ev2, "effectId", effectId);
    cJSON_AddStringToObject(ev2, "name", effects[effectId].name);
    cJSON_AddNumberToObject(ev2, "timestamp", millis());
    char* out = cJSON_PrintUnformatted(ev2);
    if (out) {
        m_http.wsBroadcastText(out, strlen(out));
        cJSON_free(out);
    }
    cJSON_Delete(ev2);

    // Legacy v2 project event: effectChanged
    cJSON* legacy = cJSON_CreateObject();
    cJSON_AddStringToObject(legacy, "type", "effectChanged");
    cJSON_AddNumberToObject(legacy, "effectId", effectId);
    cJSON_AddStringToObject(legacy, "name", effects[effectId].name);
    char* out2 = cJSON_PrintUnformatted(legacy);
    if (out2) {
        m_http.wsBroadcastText(out2, strlen(out2));
        cJSON_free(out2);
    }
    cJSON_Delete(legacy);
}

void LightwaveWebServer::notifyError(const String& message) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "error");
    cJSON_AddStringToObject(root, "message", message.c_str());
    char* out = cJSON_PrintUnformatted(root);
    if (out) {
        m_http.wsBroadcastText(out, strlen(out));
        cJSON_free(out);
    }
    cJSON_Delete(root);
}

void LightwaveWebServer::startMDNS() {
    if (m_mdnsStarted) return;
    if (WiFi.status() != WL_CONNECTED && !(WiFi.getMode() & WIFI_MODE_AP)) {
            return;
        }

        if (MDNS.begin(NetworkConfig::MDNS_HOSTNAME)) {
            MDNS.addService("http", "tcp", NetworkConfig::WEB_SERVER_PORT);
            MDNS.addService("ws", "tcp", NetworkConfig::WEB_SERVER_PORT);
        m_mdnsStarted = true;
        Serial.printf("[WebServer] mDNS started: http://%s.local\n", NetworkConfig::MDNS_HOSTNAME);
    }
}

void LightwaveWebServer::stopMDNS() {
    if (!m_mdnsStarted) return;
    MDNS.end();
    m_mdnsStarted = false;
}

void LightwaveWebServer::registerRoutes() {
    // ====================================================================
    // API v1 Routes (legacy, maintained for backward compatibility)
    // ====================================================================
    m_http.registerGet("/api/v1/", &LightwaveWebServer::handleApiDiscovery);
    m_http.registerOptions("/api/v1/", &IdfHttpServer::corsOptionsHandler);

    // OpenAPI spec (compat: /spec and /openapi.json)
    m_http.registerGet("/api/v1/spec", &LightwaveWebServer::handleOpenApi);
    m_http.registerGet("/api/v1/openapi.json", &LightwaveWebServer::handleOpenApi);

    // Core endpoints
    m_http.registerGet("/api/v1/device/status", &LightwaveWebServer::handleDeviceStatus);
    m_http.registerGet("/api/v1/effects", &LightwaveWebServer::handleEffectsList);
    m_http.registerGet("/api/v1/effects/current", &LightwaveWebServer::handleEffectsCurrent);

    // ====================================================================
    // API v2 Routes (Zone Composer + Transitions)
    // ====================================================================
    if (!WebServerV2::registerV2Routes(m_http)) {
        Serial.println("[WebServer] Warning: Some v2 routes failed to register");
    } else {
        Serial.println("[WebServer] API v2 routes registered (transitions, zones, batch)");
    }

    // ====================================================================
    // Static UI (best-effort)
    // ====================================================================
    m_http.registerGet("/", &LightwaveWebServer::handleStaticRoot);
    // Asset handler - fixed set of known files
    m_http.registerGet("/app.js", &LightwaveWebServer::handleStaticAsset);
    m_http.registerGet("/styles.css", &LightwaveWebServer::handleStaticAsset);
}

void LightwaveWebServer::onWsClientEvent(int clientFd, bool connected, void* ctx) {
    auto* self = static_cast<LightwaveWebServer*>(ctx);
    if (!self) return;
    if (connected) self->m_wsClientCount++;
    else if (self->m_wsClientCount > 0) self->m_wsClientCount--;

    // On connect: emit legacy initial state to the connecting client (helps older dashboards).
    if (connected && clientFd >= 0) {
        // Legacy: status
        cJSON* status = cJSON_CreateObject();
        cJSON_AddStringToObject(status, "type", "status");
        cJSON_AddNumberToObject(status, "effectId", currentEffect);
        cJSON_AddStringToObject(status, "effectName", effects[currentEffect].name);
        cJSON_AddNumberToObject(status, "brightness", FastLED.getBrightness());
        cJSON_AddNumberToObject(status, "speed", effectSpeed);
        cJSON_AddNumberToObject(status, "paletteId", currentPaletteIndex);
        cJSON_AddNumberToObject(status, "hue", 0);
        cJSON_AddNumberToObject(status, "fps", perfMon.getCurrentFPS());
        cJSON_AddNumberToObject(status, "cpuPercent", perfMon.getCPUUsage());
        cJSON_AddNumberToObject(status, "freeHeap", ESP.getFreeHeap());
        cJSON_AddNumberToObject(status, "uptime", millis() / 1000);
        char* s = cJSON_PrintUnformatted(status);
        if (s) {
            self->m_http.wsSendText(clientFd, s, strlen(s));
            cJSON_free(s);
        }
        cJSON_Delete(status);

        // Legacy: zone.state
        cJSON* zs = cJSON_CreateObject();
        cJSON_AddStringToObject(zs, "type", "zone.state");
        cJSON_AddBoolToObject(zs, "enabled", zoneComposer.isEnabled());
        cJSON_AddNumberToObject(zs, "zoneCount", zoneComposer.getZoneCount());
        cJSON* zones = cJSON_AddArrayToObject(zs, "zones");
        for (uint8_t i = 0; i < zoneComposer.getZoneCount(); i++) {
            cJSON* z = cJSON_CreateObject();
            cJSON_AddNumberToObject(z, "id", i);
            cJSON_AddBoolToObject(z, "enabled", zoneComposer.isZoneEnabled(i));
            cJSON_AddNumberToObject(z, "effectId", zoneComposer.getZoneEffect(i));
            cJSON_AddNumberToObject(z, "brightness", zoneComposer.getZoneBrightness(i));
            cJSON_AddNumberToObject(z, "speed", zoneComposer.getZoneSpeed(i));
            cJSON_AddNumberToObject(z, "paletteId", zoneComposer.getZonePalette(i));
            cJSON_AddItemToArray(zones, z);
        }
        char* zso = cJSON_PrintUnformatted(zs);
        if (zso) {
            self->m_http.wsSendText(clientFd, zso, strlen(zso));
            cJSON_free(zso);
        }
        cJSON_Delete(zs);
    }
}

// ============================================================================
// WebSocket v2 Command Handlers
// ============================================================================

// Forward declarations for v2 handlers
static void handleV2DeviceGetStatus(LightwaveWebServer* self);
static void handleV2DeviceGetInfo(LightwaveWebServer* self);
static void handleV2EffectsList(LightwaveWebServer* self, cJSON* data);
static void handleV2EffectsGetCurrent(LightwaveWebServer* self);
static void handleV2EffectsSetCurrent(LightwaveWebServer* self, cJSON* data);
static void handleV2EffectsGetMetadata(LightwaveWebServer* self, cJSON* data);
static void handleV2ParametersGet(LightwaveWebServer* self);
static void handleV2ParametersSet(LightwaveWebServer* self, cJSON* data);
static void handleV2TransitionsList(LightwaveWebServer* self);
static void handleV2TransitionsTrigger(LightwaveWebServer* self, cJSON* data);
static void handleV2ZonesList(LightwaveWebServer* self);
static void handleV2ZonesGet(LightwaveWebServer* self, cJSON* data);
static void handleV2ZonesUpdate(LightwaveWebServer* self, cJSON* data);
static void handleV2ZonesSetEffect(LightwaveWebServer* self, cJSON* data);
static void handleV2Batch(LightwaveWebServer* self, cJSON* data);

// Helper to send v2 response
static int s_wsReplyClientFd = -1; // set per-message by onWsMessage; -1 means broadcast
static void sendV2Response(LightwaveWebServer* self, const char* type, bool success, cJSON* data, const char* error = nullptr) {
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", type);
    cJSON_AddBoolToObject(resp, "success", success);

    if (data) {
        cJSON_AddItemToObject(resp, "data", data);
    }

    if (error) {
        // Docs format: error is an object with code/message.
        cJSON* err = cJSON_AddObjectToObject(resp, "error");
        cJSON_AddStringToObject(err, "code", "INTERNAL_ERROR");
        cJSON_AddStringToObject(err, "message", error);
    }

    cJSON_AddNumberToObject(resp, "timestamp", millis());
    cJSON_AddStringToObject(resp, "version", "2.0.0");

    char* out = cJSON_PrintUnformatted(resp);
    if (out) {
        if (s_wsReplyClientFd >= 0) {
            self->sendWsToClient(s_wsReplyClientFd, out, strlen(out));
                } else {
            self->broadcastWs(out, strlen(out));
        }
        cJSON_free(out);
    }
    cJSON_Delete(resp);
}

// ============================================================================
// Device Handlers
// ============================================================================

static void handleV2DeviceGetStatus(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "uptime", millis() / 1000);
    cJSON_AddNumberToObject(data, "freeHeap", ESP.getFreeHeap());
    cJSON_AddNumberToObject(data, "cpuFreq", ESP.getCpuFreqMHz());
    cJSON_AddNumberToObject(data, "wsClients", (double)self->getClientCount());

    sendV2Response(self, "device.status", true, data);
}

static void handleV2DeviceGetInfo(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "firmware", "LightwaveOS");
    cJSON_AddStringToObject(data, "version", "2.0.0");
    cJSON_AddStringToObject(data, "sdkVersion", ESP.getSdkVersion());
    cJSON_AddNumberToObject(data, "flashSize", ESP.getFlashChipSize());
    cJSON_AddNumberToObject(data, "flashSpeed", ESP.getFlashChipSpeed());
    cJSON_AddStringToObject(data, "chipModel", ESP.getChipModel());
    cJSON_AddNumberToObject(data, "chipCores", ESP.getChipCores());
    cJSON_AddNumberToObject(data, "chipRevision", ESP.getChipRevision());

    // Hardware config
    cJSON* hw = cJSON_AddObjectToObject(data, "hardware");
    cJSON_AddNumberToObject(hw, "ledsTotal", HardwareConfig::NUM_LEDS);
    cJSON_AddNumberToObject(hw, "strips", 2);
    cJSON_AddNumberToObject(hw, "ledsPerStrip", HardwareConfig::STRIP_LENGTH);
    cJSON_AddNumberToObject(hw, "centerPoint", HardwareConfig::STRIP_CENTER_POINT);
    cJSON_AddNumberToObject(hw, "maxZones", HardwareConfig::MAX_ZONES);

    sendV2Response(self, "device.info", true, data);
}

// ============================================================================
// Effects Handlers
// ============================================================================

static void handleV2EffectsList(LightwaveWebServer* self, cJSON* data) {
    // Docs (API_V2.md): page is 1-based, limit max 50, details optional.
    int page = 1;
    int limit = 20;
    bool details = false;

    if (data) {
        cJSON* pageNode = cJSON_GetObjectItemCaseSensitive(data, "page");
        if (pageNode && cJSON_IsNumber(pageNode)) {
            page = (int)pageNode->valuedouble;
        }
        cJSON* limitNode = cJSON_GetObjectItemCaseSensitive(data, "limit");
        if (limitNode && cJSON_IsNumber(limitNode)) {
            limit = constrain((int)limitNode->valuedouble, 1, 50);
        }
        cJSON* detailsNode = cJSON_GetObjectItemCaseSensitive(data, "details");
        if (detailsNode && cJSON_IsBool(detailsNode)) {
            details = cJSON_IsTrue(detailsNode);
        }
    }
    if (page < 1) page = 1;

    cJSON* respData = cJSON_CreateObject();
    cJSON* effectsArr = cJSON_AddArrayToObject(respData, "effects");

    int startIdx = (page - 1) * limit;
    int endIdx = min(startIdx + limit, (int)NUM_EFFECTS);

    char categoryBuf[24];
    char descBuf[80];

    for (int i = startIdx; i < endIdx; i++) {
        cJSON* e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "id", i);
        cJSON_AddStringToObject(e, "name", effects[i].name);

        if (details) {
            // Add category/description when requested
            getEffectCategoryName(i, categoryBuf, sizeof(categoryBuf));
            cJSON_AddStringToObject(e, "category", categoryBuf);
            getEffectDescription(i, descBuf, sizeof(descBuf));
            cJSON_AddStringToObject(e, "description", descBuf);
        }

        cJSON_AddItemToArray(effectsArr, e);
    }

    // Pagination metadata
    cJSON* pagination = cJSON_AddObjectToObject(respData, "pagination");
    cJSON_AddNumberToObject(pagination, "page", page);
    cJSON_AddNumberToObject(pagination, "limit", limit);
    cJSON_AddNumberToObject(pagination, "total", NUM_EFFECTS);

    sendV2Response(self, "effects.list", true, respData);
}

static void handleV2EffectsGetCurrent(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "effectId", currentEffect);
    cJSON_AddStringToObject(data, "name", effects[currentEffect].name);
    cJSON_AddNumberToObject(data, "brightness", FastLED.getBrightness());
    cJSON_AddNumberToObject(data, "speed", effectSpeed);
    cJSON_AddNumberToObject(data, "paletteId", currentPaletteIndex);

    // Visual parameters
    cJSON* params = cJSON_AddObjectToObject(data, "parameters");
    cJSON_AddNumberToObject(params, "intensity", effectIntensity);
    cJSON_AddNumberToObject(params, "saturation", effectSaturation);
    cJSON_AddNumberToObject(params, "complexity", effectComplexity);
    cJSON_AddNumberToObject(params, "variation", effectVariation);

    // Effect metadata
    char categoryBuf[24];
    char descBuf[80];
    getEffectCategoryName(currentEffect, categoryBuf, sizeof(categoryBuf));
    getEffectDescription(currentEffect, descBuf, sizeof(descBuf));

    cJSON* meta = cJSON_AddObjectToObject(data, "metadata");
    cJSON_AddStringToObject(meta, "category", categoryBuf);
    cJSON_AddStringToObject(meta, "description", descBuf);

    EffectMeta effectMeta = getEffectMeta(currentEffect);
    cJSON_AddBoolToObject(meta, "centerOrigin", (effectMeta.features & EffectFeatures::CENTER_ORIGIN) != 0);
    cJSON_AddBoolToObject(meta, "usesPalette", (effectMeta.features & EffectFeatures::USES_PALETTE) != 0);
    cJSON_AddBoolToObject(meta, "usesSpeed", (effectMeta.features & EffectFeatures::USES_SPEED) != 0);
    cJSON_AddBoolToObject(meta, "physicsBased", (effectMeta.features & EffectFeatures::PHYSICS_BASED) != 0);

    sendV2Response(self, "effects.current", true, data);
}

static void handleV2EffectsSetCurrent(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "effects.current", false, nullptr, "Missing data object");
        return;
    }

    cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(data, "effectId");
    if (!effectIdNode || !cJSON_IsNumber(effectIdNode)) {
        sendV2Response(self, "effects.current", false, nullptr, "Missing effectId");
                    return;
                }

    uint8_t newEffectId = (uint8_t)effectIdNode->valuedouble;
    if (newEffectId >= NUM_EFFECTS) {
        sendV2Response(self, "effects.current", false, nullptr, "Invalid effectId");
                        return;
                    }

    // Update the effect
    currentEffect = newEffectId;

    // Respond with current state
    handleV2EffectsGetCurrent(self);
}

static void handleV2EffectsGetMetadata(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "effects.metadata", false, nullptr, "Missing data object");
        return;
    }

    cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(data, "effectId");
    if (!effectIdNode || !cJSON_IsNumber(effectIdNode)) {
        sendV2Response(self, "effects.metadata", false, nullptr, "Missing effectId");
        return;
    }

    uint8_t effectId = (uint8_t)effectIdNode->valuedouble;
    if (effectId >= NUM_EFFECTS) {
        sendV2Response(self, "effects.metadata", false, nullptr, "Invalid effectId");
        return;
    }

    cJSON* respData = cJSON_CreateObject();
    cJSON_AddNumberToObject(respData, "effectId", effectId);
    cJSON_AddStringToObject(respData, "name", effects[effectId].name);

    char categoryBuf[24];
    char descBuf[80];
    getEffectCategoryName(effectId, categoryBuf, sizeof(categoryBuf));
    getEffectDescription(effectId, descBuf, sizeof(descBuf));

    cJSON_AddStringToObject(respData, "category", categoryBuf);
    cJSON_AddStringToObject(respData, "description", descBuf);

    // Feature flags
    EffectMeta meta = getEffectMeta(effectId);
    cJSON* features = cJSON_AddObjectToObject(respData, "features");
    cJSON_AddBoolToObject(features, "centerOrigin", (meta.features & EffectFeatures::CENTER_ORIGIN) != 0);
    cJSON_AddBoolToObject(features, "usesSpeed", (meta.features & EffectFeatures::USES_SPEED) != 0);
    cJSON_AddBoolToObject(features, "usesPalette", (meta.features & EffectFeatures::USES_PALETTE) != 0);
    cJSON_AddBoolToObject(features, "zoneAware", (meta.features & EffectFeatures::ZONE_AWARE) != 0);
    cJSON_AddBoolToObject(features, "dualStrip", (meta.features & EffectFeatures::DUAL_STRIP) != 0);
    cJSON_AddBoolToObject(features, "physicsBased", (meta.features & EffectFeatures::PHYSICS_BASED) != 0);
    cJSON_AddBoolToObject(features, "audioReactive", (meta.features & EffectFeatures::AUDIO_REACTIVE) != 0);

    // Custom parameters
    cJSON* params = cJSON_AddArrayToObject(respData, "customParams");
    for (uint8_t i = 0; i < meta.paramCount; i++) {
        cJSON* param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "name", meta.params[i].name);
        cJSON_AddNumberToObject(param, "min", meta.params[i].minVal);
        cJSON_AddNumberToObject(param, "max", meta.params[i].maxVal);
        cJSON_AddNumberToObject(param, "default", meta.params[i].defaultVal);
        cJSON_AddNumberToObject(param, "target", (int)meta.params[i].target);
        cJSON_AddItemToArray(params, param);
    }

    sendV2Response(self, "effects.metadata", true, respData);
}

// ============================================================================
// Parameters Handlers
// ============================================================================

static void handleV2ParametersGet(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "brightness", FastLED.getBrightness());
    cJSON_AddNumberToObject(data, "speed", effectSpeed);
    cJSON_AddNumberToObject(data, "paletteId", currentPaletteIndex);
    cJSON_AddNumberToObject(data, "intensity", effectIntensity);
    cJSON_AddNumberToObject(data, "saturation", effectSaturation);
    cJSON_AddNumberToObject(data, "complexity", effectComplexity);
    cJSON_AddNumberToObject(data, "variation", effectVariation);
    cJSON_AddNumberToObject(data, "fadeAmount", fadeAmount);

    // Docs: response type is "parameters"
    sendV2Response(self, "parameters", true, data);
}

static void handleV2ParametersSet(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "parameters.current", false, nullptr, "Missing data object");
                    return;
                }

    bool changed = false;

    cJSON* node = cJSON_GetObjectItemCaseSensitive(data, "brightness");
    if (node && cJSON_IsNumber(node)) {
        FastLED.setBrightness((uint8_t)node->valuedouble);
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "speed");
    if (node && cJSON_IsNumber(node)) {
        effectSpeed = constrain((uint8_t)node->valuedouble, 1, 50);
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "paletteId");
    if (node && cJSON_IsNumber(node)) {
        currentPaletteIndex = (uint8_t)node->valuedouble;
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "intensity");
    if (node && cJSON_IsNumber(node)) {
        effectIntensity = (uint8_t)node->valuedouble;
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "saturation");
    if (node && cJSON_IsNumber(node)) {
        effectSaturation = (uint8_t)node->valuedouble;
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "complexity");
    if (node && cJSON_IsNumber(node)) {
        effectComplexity = (uint8_t)node->valuedouble;
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "variation");
    if (node && cJSON_IsNumber(node)) {
        effectVariation = (uint8_t)node->valuedouble;
        changed = true;
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "fadeAmount");
    if (node && cJSON_IsNumber(node)) {
        fadeAmount = (uint8_t)node->valuedouble;
        changed = true;
    }

    // Respond with current state
    handleV2ParametersGet(self);
}

// ============================================================================
// Transitions Handlers
// ============================================================================

static const char* getTransitionName(TransitionType type) {
    switch (type) {
        case TRANSITION_FADE: return "Fade";
        case TRANSITION_WIPE_OUT: return "Wipe Out";
        case TRANSITION_WIPE_IN: return "Wipe In";
        case TRANSITION_DISSOLVE: return "Dissolve";
        case TRANSITION_PHASE_SHIFT: return "Phase Shift";
        case TRANSITION_PULSEWAVE: return "Pulse Wave";
        case TRANSITION_IMPLOSION: return "Implosion";
        case TRANSITION_IRIS: return "Iris";
        case TRANSITION_NUCLEAR: return "Nuclear";
        case TRANSITION_STARGATE: return "Stargate";
        case TRANSITION_KALEIDOSCOPE: return "Kaleidoscope";
        case TRANSITION_MANDALA: return "Mandala";
        default: return "Unknown";
    }
}

static void handleV2TransitionsList(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON* types = cJSON_AddArrayToObject(data, "types");

    for (int i = 0; i < TRANSITION_COUNT; i++) {
        cJSON* t = cJSON_CreateObject();
        cJSON_AddNumberToObject(t, "id", i);
        cJSON_AddStringToObject(t, "name", getTransitionName((TransitionType)i));
        cJSON_AddBoolToObject(t, "centerOrigin", true); // All transitions are CENTER ORIGIN
        cJSON_AddItemToArray(types, t);
    }

    cJSON_AddNumberToObject(data, "count", TRANSITION_COUNT);
    cJSON_AddNumberToObject(data, "defaultDuration", 1000);

    sendV2Response(self, "transitions.list", true, data);
}

static void handleV2TransitionsTrigger(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "transitions.triggered", false, nullptr, "Missing data object");
        return;
    }

    // Get target effect
    cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(data, "targetEffectId");
    if (!effectIdNode || !cJSON_IsNumber(effectIdNode)) {
        sendV2Response(self, "transitions.triggered", false, nullptr, "Missing targetEffectId");
        return;
    }

    uint8_t targetEffect = (uint8_t)effectIdNode->valuedouble;
    if (targetEffect >= NUM_EFFECTS) {
        sendV2Response(self, "transitions.triggered", false, nullptr, "Invalid targetEffectId");
        return;
    }

    // Get transition type (optional, defaults to random)
    TransitionType transType = TransitionEngine::getRandomTransition();
    cJSON* typeNode = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (typeNode && cJSON_IsNumber(typeNode)) {
        int typeVal = (int)typeNode->valuedouble;
        if (typeVal >= 0 && typeVal < TRANSITION_COUNT) {
            transType = (TransitionType)typeVal;
        }
    }

    // Get duration (optional, defaults to 1000ms)
    uint32_t duration = 1000;
    cJSON* durationNode = cJSON_GetObjectItemCaseSensitive(data, "duration");
    if (durationNode && cJSON_IsNumber(durationNode)) {
        duration = constrain((uint32_t)durationNode->valuedouble, 100, 5000);
    }

    // Store current state before transition
    memcpy(transitionSourceBuffer, leds, HardwareConfig::NUM_LEDS * sizeof(CRGB));

    // Update effect
    currentEffect = targetEffect;

    // Start transition (main loop will handle the actual transition)
    transitionEngine.startTransition(
        transitionSourceBuffer,
        leds,
        leds,
        transType,
        duration,
        EASE_IN_OUT_QUAD
    );

    // Response
    cJSON* respData = cJSON_CreateObject();
    cJSON_AddNumberToObject(respData, "targetEffectId", targetEffect);
    cJSON_AddStringToObject(respData, "effectName", effects[targetEffect].name);
    cJSON_AddNumberToObject(respData, "transitionType", transType);
    cJSON_AddStringToObject(respData, "transitionName", getTransitionName(transType));
    cJSON_AddNumberToObject(respData, "duration", duration);

    sendV2Response(self, "transitions.triggered", true, respData);
}

// ============================================================================
// Zones Handlers
// ============================================================================

static void handleV2ZonesList(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "enabled", zoneComposer.isEnabled());
    cJSON_AddNumberToObject(data, "zoneCount", zoneComposer.getZoneCount());
    cJSON_AddNumberToObject(data, "maxZones", HardwareConfig::MAX_ZONES);

    cJSON* zones = cJSON_AddArrayToObject(data, "zones");

    for (uint8_t i = 0; i < zoneComposer.getZoneCount(); i++) {
        cJSON* z = cJSON_CreateObject();
        cJSON_AddNumberToObject(z, "id", i);
        cJSON_AddBoolToObject(z, "enabled", zoneComposer.isZoneEnabled(i));
        cJSON_AddNumberToObject(z, "effectId", zoneComposer.getZoneEffect(i));

        if (zoneComposer.getZoneEffect(i) < NUM_EFFECTS) {
            cJSON_AddStringToObject(z, "effectName", effects[zoneComposer.getZoneEffect(i)].name);
        }

        cJSON_AddNumberToObject(z, "brightness", zoneComposer.getZoneBrightness(i));
        cJSON_AddNumberToObject(z, "speed", zoneComposer.getZoneSpeed(i));
        cJSON_AddNumberToObject(z, "paletteId", zoneComposer.getZonePalette(i));
        cJSON_AddNumberToObject(z, "blendMode", (int)zoneComposer.getZoneBlendMode(i));

        // Visual params
        cJSON* params = cJSON_AddObjectToObject(z, "visualParams");
        VisualParams vp = zoneComposer.getZoneVisualParams(i);
        cJSON_AddNumberToObject(params, "intensity", vp.intensity);
        cJSON_AddNumberToObject(params, "saturation", vp.saturation);
        cJSON_AddNumberToObject(params, "complexity", vp.complexity);
        cJSON_AddNumberToObject(params, "variation", vp.variation);

        cJSON_AddItemToArray(zones, z);
    }

    sendV2Response(self, "zones.list", true, data);
}

static void handleV2ZonesGet(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "zones.zone", false, nullptr, "Missing data object");
            return;
        }

    cJSON* zoneIdNode = cJSON_GetObjectItemCaseSensitive(data, "zoneId");
    if (!zoneIdNode || !cJSON_IsNumber(zoneIdNode)) {
        sendV2Response(self, "zones.zone", false, nullptr, "Missing zoneId");
            return;
        }

    uint8_t zoneId = (uint8_t)zoneIdNode->valuedouble;
    if (zoneId >= zoneComposer.getZoneCount()) {
        sendV2Response(self, "zones.zone", false, nullptr, "Invalid zoneId");
            return;
        }

    cJSON* respData = cJSON_CreateObject();
    cJSON_AddNumberToObject(respData, "id", zoneId);
    cJSON_AddBoolToObject(respData, "enabled", zoneComposer.isZoneEnabled(zoneId));
    cJSON_AddNumberToObject(respData, "effectId", zoneComposer.getZoneEffect(zoneId));

    if (zoneComposer.getZoneEffect(zoneId) < NUM_EFFECTS) {
        cJSON_AddStringToObject(respData, "effectName", effects[zoneComposer.getZoneEffect(zoneId)].name);
    }

    cJSON_AddNumberToObject(respData, "brightness", zoneComposer.getZoneBrightness(zoneId));
    cJSON_AddNumberToObject(respData, "speed", zoneComposer.getZoneSpeed(zoneId));
    cJSON_AddNumberToObject(respData, "paletteId", zoneComposer.getZonePalette(zoneId));
    cJSON_AddNumberToObject(respData, "blendMode", (int)zoneComposer.getZoneBlendMode(zoneId));

    // Visual params
    cJSON* params = cJSON_AddObjectToObject(respData, "visualParams");
    VisualParams vp = zoneComposer.getZoneVisualParams(zoneId);
    cJSON_AddNumberToObject(params, "intensity", vp.intensity);
    cJSON_AddNumberToObject(params, "saturation", vp.saturation);
    cJSON_AddNumberToObject(params, "complexity", vp.complexity);
    cJSON_AddNumberToObject(params, "variation", vp.variation);

    sendV2Response(self, "zones.zone", true, respData);
}

static void handleV2ZonesUpdate(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "zones.updated", false, nullptr, "Missing data object");
        return;
    }

    cJSON* zoneIdNode = cJSON_GetObjectItemCaseSensitive(data, "zoneId");
    if (!zoneIdNode || !cJSON_IsNumber(zoneIdNode)) {
        sendV2Response(self, "zones.updated", false, nullptr, "Missing zoneId");
        return;
    }

    uint8_t zoneId = (uint8_t)zoneIdNode->valuedouble;
    if (zoneId >= HardwareConfig::MAX_ZONES) {
        sendV2Response(self, "zones.updated", false, nullptr, "Invalid zoneId");
        return;
    }

    // Update zone properties
    cJSON* node = cJSON_GetObjectItemCaseSensitive(data, "enabled");
    if (node && cJSON_IsBool(node)) {
        zoneComposer.enableZone(zoneId, cJSON_IsTrue(node));
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "brightness");
    if (node && cJSON_IsNumber(node)) {
        zoneComposer.setZoneBrightness(zoneId, (uint8_t)node->valuedouble);
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "speed");
    if (node && cJSON_IsNumber(node)) {
        zoneComposer.setZoneSpeed(zoneId, constrain((uint8_t)node->valuedouble, 1, 50));
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "paletteId");
    if (node && cJSON_IsNumber(node)) {
        zoneComposer.setZonePalette(zoneId, (uint8_t)node->valuedouble);
    }

    node = cJSON_GetObjectItemCaseSensitive(data, "blendMode");
    if (node && cJSON_IsNumber(node)) {
        zoneComposer.setZoneBlendMode(zoneId, (BlendMode)(int)node->valuedouble);
    }

    // Visual params
    cJSON* params = cJSON_GetObjectItemCaseSensitive(data, "visualParams");
    if (params) {
        node = cJSON_GetObjectItemCaseSensitive(params, "intensity");
        if (node && cJSON_IsNumber(node)) {
            zoneComposer.setZoneIntensity(zoneId, (uint8_t)node->valuedouble);
        }

        node = cJSON_GetObjectItemCaseSensitive(params, "saturation");
        if (node && cJSON_IsNumber(node)) {
            zoneComposer.setZoneSaturation(zoneId, (uint8_t)node->valuedouble);
        }

        node = cJSON_GetObjectItemCaseSensitive(params, "complexity");
        if (node && cJSON_IsNumber(node)) {
            zoneComposer.setZoneComplexity(zoneId, (uint8_t)node->valuedouble);
        }

        node = cJSON_GetObjectItemCaseSensitive(params, "variation");
        if (node && cJSON_IsNumber(node)) {
            zoneComposer.setZoneVariation(zoneId, (uint8_t)node->valuedouble);
        }
    }

    // Return updated zone state
    handleV2ZonesGet(self, data);
}

static void handleV2ZonesSetEffect(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "zones.effectSet", false, nullptr, "Missing data object");
        return;
    }

    cJSON* zoneIdNode = cJSON_GetObjectItemCaseSensitive(data, "zoneId");
    cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(data, "effectId");

    if (!zoneIdNode || !cJSON_IsNumber(zoneIdNode)) {
        sendV2Response(self, "zones.effectSet", false, nullptr, "Missing zoneId");
        return;
    }

    if (!effectIdNode || !cJSON_IsNumber(effectIdNode)) {
        sendV2Response(self, "zones.effectSet", false, nullptr, "Missing effectId");
        return;
    }

    uint8_t zoneId = (uint8_t)zoneIdNode->valuedouble;
    uint8_t effectId = (uint8_t)effectIdNode->valuedouble;

    if (zoneId >= HardwareConfig::MAX_ZONES) {
        sendV2Response(self, "zones.effectSet", false, nullptr, "Invalid zoneId");
        return;
    }

    if (effectId >= NUM_EFFECTS) {
        sendV2Response(self, "zones.effectSet", false, nullptr, "Invalid effectId");
        return;
    }

    zoneComposer.setZoneEffect(zoneId, effectId);

    // Return updated zone state
    handleV2ZonesGet(self, data);
}

// ============================================================================
// Batch Handler
// ============================================================================

static void handleV2Batch(LightwaveWebServer* self, cJSON* data) {
    if (!data) {
        sendV2Response(self, "batch.result", false, nullptr, "Missing data object");
        return;
    }

    cJSON* operations = cJSON_GetObjectItemCaseSensitive(data, "operations");
    if (!operations || !cJSON_IsArray(operations)) {
        sendV2Response(self, "batch.result", false, nullptr, "Missing operations array");
        return;
    }

    int opCount = cJSON_GetArraySize(operations);
    if (opCount > 10) {
        sendV2Response(self, "batch.result", false, nullptr, "Max 10 operations per batch");
        return;
    }

    cJSON* results = cJSON_CreateArray();
    int successCount = 0;

    for (int i = 0; i < opCount; i++) {
        cJSON* op = cJSON_GetArrayItem(operations, i);
        cJSON* typeNode = cJSON_GetObjectItemCaseSensitive(op, "type");
        cJSON* opData = cJSON_GetObjectItemCaseSensitive(op, "data");

        cJSON* result = cJSON_CreateObject();
        cJSON_AddNumberToObject(result, "index", i);

        if (!typeNode || !cJSON_IsString(typeNode)) {
            cJSON_AddBoolToObject(result, "success", false);
            cJSON_AddStringToObject(result, "error", "Missing type");
            cJSON_AddItemToArray(results, result);
            continue;
        }

        const char* type = typeNode->valuestring;
        bool success = true;
        const char* error = nullptr;

        // Execute operation (simplified - just update state, no broadcast per-op)
        if (strcmp(type, "effects.setCurrent") == 0) {
            if (opData) {
                cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(opData, "effectId");
                if (effectIdNode && cJSON_IsNumber(effectIdNode)) {
                    uint8_t effectId = (uint8_t)effectIdNode->valuedouble;
                    if (effectId < NUM_EFFECTS) {
                        currentEffect = effectId;
                    } else {
                        success = false;
                        error = "Invalid effectId";
                    }
                }
            }
        } else if (strcmp(type, "parameters.set") == 0) {
            if (opData) {
                cJSON* node = cJSON_GetObjectItemCaseSensitive(opData, "brightness");
                if (node && cJSON_IsNumber(node)) {
                    FastLED.setBrightness((uint8_t)node->valuedouble);
                }
                node = cJSON_GetObjectItemCaseSensitive(opData, "speed");
                if (node && cJSON_IsNumber(node)) {
                    effectSpeed = constrain((uint8_t)node->valuedouble, 1, 50);
                }
            }
        } else if (strcmp(type, "zones.setEffect") == 0) {
            if (opData) {
                cJSON* zoneIdNode = cJSON_GetObjectItemCaseSensitive(opData, "zoneId");
                cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(opData, "effectId");
                if (zoneIdNode && effectIdNode) {
                    uint8_t zoneId = (uint8_t)zoneIdNode->valuedouble;
                    uint8_t effectId = (uint8_t)effectIdNode->valuedouble;
                    if (zoneId < HardwareConfig::MAX_ZONES && effectId < NUM_EFFECTS) {
                        zoneComposer.setZoneEffect(zoneId, effectId);
                    } else {
                        success = false;
                        error = "Invalid zone or effect";
                    }
                }
            }
        } else {
            success = false;
            error = "Unknown operation type";
        }

        cJSON_AddBoolToObject(result, "success", success);
        if (error) {
            cJSON_AddStringToObject(result, "error", error);
        }
        cJSON_AddItemToArray(results, result);

        if (success) successCount++;
    }

    cJSON* respData = cJSON_CreateObject();
    cJSON_AddItemToObject(respData, "results", results);
    cJSON_AddNumberToObject(respData, "totalOperations", opCount);
    cJSON_AddNumberToObject(respData, "successCount", successCount);
    cJSON_AddNumberToObject(respData, "failureCount", opCount - successCount);

    sendV2Response(self, "batch.result", true, respData);
}

// ============================================================================
// Enhancement / Palette WebSocket Handlers
// ============================================================================

static void handleV2EnhancementsSummary(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON* color = cJSON_AddObjectToObject(data, "colorEngine");
#if FEATURE_COLOR_ENGINE
    cJSON_AddBoolToObject(color, "available", true);
    cJSON_AddBoolToObject(color, "enabled", ColorEngine::getInstance().isEnabled());
#else
    cJSON_AddBoolToObject(color, "available", false);
    cJSON_AddBoolToObject(color, "enabled", false);
#endif
    cJSON* motion = cJSON_AddObjectToObject(data, "motionEngine");
#if FEATURE_MOTION_ENGINE
    cJSON_AddBoolToObject(motion, "available", true);
    cJSON_AddBoolToObject(motion, "enabled", MotionEngine::getInstance().isEnabled());
#else
    cJSON_AddBoolToObject(motion, "available", false);
    cJSON_AddBoolToObject(motion, "enabled", false);
#endif
    sendV2Response(self, "enhancements.summary", true, data);
}

static void handleV2EnhancementsColorGet(LightwaveWebServer* self) {
#if !FEATURE_COLOR_ENGINE
    sendV2Response(self, "enhancements.color", false, nullptr, "Color engine not available");
#else
    ColorEngine& ce = ColorEngine::getInstance();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "enabled", ce.isEnabled());
    cJSON* cb = cJSON_AddObjectToObject(data, "crossBlend");
    cJSON_AddBoolToObject(cb, "enabled", ce.isCrossBlendEnabled());
    const uint8_t pal1 = ce.getCrossBlendPalette1();
    const uint8_t pal2 = ce.getCrossBlendPalette2();
    const int pal3 = ce.getCrossBlendPalette3();
    cJSON_AddNumberToObject(cb, "palette1", pal1);
    cJSON_AddNumberToObject(cb, "palette2", pal2);
    if (pal3 < 0) cJSON_AddNullToObject(cb, "palette3");
    else cJSON_AddNumberToObject(cb, "palette3", pal3);
    cJSON_AddNumberToObject(cb, "blend1", ce.getBlendFactor1());
    cJSON_AddNumberToObject(cb, "blend2", ce.getBlendFactor2());
    cJSON_AddNumberToObject(cb, "blend3", ce.getBlendFactor3());
    cJSON* tr = cJSON_AddObjectToObject(data, "temporalRotation");
    cJSON_AddBoolToObject(tr, "enabled", ce.isTemporalRotationEnabled());
    cJSON_AddNumberToObject(tr, "speed", ce.getRotationSpeed());
    cJSON_AddNumberToObject(tr, "phase", ce.getRotationPhase());
    cJSON* df = cJSON_AddObjectToObject(data, "diffusion");
    cJSON_AddBoolToObject(df, "enabled", ce.isDiffusionEnabled());
    cJSON_AddNumberToObject(df, "amount", ce.getDiffusionAmount());
    sendV2Response(self, "enhancements.color", true, data);
#endif
}

static void handleV2EnhancementsColorSet(LightwaveWebServer* self, cJSON* doc) {
#if !FEATURE_COLOR_ENGINE
    sendV2Response(self, "enhancements.color", false, nullptr, "Color engine not available");
    cJSON_Delete(doc);
    return;
#else
    if (!doc) {
        sendV2Response(self, "enhancements.color", false, nullptr, "Missing data");
        return;
    }
    ColorEngine& ce = ColorEngine::getInstance();
    cJSON* updated = cJSON_CreateArray();
    cJSON* current = cJSON_CreateObject();
    cJSON* node = cJSON_GetObjectItemCaseSensitive(doc, "enabled");
    if (node && cJSON_IsBool(node)) {
        ce.setEnabled(cJSON_IsTrue(node));
        cJSON_AddItemToArray(updated, cJSON_CreateString("enabled"));
        cJSON_AddBoolToObject(current, "enabled", ce.isEnabled());
    }
    node = cJSON_GetObjectItemCaseSensitive(doc, "crossBlend");
    if (node && cJSON_IsObject(node)) {
        cJSON* cbEnabled = cJSON_GetObjectItemCaseSensitive(node, "enabled");
        if (cbEnabled && cJSON_IsBool(cbEnabled)) {
            ce.enableCrossBlend(cJSON_IsTrue(cbEnabled));
        }
        cJSON* p1 = cJSON_GetObjectItemCaseSensitive(node, "palette1");
        cJSON* p2 = cJSON_GetObjectItemCaseSensitive(node, "palette2");
        cJSON* p3 = cJSON_GetObjectItemCaseSensitive(node, "palette3");
        if (p1 && cJSON_IsNumber(p1) && p2 && cJSON_IsNumber(p2)) {
            int pal3 = -1;
            if (p3) {
                if (cJSON_IsNull(p3)) pal3 = -1;
                else if (cJSON_IsNumber(p3)) pal3 = (int)p3->valuedouble;
            }
            ce.setCrossBlendPalettes((uint8_t)p1->valuedouble, (uint8_t)p2->valuedouble, pal3);
        }
        cJSON_AddItemToArray(updated, cJSON_CreateString("crossBlend"));
    }
    node = cJSON_GetObjectItemCaseSensitive(doc, "temporalRotation");
    if (node && cJSON_IsObject(node)) {
        cJSON* trEnabled = cJSON_GetObjectItemCaseSensitive(node, "enabled");
        if (trEnabled && cJSON_IsBool(trEnabled)) {
            ce.enableTemporalRotation(cJSON_IsTrue(trEnabled));
        }
        cJSON* speed = cJSON_GetObjectItemCaseSensitive(node, "speed");
        if (speed && cJSON_IsNumber(speed)) {
            ce.setRotationSpeed((float)speed->valuedouble);
        }
        cJSON_AddItemToArray(updated, cJSON_CreateString("temporalRotation"));
    }
    node = cJSON_GetObjectItemCaseSensitive(doc, "diffusion");
    if (node && cJSON_IsObject(node)) {
        cJSON* dfEnabled = cJSON_GetObjectItemCaseSensitive(node, "enabled");
        if (dfEnabled && cJSON_IsBool(dfEnabled)) {
            ce.enableDiffusion(cJSON_IsTrue(dfEnabled));
        }
        cJSON* amt = cJSON_GetObjectItemCaseSensitive(doc, "amount");
        if (amt && cJSON_IsNumber(amt)) {
            ce.setDiffusionAmount((uint8_t)amt->valuedouble);
        }
        cJSON_AddItemToArray(updated, cJSON_CreateString("diffusion"));
    }
    cJSON_Delete(doc);
    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "updated", updated);
    cJSON_AddItemToObject(data, "current", current);
    sendV2Response(self, "enhancements.color", true, data);
#endif
}

static void handleV2EnhancementsColorReset(LightwaveWebServer* self) {
#if !FEATURE_COLOR_ENGINE
    sendV2Response(self, "enhancements.color", false, nullptr, "Color engine not available");
#else
    ColorEngine::getInstance().reset();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "message", "Color engine reset to defaults");
    sendV2Response(self, "enhancements.color", true, data);
#endif
}

static void handleV2EnhancementsMotionGet(LightwaveWebServer* self) {
#if !FEATURE_MOTION_ENGINE
    cJSON* data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "enabled", false);
    cJSON_AddStringToObject(data, "message", "Motion engine not available");
    sendV2Response(self, "enhancements.motion", true, data);
#else
    MotionEngine& me = MotionEngine::getInstance();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "enabled", me.isEnabled());
    cJSON_AddNumberToObject(data, "warpStrength", me.getWarpStrength());
    cJSON_AddNumberToObject(data, "warpFrequency", me.getWarpFrequency());
    sendV2Response(self, "enhancements.motion", true, data);
#endif
}

static void handleV2EnhancementsMotionSet(LightwaveWebServer* self, cJSON* doc) {
#if !FEATURE_MOTION_ENGINE
    sendV2Response(self, "enhancements.motion", false, nullptr, "Motion engine not available");
    cJSON_Delete(doc);
    return;
#else
    if (!doc) {
        sendV2Response(self, "enhancements.motion", false, nullptr, "Missing data");
        return;
    }
    MotionEngine& me = MotionEngine::getInstance();
    cJSON* updated = cJSON_CreateArray();
    cJSON* current = cJSON_CreateObject();
    cJSON* node = cJSON_GetObjectItemCaseSensitive(doc, "enabled");
    if (node && cJSON_IsBool(node)) {
        if (cJSON_IsTrue(node)) me.enable();
        else me.disable();
        cJSON_AddItemToArray(updated, cJSON_CreateString("enabled"));
        cJSON_AddBoolToObject(current, "enabled", me.isEnabled());
    }
    node = cJSON_GetObjectItemCaseSensitive(doc, "warpStrength");
    if (node && cJSON_IsNumber(node)) {
        me.setWarpStrength((uint8_t)node->valuedouble);
        cJSON_AddItemToArray(updated, cJSON_CreateString("warpStrength"));
        cJSON_AddNumberToObject(current, "warpStrength", me.getWarpStrength());
    }
    node = cJSON_GetObjectItemCaseSensitive(doc, "warpFrequency");
    if (node && cJSON_IsNumber(node)) {
        me.setWarpFrequency((uint8_t)node->valuedouble);
        cJSON_AddItemToArray(updated, cJSON_CreateString("warpFrequency"));
        cJSON_AddNumberToObject(current, "warpFrequency", me.getWarpFrequency());
    }
    cJSON_Delete(doc);
    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "updated", updated);
    cJSON_AddItemToObject(data, "current", current);
    sendV2Response(self, "enhancements.motion", true, data);
#endif
}

static void handleV2PalettesList(LightwaveWebServer* self) {
    cJSON* data = cJSON_CreateObject();
    cJSON* arr = cJSON_AddArrayToObject(data, "palettes");
    for (uint8_t i = 0; i < gMasterPaletteCount; i++) {
        cJSON* p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "id", i);
        cJSON_AddStringToObject(p, "name", MasterPaletteNames[i]);
        cJSON* colors = cJSON_AddArrayToObject(p, "colors");
        // Simplified: placeholder samples; full palette sampling can be added later.
        for (int j = 0; j < 4; j++) {
            cJSON* c = cJSON_CreateObject();
            cJSON_AddNumberToObject(c, "r", 128);
            cJSON_AddNumberToObject(c, "g", 128);
            cJSON_AddNumberToObject(c, "b", 128);
            cJSON_AddItemToArray(colors, c);
        }
        cJSON_AddItemToArray(arr, p);
    }
    cJSON_AddNumberToObject(data, "total", gMasterPaletteCount);
    sendV2Response(self, "palettes.list", true, data);
}

static void handleV2PalettesGet(LightwaveWebServer* self, cJSON* doc) {
    if (!doc) {
        sendV2Response(self, "palettes.get", false, nullptr, "Missing data");
        return;
    }
    cJSON* idNode = cJSON_GetObjectItemCaseSensitive(doc, "id");
    if (!idNode || !cJSON_IsNumber(idNode)) {
        sendV2Response(self, "palettes.get", false, nullptr, "Missing palette id");
        cJSON_Delete(doc);
        return;
    }
    const int id = (int)idNode->valuedouble;
    if (id < 0 || id >= gMasterPaletteCount) {
        sendV2Response(self, "palettes.get", false, nullptr, "Invalid palette id");
        cJSON_Delete(doc);
            return;
        }
    cJSON_Delete(doc);
    cJSON* data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", id);
    cJSON_AddStringToObject(data, "name", MasterPaletteNames[id]);
    // Simplified: full gradient omitted for brevity
    sendV2Response(self, "palettes.get", true, data);
}

// ============================================================================
// Main WebSocket Message Handler
// ============================================================================

void LightwaveWebServer::onWsMessage(int clientFd, const char* json, size_t len, void* ctx) {
    auto* self = static_cast<LightwaveWebServer*>(ctx);
    if (!self || !json || len == 0) return;

    // Docs hard limit
    if (len > 1024) {
        Serial.printf("[WebServer] WS[%d] message too large: %zu bytes (max 1024)\n", clientFd, len);
        s_wsReplyClientFd = clientFd;
        sendV2Response(self, "error", false, nullptr, "Message too large");
        return;
    }

    cJSON* doc = cJSON_ParseWithLength(json, len);
    if (!doc) {
        Serial.printf("[WebServer] WS[%d] malformed JSON (len=%zu)\n", clientFd, len);
        // Best-effort legacy error frame
        cJSON* err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "type", "error");
        cJSON_AddStringToObject(err, "message", "Malformed JSON");
        char* out = cJSON_PrintUnformatted(err);
        if (out) {
            self->m_http.wsSendText(clientFd, out, strlen(out));
            cJSON_free(out);
        }
        cJSON_Delete(err);
        return;
    }

    cJSON* typeNode = cJSON_GetObjectItemCaseSensitive(doc, "type");
    if (!typeNode || !cJSON_IsString(typeNode)) {
        Serial.printf("[WebServer] WS[%d] missing type field\n", clientFd);
        s_wsReplyClientFd = clientFd;
        sendV2Response(self, "error", false, nullptr, "Missing type field");
        cJSON_Delete(doc);
        return;
    }
    const char* type = typeNode->valuestring;

    // Set default reply target for v2 response envelopes
    s_wsReplyClientFd = clientFd;

    // =========================================================================
    // API v2 (docs) - flat message format
    // =========================================================================
    if (strcmp(type, "device.getStatus") == 0) {
        handleV2DeviceGetStatus(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "device.getInfo") == 0) {
        handleV2DeviceGetInfo(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "effects.list") == 0) {
        handleV2EffectsList(self, doc); // page/limit/details are at top level
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "effects.getCurrent") == 0) {
        // If legacy requestId is present, handle legacy mode below.
        cJSON* reqId = cJSON_GetObjectItemCaseSensitive(doc, "requestId");
        if (!reqId || !cJSON_IsString(reqId)) {
            handleV2EffectsGetCurrent(self);
            cJSON_Delete(doc);
        return;
    }
    }
    if (strcmp(type, "effects.getMetadata") == 0) {
        handleV2EffectsGetMetadata(self, doc); // effectId at top level
        cJSON_Delete(doc);
            return;
        }
    if (strcmp(type, "effects.setCurrent") == 0) {
        cJSON* eid = cJSON_GetObjectItemCaseSensitive(doc, "effectId");
        if (!eid || !cJSON_IsNumber(eid)) {
            sendV2Response(self, "error", false, nullptr, "Missing effectId");
            cJSON_Delete(doc);
            return;
        }
        const uint8_t prev = currentEffect;
        const uint8_t next = (uint8_t)eid->valuedouble;
        if (next >= NUM_EFFECTS) {
            sendV2Response(self, "error", false, nullptr, "Invalid effectId");
            cJSON_Delete(doc);
            return;
        }
        currentEffect = next;

        // Broadcast per docs: effects.changed (no success/version)
        cJSON* ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "effects.changed");
        cJSON_AddNumberToObject(ev, "effectId", next);
        cJSON_AddStringToObject(ev, "name", effects[next].name);
        cJSON_AddNumberToObject(ev, "timestamp", millis());
        char* out = cJSON_PrintUnformatted(ev);
        if (out) {
            // broadcast to all
            self->m_http.wsBroadcastText(out, strlen(out));
            cJSON_free(out);
        }
        cJSON_Delete(ev);

        // Optional: transition.started if transition object provided
        cJSON* trans = cJSON_GetObjectItemCaseSensitive(doc, "transition");
        if (trans && cJSON_IsObject(trans)) {
            cJSON* tType = cJSON_GetObjectItemCaseSensitive(trans, "type");
            cJSON* tDur = cJSON_GetObjectItemCaseSensitive(trans, "duration");
            if (tType && cJSON_IsNumber(tType) && tDur && cJSON_IsNumber(tDur)) {
                cJSON* tev = cJSON_CreateObject();
                cJSON_AddStringToObject(tev, "type", "transition.started");
                cJSON_AddNumberToObject(tev, "fromEffect", prev);
                cJSON_AddNumberToObject(tev, "toEffect", next);
                cJSON_AddNumberToObject(tev, "transitionType", (int)tType->valuedouble);
                cJSON_AddNumberToObject(tev, "duration", (int)tDur->valuedouble);
                cJSON_AddNumberToObject(tev, "timestamp", millis());
                char* tout = cJSON_PrintUnformatted(tev);
                if (tout) {
                    self->m_http.wsBroadcastText(tout, strlen(tout));
                    cJSON_free(tout);
                }
                cJSON_Delete(tev);
            }
        }

        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "parameters.get") == 0) {
        cJSON* reqId = cJSON_GetObjectItemCaseSensitive(doc, "requestId");
        if (!reqId || !cJSON_IsString(reqId)) {
            handleV2ParametersGet(self);
            cJSON_Delete(doc);
            return;
        }
    }
    if (strcmp(type, "parameters.set") == 0) {
        // Apply fields and broadcast parameters.changed (no success/version)
        cJSON* updated = cJSON_CreateArray();
        bool changed = false;

        auto mark = [&](const char* k){ cJSON_AddItemToArray(updated, cJSON_CreateString(k)); changed = true; };

        cJSON* n = nullptr;
        n = cJSON_GetObjectItemCaseSensitive(doc, "brightness");
        if (n && cJSON_IsNumber(n)) { FastLED.setBrightness((uint8_t)n->valuedouble); mark("brightness"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "speed");
        if (n && cJSON_IsNumber(n)) { effectSpeed = constrain((uint8_t)n->valuedouble, 1, 50); mark("speed"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "paletteId");
        if (n && cJSON_IsNumber(n)) { currentPaletteIndex = (uint8_t)n->valuedouble; mark("paletteId"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "intensity");
        if (n && cJSON_IsNumber(n)) { effectIntensity = (uint8_t)n->valuedouble; mark("intensity"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "variation");
        if (n && cJSON_IsNumber(n)) { effectVariation = (uint8_t)n->valuedouble; mark("variation"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "complexity");
        if (n && cJSON_IsNumber(n)) { effectComplexity = (uint8_t)n->valuedouble; mark("complexity"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "saturation");
        if (n && cJSON_IsNumber(n)) { effectSaturation = (uint8_t)n->valuedouble; mark("saturation"); }

        if (!changed) {
            cJSON_Delete(updated);
            sendV2Response(self, "error", false, nullptr, "No recognised fields to update");
            cJSON_Delete(doc);
        return;
    }

        cJSON* ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "parameters.changed");
        cJSON_AddItemToObject(ev, "updated", updated);
        cJSON* cur = cJSON_AddObjectToObject(ev, "current");
        cJSON_AddNumberToObject(cur, "brightness", FastLED.getBrightness());
        cJSON_AddNumberToObject(cur, "speed", effectSpeed);
        cJSON_AddNumberToObject(cur, "paletteId", currentPaletteIndex);
        cJSON_AddNumberToObject(cur, "intensity", effectIntensity);
        cJSON_AddNumberToObject(cur, "saturation", effectSaturation);
        cJSON_AddNumberToObject(cur, "complexity", effectComplexity);
        cJSON_AddNumberToObject(cur, "variation", effectVariation);
        cJSON_AddNumberToObject(ev, "timestamp", millis());
        char* out = cJSON_PrintUnformatted(ev);
        if (out) {
            self->m_http.wsBroadcastText(out, strlen(out));
            cJSON_free(out);
        }
        cJSON_Delete(ev);

        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "transitions.list") == 0) {
        handleV2TransitionsList(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "transitions.trigger") == 0) {
        cJSON* toEff = cJSON_GetObjectItemCaseSensitive(doc, "toEffect");
        if (!toEff || !cJSON_IsNumber(toEff)) {
            sendV2Response(self, "error", false, nullptr, "Missing toEffect");
            cJSON_Delete(doc);
        return;
    }
        const uint8_t prev = currentEffect;
        const uint8_t next = (uint8_t)toEff->valuedouble;
        if (next >= NUM_EFFECTS) {
            sendV2Response(self, "error", false, nullptr, "Invalid toEffect");
            cJSON_Delete(doc);
            return;
        }
        TransitionType transType = TransitionEngine::getRandomTransition();
        cJSON* t = cJSON_GetObjectItemCaseSensitive(doc, "type");
        if (t && cJSON_IsNumber(t)) {
            const int tv = (int)t->valuedouble;
            if (tv >= 0 && tv < TRANSITION_COUNT) transType = (TransitionType)tv;
        }
        uint32_t duration = 1000;
        cJSON* d = cJSON_GetObjectItemCaseSensitive(doc, "duration");
        if (d && cJSON_IsNumber(d)) duration = constrain((uint32_t)d->valuedouble, 100U, 5000U);

        memcpy(transitionSourceBuffer, leds, HardwareConfig::NUM_LEDS * sizeof(CRGB));
        currentEffect = next;
        transitionEngine.startTransition(transitionSourceBuffer, leds, leds, transType, duration, EASE_IN_OUT_QUAD);

        // Broadcast per docs: transition.started
        cJSON* ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "transition.started");
        cJSON_AddNumberToObject(ev, "fromEffect", prev);
        cJSON_AddNumberToObject(ev, "toEffect", next);
        cJSON_AddNumberToObject(ev, "transitionType", (int)transType);
        cJSON_AddNumberToObject(ev, "duration", (int)duration);
        cJSON_AddNumberToObject(ev, "timestamp", millis());
        char* out = cJSON_PrintUnformatted(ev);
        if (out) { self->m_http.wsBroadcastText(out, strlen(out)); cJSON_free(out); }
        cJSON_Delete(ev);

        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "zones.list") == 0) {
        handleV2ZonesList(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "zones.get") == 0) {
        handleV2ZonesGet(self, doc);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "zones.update") == 0) {
        cJSON* zid = cJSON_GetObjectItemCaseSensitive(doc, "zoneId");
        if (!zid || !cJSON_IsNumber(zid)) {
            sendV2Response(self, "error", false, nullptr, "Missing zoneId");
            cJSON_Delete(doc);
        return;
    }
        const uint8_t zoneId = (uint8_t)zid->valuedouble;
        if (zoneId >= HardwareConfig::MAX_ZONES) {
            sendV2Response(self, "error", false, nullptr, "Invalid zoneId");
            cJSON_Delete(doc);
            return;
        }
        cJSON* updated = cJSON_CreateArray();
        auto mark = [&](const char* k){ cJSON_AddItemToArray(updated, cJSON_CreateString(k)); };
        cJSON* n = nullptr;
        n = cJSON_GetObjectItemCaseSensitive(doc, "enabled");
        if (n && cJSON_IsBool(n)) { zoneComposer.enableZone(zoneId, cJSON_IsTrue(n)); mark("enabled"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "brightness");
        if (n && cJSON_IsNumber(n)) { zoneComposer.setZoneBrightness(zoneId, (uint8_t)n->valuedouble); mark("brightness"); }
        n = cJSON_GetObjectItemCaseSensitive(doc, "speed");
        if (n && cJSON_IsNumber(n)) { zoneComposer.setZoneSpeed(zoneId, constrain((uint8_t)n->valuedouble, 1, 50)); mark("speed"); }

        // Broadcast per docs: zones.changed
        cJSON* ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "zones.changed");
        cJSON_AddNumberToObject(ev, "zoneId", zoneId);
        cJSON_AddItemToObject(ev, "updated", updated);
        cJSON_AddNumberToObject(ev, "timestamp", millis());
        char* out = cJSON_PrintUnformatted(ev);
        if (out) { self->m_http.wsBroadcastText(out, strlen(out)); cJSON_free(out); }
        cJSON_Delete(ev);

        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "zones.setEffect") == 0) {
        cJSON* zid = cJSON_GetObjectItemCaseSensitive(doc, "zoneId");
        cJSON* eid = cJSON_GetObjectItemCaseSensitive(doc, "effectId");
        if (!zid || !eid || !cJSON_IsNumber(zid) || !cJSON_IsNumber(eid)) {
            sendV2Response(self, "error", false, nullptr, "Missing zoneId/effectId");
            cJSON_Delete(doc);
            return;
        }
        const uint8_t zoneId = (uint8_t)zid->valuedouble;
        const uint8_t effId = (uint8_t)eid->valuedouble;
        if (zoneId >= HardwareConfig::MAX_ZONES || effId >= NUM_EFFECTS) {
            sendV2Response(self, "error", false, nullptr, "Invalid zoneId/effectId");
            cJSON_Delete(doc);
            return;
        }
        zoneComposer.setZoneEffect(zoneId, effId);
        cJSON* ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "zones.effectChanged");
        cJSON_AddNumberToObject(ev, "zoneId", zoneId);
        cJSON_AddNumberToObject(ev, "effectId", effId);
        cJSON_AddStringToObject(ev, "effectName", effects[effId].name);
        cJSON_AddNumberToObject(ev, "timestamp", millis());
        char* out = cJSON_PrintUnformatted(ev);
        if (out) { self->m_http.wsBroadcastText(out, strlen(out)); cJSON_free(out); }
        cJSON_Delete(ev);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "batch") == 0) {
        // Docs format: operations[{method,path,body}]
        cJSON* ops = cJSON_GetObjectItemCaseSensitive(doc, "operations");
        if (!ops || !cJSON_IsArray(ops)) {
            sendV2Response(self, "batch.result", false, nullptr, "Missing operations array");
            cJSON_Delete(doc);
            return;
        }
        const int opCount = cJSON_GetArraySize(ops);
        if (opCount > 10) {
            sendV2Response(self, "batch.result", false, nullptr, "Max 10 operations per batch");
            cJSON_Delete(doc);
            return;
        }
        int succeeded = 0, failed = 0;
        for (int i = 0; i < opCount; i++) {
            cJSON* op = cJSON_GetArrayItem(ops, i);
            cJSON* m = cJSON_GetObjectItemCaseSensitive(op, "method");
            cJSON* p = cJSON_GetObjectItemCaseSensitive(op, "path");
            cJSON* b = cJSON_GetObjectItemCaseSensitive(op, "body");
            if (!m || !p || !cJSON_IsString(m) || !cJSON_IsString(p)) { failed++; continue; }
            if (strcmp(m->valuestring, "PATCH") == 0 && strcmp(p->valuestring, "/api/v2/parameters") == 0) {
                // reuse parameter parsing path
                bool ok = false;
                if (b && cJSON_IsObject(b)) {
                    cJSON* n = cJSON_GetObjectItemCaseSensitive(b, "brightness");
                    if (n && cJSON_IsNumber(n)) { FastLED.setBrightness((uint8_t)n->valuedouble); ok = true; }
                    n = cJSON_GetObjectItemCaseSensitive(b, "speed");
                    if (n && cJSON_IsNumber(n)) { effectSpeed = constrain((uint8_t)n->valuedouble, 1, 50); ok = true; }
                }
                ok ? succeeded++ : failed++;
            } else if (strcmp(m->valuestring, "PUT") == 0 && strcmp(p->valuestring, "/api/v2/effects/current") == 0) {
                bool ok = false;
                if (b && cJSON_IsObject(b)) {
                    cJSON* n = cJSON_GetObjectItemCaseSensitive(b, "effectId");
                    if (n && cJSON_IsNumber(n) && (int)n->valuedouble >= 0 && (int)n->valuedouble < NUM_EFFECTS) {
                        currentEffect = (uint8_t)n->valuedouble;
                        ok = true;
                    }
                }
                ok ? succeeded++ : failed++;
            } else {
                failed++;
            }
        }
        cJSON* respData = cJSON_CreateObject();
        cJSON_AddNumberToObject(respData, "processed", opCount);
        cJSON_AddNumberToObject(respData, "succeeded", succeeded);
        cJSON_AddNumberToObject(respData, "failed", failed);
        sendV2Response(self, "batch.result", true, respData);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "ping") == 0) {
        cJSON* pong = cJSON_CreateObject();
        cJSON_AddNumberToObject(pong, "serverTime", millis());
        sendV2Response(self, "pong", true, pong);
        cJSON_Delete(doc);
        return;
    }

    // Enhancement endpoints
    if (strcmp(type, "enhancements.get") == 0) {
        handleV2EnhancementsSummary(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "enhancements.color.get") == 0) {
        handleV2EnhancementsColorGet(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "enhancements.color.set") == 0) {
        handleV2EnhancementsColorSet(self, doc);
        return; // doc deleted inside handler
    }
    if (strcmp(type, "enhancements.color.reset") == 0) {
        handleV2EnhancementsColorReset(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "enhancements.motion.get") == 0) {
        handleV2EnhancementsMotionGet(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "enhancements.motion.set") == 0) {
        handleV2EnhancementsMotionSet(self, doc);
        return; // doc deleted inside handler
    }

    // Palette endpoints
    if (strcmp(type, "palettes.list") == 0) {
        handleV2PalettesList(self);
        cJSON_Delete(doc);
        return;
    }
    if (strcmp(type, "palettes.get") == 0) {
        handleV2PalettesGet(self, doc);
        return; // doc deleted inside handler
    }

    // =========================================================================
    // Legacy compatibility (v2/src network WebServer.cpp)
    // =========================================================================
    cJSON* requestIdNode = cJSON_GetObjectItemCaseSensitive(doc, "requestId");
    const bool legacyReqResp = requestIdNode && cJSON_IsString(requestIdNode);

    if (strcmp(type, "setEffect") == 0) {
        cJSON* eid = cJSON_GetObjectItemCaseSensitive(doc, "effectId");
        if (eid && cJSON_IsNumber(eid)) {
            const uint8_t id = (uint8_t)eid->valuedouble;
            if (id < NUM_EFFECTS) {
                currentEffect = id;
                self->notifyEffectChange(id);
            }
        }
    } else if (strcmp(type, "nextEffect") == 0) {
        currentEffect = (uint8_t)((currentEffect + 1) % NUM_EFFECTS);
        self->notifyEffectChange(currentEffect);
    } else if (strcmp(type, "prevEffect") == 0) {
        currentEffect = (uint8_t)((currentEffect + NUM_EFFECTS - 1) % NUM_EFFECTS);
        self->notifyEffectChange(currentEffect);
    } else if (strcmp(type, "setBrightness") == 0) {
        cJSON* v = cJSON_GetObjectItemCaseSensitive(doc, "value");
        if (v && cJSON_IsNumber(v)) FastLED.setBrightness((uint8_t)v->valuedouble);
    } else if (strcmp(type, "setSpeed") == 0) {
        cJSON* v = cJSON_GetObjectItemCaseSensitive(doc, "value");
        if (v && cJSON_IsNumber(v)) effectSpeed = constrain((uint8_t)v->valuedouble, 1, 50);
    } else if (strcmp(type, "setPalette") == 0) {
        cJSON* v = cJSON_GetObjectItemCaseSensitive(doc, "paletteId");
        if (v && cJSON_IsNumber(v)) currentPaletteIndex = (uint8_t)v->valuedouble;
    } else if (strcmp(type, "zone.enable") == 0) {
        cJSON* en = cJSON_GetObjectItemCaseSensitive(doc, "enable");
        if (en && cJSON_IsBool(en)) {
            if (cJSON_IsTrue(en)) zoneComposer.enable();
            else zoneComposer.disable();
        }
    } else if (strcmp(type, "zone.setEffect") == 0) {
        cJSON* zid = cJSON_GetObjectItemCaseSensitive(doc, "zoneId");
        cJSON* eid = cJSON_GetObjectItemCaseSensitive(doc, "effectId");
        if (zid && eid && cJSON_IsNumber(zid) && cJSON_IsNumber(eid)) {
            zoneComposer.setZoneEffect((uint8_t)zid->valuedouble, (uint8_t)eid->valuedouble);
        }
    } else if (strcmp(type, "getStatus") == 0) {
        // Broadcast legacy status
        cJSON* status = cJSON_CreateObject();
        cJSON_AddStringToObject(status, "type", "status");
        cJSON_AddNumberToObject(status, "effectId", currentEffect);
        cJSON_AddStringToObject(status, "effectName", effects[currentEffect].name);
        cJSON_AddNumberToObject(status, "brightness", FastLED.getBrightness());
        cJSON_AddNumberToObject(status, "speed", effectSpeed);
        cJSON_AddNumberToObject(status, "paletteId", currentPaletteIndex);
        cJSON_AddNumberToObject(status, "hue", 0);
        cJSON_AddNumberToObject(status, "fps", perfMon.getCurrentFPS());
        cJSON_AddNumberToObject(status, "cpuPercent", perfMon.getCPUUsage());
        cJSON_AddNumberToObject(status, "freeHeap", ESP.getFreeHeap());
        cJSON_AddNumberToObject(status, "uptime", millis() / 1000);
        char* out = cJSON_PrintUnformatted(status);
        if (out) { self->m_http.wsBroadcastText(out, strlen(out)); cJSON_free(out); }
        cJSON_Delete(status);
    } else if (legacyReqResp && strcmp(type, "effects.getCurrent") == 0) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", "effects.current");
        cJSON_AddStringToObject(resp, "requestId", requestIdNode->valuestring);
        cJSON* d = cJSON_AddObjectToObject(resp, "data");
        cJSON_AddNumberToObject(d, "effectId", currentEffect);
        cJSON_AddStringToObject(d, "name", effects[currentEffect].name);
        cJSON_AddNumberToObject(d, "brightness", FastLED.getBrightness());
        cJSON_AddNumberToObject(d, "speed", effectSpeed);
        char* out = cJSON_PrintUnformatted(resp);
        if (out) { self->m_http.wsSendText(clientFd, out, strlen(out)); cJSON_free(out); }
        cJSON_Delete(resp);
    } else if (legacyReqResp && strcmp(type, "parameters.get") == 0) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", "parameters");
        cJSON_AddStringToObject(resp, "requestId", requestIdNode->valuestring);
        cJSON* d = cJSON_AddObjectToObject(resp, "data");
        cJSON_AddNumberToObject(d, "brightness", FastLED.getBrightness());
        cJSON_AddNumberToObject(d, "speed", effectSpeed);
        cJSON_AddNumberToObject(d, "paletteId", currentPaletteIndex);
        cJSON_AddNumberToObject(d, "hue", 0);
        char* out = cJSON_PrintUnformatted(resp);
        if (out) { self->m_http.wsSendText(clientFd, out, strlen(out)); cJSON_free(out); }
        cJSON_Delete(resp);
        cJSON_Delete(doc);
        return;
    }

    // =========================================================================
    // Unknown command type - log and return error
    // =========================================================================
    Serial.printf("[WebServer] WS[%d] unknown command type: '%s' (len=%zu)\n", clientFd, type, len);
    sendV2Response(self, "error", false, nullptr, "Unknown command type");
    cJSON_Delete(doc);
}

esp_err_t LightwaveWebServer::handleApiDiscovery(httpd_req_t* req) {
    // Minimal v1 discovery response: { success: true, data: { ... } }
    static constexpr const char* API_VERSION = "1.0";
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);

    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddStringToObject(data, "name", "LightwaveOS");
    cJSON_AddStringToObject(data, "apiVersion", API_VERSION);
    cJSON_AddStringToObject(data, "description", "ESP32-S3 LED Control System");

    cJSON* hw = cJSON_AddObjectToObject(data, "hardware");
    cJSON_AddNumberToObject(hw, "ledsTotal", 320);
    cJSON_AddNumberToObject(hw, "strips", 2);
    cJSON_AddNumberToObject(hw, "centerPoint", 79);
    cJSON_AddNumberToObject(hw, "maxZones", 4);

    cJSON* links = cJSON_AddObjectToObject(data, "_links");
    cJSON_AddStringToObject(links, "self", "/api/v1/");
    cJSON_AddStringToObject(links, "device", "/api/v1/device/status");
    cJSON_AddStringToObject(links, "effects", "/api/v1/effects");
    cJSON_AddStringToObject(links, "parameters", "/api/v1/parameters");
    cJSON_AddStringToObject(links, "transitions", "/api/v1/transitions/types");
    cJSON_AddStringToObject(links, "batch", "/api/v1/batch");
    cJSON_AddStringToObject(links, "openapi", "/api/v1/openapi.json");
    cJSON_AddStringToObject(links, "websocket", "/ws");

    cJSON_AddNumberToObject(root, "timestamp", millis());
    cJSON_AddStringToObject(root, "version", API_VERSION);

    // Access server instance via global (safe: single server)
    esp_err_t res = webServer.m_http.sendJson(req, 200, root);
    cJSON_Delete(root);
    return res;
}

esp_err_t LightwaveWebServer::handleOpenApi(httpd_req_t* req) {
    // Serve OpenAPI spec from PROGMEM.
    // IMPORTANT: do NOT call corsOptionsHandler() here (it sends a 204 response).
    // Instead, attach the same CORS headers and proceed with a normal 200 response.
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, X-OTA-Token");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "200 OK");

    // OPENAPI_SPEC is in PROGMEM; we can send via chunked response.
    httpd_resp_send(req, OPENAPI_SPEC, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t LightwaveWebServer::handleDeviceStatus(httpd_req_t* req) {
    static constexpr const char* API_VERSION = "1.0";
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);

    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "uptime", millis() / 1000);
    cJSON_AddNumberToObject(data, "freeHeap", ESP.getFreeHeap());
    cJSON_AddNumberToObject(data, "cpuFreq", ESP.getCpuFreqMHz());
    cJSON_AddNumberToObject(data, "wsClients", (double)webServer.getClientCount());

    cJSON_AddNumberToObject(root, "timestamp", millis());
    cJSON_AddStringToObject(root, "version", API_VERSION);

    esp_err_t res = webServer.m_http.sendJson(req, 200, root);
    cJSON_Delete(root);
    return res;
}

esp_err_t LightwaveWebServer::handleEffectsList(httpd_req_t* req) {
    static constexpr const char* API_VERSION = "1.0";
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* effectsArr = cJSON_AddArrayToObject(data, "effects");

    for (uint16_t i = 0; i < NUM_EFFECTS; i++) {
        cJSON* e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "id", i);
        cJSON_AddStringToObject(e, "name", effects[i].name);
        cJSON_AddItemToArray(effectsArr, e);
    }

    cJSON_AddNumberToObject(root, "timestamp", millis());
    cJSON_AddStringToObject(root, "version", API_VERSION);

    esp_err_t res = webServer.m_http.sendJson(req, 200, root);
    cJSON_Delete(root);
    return res;
}

esp_err_t LightwaveWebServer::handleEffectsCurrent(httpd_req_t* req) {
    static constexpr const char* API_VERSION = "1.0";
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);

    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "effectId", currentEffect);
    cJSON_AddStringToObject(data, "name", effects[currentEffect].name);
    cJSON_AddNumberToObject(data, "brightness", FastLED.getBrightness());
    cJSON_AddNumberToObject(data, "speed", effectSpeed);
    cJSON_AddNumberToObject(data, "paletteId", currentPaletteIndex);
    cJSON_AddNumberToObject(data, "intensity", effectIntensity);
    cJSON_AddNumberToObject(data, "saturation", effectSaturation);
    cJSON_AddNumberToObject(data, "complexity", effectComplexity);
    cJSON_AddNumberToObject(data, "variation", effectVariation);

    cJSON_AddNumberToObject(root, "timestamp", millis());
    cJSON_AddStringToObject(root, "version", API_VERSION);

    esp_err_t res = webServer.m_http.sendJson(req, 200, root);
    cJSON_Delete(root);
    return res;
}

static esp_err_t sendSpiffsFile(httpd_req_t* req, const char* path, const char* contentType) {
    File f = SPIFFS.open(path, "r");
    if (!f) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_send(req, "Not found", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_set_type(req, contentType);
    httpd_resp_set_status(req, "200 OK");

    // Stream in chunks
    uint8_t buf[1024];
    while (f.available()) {
        const size_t r = f.read(buf, sizeof(buf));
        if (r == 0) break;
        const esp_err_t err = httpd_resp_send_chunk(req, (const char*)buf, r);
        if (err != ESP_OK) {
            f.close();
            httpd_resp_send_chunk(req, nullptr, 0);
            return err;
        }
    }
    f.close();
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t LightwaveWebServer::handleStaticRoot(httpd_req_t* req) {
    return sendSpiffsFile(req, "/index.html", "text/html");
}

esp_err_t LightwaveWebServer::handleStaticAsset(httpd_req_t* req) {
    const char* uri = req->uri ? req->uri : "/";
    if (strcmp(uri, "/app.js") == 0) {
        return sendSpiffsFile(req, "/app.js", "application/javascript");
    }
    if (strcmp(uri, "/styles.css") == 0) {
        return sendSpiffsFile(req, "/styles.css", "text/css");
    }
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "Not found", HTTPD_RESP_USE_STRLEN);
}

#endif // FEATURE_WEB_SERVER


