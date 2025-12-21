/**
 * @file WebServerV2.cpp
 * @brief API v2 REST endpoint implementations for LightwaveOS
 *
 * Implements Zone Composer and Transition control endpoints.
 * All responses follow v2 format: {success, data, timestamp, version: "2.0"}
 */

#include "WebServerV2.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <FastLED.h>
#include <cJSON.h>
#include <cstring>

#include "OpenApiSpecV2.h"
#include "IdfHttpServer.h"
#include "WebServer.h"
#include "../config/hardware_config.h"
#include "../effects.h"
#include "../effects/EffectMetadata.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/transitions/TransitionEngine.h"
#include "../effects/engines/ColorEngine.h"
#include "../effects/engines/MotionEngine.h"

// Palette extern declarations (avoid including full headers with PROGMEM definitions)
extern const TProgmemRGBGradientPaletteRef gMasterPalettes[];
extern const uint8_t gMasterPaletteCount;
extern const char* MasterPaletteNames[];

// ============================================================================
// External references from main.cpp
// ============================================================================
extern uint8_t currentEffect;
extern uint8_t effectSpeed;
extern uint8_t effectIntensity, effectSaturation, effectComplexity, effectVariation;
extern uint8_t currentPaletteIndex;
extern Effect effects[];
extern const uint8_t NUM_EFFECTS;
extern TransitionEngine transitionEngine;
extern ZoneComposer zoneComposer;
extern bool useRandomTransitions;
extern CRGB leds[];
extern CRGB transitionSourceBuffer[];
extern CRGB transitionBuffer[];

// Global webServer for sendJson access
extern class LightwaveWebServer webServer;

namespace WebServerV2 {

// ============================================================================
// Transition Type Metadata (12 types, all CENTER ORIGIN compliant)
// ============================================================================
const TransitionTypeInfo TRANSITION_TYPE_INFO[] = {
    {"Fade", "CENTER ORIGIN crossfade - radiates from center"},
    {"Wipe Out", "Wipe from center outward"},
    {"Wipe In", "Wipe from edges inward to center"},
    {"Dissolve", "Random pixel transition"},
    {"Phase Shift", "Frequency-based morph"},
    {"Pulsewave", "Concentric energy pulses from center"},
    {"Implosion", "Particles converge and collapse to center"},
    {"Iris", "Mechanical aperture open/close from center"},
    {"Nuclear", "Chain reaction explosion from center"},
    {"Stargate", "Event horizon portal effect at center"},
    {"Kaleidoscope", "Symmetric crystal patterns from center"},
    {"Mandala", "Sacred geometry radiating from center"}
};
const uint8_t TRANSITION_TYPE_COUNT = 12;

// ============================================================================
// Zone Preset Metadata
// ============================================================================
static const char* ZONE_PRESET_NAMES[] = {
    "Single",
    "Dual",
    "Triple",
    "Quad",
    "Alternating"
};
static const char* ZONE_PRESET_DESCRIPTIONS[] = {
    "Single zone covering all LEDs",
    "Two symmetric zones from center",
    "Three zones: center, middle, outer",
    "Four equal zones from center outward",
    "Alternating zone pattern"
};
static const uint8_t ZONE_PRESET_COUNTS[] = {1, 2, 3, 4, 4};

// ============================================================================
// Response Helpers
// ============================================================================

static void addCorsHeaders(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, X-OTA-Token");
}

static cJSON* createV2Response(bool success) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", success);
    return root;
}

static void addV2Metadata(cJSON* root) {
    cJSON_AddNumberToObject(root, "timestamp", millis());
    cJSON_AddStringToObject(root, "version", API_VERSION);
}

static esp_err_t sendV2Json(httpd_req_t* req, int statusCode, cJSON* root) {
    if (!req || !root) return ESP_FAIL;

    addCorsHeaders(req);
    addV2Metadata(root);
    httpd_resp_set_type(req, "application/json");

    const char* statusLine = "200 OK";
    switch (statusCode) {
        case 200: statusLine = "200 OK"; break;
        case 201: statusLine = "201 Created"; break;
        case 204: statusLine = "204 No Content"; break;
        case 400: statusLine = "400 Bad Request"; break;
        case 401: statusLine = "401 Unauthorized"; break;
        case 404: statusLine = "404 Not Found"; break;
        case 422: statusLine = "422 Unprocessable Entity"; break;
        case 429: statusLine = "429 Too Many Requests"; break;
        case 500: statusLine = "500 Internal Server Error"; break;
        default:  statusLine = "200 OK"; break;
    }
    httpd_resp_set_status(req, statusLine);

    char* out = cJSON_PrintUnformatted(root);
    if (!out) {
        return httpd_resp_send(req,
                               "{\"success\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"message\":\"JSON encode failed\"},\"version\":\"2.0\"}",
                               HTTPD_RESP_USE_STRLEN);
    }

    const esp_err_t res = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    cJSON_free(out);
    return res;
}

static esp_err_t sendV2Error(httpd_req_t* req, int statusCode, const char* errorCode, const char* message, const char* field = nullptr) {
    cJSON* root = createV2Response(false);
    cJSON* error = cJSON_AddObjectToObject(root, "error");
    cJSON_AddStringToObject(error, "code", errorCode);
    cJSON_AddStringToObject(error, "message", message);
    if (field) {
        cJSON_AddStringToObject(error, "field", field);
    }
    esp_err_t result = sendV2Json(req, statusCode, root);
    cJSON_Delete(root);
    return result;
}

// Read request body helper
static char* readRequestBody(httpd_req_t* req, size_t maxSize = 2048) {
    if (!req || req->content_len == 0) return nullptr;
    if (req->content_len > maxSize) return nullptr;

    char* buf = (char*)malloc(req->content_len + 1);
    if (!buf) return nullptr;

    size_t read = 0;
    while (read < req->content_len) {
        int r = httpd_req_recv(req, buf + read, req->content_len - read);
        if (r <= 0) {
            free(buf);
            return nullptr;
        }
        read += (size_t)r;
    }
    buf[read] = '\0';
    return buf;
}

// Extract zone ID from URI like "/api/v2/zones/2" or "/api/v2/zones/2/effect"
static bool extractZoneId(const char* uri, uint8_t& zoneId) {
    if (!uri) return false;

    // Find "/zones/" in the URI
    const char* zonesPos = strstr(uri, "/zones/");
    if (!zonesPos) return false;

    // Move past "/zones/"
    const char* idStart = zonesPos + 7;
    if (*idStart == '\0') return false;

    // Check for "presets" (not a zone ID)
    if (strncmp(idStart, "presets", 7) == 0) return false;

    // Parse the zone ID
    char* endPtr;
    long id = strtol(idStart, &endPtr, 10);
    if (endPtr == idStart) return false;
    if (id < 0 || id > 3) return false;

    zoneId = (uint8_t)id;
    return true;
}

// Extract preset ID from URI like "/api/v2/zones/presets/2"
static bool extractPresetId(const char* uri, uint8_t& presetId) {
    if (!uri) return false;

    // Find "/presets/" in the URI
    const char* presetsPos = strstr(uri, "/presets/");
    if (!presetsPos) return false;

    // Move past "/presets/"
    const char* idStart = presetsPos + 9;
    if (*idStart == '\0') return false;

    // Parse the preset ID
    char* endPtr;
    long id = strtol(idStart, &endPtr, 10);
    if (endPtr == idStart) return false;
    if (id < 0 || id > 4) return false;

    presetId = (uint8_t)id;
    return true;
}

// ============================================================================
// Helper: Parse query parameter as integer
// ============================================================================
static int parseQueryInt(httpd_req_t* req, const char* key, int defaultVal) {
    char queryBuf[128] = {0};
    if (httpd_req_get_url_query_str(req, queryBuf, sizeof(queryBuf)) != ESP_OK) {
        return defaultVal;
    }

    char valueBuf[16] = {0};
    if (httpd_query_key_value(queryBuf, key, valueBuf, sizeof(valueBuf)) != ESP_OK) {
        return defaultVal;
    }

    return atoi(valueBuf);
}

