#pragma once
// ============================================================================
// WsMessageRouter - WebSocket Message Router for Tab5.encoder
// ============================================================================
// Routes incoming WebSocket JSON messages by "type" field to appropriate
// handlers. Handles parameter synchronization with echo prevention.
//
// Supported message types:
// - "status"             : Full parameter sync from LightwaveOS
// - "device.status"      : Device info (logged, not processed)
// - "parameters.changed" : Parameter change notification (triggers refresh)
// - "zone.status"        : Zone-specific sync (NEW for Tab5)
//
// Ported from K1.8encoderS3, extended for:
// - 16 parameters (dual M5ROTATE8)
// - Zone status handling
// - JsonDocument (ArduinoJson v7+)
// ============================================================================

#include <ArduinoJson.h>
#include "../parameters/ParameterHandler.h"
#include "../parameters/ParameterMap.h"

// Forward declaration for WebSocketClient (if we need to request status)
class WebSocketClient;

/**
 * @class WsMessageRouter
 * @brief Static class for routing incoming WebSocket messages
 *
 * Usage:
 *   WsMessageRouter::init(&parameterHandler, &wsClient);
 *   // In WebSocketClient message callback:
 *   WsMessageRouter::route(doc);
 */
class WsMessageRouter {
public:
    /**
     * @brief Initialize router with parameter handler and optional WebSocket client
     * @param paramHandler Parameter handler for status messages
     * @param wsClient WebSocket client for requesting status refreshes (optional)
     */
    static void init(ParameterHandler* paramHandler, WebSocketClient* wsClient = nullptr) {
        s_paramHandler = paramHandler;
        s_wsClient = wsClient;
    }

    /**
     * @brief Route incoming WebSocket message by type
     * @param doc Parsed JSON document
     * @return true if message was handled, false if unknown/ignored
     */
    static bool route(JsonDocument& doc) {
        // Must have "type" field (LightwaveOS protocol)
        // ArduinoJson 7 pattern: use is<T>() instead of containsKey()
        if (!doc["type"].is<const char*>()) {
            return false;
        }

        const char* type = doc["type"];
        if (!type) {
            return false;
        }

        // Route by message type
        if (strcmp(type, "status") == 0) {
            handleStatus(doc);
            return true;
        }

        if (strcmp(type, "device.status") == 0) {
            handleDeviceStatus(doc);
            return true;
        }

        if (strcmp(type, "parameters.changed") == 0) {
            handleParametersChanged(doc);
            return true;
        }

        if (strcmp(type, "zone.status") == 0) {
            handleZoneStatus(doc);
            return true;
        }

        if (strcmp(type, "effects.changed") == 0) {
            handleEffectsChanged(doc);
            return true;
        }

        // Unknown message type - ignore silently
        // (Rate-limited logging could be added here for debugging)
        return false;
    }

private:
    static inline ParameterHandler* s_paramHandler = nullptr;
    static inline WebSocketClient* s_wsClient = nullptr;

    /**
     * @brief Handle "status" message from LightwaveOS
     *
     * Applies all parameter values from status message.
     * Echo prevention: uses setValue(..., false) to avoid triggering callbacks.
     *
     * Expected format:
     * {
     *   "type": "status",
     *   "effectId": 12,
     *   "brightness": 128,
     *   "paletteId": 5,
     *   "speed": 25,
     *   "intensity": 128,
     *   "saturation": 255,
     *   "complexity": 128,
     *   "variation": 0
     * }
     */
    static void handleStatus(JsonDocument& doc) {
        if (s_paramHandler) {
            bool updated = s_paramHandler->applyStatus(doc);
            if (updated) {
                Serial.println("[WsRouter] Status applied");
            }
        }
    }

    /**
     * @brief Handle "device.status" message
     *
     * Device information (uptime, firmware version, etc.)
     * Currently logged but not processed.
     *
     * Expected format:
     * {
     *   "type": "device.status",
     *   "uptime": 12345,
     *   "freeHeap": 123456,
     *   "firmwareVersion": "2.0.0"
     * }
     */
    static void handleDeviceStatus(JsonDocument& doc) {
        // Optional: Log device status for debugging
        if (doc["uptime"].is<unsigned long>()) {
            Serial.printf("[WsRouter] Device uptime: %lu sec\n",
                          doc["uptime"].as<unsigned long>());
        }
        // Could store device info for display on Tab5 screen
        (void)doc;
    }

