/**
 * @file V1ApiRoutes.cpp
 * @brief V1 API route registration implementation
 *
 * Registers all /api/v1/* routes, delegating to handler classes where available
 * and calling WebServer handler methods for routes not yet extracted.
 */

#include "V1ApiRoutes.h"
#include "../WebServer.h"
#include "../ApiResponse.h"
#include "../RequestValidator.h"
#include "handlers/DeviceHandlers.h"
#include "handlers/EffectHandlers.h"
#include "handlers/ZoneHandlers.h"
#include "handlers/ParameterHandlers.h"
#include "handlers/PaletteHandlers.h"
#include "handlers/TransitionHandlers.h"
#include "handlers/NarrativeHandlers.h"
#include "handlers/SystemHandlers.h"
#include "handlers/BatchHandlers.h"
#include "handlers/AudioHandlers.h"
#include "handlers/DebugHandlers.h"
#include "handlers/EffectPresetHandlers.h"
#include "handlers/FirmwareHandlers.h"
#include "handlers/PresetHandlers.h"
#include "handlers/ZonePresetHandlers.h"
#include <ESPAsyncWebServer.h>
#include <Arduino.h>

#define LW_LOG_TAG "V1Api"
#include "../../utils/Log.h"

#if FEATURE_MULTI_DEVICE
#include "../../sync/DeviceUUID.h"
#endif

// Extern declaration for zone config manager (defined in main.cpp)
namespace lightwaveos {
namespace persistence {
class ZoneConfigManager;
class PresetManager;
}
}
extern lightwaveos::persistence::ZoneConfigManager* zoneConfigMgr;
extern lightwaveos::persistence::PresetManager* presetMgr;

namespace lightwaveos {
namespace network {
namespace webserver {

void V1ApiRoutes::registerRoutes(
    HttpRouteRegistry& registry,
    const WebServerContext& ctx,
    WebServer* server,
    std::function<bool(AsyncWebServerRequest*)> checkRateLimit,
    std::function<bool(AsyncWebServerRequest*)> checkAPIKey,
    std::function<void()> broadcastStatus,
    std::function<void()> broadcastZoneState
) {
    LW_LOGI("V1ApiRoutes::registerRoutes() called");

    // Simple test route - registered before /api/v1/ to ensure route matching order
    registry.onGet("/api/v1/ping", [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"pong\":true}");
    });

    // API Discovery - GET /api/v1/ (public)
    registry.onGet("/api/v1/", [checkRateLimit](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handlers::SystemHandlers::handleApiDiscovery(request);
    });

    // Health - GET /api/v1/health (public)
    registry.onGet("/api/v1/health", [ctx, server, checkRateLimit](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handlers::SystemHandlers::handleHealth(request, ctx.renderer, server->getWebSocket());
    });

    // Device Status - GET /api/v1/device/status
    registry.onGet("/api/v1/device/status", [ctx, checkRateLimit, checkAPIKey, server](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        // Need ws client count - get from server
        size_t wsCount = server->getClientCount();
        handlers::DeviceHandlers::handleStatus(request, ctx.actorSystem, ctx.renderer, ctx.startTime, ctx.apMode, wsCount);
    });

    // Device Info - GET /api/v1/device/info
    registry.onGet("/api/v1/device/info", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::DeviceHandlers::handleInfo(request, ctx.actorSystem, ctx.renderer);
    });

#if FEATURE_MULTI_DEVICE
    // Sync Status - GET /api/v1/sync/status
    registry.onGet("/api/v1/sync/status", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        char buffer[256];
        size_t len = snprintf(buffer, sizeof(buffer),
            "{\"success\":true,\"data\":{\"enabled\":true,\"uuid\":\"%s\"},\"version\":\"1.0\"}",
            DEVICE_UUID.toString()
        );
        request->send(200, "application/json", buffer);
    });
#endif