// ============================================================================
// Helper: Extract path segment (e.g., /api/v2/effects/5 -> "5")
// ============================================================================
static bool extractPathSegment(const char* uri, const char* prefix, char* outBuf, size_t bufLen) {
    if (!uri || !prefix || !outBuf || bufLen == 0) return false;

    const char* start = strstr(uri, prefix);
    if (!start) return false;

    start += strlen(prefix);
    while (*start == '/') start++;

    const char* end = start;
    while (*end && *end != '/' && *end != '?') end++;

    size_t len = end - start;
    if (len == 0 || len >= bufLen) return false;

    memcpy(outBuf, start, len);
    outBuf[len] = '\0';
    return true;
}

// ============================================================================
// Enhancement Engines Helpers (FEATURE_ENHANCEMENT_ENGINES)
// ============================================================================

static void addHexColor(cJSON* arr, const CRGB& c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    cJSON_AddItemToArray(arr, cJSON_CreateString(buf));
}

static void addPaletteSamples(cJSON* arr, uint8_t paletteId, uint8_t sampleCount) {
    if (paletteId >= gMasterPaletteCount) return;
    CRGBPalette16 pal(gMasterPalettes[paletteId]);
    // Sample at roughly-even positions.
    for (uint8_t i = 0; i < sampleCount; i++) {
        const uint8_t idx = (uint8_t)((uint16_t)i * 255 / (sampleCount - 1));
        addHexColor(arr, ColorFromPalette(pal, idx, 255, LINEARBLEND));
    }
}

// ============================================================================
// Discovery & Device Endpoints (5)
// ============================================================================

