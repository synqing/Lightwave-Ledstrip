/**
 * @file DebugHandlers.h
 * @brief Debug-related HTTP handlers for audio verbosity and memory profiling
 *
 * Provides REST API endpoints for runtime control of audio debug verbosity
 * and zone system memory profiling (Phase 2c.2).
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../../../config/features.h"

// Forward declarations
namespace lightwaveos { namespace zones { class ZoneComposer; } }

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Debug-related HTTP handlers
 *
 * Endpoints:
 *   GET  /api/v1/debug/audio - Get current verbosity settings
 *   POST /api/v1/debug/audio - Set verbosity level and/or base interval
 *   GET  /api/v1/debug/memory/zones - Get zone system memory stats (Phase 2c.2)
 */
class DebugHandlers {
public:
    // ==================== Zone Memory Profiling (Phase 2c.2) ====================

    /**
     * @brief Handle GET /api/v1/debug/memory/zones
     *
     * Returns zone system memory footprint:
     * - configSize: Per-zone config storage bytes
     * - bufferSize: LED buffer bytes (all zone buffers)
     * - composerOverhead: ZoneComposer struct size
     * - totalZoneBytes: Total zone system RAM footprint
     * - presetStorageMax: Max NVS usage for zone presets
     * - activeZones: Currently enabled zone count
     * - heapFree: ESP free heap
     * - heapLargestBlock: Largest contiguous free block
     *
     * @param request HTTP request
     * @param zoneComposer ZoneComposer instance for memory stats
     */
    static void handleZoneMemoryStats(AsyncWebServerRequest* request,
                                       zones::ZoneComposer* zoneComposer);

#if FEATURE_AUDIO_SYNC
    /**
     * @brief Handle GET /api/v1/debug/audio
     *
     * Returns current audio debug configuration:
     * - verbosity (0-5)
     * - baseInterval (frames between logs)
     * - derived intervals for each verbosity level
     */
    static void handleAudioDebugGet(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/debug/audio
     *
     * Sets audio debug configuration.
     * Request body:
     * {
     *   "verbosity": 0-5,        // optional
     *   "baseInterval": 1-1000   // optional
     * }
     */
    static void handleAudioDebugSet(AsyncWebServerRequest* request,
                                     uint8_t* data, size_t len);
#endif
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