    // Effect Metadata - GET /api/v1/effects/metadata?id=N
    registry.onGet("/api/v1/effects/metadata", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::EffectHandlers::handleMetadata(request, ctx.renderer);
    });

    // Effect Parameters - GET /api/v1/effects/parameters?id=N
    registry.onGet("/api/v1/effects/parameters", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::EffectHandlers::handleParametersGet(request, ctx.renderer);
    });

    // Effect Parameters - POST /api/v1/effects/parameters
    registry.onPost("/api/v1/effects/parameters",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::EffectHandlers::handleParametersSet(request, data, len, ctx.renderer);
        }
    );

    // Effect Parameters - PATCH /api/v1/effects/parameters (compat)
    registry.onPatch("/api/v1/effects/parameters",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::EffectHandlers::handleParametersSet(request, data, len, ctx.renderer);
        }
    );

    // Effect Families - GET /api/v1/effects/families
    registry.onGet("/api/v1/effects/families", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::EffectHandlers::handleFamilies(request);
    });

    // Effects List - GET /api/v1/effects
    registry.onGet("/api/v1/effects", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::EffectHandlers::handleList(request, ctx.renderer);
    });

    // Current Effect - GET /api/v1/effects/current
    registry.onGet("/api/v1/effects/current", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::EffectHandlers::handleCurrent(request, ctx.renderer);
    });

    // Set Effect - POST /api/v1/effects/set
    registry.onPost("/api/v1/effects/set",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus, server](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::EffectHandlers::handleSet(request, data, len, ctx.actorSystem, server->getCachedRendererState(), broadcastStatus);
        }
    );

    // Set Effect - PUT /api/v1/effects/current (compat)
    registry.onPut("/api/v1/effects/current",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus, server](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::EffectHandlers::handleSet(request, data, len, ctx.actorSystem, server->getCachedRendererState(), broadcastStatus);
        }
    );

    // Get Parameters - GET /api/v1/parameters
    registry.onGet("/api/v1/parameters", [ctx, checkRateLimit, checkAPIKey, server](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ParameterHandlers::handleGet(request, server->getCachedRendererState());
    });

    // Set Parameters - POST /api/v1/parameters
    registry.onPost("/api/v1/parameters",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ParameterHandlers::handleSet(request, data, len, ctx.actorSystem, broadcastStatus);
        }
    );

    // Set Parameters - PATCH /api/v1/parameters (compat)
    registry.onPatch("/api/v1/parameters",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ParameterHandlers::handleSet(request, data, len, ctx.actorSystem, broadcastStatus);
        }
    );

    // Audio routes - delegate to AudioHandlers
    registry.onGet("/api/v1/audio/parameters", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleParametersGet(request, ctx.actorSystem, ctx.renderer);
    });

    registry.onPost("/api/v1/audio/parameters",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::AudioHandlers::handleParametersSet(request, data, len, ctx.actorSystem, ctx.renderer);
        }
    );

    registry.onPatch("/api/v1/audio/parameters",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::AudioHandlers::handleParametersSet(request, data, len, ctx.actorSystem, ctx.renderer);
        }
    );

    registry.onPost("/api/v1/audio/control",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::AudioHandlers::handleControl(request, data, len, ctx.actorSystem);
        }
    );

    registry.onGet("/api/v1/audio/state", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleStateGet(request, ctx.actorSystem);
    });

    registry.onGet("/api/v1/audio/tempo", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleTempoGet(request, ctx.actorSystem);
    });

    registry.onGet("/api/v1/audio/presets", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handlePresetsList(request);
    });

    registry.onPost("/api/v1/audio/presets",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::AudioHandlers::handlePresetSave(request, data, len, ctx.actorSystem, ctx.renderer);
        }
    );

    registry.onGet("/api/v1/audio/presets/get", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handlePresetGet(request, id);
    });

    registry.onPost("/api/v1/audio/presets/apply", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handlePresetApply(request, id, ctx.actorSystem, ctx.renderer);
    });

    registry.onDelete("/api/v1/audio/presets/delete", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handlePresetDelete(request, id);
    });

    // Audio mappings routes
    registry.onGet("/api/v1/audio/mappings/sources", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleMappingsListSources(request);
    });

    registry.onGet("/api/v1/audio/mappings/targets", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleMappingsListTargets(request);
    });

    registry.onGet("/api/v1/audio/mappings/curves", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleMappingsListCurves(request);
    });

    registry.onGet("/api/v1/audio/mappings", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleMappingsList(request, ctx.renderer);
    });

    registry.onGet("/api/v1/audio/mappings/effect", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handleMappingsGet(request, id, ctx.renderer);
    });

    registry.onPost("/api/v1/audio/mappings/effect",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            if (!request->hasParam("id")) {
                sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                                  ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
                return;
            }
            uint8_t id = request->getParam("id")->value().toInt();
            handlers::AudioHandlers::handleMappingsSet(request, id, data, len, ctx.renderer);
        }
    );

    registry.onDelete("/api/v1/audio/mappings/effect", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handleMappingsDelete(request, id);
    });

    registry.onPost("/api/v1/audio/mappings/enable", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handleMappingsEnable(request, id, true);
    });

    registry.onPost("/api/v1/audio/mappings/disable", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::AudioHandlers::handleMappingsEnable(request, id, false);
    });

    registry.onGet("/api/v1/audio/mappings/stats", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleMappingsStats(request);
    });

    // Zone AGC routes
    registry.onGet("/api/v1/audio/zone-agc", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleZoneAGCGet(request, ctx.actorSystem);
    });

    registry.onPost("/api/v1/audio/zone-agc",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::AudioHandlers::handleZoneAGCSet(request, data, len, ctx.actorSystem);
        }
    );

    // Spike detection routes
    registry.onGet("/api/v1/audio/spike-detection", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleSpikeDetectionGet(request, ctx.actorSystem);
    });

    registry.onPost("/api/v1/audio/spike-detection/reset", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleSpikeDetectionReset(request, ctx.actorSystem);
    });

    // Calibration routes
    registry.onGet("/api/v1/audio/calibrate", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleCalibrateStatus(request, ctx.actorSystem);
    });

    registry.onPost("/api/v1/audio/calibrate/start",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::AudioHandlers::handleCalibrateStart(request, data, len, ctx.actorSystem);
        }
    );

    registry.onPost("/api/v1/audio/calibrate/cancel", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleCalibrateCancel(request, ctx.actorSystem);
    });

    registry.onPost("/api/v1/audio/calibrate/apply", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleCalibrateApply(request, ctx.actorSystem);
    });