esp_err_t handleV2Discovery(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddStringToObject(data, "name", "LightwaveOS");
    cJSON_AddStringToObject(data, "apiVersion", API_VERSION);
    cJSON_AddStringToObject(data, "description", "ESP32-S3 LED Control System with Light Guide Plate physics");

    // Hardware info
    cJSON* hw = cJSON_AddObjectToObject(data, "hardware");
    cJSON_AddNumberToObject(hw, "ledsTotal", HardwareConfig::TOTAL_LEDS);
    cJSON_AddNumberToObject(hw, "strips", 2);
    cJSON_AddNumberToObject(hw, "ledsPerStrip", HardwareConfig::LEDS_PER_STRIP);
    cJSON_AddNumberToObject(hw, "centerPoint", 79);
    cJSON_AddNumberToObject(hw, "maxZones", HardwareConfig::MAX_ZONES);
    cJSON_AddStringToObject(hw, "chipModel", ESP.getChipModel());
    cJSON_AddNumberToObject(hw, "cpuFreqMHz", ESP.getCpuFreqMHz());

    // Capabilities
    cJSON* caps = cJSON_AddObjectToObject(data, "capabilities");
    cJSON_AddBoolToObject(caps, "centerOrigin", true);
    cJSON_AddBoolToObject(caps, "zones", true);
    cJSON_AddBoolToObject(caps, "transitions", true);
    cJSON_AddBoolToObject(caps, "websocket", true);
    cJSON_AddNumberToObject(caps, "effectCount", NUM_EFFECTS);

    // HATEOAS links
    cJSON* links = cJSON_AddObjectToObject(data, "_links");
    cJSON_AddStringToObject(links, "self", "/api/v2/");
    cJSON_AddStringToObject(links, "device", "/api/v2/device");
    cJSON_AddStringToObject(links, "deviceStatus", "/api/v2/device/status");
    cJSON_AddStringToObject(links, "deviceInfo", "/api/v2/device/info");
    cJSON_AddStringToObject(links, "effects", "/api/v2/effects");
    cJSON_AddStringToObject(links, "effectsCurrent", "/api/v2/effects/current");
    cJSON_AddStringToObject(links, "effectsCategories", "/api/v2/effects/categories");
    cJSON_AddStringToObject(links, "parameters", "/api/v2/parameters");
    cJSON_AddStringToObject(links, "transitions", "/api/v2/transitions");
    cJSON_AddStringToObject(links, "zones", "/api/v2/zones");
    cJSON_AddStringToObject(links, "openapi", "/api/v2/openapi.json");
    cJSON_AddStringToObject(links, "websocket", "/ws");

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2OpenApi(httpd_req_t* req) {
    addCorsHeaders(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "200 OK");
    return httpd_resp_send(req, OPENAPI_SPEC_V2, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handleV2Device(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    // Status section
    cJSON* status = cJSON_AddObjectToObject(data, "status");
    cJSON_AddNumberToObject(status, "uptime", millis() / 1000);
    cJSON_AddNumberToObject(status, "freeHeap", ESP.getFreeHeap());
    cJSON_AddNumberToObject(status, "minFreeHeap", ESP.getMinFreeHeap());
    cJSON_AddNumberToObject(status, "cpuFreqMHz", ESP.getCpuFreqMHz());
    cJSON_AddNumberToObject(status, "wsClients", (double)webServer.getClientCount());

    // Info section
    cJSON* info = cJSON_AddObjectToObject(data, "info");
    cJSON_AddStringToObject(info, "firmware", "LightwaveOS");
    cJSON_AddStringToObject(info, "firmwareVersion", "2.0.0");
    cJSON_AddStringToObject(info, "sdkVersion", ESP.getSdkVersion());
    cJSON_AddStringToObject(info, "chipModel", ESP.getChipModel());
    cJSON_AddNumberToObject(info, "chipRevision", ESP.getChipRevision());
    cJSON_AddNumberToObject(info, "flashSize", ESP.getFlashChipSize());

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2DeviceStatus(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddNumberToObject(data, "uptime", millis() / 1000);
    cJSON_AddNumberToObject(data, "freeHeap", ESP.getFreeHeap());
    cJSON_AddNumberToObject(data, "minFreeHeap", ESP.getMinFreeHeap());
    cJSON_AddNumberToObject(data, "heapFragmentation",
        ESP.getFreeHeap() > 0 ? 100 - (100 * ESP.getMaxAllocHeap() / ESP.getFreeHeap()) : 0);
    cJSON_AddNumberToObject(data, "cpuFreqMHz", ESP.getCpuFreqMHz());
    cJSON_AddNumberToObject(data, "wsClients", (double)webServer.getClientCount());

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2DeviceInfo(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddStringToObject(data, "firmware", "LightwaveOS");
    cJSON_AddStringToObject(data, "firmwareVersion", "2.0.0");
    cJSON_AddStringToObject(data, "apiVersion", API_VERSION);
    cJSON_AddStringToObject(data, "sdkVersion", ESP.getSdkVersion());
    cJSON_AddStringToObject(data, "chipModel", ESP.getChipModel());
    cJSON_AddNumberToObject(data, "chipRevision", ESP.getChipRevision());
    cJSON_AddNumberToObject(data, "cpuCores", ESP.getChipCores());
    cJSON_AddNumberToObject(data, "flashSize", ESP.getFlashChipSize());
    cJSON_AddNumberToObject(data, "flashSpeed", ESP.getFlashChipSpeed());
    cJSON_AddNumberToObject(data, "sketchSize", ESP.getSketchSize());
    cJSON_AddNumberToObject(data, "freeSketchSpace", ESP.getFreeSketchSpace());

    cJSON* hw = cJSON_AddObjectToObject(data, "hardware");
    cJSON_AddNumberToObject(hw, "ledsTotal", HardwareConfig::TOTAL_LEDS);
    cJSON_AddNumberToObject(hw, "strips", 2);
    cJSON_AddNumberToObject(hw, "ledsPerStrip", HardwareConfig::LEDS_PER_STRIP);
    cJSON_AddNumberToObject(hw, "maxZones", HardwareConfig::MAX_ZONES);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Effects Endpoints (5)
// ============================================================================

esp_err_t handleV2EffectsList(httpd_req_t* req) {
    int offset = parseQueryInt(req, "offset", 0);
    int limit = parseQueryInt(req, "limit", 20);

    if (offset < 0) offset = 0;
    if (limit < 1) limit = 1;
    if (limit > 50) limit = 50;
    if (offset >= NUM_EFFECTS) offset = NUM_EFFECTS > 0 ? NUM_EFFECTS - 1 : 0;

    int end = offset + limit;
    if (end > NUM_EFFECTS) end = NUM_EFFECTS;

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "total", NUM_EFFECTS);
    cJSON_AddNumberToObject(data, "offset", offset);
    cJSON_AddNumberToObject(data, "limit", limit);
    cJSON_AddNumberToObject(data, "count", end - offset);

    cJSON* effectsArr = cJSON_AddArrayToObject(data, "effects");

    for (int i = offset; i < end && i < NUM_EFFECTS; i++) {
        cJSON* e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "id", i);
        cJSON_AddStringToObject(e, "name", effects[i].name);

        if (i < EFFECT_METADATA_COUNT) {
            EffectMeta meta = getEffectMeta(i);
            char categoryBuf[24];
            getEffectCategoryName(i, categoryBuf, sizeof(categoryBuf));
            cJSON_AddStringToObject(e, "category", categoryBuf);
            cJSON_AddNumberToObject(e, "categoryId", meta.category);

            char descBuf[80];
            getEffectDescription(i, descBuf, sizeof(descBuf));
            cJSON_AddStringToObject(e, "description", descBuf);

            cJSON_AddBoolToObject(e, "centerOrigin",
                (meta.features & EffectFeatures::CENTER_ORIGIN) != 0);
        }

        cJSON_AddItemToArray(effectsArr, e);
    }

    // Pagination links
    cJSON* links = cJSON_AddObjectToObject(data, "_links");
    cJSON_AddStringToObject(links, "self", "/api/v2/effects");
    if (offset > 0) {
        int prevOffset = offset - limit;
        if (prevOffset < 0) prevOffset = 0;
        char prevLink[64];
        snprintf(prevLink, sizeof(prevLink), "/api/v2/effects?offset=%d&limit=%d", prevOffset, limit);
        cJSON_AddStringToObject(links, "prev", prevLink);
    }
    if (end < NUM_EFFECTS) {
        char nextLink[64];
        snprintf(nextLink, sizeof(nextLink), "/api/v2/effects?offset=%d&limit=%d", end, limit);
        cJSON_AddStringToObject(links, "next", nextLink);
    }

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2EffectsCurrent(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddNumberToObject(data, "effectId", currentEffect);
    cJSON_AddStringToObject(data, "name", effects[currentEffect].name);

    if (currentEffect < EFFECT_METADATA_COUNT) {
        EffectMeta meta = getEffectMeta(currentEffect);
        char categoryBuf[24];
        getEffectCategoryName(currentEffect, categoryBuf, sizeof(categoryBuf));
        cJSON_AddStringToObject(data, "category", categoryBuf);
        cJSON_AddNumberToObject(data, "categoryId", meta.category);

        char descBuf[80];
        getEffectDescription(currentEffect, descBuf, sizeof(descBuf));
        cJSON_AddStringToObject(data, "description", descBuf);
    }

    cJSON* params = cJSON_AddObjectToObject(data, "parameters");
    cJSON_AddNumberToObject(params, "brightness", FastLED.getBrightness());
    cJSON_AddNumberToObject(params, "speed", effectSpeed);
    cJSON_AddNumberToObject(params, "paletteId", currentPaletteIndex);
    cJSON_AddNumberToObject(params, "intensity", effectIntensity);
    cJSON_AddNumberToObject(params, "saturation", effectSaturation);
    cJSON_AddNumberToObject(params, "complexity", effectComplexity);
    cJSON_AddNumberToObject(params, "variation", effectVariation);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2EffectsSet(httpd_req_t* req) {
    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(doc, "effectId");
    if (!effectIdNode || !cJSON_IsNumber(effectIdNode)) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "MISSING_FIELD", "effectId is required", "effectId");
    }

    int effectId = (int)effectIdNode->valuedouble;
    if (effectId < 0 || effectId >= NUM_EFFECTS) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "OUT_OF_RANGE", "effectId out of range", "effectId");
    }

    cJSON_Delete(doc);

    currentEffect = (uint8_t)effectId;
    webServer.notifyEffectChange(currentEffect);

    return handleV2EffectsCurrent(req);
}

esp_err_t handleV2EffectById(httpd_req_t* req) {
    char idBuf[8];
    if (!extractPathSegment(req->uri, "/api/v2/effects/", idBuf, sizeof(idBuf))) {
        return sendV2Error(req, 400, "INVALID_PATH", "Effect ID not found in path");
    }

    // Skip non-numeric paths (categories, current)
    if (!isdigit(idBuf[0])) {
        return sendV2Error(req, 404, "NOT_FOUND", "Effect not found");
    }

    int effectId = atoi(idBuf);
    if (effectId < 0 || effectId >= NUM_EFFECTS) {
        return sendV2Error(req, 404, "NOT_FOUND", "Effect not found");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddNumberToObject(data, "id", effectId);
    cJSON_AddStringToObject(data, "name", effects[effectId].name);

    if (effectId < EFFECT_METADATA_COUNT) {
        EffectMeta meta = getEffectMeta(effectId);

        char categoryBuf[24];
        getEffectCategoryName(effectId, categoryBuf, sizeof(categoryBuf));
        cJSON_AddStringToObject(data, "category", categoryBuf);
        cJSON_AddNumberToObject(data, "categoryId", meta.category);

        char descBuf[80];
        getEffectDescription(effectId, descBuf, sizeof(descBuf));
        cJSON_AddStringToObject(data, "description", descBuf);

        cJSON* features = cJSON_AddObjectToObject(data, "features");
        cJSON_AddBoolToObject(features, "centerOrigin",
            (meta.features & EffectFeatures::CENTER_ORIGIN) != 0);
        cJSON_AddBoolToObject(features, "usesSpeed",
            (meta.features & EffectFeatures::USES_SPEED) != 0);
        cJSON_AddBoolToObject(features, "usesPalette",
            (meta.features & EffectFeatures::USES_PALETTE) != 0);
        cJSON_AddBoolToObject(features, "zoneAware",
            (meta.features & EffectFeatures::ZONE_AWARE) != 0);
        cJSON_AddBoolToObject(features, "dualStrip",
            (meta.features & EffectFeatures::DUAL_STRIP) != 0);
        cJSON_AddBoolToObject(features, "physicsBased",
            (meta.features & EffectFeatures::PHYSICS_BASED) != 0);

        if (meta.paramCount > 0) {
            cJSON* customParams = cJSON_AddArrayToObject(data, "customParameters");
            for (uint8_t p = 0; p < meta.paramCount && p < 4; p++) {
                if (strlen(meta.params[p].name) > 0) {
                    cJSON* param = cJSON_CreateObject();
                    cJSON_AddStringToObject(param, "name", meta.params[p].name);
                    cJSON_AddNumberToObject(param, "min", meta.params[p].minVal);
                    cJSON_AddNumberToObject(param, "max", meta.params[p].maxVal);
                    cJSON_AddNumberToObject(param, "default", meta.params[p].defaultVal);
                    cJSON_AddItemToArray(customParams, param);
                }
            }
        }
    }

    cJSON_AddBoolToObject(data, "active", currentEffect == effectId);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2EffectsCategories(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "total", CAT_COUNT);

    cJSON* categories = cJSON_AddArrayToObject(data, "categories");

    // Count effects per category
    uint8_t categoryCounts[CAT_COUNT] = {0};
    for (uint8_t i = 0; i < NUM_EFFECTS && i < EFFECT_METADATA_COUNT; i++) {
        EffectMeta meta = getEffectMeta(i);
        if (meta.category < CAT_COUNT) {
            categoryCounts[meta.category]++;
        }
    }

    for (uint8_t c = 0; c < CAT_COUNT; c++) {
        cJSON* cat = cJSON_CreateObject();
        cJSON_AddNumberToObject(cat, "id", c);

        char nameBuf[24];
        strncpy_P(nameBuf, (char*)pgm_read_ptr(&CATEGORY_NAMES[c]), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        cJSON_AddStringToObject(cat, "name", nameBuf);

        cJSON_AddNumberToObject(cat, "count", categoryCounts[c]);
        cJSON_AddItemToArray(categories, cat);
    }

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Parameters Endpoints (4)
// ============================================================================

esp_err_t handleV2ParametersGet(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddNumberToObject(data, "brightness", FastLED.getBrightness());
    cJSON_AddNumberToObject(data, "speed", effectSpeed);
    cJSON_AddNumberToObject(data, "paletteId", currentPaletteIndex);
    cJSON_AddNumberToObject(data, "intensity", effectIntensity);
    cJSON_AddNumberToObject(data, "saturation", effectSaturation);
    cJSON_AddNumberToObject(data, "complexity", effectComplexity);
    cJSON_AddNumberToObject(data, "variation", effectVariation);

    cJSON* meta = cJSON_AddObjectToObject(data, "_meta");
    cJSON_AddStringToObject(meta, "brightness_range", "0-255");
    cJSON_AddStringToObject(meta, "speed_range", "1-50");
    cJSON_AddStringToObject(meta, "others_range", "0-255");

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ParametersPatch(httpd_req_t* req) {
    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    cJSON* node;

    node = cJSON_GetObjectItemCaseSensitive(doc, "brightness");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 0 && val <= 255) FastLED.setBrightness(val);
    }

    node = cJSON_GetObjectItemCaseSensitive(doc, "speed");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 1 && val <= 50) effectSpeed = val;
    }

    node = cJSON_GetObjectItemCaseSensitive(doc, "paletteId");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 0 && val <= 255) currentPaletteIndex = val;
    }

    node = cJSON_GetObjectItemCaseSensitive(doc, "intensity");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 0 && val <= 255) effectIntensity = val;
    }

    node = cJSON_GetObjectItemCaseSensitive(doc, "saturation");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 0 && val <= 255) effectSaturation = val;
    }

    node = cJSON_GetObjectItemCaseSensitive(doc, "complexity");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 0 && val <= 255) effectComplexity = val;
    }

    node = cJSON_GetObjectItemCaseSensitive(doc, "variation");
    if (node && cJSON_IsNumber(node)) {
        int val = (int)node->valuedouble;
        if (val >= 0 && val <= 255) effectVariation = val;
    }

    cJSON_Delete(doc);

    return handleV2ParametersGet(req);
}

