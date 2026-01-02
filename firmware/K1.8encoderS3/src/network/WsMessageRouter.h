#pragma once

#include <ArduinoJson.h>

// Forward declarations
class ParameterHandler;

/**
 * @file WsMessageRouter.h
 * @brief Client-side WebSocket message router
 * 
 * Routes inbound messages by "type" field to appropriate handlers.
 * Matches LightwaveOS server protocol expectations.
 */

class WsMessageRouter {
public:
    /**
     * @brief Initialize router with parameter handler
     * @param paramHandler Parameter handler for status messages
     */
    static void init(ParameterHandler* paramHandler);

    /**
     * @brief Route incoming WebSocket message
     * @param doc Parsed JSON document
     * @return true if message was handled, false if unknown/ignored
     */
    static bool route(StaticJsonDocument<512>& doc);

private:
    static ParameterHandler* s_paramHandler;
    
    /**
     * @brief Handle "status" message from LightwaveOS
     * @param doc JSON document with type="status"
     */
    static void handleStatus(StaticJsonDocument<512>& doc);
    
    /**
     * @brief Handle "device.status" message (optional, can ignore)
     * @param doc JSON document with type="device.status"
     */
    static void handleDeviceStatus(StaticJsonDocument<512>& doc);
    
    /**
     * @brief Handle "parameters.changed" message (optional, can trigger refresh)
     * @param doc JSON document with type="parameters.changed"
     */
    static void handleParametersChanged(StaticJsonDocument<512>& doc);
    
    /**
     * @brief Handle "effects.changed" message (optional, can trigger refresh)
     * @param doc JSON document with type="effects.changed"
     */
    static void handleEffectsChanged(StaticJsonDocument<512>& doc);
};