    /**
     * @brief Handle "parameters.changed" message
     *
     * Notification that parameters have changed on the server.
     * Could trigger a status refresh request.
     *
     * Expected format:
     * {
     *   "type": "parameters.changed",
     *   "source": "web"  // Optional: who made the change
     * }
     */
    static void handleParametersChanged(JsonDocument& doc) {
        // Optional: Request fresh status from server
        // For now, rely on periodic status broadcasts from LightwaveOS
        Serial.println("[WsRouter] Parameters changed notification");
        (void)doc;
    }

    /**
     * @brief Handle "zone.status" message (NEW for Tab5)
     *
     * Zone-specific parameter sync for multi-zone LED configurations.
     * Supports up to 4 zones, each with independent effect/brightness.
     *
     * Expected format:
     * {
     *   "type": "zone.status",
     *   "enabled": true,
     *   "zoneCount": 2,
     *   "zones": [
     *     {"id": 0, "effectId": 5, "brightness": 200, "speed": 25},
     *     {"id": 1, "effectId": 8, "brightness": 180, "speed": 30},
     *     ...
     *   ]
     * }
     */
    static void handleZoneStatus(JsonDocument& doc) {
        if (!s_paramHandler) {
            return;
        }

        // Check if zones array exists (ArduinoJson 7 pattern)
        if (!doc["zones"].is<JsonArray>()) {
            Serial.println("[WsRouter] zone.status: missing zones array");
            return;
        }

        JsonArray zones = doc["zones"].as<JsonArray>();
        int zoneCount = 0;

        for (JsonObject zone : zones) {
            if (!zone["id"].is<uint8_t>()) continue;

            uint8_t zoneId = zone["id"].as<uint8_t>();

            // Map zone parameters to Unit B encoders (indices 8-15)
            // Zone 0 -> Zone0Effect (8), Zone0Brightness (9)
            // Zone 1 -> Zone1Effect (10), Zone1Brightness (11)
            // etc.
            if (zoneId > 3) {
                Serial.printf("[WsRouter] Zone %d out of range (max 3)\n", zoneId);
                continue;
            }

            // Map zone ID to the correct ParameterId enum values
            ParameterId effectParam, brightnessParam;
            switch (zoneId) {
                case 0:
                    effectParam = ParameterId::Zone0Effect;
                    brightnessParam = ParameterId::Zone0Brightness;
                    break;
                case 1:
                    effectParam = ParameterId::Zone1Effect;
                    brightnessParam = ParameterId::Zone1Brightness;
                    break;
                case 2:
                    effectParam = ParameterId::Zone2Effect;
                    brightnessParam = ParameterId::Zone2Brightness;
                    break;
                case 3:
                    effectParam = ParameterId::Zone3Effect;
                    brightnessParam = ParameterId::Zone3Brightness;
                    break;
                default:
                    continue;  // Should not reach here
            }

            // Zone effectId
            if (zone["effectId"].is<uint8_t>()) {
                uint8_t effectId = zone["effectId"].as<uint8_t>();
                s_paramHandler->setValue(effectParam, effectId);
            }

            // Zone brightness
            if (zone["brightness"].is<uint8_t>()) {
                uint8_t brightness = zone["brightness"].as<uint8_t>();
                s_paramHandler->setValue(brightnessParam, brightness);
            }

            zoneCount++;
        }

        if (zoneCount > 0) {
            Serial.printf("[WsRouter] Zone status: %d zones synced\n", zoneCount);
        }
    }

    /**
     * @brief Handle "effects.changed" message
     *
     * Notification that effects list has changed on the server.
     * Could trigger effect metadata refresh.
     *
     * Expected format:
     * {
     *   "type": "effects.changed",
     *   "count": 45
     * }
     */
    static void handleEffectsChanged(JsonDocument& doc) {
        // Optional: Could request updated effect list
        Serial.println("[WsRouter] Effects changed notification");
        (void)doc;
    }
};