esp_err_t handleV2ParameterGet(httpd_req_t* req) {
    char nameBuf[24];
    if (!extractPathSegment(req->uri, "/api/v2/parameters/", nameBuf, sizeof(nameBuf))) {
        return sendV2Error(req, 400, "INVALID_PATH", "Parameter name not found");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddStringToObject(data, "name", nameBuf);

    int value = -1;
    int minVal = 0, maxVal = 255;

    if (strcmp(nameBuf, "brightness") == 0) {
        value = FastLED.getBrightness();
    } else if (strcmp(nameBuf, "speed") == 0) {
        value = effectSpeed;
        minVal = 1; maxVal = 50;
    } else if (strcmp(nameBuf, "paletteId") == 0) {
        value = currentPaletteIndex;
    } else if (strcmp(nameBuf, "intensity") == 0) {
        value = effectIntensity;
    } else if (strcmp(nameBuf, "saturation") == 0) {
        value = effectSaturation;
    } else if (strcmp(nameBuf, "complexity") == 0) {
        value = effectComplexity;
    } else if (strcmp(nameBuf, "variation") == 0) {
        value = effectVariation;
    } else {
        cJSON_Delete(root);
        return sendV2Error(req, 404, "NOT_FOUND", "Unknown parameter");
    }

    cJSON_AddNumberToObject(data, "value", value);
    cJSON_AddNumberToObject(data, "min", minVal);
    cJSON_AddNumberToObject(data, "max", maxVal);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ParameterSet(httpd_req_t* req) {
    char nameBuf[24];
    if (!extractPathSegment(req->uri, "/api/v2/parameters/", nameBuf, sizeof(nameBuf))) {
        return sendV2Error(req, 400, "INVALID_PATH", "Parameter name not found");
    }

    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    cJSON* valueNode = cJSON_GetObjectItemCaseSensitive(doc, "value");
    if (!valueNode || !cJSON_IsNumber(valueNode)) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "MISSING_FIELD", "value is required", "value");
    }

    int value = (int)valueNode->valuedouble;
    cJSON_Delete(doc);

    bool found = true;

    if (strcmp(nameBuf, "brightness") == 0) {
        if (value >= 0 && value <= 255) FastLED.setBrightness(value);
    } else if (strcmp(nameBuf, "speed") == 0) {
        if (value >= 1 && value <= 50) effectSpeed = value;
    } else if (strcmp(nameBuf, "paletteId") == 0) {
        if (value >= 0 && value <= 255) currentPaletteIndex = value;
    } else if (strcmp(nameBuf, "intensity") == 0) {
        if (value >= 0 && value <= 255) effectIntensity = value;
    } else if (strcmp(nameBuf, "saturation") == 0) {
        if (value >= 0 && value <= 255) effectSaturation = value;
    } else if (strcmp(nameBuf, "complexity") == 0) {
        if (value >= 0 && value <= 255) effectComplexity = value;
    } else if (strcmp(nameBuf, "variation") == 0) {
        if (value >= 0 && value <= 255) effectVariation = value;
    } else {
        found = false;
    }

    if (!found) {
        return sendV2Error(req, 404, "NOT_FOUND", "Unknown parameter");
    }

    return handleV2ParameterGet(req);
}

// ============================================================================
// Transition Endpoints Implementation
// ============================================================================

esp_err_t handleV2TransitionsList(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* transitions = cJSON_AddArrayToObject(data, "transitions");

    for (uint8_t i = 0; i < TRANSITION_TYPE_COUNT; i++) {
        cJSON* t = cJSON_CreateObject();
        cJSON_AddNumberToObject(t, "id", i);
        cJSON_AddStringToObject(t, "name", TRANSITION_TYPE_INFO[i].name);
        cJSON_AddStringToObject(t, "description", TRANSITION_TYPE_INFO[i].description);
        cJSON_AddBoolToObject(t, "centerOrigin", true);  // All v2 transitions are CENTER ORIGIN
        cJSON_AddItemToArray(transitions, t);
    }

    cJSON_AddNumberToObject(data, "count", TRANSITION_TYPE_COUNT);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2TransitionsConfig(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    // Get current transition engine state
    bool isActive = transitionEngine.isActive();
    TransitionType currentType = transitionEngine.getCurrentType();
    float progress = transitionEngine.getProgress();

    cJSON_AddBoolToObject(data, "enabled", true);  // Transitions always enabled
    cJSON_AddBoolToObject(data, "active", isActive);
    cJSON_AddNumberToObject(data, "currentType", currentType);
    cJSON_AddStringToObject(data, "currentTypeName", TRANSITION_TYPE_INFO[currentType].name);
    cJSON_AddNumberToObject(data, "progress", progress);
    cJSON_AddNumberToObject(data, "defaultDuration", 1000);  // Default 1 second
    cJSON_AddBoolToObject(data, "randomize", useRandomTransitions);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2TransitionsConfigUpdate(httpd_req_t* req) {
    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    // Update randomize setting if provided
    cJSON* randomize = cJSON_GetObjectItemCaseSensitive(doc, "randomize");
    if (randomize && cJSON_IsBool(randomize)) {
        useRandomTransitions = cJSON_IsTrue(randomize);
    }

    cJSON_Delete(doc);

    // Return updated config
    return handleV2TransitionsConfig(req);
}

esp_err_t handleV2TransitionsTrigger(httpd_req_t* req) {
    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    // Required: toEffect
    cJSON* toEffectNode = cJSON_GetObjectItemCaseSensitive(doc, "toEffect");
    if (!toEffectNode || !cJSON_IsNumber(toEffectNode)) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "MISSING_FIELD", "toEffect is required", "toEffect");
    }

    uint8_t toEffect = (uint8_t)toEffectNode->valuedouble;
    if (toEffect >= NUM_EFFECTS) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "OUT_OF_RANGE", "Invalid effect ID", "toEffect");
    }

    // Optional: type (default random or FADE)
    TransitionType transType = useRandomTransitions ?
        TransitionEngine::getRandomTransition() : TRANSITION_FADE;
    cJSON* typeNode = cJSON_GetObjectItemCaseSensitive(doc, "type");
    if (typeNode && cJSON_IsNumber(typeNode)) {
        int typeVal = (int)typeNode->valuedouble;
        if (typeVal >= 0 && typeVal < TRANSITION_COUNT) {
            transType = (TransitionType)typeVal;
        }
    }

    // Optional: duration (default 1000ms)
    uint32_t duration = 1000;
    cJSON* durationNode = cJSON_GetObjectItemCaseSensitive(doc, "duration");
    if (durationNode && cJSON_IsNumber(durationNode)) {
        duration = (uint32_t)durationNode->valuedouble;
        if (duration < 100) duration = 100;
        if (duration > 10000) duration = 10000;
    }

    cJSON_Delete(doc);

    // Capture current state as source
    memcpy(transitionSourceBuffer, leds, sizeof(CRGB) * HardwareConfig::NUM_LEDS);

    // Render target effect to transitionBuffer
    // Note: This requires the effect to be rendered - for now we use the current buffer
    // In a full implementation, you'd render the target effect to transitionBuffer

    // Start the transition
    transitionEngine.setDualStripMode(true);
    transitionEngine.startTransition(
        transitionSourceBuffer,
        leds,  // Target will be rendered by main loop
        leds,
        transType,
        duration,
        EASE_IN_OUT_QUAD
    );

    // Update current effect (main loop will render it)
    currentEffect = toEffect;

    // Build response
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "toEffect", toEffect);
    cJSON_AddStringToObject(data, "effectName", effects[toEffect].name);
    cJSON_AddNumberToObject(data, "transitionType", transType);
    cJSON_AddStringToObject(data, "transitionName", TRANSITION_TYPE_INFO[transType].name);
    cJSON_AddNumberToObject(data, "duration", duration);
    cJSON_AddStringToObject(data, "status", "started");

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Zone Endpoints Implementation
// ============================================================================

