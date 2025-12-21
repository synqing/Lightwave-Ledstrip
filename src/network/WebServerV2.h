#ifndef WEB_SERVER_V2_H
#define WEB_SERVER_V2_H

/**
 * @file WebServerV2.h
 * @brief API v2 REST endpoint handlers for LightwaveOS
 *
 * Provides improved API design with:
 * - Consistent response format with version 2.0
 * - HATEOAS links for discoverability
 * - Pagination support for effects list
 * - Rich metadata including categories and descriptions
 * - PUT/PATCH semantics for updates
 *
 * All handlers are static functions registered with IdfHttpServer.
 */

#include "config/features.h"

#if FEATURE_WEB_SERVER

extern "C" {
#include "esp_http_server.h"
}

// Forward declaration (global namespace)
class IdfHttpServer;

namespace WebServerV2 {

// ============================================================================
// API Version
// ============================================================================
static constexpr const char* API_VERSION = "2.0.0";

// ============================================================================
// Discovery & Device Endpoints
// ============================================================================

/**
 * @brief GET /api/v2/ - API discovery with HATEOAS links
 * Returns API metadata, hardware info, and navigational links
 */
esp_err_t handleV2Discovery(httpd_req_t* req);

/**
 * @brief GET /api/v2/openapi.json - OpenAPI 3.0 specification
 * Returns the machine-readable API specification
 */
esp_err_t handleV2OpenApi(httpd_req_t* req);

/**
 * @brief GET /api/v2/device - Combined device status and info
 * Returns merged status and hardware information
 */
esp_err_t handleV2Device(httpd_req_t* req);

/**
 * @brief GET /api/v2/device/status - Runtime status
 * Returns uptime, heap, CPU frequency, WebSocket clients
 */
esp_err_t handleV2DeviceStatus(httpd_req_t* req);

/**
 * @brief GET /api/v2/device/info - Static device info
 * Returns firmware version, SDK version, flash info
 */
esp_err_t handleV2DeviceInfo(httpd_req_t* req);

// ============================================================================
// Effects Endpoints
// ============================================================================

/**
 * @brief GET /api/v2/effects - Paginated effects list with categories
 * Query params: offset (default 0), limit (default 20, max 50)
 * Returns effects array with id, name, category, description
 */
esp_err_t handleV2EffectsList(httpd_req_t* req);

/**
 * @brief GET /api/v2/effects/current - Current effect state
 * Returns current effect ID, name, and all active parameters
 */
esp_err_t handleV2EffectsCurrent(httpd_req_t* req);

/**
 * @brief PUT /api/v2/effects/current - Set current effect
 * Body: { "effectId": <number> }
 * Returns the newly set effect state
 */
esp_err_t handleV2EffectsSet(httpd_req_t* req);

/**
 * @brief GET /api/v2/effects/{id} - Single effect metadata
 * Path param: id (effect index)
 * Returns full metadata for one effect
 */
esp_err_t handleV2EffectById(httpd_req_t* req);

/**
 * @brief GET /api/v2/effects/categories - List all categories
 * Returns array of category objects with id, name, count
 */
esp_err_t handleV2EffectsCategories(httpd_req_t* req);

// ============================================================================
// Parameters Endpoints
// ============================================================================

/**
 * @brief GET /api/v2/parameters - All visual parameters
 * Returns brightness, speed, palette, intensity, saturation, complexity, variation
 */
esp_err_t handleV2ParametersGet(httpd_req_t* req);

/**
 * @brief PATCH /api/v2/parameters - Update multiple parameters
 * Body: { "brightness": N, "speed": N, ... } (all optional)
 * Returns updated parameter values
 */
esp_err_t handleV2ParametersPatch(httpd_req_t* req);

/**
 * @brief GET /api/v2/parameters/{name} - Single parameter value
 * Path param: name (brightness, speed, paletteId, intensity, etc.)
 */
esp_err_t handleV2ParameterGet(httpd_req_t* req);

/**
 * @brief PUT /api/v2/parameters/{name} - Set single parameter
 * Body: { "value": N }
 */
esp_err_t handleV2ParameterSet(httpd_req_t* req);

// ============================================================================
// Transition Endpoints (4)
// ============================================================================

/**
 * Transition type metadata for API responses
 */
struct TransitionTypeInfo {
    const char* name;
    const char* description;
};

// Transition type descriptions array (12 types)
extern const TransitionTypeInfo TRANSITION_TYPE_INFO[];
extern const uint8_t TRANSITION_TYPE_COUNT;

/**
 * @brief GET /api/v2/transitions - List all transition types
 * Returns array of {id, name, description, centerOrigin: true}
 */
esp_err_t handleV2TransitionsList(httpd_req_t* req);

/**
 * @brief GET /api/v2/transitions/config - Current transition settings
 * Returns {enabled, duration, type, randomize}
 */
esp_err_t handleV2TransitionsConfig(httpd_req_t* req);

/**
 * @brief PATCH /api/v2/transitions/config - Update transition settings
 * Body: {enabled?: bool, duration?: number, type?: number, randomize?: bool}
 */
esp_err_t handleV2TransitionsConfigUpdate(httpd_req_t* req);

/**
 * @brief POST /api/v2/transitions/trigger - Trigger a transition
 * Body: {toEffect: number, type?: number, duration?: number}
 */
esp_err_t handleV2TransitionsTrigger(httpd_req_t* req);

// ============================================================================
// Zone Endpoints (10)
// ============================================================================

/**
 * @brief GET /api/v2/zones - List all zones with status
 * Returns {enabled, zoneCount, zones: [{id, enabled, effectId, effectName, ...}]}
 */
esp_err_t handleV2ZonesList(httpd_req_t* req);

/**
 * @brief POST /api/v2/zones - Enable/configure zone system
 * Body: {enabled: bool, count?: number (1-4)}
 */
esp_err_t handleV2ZonesEnable(httpd_req_t* req);

/**
 * @brief GET /api/v2/zones/{id} - Get single zone details
 * Path param: id (0-3)
 */
esp_err_t handleV2ZoneGet(httpd_req_t* req);

/**
 * @brief PATCH /api/v2/zones/{id} - Update zone configuration
 * Body: {enabled?: bool, brightness?: number, speed?: number, palette?: number, blendMode?: number}
 */
esp_err_t handleV2ZoneUpdate(httpd_req_t* req);

/**
 * @brief DELETE /api/v2/zones/{id} - Disable a zone
 */
esp_err_t handleV2ZoneDelete(httpd_req_t* req);

/**
 * @brief GET /api/v2/zones/{id}/effect - Get zone's current effect
 */
esp_err_t handleV2ZoneEffectGet(httpd_req_t* req);

/**
 * @brief PUT /api/v2/zones/{id}/effect - Set zone effect
 * Body: {effectId: number}
 */
esp_err_t handleV2ZoneEffectSet(httpd_req_t* req);

/**
 * @brief GET /api/v2/zones/{id}/parameters - Get zone visual parameters
 */
esp_err_t handleV2ZoneParametersGet(httpd_req_t* req);

/**
 * @brief PATCH /api/v2/zones/{id}/parameters - Update zone parameters
 * Body: {intensity?: number, saturation?: number, complexity?: number, variation?: number}
 */
esp_err_t handleV2ZoneParametersUpdate(httpd_req_t* req);

/**
 * @brief GET /api/v2/zones/presets - List zone presets
 * Returns array of {id, name, zoneCount, description}
 */
esp_err_t handleV2ZonePresetsList(httpd_req_t* req);

/**
 * @brief POST /api/v2/zones/presets/{id} - Apply a zone preset
 * Path param: id (0-4 for Single, Dual, Triple, Quad, Alternating)
 */
esp_err_t handleV2ZonePresetApply(httpd_req_t* req);

// ============================================================================
// Enhancement Endpoints (6)
// ============================================================================
/**
 * @brief GET /api/v2/enhancements - Summary of enhancement systems
 */
esp_err_t handleV2EnhancementsSummary(httpd_req_t* req);

/**
 * @brief GET /api/v2/enhancements/color - Get ColorEngine configuration
 */
esp_err_t handleV2EnhancementsColorGet(httpd_req_t* req);

/**
 * @brief PATCH /api/v2/enhancements/color - Update ColorEngine configuration
 */
esp_err_t handleV2EnhancementsColorPatch(httpd_req_t* req);

/**
 * @brief POST /api/v2/enhancements/color/reset - Reset ColorEngine to defaults
 */
esp_err_t handleV2EnhancementsColorReset(httpd_req_t* req);

/**
 * @brief GET /api/v2/enhancements/motion - Get MotionEngine configuration
 */
esp_err_t handleV2EnhancementsMotionGet(httpd_req_t* req);

/**
 * @brief PATCH /api/v2/enhancements/motion - Update MotionEngine configuration
 */
esp_err_t handleV2EnhancementsMotionPatch(httpd_req_t* req);

// ============================================================================
// Palettes Endpoints (2)
// ============================================================================
/**
 * @brief GET /api/v2/palettes - List available palettes
 */
esp_err_t handleV2PalettesList(httpd_req_t* req);

/**
 * @brief GET /api/v2/palettes/{id} - Get palette details
 */
esp_err_t handleV2PaletteById(httpd_req_t* req);

// ============================================================================
// Batch Operations (1)
// ============================================================================

/**
 * Maximum operations per batch request
 */
static constexpr uint8_t MAX_BATCH_OPERATIONS = 10;

/**
 * @brief POST /api/v2/batch - Execute multiple operations
 * Body: {operations: [{method: string, path: string, body?: object}, ...]}
 * Returns {results: [{success: bool, status: number, data/error: ...}, ...]}
 */
esp_err_t handleV2Batch(httpd_req_t* req);

// ============================================================================
// Registration Helper
// ============================================================================

/**
 * @brief Register all v2 endpoints with the HTTP server
 * @param server Pointer to IdfHttpServer instance
 * @return true if all routes registered successfully
 */
bool registerV2Routes(::IdfHttpServer& server);

}  // namespace WebServerV2

#endif // FEATURE_WEB_SERVER

#endif // WEB_SERVER_V2_H