#if FEATURE_AUDIO_BENCHMARK
    // Benchmark routes
    registry.onGet("/api/v1/audio/benchmark", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleBenchmarkGet(request, ctx.actorSystem, ctx);
    });

    registry.onPost("/api/v1/audio/benchmark/start", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleBenchmarkStart(request, ctx.actorSystem, ctx);
    });

    registry.onPost("/api/v1/audio/benchmark/stop", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleBenchmarkStop(request, ctx.actorSystem, ctx);
    });

    registry.onGet("/api/v1/audio/benchmark/history", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::AudioHandlers::handleBenchmarkHistory(request, ctx.actorSystem);
    });
#endif

    // Debug routes - Audio debug verbosity control (always available with FEATURE_AUDIO_SYNC)
    registry.onGet("/api/v1/debug/audio", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
#if FEATURE_AUDIO_SYNC
        handlers::DebugHandlers::handleAudioDebugGet(request);
#else
        sendSuccessResponse(request, [](JsonObject& data) {
            data["verbosity"] = 0;
            data["message"] = "Audio sync not enabled";
        });
#endif
    });

    registry.onPost("/api/v1/debug/audio",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::DebugHandlers::handleAudioDebugSet(request, data, len);
        }
    );

    // Debug routes - Zone memory profiling (Phase 2c.2)
    registry.onGet("/api/v1/debug/memory/zones", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::DebugHandlers::handleZoneMemoryStats(request, ctx.zoneComposer);
    });

    // Transition routes
    registry.onGet("/api/v1/transitions/types", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::TransitionHandlers::handleTypes(request);
    });

    registry.onPost("/api/v1/transitions/trigger",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus, server](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::TransitionHandlers::handleTrigger(request, data, len, ctx.actorSystem, server->getCachedRendererState(), broadcastStatus);
        }
    );

    registry.onGet("/api/v1/transitions/config", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::TransitionHandlers::handleConfigGet(request, ctx.renderer);
    });

    registry.onPost("/api/v1/transitions/config",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::TransitionHandlers::handleConfigSet(request, data, len);
        }
    );

    // Batch operations
    registry.onPost("/api/v1/batch",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::BatchHandlers::handleExecute(request, data, len, ctx.actorSystem, ctx.executeBatchAction, broadcastStatus);
        }
    );

    // Palette routes
    registry.onGet("/api/v1/palettes", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::PaletteHandlers::handleList(request, ctx.renderer);
    });

    registry.onGet("/api/v1/palettes/current", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::PaletteHandlers::handleCurrent(request, ctx.renderer);
    });

    registry.onPost("/api/v1/palettes/set",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastStatus](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::PaletteHandlers::handleSet(request, data, len, ctx.actorSystem, broadcastStatus);
        }
    );

    // Narrative routes
    registry.onGet("/api/v1/narrative/status", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::NarrativeHandlers::handleStatus(request);
    });

    registry.onGet("/api/v1/narrative/config", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::NarrativeHandlers::handleConfigGet(request);
    });

    registry.onPost("/api/v1/narrative/config",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::NarrativeHandlers::handleConfigSet(request, data, len);
        }
    );

    // OpenAPI spec
    registry.onGet("/api/v1/openapi.json", [checkRateLimit](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handlers::SystemHandlers::handleOpenApiSpec(request);
    });

    // Zone routes - delegate to ZoneHandlers
    registry.onGet("/api/v1/zones", [ctx, checkRateLimit, checkAPIKey, server](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleList(request, ctx.actorSystem, server->getCachedRendererState(), ctx.zoneComposer);
    });

    registry.onPost("/api/v1/zones/layout",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleLayout(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone regex routes - GET /api/v1/zones/:id
    registry.onGetRegex("^\\/api\\/v1\\/zones\\/([0-3])$", [ctx, server, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleGet(request, ctx.actorSystem, server->getCachedRendererState(), ctx.zoneComposer);
    });

    // Zone regex routes - POST /api/v1/zones/:id/effect
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/effect$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, server, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleSetEffect(request, data, len, ctx.actorSystem, server->getCachedRendererState(), ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone regex routes - POST /api/v1/zones/:id/brightness
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/brightness$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleSetBrightness(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone regex routes - POST /api/v1/zones/:id/speed
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/speed$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleSetSpeed(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone regex routes - POST /api/v1/zones/:id/palette
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/palette$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleSetPalette(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone regex routes - POST /api/v1/zones/:id/blend
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/blend$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleSetBlend(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone regex routes - POST /api/v1/zones/:id/enabled
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/enabled$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleSetEnabled(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // Zone non-regex route - POST /api/v1/zones/enabled
    registry.onPost("/api/v1/zones/enabled",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, server, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;

            if (!ctx.zoneComposer) {
                sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                                  ErrorCodes::FEATURE_DISABLED, "Zone system not available");
                return;
            }

            JsonDocument doc;
            VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEnabled, request);

            bool enabled = doc["enabled"];
            ctx.zoneComposer->setEnabled(enabled);

            // Send WebSocket event
            StaticJsonDocument<128> eventDoc;
            eventDoc["type"] = "zones.enabledChanged";
            eventDoc["enabled"] = enabled;
            String eventOutput;
            serializeJson(eventDoc, eventOutput);
            if (server->getWebSocket()) {
                server->getWebSocket()->textAll(eventOutput);
            }

            broadcastZoneState();

            sendSuccessResponse(request, [enabled](JsonObject& respData) {
                respData["enabled"] = enabled;
            });
        }
    );

    // ==================== Zone Persistence Routes (Phase 1.5) ====================

    // GET /api/v1/zones/config - Get zone persistence status
    registry.onGet("/api/v1/zones/config", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleConfigGet(request, ctx.zoneComposer, zoneConfigMgr);
    });

    // POST /api/v1/zones/config/save - Save zone config to NVS
    registry.onPost("/api/v1/zones/config/save", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleConfigSave(request, ctx.zoneComposer, zoneConfigMgr);
    });

    // POST /api/v1/zones/config/load - Reload config from NVS
    registry.onPost("/api/v1/zones/config/load", [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleConfigLoad(request, ctx.zoneComposer, zoneConfigMgr, broadcastZoneState);
    });

    // ==================== Zone Timing Metrics Routes (Phase 2a.1) ====================

    // GET /api/v1/zones/timing - Get zone composition timing metrics
    registry.onGet("/api/v1/zones/timing", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleTimingGet(request, ctx.zoneComposer);
    });

    // POST /api/v1/zones/timing/reset - Reset timing metrics
    registry.onPost("/api/v1/zones/timing/reset", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZoneHandlers::handleTimingReset(request, ctx.zoneComposer);
    });

    // ==================== Zone Audio Config Routes (Phase 2b.1) ====================

    // GET /api/v1/zones/:id/audio - Get zone audio config
    registry.onGetRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/audio$", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        // Extract zone ID from URL
        String path = request->url();
        int zonesIdx = path.indexOf("/zones/");
        uint8_t zoneId = (zonesIdx >= 0 && zonesIdx + 7 < path.length())
            ? path.charAt(zonesIdx + 7) - '0'
            : 255;
        handlers::ZoneHandlers::handleAudioConfigGet(request, zoneId, ctx.zoneComposer);
    });

    // POST /api/v1/zones/:id/audio - Set zone audio config
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/audio$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            // Extract zone ID from URL
            String path = request->url();
            int zonesIdx = path.indexOf("/zones/");
            uint8_t zoneId = (zonesIdx >= 0 && zonesIdx + 7 < path.length())
                ? path.charAt(zonesIdx + 7) - '0'
                : 255;
            handlers::ZoneHandlers::handleAudioConfigSet(request, data, len, zoneId, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // ==================== Zone Beat Trigger Routes (Phase 2b.2) ====================

    // GET /api/v1/zones/:id/beat-trigger - Get zone beat trigger config
    registry.onGetRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/beat-trigger$", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        // Extract zone ID from URL
        String path = request->url();
        int zonesIdx = path.indexOf("/zones/");
        uint8_t zoneId = (zonesIdx >= 0 && zonesIdx + 7 < path.length())
            ? path.charAt(zonesIdx + 7) - '0'
            : 255;
        handlers::ZoneHandlers::handleBeatTriggerGet(request, zoneId, ctx.zoneComposer);
    });

    // POST /api/v1/zones/:id/beat-trigger - Set zone beat trigger config
    registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/beat-trigger$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            // Extract zone ID from URL
            String path = request->url();
            int zonesIdx = path.indexOf("/zones/");
            uint8_t zoneId = (zonesIdx >= 0 && zonesIdx + 7 < path.length())
                ? path.charAt(zonesIdx + 7) - '0'
                : 255;
            handlers::ZoneHandlers::handleBeatTriggerSet(request, data, len, zoneId, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // ==================== Zone Reordering Route (Phase 2c.1) ====================

    // POST /api/v1/zones/reorder - Reorder zones with CENTER ORIGIN constraint
    registry.onPost("/api/v1/zones/reorder",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZoneHandlers::handleReorder(request, data, len, ctx.zoneComposer, broadcastZoneState);
        }
    );

    // ==================== Zone Preset Library Routes ====================

    // GET /api/v1/presets - List all saved presets
    registry.onGet("/api/v1/presets", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::PresetHandlers::handleList(request, presetMgr);
    });

    // GET /api/v1/presets/{name} - Download preset as JSON file
    registry.onGetRegex("^\\/api\\/v1\\/presets\\/([^/]+)$", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::PresetHandlers::handleGet(request, presetMgr);
    });

    // POST /api/v1/presets - Upload/save new preset (JSON body)
    registry.onPost("/api/v1/presets",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::PresetHandlers::handleSave(request, data, len, presetMgr);
        }
    );

    // PUT /api/v1/presets/{name} - Update existing preset
    registry.onPutRegex("^\\/api\\/v1\\/presets\\/([^/]+)$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::PresetHandlers::handleUpdate(request, data, len, presetMgr);
        }
    );

    // DELETE /api/v1/presets/{name} - Delete a preset
    registry.onDeleteRegex("^\\/api\\/v1\\/presets\\/([^/]+)$", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::PresetHandlers::handleDelete(request, presetMgr);
    });

    // POST /api/v1/presets/{name}/rename - Rename a preset
    registry.onPostRegex("^\\/api\\/v1\\/presets\\/([^/]+)\\/rename$",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::PresetHandlers::handleRename(request, data, len, presetMgr);
        }
    );

    // POST /api/v1/presets/{name}/load - Load preset into active zone config
    registry.onPostRegex("^\\/api\\/v1\\/presets\\/([^/]+)\\/load$", [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::PresetHandlers::handleLoad(request, ctx.zoneComposer, zoneConfigMgr, presetMgr, broadcastZoneState);
    });

    // POST /api/v1/presets/save-current - Save current config as new preset
    registry.onPost("/api/v1/presets/save-current",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::PresetHandlers::handleSaveCurrent(request, data, len, ctx.zoneComposer, zoneConfigMgr, presetMgr);
        }
    );

    // ==================== Effect Preset Routes ====================
    // List all saved effect presets
    registry.onGet("/api/v1/effect-presets", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::EffectPresetHandlers::handleList(request);
    });

    // Save current effect config as new preset
    registry.onPost("/api/v1/effect-presets",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::EffectPresetHandlers::handleSave(request, data, len, ctx.renderer);
        }
    );

    // Get preset by ID
    registry.onGet("/api/v1/effect-presets/get", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::EffectPresetHandlers::handleGet(request, id);
    });

    // Apply preset by ID
    registry.onPost("/api/v1/effect-presets/apply", [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::EffectPresetHandlers::handleApply(request, id, ctx.actorSystem, ctx.renderer);
    });

    // Delete preset by ID
    registry.onDelete("/api/v1/effect-presets/delete", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::EffectPresetHandlers::handleDelete(request, id);
    });

    // ==================== Zone Preset Routes (Phase 2a.2) ====================
    // List all saved zone presets
    registry.onGet("/api/v1/zone-presets", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        handlers::ZonePresetHandlers::handleList(request);
    });

    // Save current zone config as new preset
    registry.onPost("/api/v1/zone-presets",
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [ctx, checkRateLimit, checkAPIKey](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            if (!checkRateLimit(request)) return;
            if (!checkAPIKey(request)) return;
            handlers::ZonePresetHandlers::handleSave(request, data, len, ctx.zoneComposer);
        }
    );

    // Get zone preset by ID
    registry.onGet("/api/v1/zone-presets/get", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::ZonePresetHandlers::handleGet(request, id);
    });

    // Apply zone preset by ID
    registry.onPost("/api/v1/zone-presets/apply", [ctx, checkRateLimit, checkAPIKey, broadcastZoneState](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::ZonePresetHandlers::handleApply(request, id, ctx.actorSystem, ctx.zoneComposer, broadcastZoneState);
    });

    // Delete zone preset by ID
    registry.onDelete("/api/v1/zone-presets/delete", [checkRateLimit, checkAPIKey](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        if (!checkAPIKey(request)) return;
        if (!request->hasParam("id")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::MISSING_FIELD, "Missing id parameter", "id");
            return;
        }
        uint8_t id = request->getParam("id")->value().toInt();
        handlers::ZonePresetHandlers::handleDelete(request, id);
    });

    // ==================== Firmware/OTA Routes ====================

    // Firmware version - GET /api/v1/firmware/version (public - no auth required)
    registry.onGet("/api/v1/firmware/version", [checkRateLimit](AsyncWebServerRequest* request) {
        if (!checkRateLimit(request)) return;
        handlers::FirmwareHandlers::handleVersion(request);
    });

    // V1 API OTA update - POST /api/v1/firmware/update
    // Uses X-OTA-Token header for authentication
    registry.onPost("/api/v1/firmware/update",
        [](AsyncWebServerRequest* request) {
            // Request handler - called after upload completes
            handlers::FirmwareHandlers::handleV1Update(request,
                handlers::FirmwareHandlers::checkOTAToken);
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            // Upload handler - processes firmware chunks
            handlers::FirmwareHandlers::handleUpload(request, filename, index, data, len, final);
        }
    );

    // Legacy OTA update - POST /update
    // Simple text response format for curl compatibility
    registry.onPost("/update",
        [](AsyncWebServerRequest* request) {
            // Request handler - called after upload completes
            handlers::FirmwareHandlers::handleLegacyUpdate(request,
                handlers::FirmwareHandlers::checkOTAToken);
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            // Upload handler - processes firmware chunks
            handlers::FirmwareHandlers::handleUpload(request, filename, index, data, len, final);
        }
    );
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