esp_err_t handleV2ZonesList(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    bool systemEnabled = zoneComposer.isEnabled();
    uint8_t zoneCount = zoneComposer.getZoneCount();

    cJSON_AddBoolToObject(data, "enabled", systemEnabled);
    cJSON_AddNumberToObject(data, "zoneCount", zoneCount);
    cJSON_AddNumberToObject(data, "maxZones", HardwareConfig::MAX_ZONES);

    cJSON* zones = cJSON_AddArrayToObject(data, "zones");

    for (uint8_t i = 0; i < HardwareConfig::MAX_ZONES; i++) {
        cJSON* zone = cJSON_CreateObject();
        cJSON_AddNumberToObject(zone, "id", i);
        cJSON_AddBoolToObject(zone, "enabled", zoneComposer.isZoneEnabled(i));
        cJSON_AddBoolToObject(zone, "active", i < zoneCount);

        uint8_t effectId = zoneComposer.getZoneEffect(i);
        cJSON_AddNumberToObject(zone, "effectId", effectId);
        if (effectId < NUM_EFFECTS) {
            cJSON_AddStringToObject(zone, "effectName", effects[effectId].name);
        }

        cJSON_AddNumberToObject(zone, "brightness", zoneComposer.getZoneBrightness(i));
        cJSON_AddNumberToObject(zone, "speed", zoneComposer.getZoneSpeed(i));
        cJSON_AddNumberToObject(zone, "palette", zoneComposer.getZonePalette(i));
        cJSON_AddNumberToObject(zone, "blendMode", (uint8_t)zoneComposer.getZoneBlendMode(i));

        // Add visual parameters
        VisualParams params = zoneComposer.getZoneVisualParams(i);
        cJSON* paramsObj = cJSON_AddObjectToObject(zone, "parameters");
        cJSON_AddNumberToObject(paramsObj, "intensity", params.intensity);
        cJSON_AddNumberToObject(paramsObj, "saturation", params.saturation);
        cJSON_AddNumberToObject(paramsObj, "complexity", params.complexity);
        cJSON_AddNumberToObject(paramsObj, "variation", params.variation);

        cJSON_AddItemToArray(zones, zone);
    }

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ZonesEnable(httpd_req_t* req) {
    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    // Required: enabled
    cJSON* enabledNode = cJSON_GetObjectItemCaseSensitive(doc, "enabled");
    if (!enabledNode || !cJSON_IsBool(enabledNode)) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "MISSING_FIELD", "enabled is required", "enabled");
    }

    bool enabled = cJSON_IsTrue(enabledNode);

    // Optional: count (1-4)
    cJSON* countNode = cJSON_GetObjectItemCaseSensitive(doc, "count");
    if (countNode && cJSON_IsNumber(countNode)) {
        int count = (int)countNode->valuedouble;
        if (count >= 1 && count <= 4) {
            zoneComposer.setZoneCount((uint8_t)count);
        }
    }

    cJSON_Delete(doc);

    // Enable or disable zone system
    if (enabled) {
        zoneComposer.enable();
    } else {
        zoneComposer.disable();
    }

    // Save configuration
    zoneComposer.saveConfig();

    // Return updated zones list
    return handleV2ZonesList(req);
}

esp_err_t handleV2ZoneGet(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddNumberToObject(data, "id", zoneId);
    cJSON_AddBoolToObject(data, "enabled", zoneComposer.isZoneEnabled(zoneId));
    cJSON_AddBoolToObject(data, "active", zoneId < zoneComposer.getZoneCount());

    uint8_t effectId = zoneComposer.getZoneEffect(zoneId);
    cJSON_AddNumberToObject(data, "effectId", effectId);
    if (effectId < NUM_EFFECTS) {
        cJSON_AddStringToObject(data, "effectName", effects[effectId].name);
    }

    cJSON_AddNumberToObject(data, "brightness", zoneComposer.getZoneBrightness(zoneId));
    cJSON_AddNumberToObject(data, "speed", zoneComposer.getZoneSpeed(zoneId));
    cJSON_AddNumberToObject(data, "palette", zoneComposer.getZonePalette(zoneId));
    cJSON_AddNumberToObject(data, "blendMode", (uint8_t)zoneComposer.getZoneBlendMode(zoneId));

    // Visual parameters
    VisualParams params = zoneComposer.getZoneVisualParams(zoneId);
    cJSON* paramsObj = cJSON_AddObjectToObject(data, "parameters");
    cJSON_AddNumberToObject(paramsObj, "intensity", params.intensity);
    cJSON_AddNumberToObject(paramsObj, "saturation", params.saturation);
    cJSON_AddNumberToObject(paramsObj, "complexity", params.complexity);
    cJSON_AddNumberToObject(paramsObj, "variation", params.variation);

    // HATEOAS links
    cJSON* links = cJSON_AddObjectToObject(data, "_links");
    char selfLink[64], effectLink[64], paramsLink[64];
    snprintf(selfLink, sizeof(selfLink), "/api/v2/zones/%d", zoneId);
    snprintf(effectLink, sizeof(effectLink), "/api/v2/zones/%d/effect", zoneId);
    snprintf(paramsLink, sizeof(paramsLink), "/api/v2/zones/%d/parameters", zoneId);
    cJSON_AddStringToObject(links, "self", selfLink);
    cJSON_AddStringToObject(links, "effect", effectLink);
    cJSON_AddStringToObject(links, "parameters", paramsLink);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ZoneUpdate(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    // Update fields if provided
    cJSON* enabledNode = cJSON_GetObjectItemCaseSensitive(doc, "enabled");
    if (enabledNode && cJSON_IsBool(enabledNode)) {
        zoneComposer.enableZone(zoneId, cJSON_IsTrue(enabledNode));
    }

    cJSON* brightnessNode = cJSON_GetObjectItemCaseSensitive(doc, "brightness");
    if (brightnessNode && cJSON_IsNumber(brightnessNode)) {
        uint8_t val = (uint8_t)brightnessNode->valuedouble;
        zoneComposer.setZoneBrightness(zoneId, val);
    }

    cJSON* speedNode = cJSON_GetObjectItemCaseSensitive(doc, "speed");
    if (speedNode && cJSON_IsNumber(speedNode)) {
        uint8_t val = (uint8_t)speedNode->valuedouble;
        if (val >= 1 && val <= 50) {
            zoneComposer.setZoneSpeed(zoneId, val);
        }
    }

    cJSON* paletteNode = cJSON_GetObjectItemCaseSensitive(doc, "palette");
    if (paletteNode && cJSON_IsNumber(paletteNode)) {
        uint8_t val = (uint8_t)paletteNode->valuedouble;
        zoneComposer.setZonePalette(zoneId, val);
    }

    cJSON* blendModeNode = cJSON_GetObjectItemCaseSensitive(doc, "blendMode");
    if (blendModeNode && cJSON_IsNumber(blendModeNode)) {
        uint8_t val = (uint8_t)blendModeNode->valuedouble;
        if (val <= 4) {  // Valid blend modes
            zoneComposer.setZoneBlendMode(zoneId, (BlendMode)val);
        }
    }

    cJSON_Delete(doc);

    // Save configuration
    zoneComposer.saveConfig();

    // Return updated zone
    return handleV2ZoneGet(req);
}

esp_err_t handleV2ZoneDelete(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    // Disable the zone
    zoneComposer.enableZone(zoneId, false);
    zoneComposer.saveConfig();

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "zoneId", zoneId);
    cJSON_AddStringToObject(data, "status", "disabled");

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ZoneEffectGet(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    uint8_t effectId = zoneComposer.getZoneEffect(zoneId);
    cJSON_AddNumberToObject(data, "zoneId", zoneId);
    cJSON_AddNumberToObject(data, "effectId", effectId);
    if (effectId < NUM_EFFECTS) {
        cJSON_AddStringToObject(data, "effectName", effects[effectId].name);
    }

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ZoneEffectSet(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    cJSON* effectIdNode = cJSON_GetObjectItemCaseSensitive(doc, "effectId");
    if (!effectIdNode || !cJSON_IsNumber(effectIdNode)) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "MISSING_FIELD", "effectId is required", "effectId");
    }

    uint8_t effectId = (uint8_t)effectIdNode->valuedouble;
    if (effectId >= NUM_EFFECTS) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "OUT_OF_RANGE", "Invalid effect ID", "effectId");
    }

    cJSON_Delete(doc);

    zoneComposer.setZoneEffect(zoneId, effectId);
    zoneComposer.saveConfig();

    return handleV2ZoneEffectGet(req);
}

