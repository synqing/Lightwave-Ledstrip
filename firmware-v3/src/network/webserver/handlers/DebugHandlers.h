// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file DebugHandlers.h
 * @brief Debug-related HTTP handlers for unified debug configuration
 *
 * Provides REST API endpoints for runtime control of debug verbosity
 * across all domains (audio, render, network, actor, system).
 *
 * Unified Debug Endpoints (always available):
 *   GET  /api/v1/debug/config  - Get full debug configuration
 *   POST /api/v1/debug/config  - Update debug configuration
 *   POST /api/v1/debug/status  - Trigger one-shot status (returns JSON)
 *
 * Legacy Audio Debug Endpoints (guarded by FEATURE_AUDIO_SYNC):
 *   GET  /api/v1/debug/audio   - Get current audio verbosity settings
 *   POST /api/v1/debug/audio   - Set audio verbosity level
 *
 * @see docs/debugging/SERIAL_DEBUG_REDESIGN_PROPOSAL.md
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../../../config/features.h"

// Forward declarations
namespace lightwaveos {
namespace actors {
class ActorSystem;
}
namespace zones {
class ZoneComposer;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
class UdpStreamer;
namespace handlers {

/**
 * @brief Debug-related HTTP handlers
 */
class DebugHandlers {
public:
    // ========================================================================
    // Unified Debug Config Handlers (always available)
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/debug/config
     *
     * Returns the full debug configuration:
     * {
     *   "globalLevel": 2,
     *   "domains": {
     *     "audio": -1, "render": -1, "network": -1, "actor": -1, "system": -1
     *   },
     *   "intervals": { "status": 0, "spectrum": 0 },
     *   "effectiveLevels": {
     *     "audio": 2, "render": 2, "network": 2, "actor": 2, "system": 2
     *   },
     *   "levels": ["OFF", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"]
     * }
     */
    static void handleDebugConfigGet(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/debug/config
     *
     * Updates debug configuration. All fields optional:
     * {
     *   "globalLevel": 3,        // 0-5
     *   "audio": 4,              // -1 to use global, 0-5 for override
     *   "render": -1,
     *   "network": -1,
     *   "actor": -1,
     *   "system": -1,
     *   "statusInterval": 30,    // seconds (0=off)
     *   "spectrumInterval": 0    // seconds (0=off)
     * }
     */
    static void handleDebugConfigSet(AsyncWebServerRequest* request,
                                      uint8_t* data, size_t len);

    /**
     * @brief Handle POST /api/v1/debug/status
     *
     * Triggers one-shot status print and returns data as JSON:
     * {
     *   "audio": {
     *     "micLevelDb": -12.3, "rms": 0.234, "rmsPreGain": 0.156,
     *     "agcGain": 1.5, "dcEstimate": 123.4, "clips": 0,
     *     "captures": 50000, "capturesFailed": 0
     *   },
     *   "memory": {
     *     "heapFree": 123456, "heapMin": 100000, "heapMaxBlock": 80000
     *   }
     * }
     *
     * @param request The HTTP request
     * @param actorSystem Reference to the actor system for audio access
     */
    static void handleDebugStatus(AsyncWebServerRequest* request,
                                   lightwaveos::actors::ActorSystem& actorSystem);

    // ========================================================================
    // Legacy Audio Debug Handlers (backward compatibility)
    // ========================================================================

    // ========================================================================
    // Zone Memory Stats Handler
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/debug/memory/zones
     *
     * Returns zone system memory statistics:
     * {
     *   "zones": [ { "id": 0, "bufferSize": 640, ... }, ... ],
     *   "totalMemory": 2560,
     *   "composerOverhead": 256
     * }
     */
    static void handleZoneMemoryStats(AsyncWebServerRequest* request,
                                       lightwaveos::zones::ZoneComposer* zoneComposer);

    /**
     * @brief Handle GET /api/v1/debug/udp
     *
     * Returns UDP streaming counters and backoff state:
     * {
     *   "started": true,
     *   "subscribers": 1,
     *   "suppressed": 0,
     *   "cooldownRemainingMs": 0,
     *   "consecutiveFailures": 0,
     *   "lastFailureMs": 1234,
     *   "lastFailureAgoMs": 250,
     *   "led": {"attempts": 10, "success": 10, "failures": 0},
     *   "audio": {"attempts": 10, "success": 9, "failures": 1}
     * }
     */
    static void handleUdpStatsGet(AsyncWebServerRequest* request,
                                   webserver::UdpStreamer* udpStreamer);

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
