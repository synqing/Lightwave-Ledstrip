// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file V1ApiRoutes.h
 * @brief V1 API route registration (/api/v1/*)
 *
 * Handles modern REST API v1 endpoints with standardized responses.
 * Delegates to handler classes where available.
 */

#pragma once

#include "HttpRouteRegistry.h"
#include "WebServerContext.h"
#include <functional>

// Forward declaration
namespace lightwaveos {
namespace network {
class WebServer;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief V1 API route registrar
 */
class V1ApiRoutes {
public:
    /**
     * @brief Register all V1 API routes
     * @param registry Route registry
     * @param ctx WebServer context
     * @param server WebServer instance (for calling handler methods)
     * @param checkRateLimit Rate limit checker function
     * @param checkAPIKey API key checker function
     * @param broadcastStatus Status broadcast callback
     * @param broadcastZoneState Zone state broadcast callback
     */
    static void registerRoutes(
        HttpRouteRegistry& registry,
        const WebServerContext& ctx,
        WebServer* server,
        std::function<bool(AsyncWebServerRequest*)> checkRateLimit,
        std::function<bool(AsyncWebServerRequest*)> checkAPIKey,
        std::function<void()> broadcastStatus,
        std::function<void()> broadcastZoneState
    );
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