esp_err_t handleV2ZoneParametersGet(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddNumberToObject(data, "zoneId", zoneId);

    VisualParams params = zoneComposer.getZoneVisualParams(zoneId);
    cJSON_AddNumberToObject(data, "intensity", params.intensity);
    cJSON_AddNumberToObject(data, "saturation", params.saturation);
    cJSON_AddNumberToObject(data, "complexity", params.complexity);
    cJSON_AddNumberToObject(data, "variation", params.variation);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ZoneParametersUpdate(httpd_req_t* req) {
    uint8_t zoneId;
    if (!extractZoneId(req->uri, zoneId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid zone ID");
    }

    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    // Update individual parameters
    cJSON* intensityNode = cJSON_GetObjectItemCaseSensitive(doc, "intensity");
    if (intensityNode && cJSON_IsNumber(intensityNode)) {
        zoneComposer.setZoneIntensity(zoneId, (uint8_t)intensityNode->valuedouble);
    }

    cJSON* saturationNode = cJSON_GetObjectItemCaseSensitive(doc, "saturation");
    if (saturationNode && cJSON_IsNumber(saturationNode)) {
        zoneComposer.setZoneSaturation(zoneId, (uint8_t)saturationNode->valuedouble);
    }

    cJSON* complexityNode = cJSON_GetObjectItemCaseSensitive(doc, "complexity");
    if (complexityNode && cJSON_IsNumber(complexityNode)) {
        zoneComposer.setZoneComplexity(zoneId, (uint8_t)complexityNode->valuedouble);
    }

    cJSON* variationNode = cJSON_GetObjectItemCaseSensitive(doc, "variation");
    if (variationNode && cJSON_IsNumber(variationNode)) {
        zoneComposer.setZoneVariation(zoneId, (uint8_t)variationNode->valuedouble);
    }

    cJSON_Delete(doc);

    return handleV2ZoneParametersGet(req);
}

esp_err_t handleV2ZonePresetsList(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* presets = cJSON_AddArrayToObject(data, "presets");

    for (uint8_t i = 0; i < 5; i++) {
        cJSON* preset = cJSON_CreateObject();
        cJSON_AddNumberToObject(preset, "id", i);
        cJSON_AddStringToObject(preset, "name", ZONE_PRESET_NAMES[i]);
        cJSON_AddStringToObject(preset, "description", ZONE_PRESET_DESCRIPTIONS[i]);
        cJSON_AddNumberToObject(preset, "zoneCount", ZONE_PRESET_COUNTS[i]);

        char applyLink[64];
        snprintf(applyLink, sizeof(applyLink), "/api/v2/zones/presets/%d", i);
        cJSON_AddStringToObject(preset, "_apply", applyLink);

        cJSON_AddItemToArray(presets, preset);
    }

    cJSON_AddNumberToObject(data, "count", 5);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2ZonePresetApply(httpd_req_t* req) {
    uint8_t presetId;
    if (!extractPresetId(req->uri, presetId)) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Invalid preset ID (0-4)");
    }

    // Load the preset
    bool success = zoneComposer.loadPreset(presetId);
    if (!success) {
        return sendV2Error(req, 500, "INTERNAL_ERROR", "Failed to load preset");
    }

    zoneComposer.saveConfig();

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "presetId", presetId);
    cJSON_AddStringToObject(data, "presetName", ZONE_PRESET_NAMES[presetId]);
    cJSON_AddNumberToObject(data, "zoneCount", zoneComposer.getZoneCount());
    cJSON_AddBoolToObject(data, "enabled", zoneComposer.isEnabled());
    cJSON_AddStringToObject(data, "status", "applied");

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Batch Operations Implementation
// ============================================================================

esp_err_t handleV2Batch(httpd_req_t* req) {
    char* body = readRequestBody(req);
    if (!body) {
        return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    }

    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) {
        return sendV2Error(req, 400, "INVALID_JSON", "Failed to parse JSON");
    }

    cJSON* operationsNode = cJSON_GetObjectItemCaseSensitive(doc, "operations");
    if (!operationsNode || !cJSON_IsArray(operationsNode)) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "MISSING_FIELD", "operations array is required", "operations");
    }

    int opCount = cJSON_GetArraySize(operationsNode);
    if (opCount > MAX_BATCH_OPERATIONS) {
        cJSON_Delete(doc);
        return sendV2Error(req, 400, "OUT_OF_RANGE",
            "Maximum 10 operations per batch", "operations");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* results = cJSON_AddArrayToObject(data, "results");

    int succeeded = 0;
    int failed = 0;

    auto addResult = [&](int opIndex,
                         const char* method,
                         const char* path,
                         bool ok,
                         int statusCode,
                         const char* errMsg = nullptr) {
        cJSON* r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "operation", opIndex);
        cJSON_AddStringToObject(r, "method", method ? method : "");
        cJSON_AddStringToObject(r, "path", path ? path : "");
        cJSON_AddBoolToObject(r, "success", ok);
        cJSON_AddNumberToObject(r, "statusCode", statusCode);
        if (!ok && errMsg) {
            cJSON_AddStringToObject(r, "error", errMsg);
        }
        cJSON_AddItemToArray(results, r);
    };

    auto applyParametersPatch = [&](const cJSON* bodyObj) -> bool {
        if (!bodyObj || !cJSON_IsObject(bodyObj)) return false;
        bool any = false;
        const cJSON* n = nullptr;
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "brightness");
        if (n && cJSON_IsNumber(n)) { FastLED.setBrightness((uint8_t)n->valuedouble); any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "speed");
        if (n && cJSON_IsNumber(n)) { effectSpeed = constrain((uint8_t)n->valuedouble, 1, 50); any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "paletteId");
        if (n && cJSON_IsNumber(n)) { currentPaletteIndex = (uint8_t)n->valuedouble; any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "intensity");
        if (n && cJSON_IsNumber(n)) { effectIntensity = (uint8_t)n->valuedouble; any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "saturation");
        if (n && cJSON_IsNumber(n)) { effectSaturation = (uint8_t)n->valuedouble; any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "complexity");
        if (n && cJSON_IsNumber(n)) { effectComplexity = (uint8_t)n->valuedouble; any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "variation");
        if (n && cJSON_IsNumber(n)) { effectVariation = (uint8_t)n->valuedouble; any = true; }
        return any;
    };

    auto applyEffectsSetCurrent = [&](const cJSON* bodyObj) -> bool {
        if (!bodyObj || !cJSON_IsObject(bodyObj)) return false;
        const cJSON* eid = cJSON_GetObjectItemCaseSensitive(bodyObj, "effectId");
        if (!eid || !cJSON_IsNumber(eid)) return false;
        const int id = (int)eid->valuedouble;
        if (id < 0 || id >= NUM_EFFECTS) return false;
        currentEffect = (uint8_t)id;
        return true;
    };

    auto applyZonePatch = [&](uint8_t zoneId, const cJSON* bodyObj) -> bool {
        if (!bodyObj || !cJSON_IsObject(bodyObj)) return false;
        if (zoneId >= HardwareConfig::MAX_ZONES) return false;
        bool any = false;
        const cJSON* n = nullptr;
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "enabled");
        if (n && cJSON_IsBool(n)) { zoneComposer.enableZone(zoneId, cJSON_IsTrue(n)); any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "brightness");
        if (n && cJSON_IsNumber(n)) { zoneComposer.setZoneBrightness(zoneId, (uint8_t)n->valuedouble); any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "speed");
        if (n && cJSON_IsNumber(n)) { zoneComposer.setZoneSpeed(zoneId, constrain((uint8_t)n->valuedouble, 1, 50)); any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "palette");
        if (n && cJSON_IsNumber(n)) { zoneComposer.setZonePalette(zoneId, (uint8_t)n->valuedouble); any = true; }
        n = cJSON_GetObjectItemCaseSensitive(bodyObj, "blendMode");
        if (n && cJSON_IsNumber(n)) { zoneComposer.setZoneBlendMode(zoneId, (BlendMode)((uint8_t)n->valuedouble)); any = true; }
        if (any) zoneComposer.saveConfig();
        return any;
    };

    for (int i = 0; i < opCount; i++) {
        cJSON* op = cJSON_GetArrayItem(operationsNode, i);
        cJSON* methodNode = cJSON_GetObjectItemCaseSensitive(op, "method");
        cJSON* pathNode = cJSON_GetObjectItemCaseSensitive(op, "path");
        cJSON* bodyNode = cJSON_GetObjectItemCaseSensitive(op, "body");

        if (!methodNode || !cJSON_IsString(methodNode) || !pathNode || !cJSON_IsString(pathNode)) {
            addResult(i, "", "", false, 400, "method and path required");
            failed++;
            continue;
        }

        const char* method = methodNode->valuestring;
        const char* path = pathNode->valuestring;

        bool ok = false;
        int status = 200;
        const char* err = nullptr;

        if (strcmp(method, "PATCH") == 0 && strcmp(path, "/api/v2/parameters") == 0) {
            ok = applyParametersPatch(bodyNode);
            if (!ok) { status = 400; err = "Invalid parameters body"; }
        } else if (strcmp(method, "PUT") == 0 && strcmp(path, "/api/v2/effects/current") == 0) {
            ok = applyEffectsSetCurrent(bodyNode);
            if (!ok) { status = 400; err = "Invalid effectId"; }
        } else if (strcmp(method, "PATCH") == 0 && strncmp(path, "/api/v2/zones/", 13) == 0) {
            // Expect /api/v2/zones/{id}
            const char* idStr = path + 13;
            const int zoneId = atoi(idStr);
            ok = (zoneId >= 0 && zoneId < 4) && applyZonePatch((uint8_t)zoneId, bodyNode);
            if (!ok) { status = 400; err = "Invalid zone update"; }
        } else {
            ok = false;
            status = 400;
            err = "Unsupported operation";
        }

        addResult(i, method, path, ok, status, err);
        if (ok) succeeded++;
        else failed++;
    }

    cJSON_Delete(doc);

    cJSON_AddNumberToObject(data, "processed", opCount);
    cJSON_AddNumberToObject(data, "succeeded", succeeded);
    cJSON_AddNumberToObject(data, "failed", failed);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Enhancement Endpoints (6)
// ============================================================================

esp_err_t handleV2EnhancementsSummary(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

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

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2EnhancementsColorGet(httpd_req_t* req) {
#if !FEATURE_COLOR_ENGINE
    return sendV2Error(req, 501, "NOT_IMPLEMENTED", "Color engine not available in current build");
#else
    ColorEngine& ce = ColorEngine::getInstance();
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");

    cJSON_AddBoolToObject(data, "enabled", ce.isEnabled());

    // crossBlend
    cJSON* cb = cJSON_AddObjectToObject(data, "crossBlend");
    cJSON_AddBoolToObject(cb, "enabled", ce.isCrossBlendEnabled());
    const uint8_t pal1 = ce.getCrossBlendPalette1();
    const uint8_t pal2 = ce.getCrossBlendPalette2();
    const int pal3 = ce.getCrossBlendPalette3(); // -1 when unset
    cJSON_AddNumberToObject(cb, "palette1", pal1);
    cJSON_AddNumberToObject(cb, "palette2", pal2);
    if (pal3 < 0) cJSON_AddNullToObject(cb, "palette3");
    else cJSON_AddNumberToObject(cb, "palette3", pal3);
    cJSON_AddNumberToObject(cb, "blend1", ce.getBlendFactor1());
    cJSON_AddNumberToObject(cb, "blend2", ce.getBlendFactor2());
    cJSON_AddNumberToObject(cb, "blend3", ce.getBlendFactor3());

    // temporalRotation
    cJSON* tr = cJSON_AddObjectToObject(data, "temporalRotation");
    cJSON_AddBoolToObject(tr, "enabled", ce.isTemporalRotationEnabled());
    cJSON_AddNumberToObject(tr, "speed", ce.getRotationSpeed());
    cJSON_AddNumberToObject(tr, "phase", ce.getRotationPhase());

    // diffusion
    cJSON* df = cJSON_AddObjectToObject(data, "diffusion");
    cJSON_AddBoolToObject(df, "enabled", ce.isDiffusionEnabled());
    cJSON_AddNumberToObject(df, "amount", ce.getDiffusionAmount());

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
#endif
}

esp_err_t handleV2EnhancementsColorPatch(httpd_req_t* req) {
#if !FEATURE_COLOR_ENGINE
    return sendV2Error(req, 501, "NOT_IMPLEMENTED", "Color engine not available in current build");
#else
    char* body = readRequestBody(req);
    if (!body) return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) return sendV2Error(req, 400, "INVALID_JSON", "Malformed JSON");

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
        cJSON* amt = cJSON_GetObjectItemCaseSensitive(node, "amount");
        if (amt && cJSON_IsNumber(amt)) {
            ce.setDiffusionAmount((uint8_t)amt->valuedouble);
        }
        cJSON_AddItemToArray(updated, cJSON_CreateString("diffusion"));
    }

    cJSON_Delete(doc);

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddItemToObject(data, "updated", updated);
    cJSON_AddItemToObject(data, "current", current);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
#endif
}

esp_err_t handleV2EnhancementsColorReset(httpd_req_t* req) {
#if !FEATURE_COLOR_ENGINE
    return sendV2Error(req, 501, "NOT_IMPLEMENTED", "Color engine not available in current build");
#else
    ColorEngine::getInstance().reset();
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddStringToObject(data, "message", "Color engine reset to defaults");
    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
#endif
}

esp_err_t handleV2EnhancementsMotionGet(httpd_req_t* req) {
#if !FEATURE_MOTION_ENGINE
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddBoolToObject(data, "enabled", false);
    cJSON_AddStringToObject(data, "message", "Motion engine not available in current build");
    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
#else
    MotionEngine& me = MotionEngine::getInstance();
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddBoolToObject(data, "enabled", me.isEnabled());
    cJSON_AddNumberToObject(data, "warpStrength", me.getWarpStrength());
    cJSON_AddNumberToObject(data, "warpFrequency", me.getWarpFrequency());
    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
#endif
}

esp_err_t handleV2EnhancementsMotionPatch(httpd_req_t* req) {
#if !FEATURE_MOTION_ENGINE
    return sendV2Error(req, 501, "NOT_IMPLEMENTED", "Motion engine not available in current build");
#else
    char* body = readRequestBody(req);
    if (!body) return sendV2Error(req, 400, "INVALID_JSON", "Request body required");
    cJSON* doc = cJSON_Parse(body);
    free(body);
    if (!doc) return sendV2Error(req, 400, "INVALID_JSON", "Malformed JSON");

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

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddItemToObject(data, "updated", updated);
    cJSON_AddItemToObject(data, "current", current);
    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
#endif
}

// ============================================================================
// Palettes Endpoints (2)
// ============================================================================

esp_err_t handleV2PalettesList(httpd_req_t* req) {
    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON* arr = cJSON_AddArrayToObject(data, "palettes");

    // Keep response bounded: include id + name + 4 sample colours.
    for (uint8_t i = 0; i < gMasterPaletteCount; i++) {
        cJSON* p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "id", i);
        cJSON_AddStringToObject(p, "name", MasterPaletteNames[i]);
        cJSON* colors = cJSON_AddArrayToObject(p, "colors");
        addPaletteSamples(colors, i, 4);
        cJSON_AddItemToArray(arr, p);
    }
    cJSON_AddNumberToObject(data, "total", gMasterPaletteCount);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t handleV2PaletteById(httpd_req_t* req) {
    char idBuf[16];
    if (!extractPathSegment(req->uri, "/api/v2/palettes/", idBuf, sizeof(idBuf))) {
        return sendV2Error(req, 400, "INVALID_VALUE", "Missing palette ID", "id");
    }
    const int id = atoi(idBuf);
    if (id < 0 || id >= gMasterPaletteCount) {
        return sendV2Error(req, 404, "RESOURCE_NOT_FOUND", "Palette not found", "id");
    }

    cJSON* root = createV2Response(true);
    cJSON* data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "id", id);
    cJSON_AddStringToObject(data, "name", MasterPaletteNames[id]);

    cJSON* colors = cJSON_AddArrayToObject(data, "colors");
    addPaletteSamples(colors, (uint8_t)id, 16);

    esp_err_t result = sendV2Json(req, 200, root);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Route Registration
// ============================================================================

bool registerV2Routes(::IdfHttpServer& server) {
    bool allOk = true;

    // ========== Discovery & Device Endpoints (5) ==========
    allOk &= server.registerGet("/api/v2/", handleV2Discovery);
    allOk &= server.registerOptions("/api/v2/", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/openapi.json", handleV2OpenApi);
    allOk &= server.registerOptions("/api/v2/openapi.json", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/device", handleV2Device);
    allOk &= server.registerOptions("/api/v2/device", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/device/status", handleV2DeviceStatus);
    allOk &= server.registerOptions("/api/v2/device/status", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/device/info", handleV2DeviceInfo);
    allOk &= server.registerOptions("/api/v2/device/info", ::IdfHttpServer::corsOptionsHandler);

    // ========== Effects Endpoints (5) ==========
    allOk &= server.registerGet("/api/v2/effects", handleV2EffectsList);
    allOk &= server.registerOptions("/api/v2/effects", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/effects/current", handleV2EffectsCurrent);
    allOk &= server.registerPut("/api/v2/effects/current", handleV2EffectsSet);
    allOk &= server.registerOptions("/api/v2/effects/current", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/effects/categories", handleV2EffectsCategories);
    allOk &= server.registerOptions("/api/v2/effects/categories", ::IdfHttpServer::corsOptionsHandler);

    // Effects by ID (register numbered paths 0-19 for common range)
    // Note: Higher IDs can be accessed via /api/v2/effects?offset=N
    for (int i = 0; i < 20; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/api/v2/effects/%d", i);
        allOk &= server.registerGet(path, handleV2EffectById);
    }

    // ========== Parameters Endpoints (4) ==========
    allOk &= server.registerGet("/api/v2/parameters", handleV2ParametersGet);
    allOk &= server.registerPatch("/api/v2/parameters", handleV2ParametersPatch);
    allOk &= server.registerOptions("/api/v2/parameters", ::IdfHttpServer::corsOptionsHandler);

    // Individual parameter routes
    const char* paramNames[] = {
        "brightness", "speed", "paletteId", "intensity",
        "saturation", "complexity", "variation"
    };
    for (const char* name : paramNames) {
        char path[48];
        snprintf(path, sizeof(path), "/api/v2/parameters/%s", name);
        allOk &= server.registerGet(path, handleV2ParameterGet);
        allOk &= server.registerPut(path, handleV2ParameterSet);
        allOk &= server.registerOptions(path, ::IdfHttpServer::corsOptionsHandler);
    }

    // ========== Transition Endpoints ==========
    allOk &= server.registerGet("/api/v2/transitions", handleV2TransitionsList);
    allOk &= server.registerOptions("/api/v2/transitions", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/transitions/config", handleV2TransitionsConfig);
    allOk &= server.registerPatch("/api/v2/transitions/config", handleV2TransitionsConfigUpdate);
    allOk &= server.registerOptions("/api/v2/transitions/config", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerPost("/api/v2/transitions/trigger", handleV2TransitionsTrigger);
    allOk &= server.registerOptions("/api/v2/transitions/trigger", ::IdfHttpServer::corsOptionsHandler);

    // Zone endpoints
    allOk &= server.registerGet("/api/v2/zones", handleV2ZonesList);
    allOk &= server.registerPost("/api/v2/zones", handleV2ZonesEnable);
    allOk &= server.registerOptions("/api/v2/zones", ::IdfHttpServer::corsOptionsHandler);

    // Zone presets (must be registered before zone/{id} to avoid conflicts)
    allOk &= server.registerGet("/api/v2/zones/presets", handleV2ZonePresetsList);
    allOk &= server.registerOptions("/api/v2/zones/presets", ::IdfHttpServer::corsOptionsHandler);

    // Individual zone routes - register specific patterns
    // Note: ESP-IDF httpd doesn't support path parameters directly,
    // so we register handlers that parse the URI manually
    for (uint8_t i = 0; i < 4; i++) {
        char path[64];

        // /api/v2/zones/{id}
        snprintf(path, sizeof(path), "/api/v2/zones/%d", i);
        allOk &= server.registerGet(path, handleV2ZoneGet);
        allOk &= server.registerPatch(path, handleV2ZoneUpdate);
        allOk &= server.registerDelete(path, handleV2ZoneDelete);
        allOk &= server.registerOptions(path, ::IdfHttpServer::corsOptionsHandler);

        // /api/v2/zones/{id}/effect
        snprintf(path, sizeof(path), "/api/v2/zones/%d/effect", i);
        allOk &= server.registerGet(path, handleV2ZoneEffectGet);
        allOk &= server.registerPut(path, handleV2ZoneEffectSet);
        allOk &= server.registerOptions(path, ::IdfHttpServer::corsOptionsHandler);

        // /api/v2/zones/{id}/parameters
        snprintf(path, sizeof(path), "/api/v2/zones/%d/parameters", i);
        allOk &= server.registerGet(path, handleV2ZoneParametersGet);
        allOk &= server.registerPatch(path, handleV2ZoneParametersUpdate);
        allOk &= server.registerOptions(path, ::IdfHttpServer::corsOptionsHandler);
    }

    // Zone preset apply routes
    for (uint8_t i = 0; i < 5; i++) {
        char path[64];
        // Canonical (docs): /api/v2/zones/presets/{id}/load
        snprintf(path, sizeof(path), "/api/v2/zones/presets/%d/load", i);
        allOk &= server.registerPost(path, handleV2ZonePresetApply);
        allOk &= server.registerOptions(path, ::IdfHttpServer::corsOptionsHandler);

        // Back-compat: allow /api/v2/zones/presets/{id}
        snprintf(path, sizeof(path), "/api/v2/zones/presets/%d", i);
        allOk &= server.registerPost(path, handleV2ZonePresetApply);
        allOk &= server.registerOptions(path, ::IdfHttpServer::corsOptionsHandler);
    }

    // Enhancements (docs)
    allOk &= server.registerGet("/api/v2/enhancements", handleV2EnhancementsSummary);
    allOk &= server.registerOptions("/api/v2/enhancements", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/enhancements/color", handleV2EnhancementsColorGet);
    allOk &= server.registerPatch("/api/v2/enhancements/color", handleV2EnhancementsColorPatch);
    allOk &= server.registerOptions("/api/v2/enhancements/color", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerPost("/api/v2/enhancements/color/reset", handleV2EnhancementsColorReset);
    allOk &= server.registerOptions("/api/v2/enhancements/color/reset", ::IdfHttpServer::corsOptionsHandler);

    allOk &= server.registerGet("/api/v2/enhancements/motion", handleV2EnhancementsMotionGet);
    allOk &= server.registerPatch("/api/v2/enhancements/motion", handleV2EnhancementsMotionPatch);
    allOk &= server.registerOptions("/api/v2/enhancements/motion", ::IdfHttpServer::corsOptionsHandler);

    // Palettes (docs)
    allOk &= server.registerGet("/api/v2/palettes", handleV2PalettesList);
    allOk &= server.registerOptions("/api/v2/palettes", ::IdfHttpServer::corsOptionsHandler);

    // Palettes by ID (0-19 for common range - we have 75 total)
    for (int i = 0; i < 20; i++) {
        char path[48];
        snprintf(path, sizeof(path), "/api/v2/palettes/%d", i);
        allOk &= server.registerGet(path, handleV2PaletteById);
    }

    // Batch endpoint
    allOk &= server.registerPost("/api/v2/batch", handleV2Batch);
    allOk &= server.registerOptions("/api/v2/batch", ::IdfHttpServer::corsOptionsHandler);

    return allOk;
}

}  // namespace WebServerV2

#endif // FEATURE_WEB_SERVER
