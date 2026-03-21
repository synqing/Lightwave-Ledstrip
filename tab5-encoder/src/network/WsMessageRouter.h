// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
#include <cstring>
#include <cmath>
#include "../parameters/ParameterHandler.h"
#include "../parameters/ParameterMap.h"
#include "../zones/ZoneDefinition.h"
#include "../ui/ZoneComposerUI.h"
#include "../ui/ControlSurfaceUI.h"
#include "../ui/DisplayUI.h"
#include "WebSocketClient.h"

// External function to cache palette names (defined in main.cpp)
extern void cachePaletteName(uint8_t id, const char* name);
extern void cacheEffectName(uint16_t id, const char* name);
extern bool isEffectNameHoldoffActive();

// ============================================================================
// Router logging (compile-time, default off)
// ============================================================================
#ifndef TAB5_WS_TRACE
#define TAB5_WS_TRACE 0
#endif

#if TAB5_WS_TRACE
#define TAB5_WS_PRINTF(...) Serial.printf(__VA_ARGS__)
#define TAB5_WS_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define TAB5_WS_PRINTF(...) ((void)0)
#define TAB5_WS_PRINTLN(...) ((void)0)
#endif

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
     * @param zoneComposerUI Zone composer UI for zone state updates (optional)
     * @param displayUI Display UI for color correction updates (optional)
     */
    static void init(ParameterHandler* paramHandler, WebSocketClient* wsClient = nullptr, ZoneComposerUI* zoneComposerUI = nullptr, DisplayUI* displayUI = nullptr) {
        s_paramHandler = paramHandler;
        s_wsClient = wsClient;
        s_zoneComposerUI = zoneComposerUI;
        s_displayUI = displayUI;
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

        if (strcmp(type, "zones.changed") == 0 || strcmp(type, "zones.stateChanged") == 0 || strcmp(type, "zones.enabledChanged") == 0) {
            handleZonesChanged(doc);
            return true;
        }

        if (strcmp(type, "zones.list") == 0) {
            handleZonesList(doc);
            return true;
        }

        if (strcmp(type, "effects.changed") == 0 || strcmp(type, "effectChanged") == 0) {
            handleEffectsChanged(doc);
            return true;
        }

        if (strcmp(type, "effects.current") == 0) {
            handleEffectsCurrent(doc);
            return true;
        }

        if (strcmp(type, "colorCorrection.getConfig") == 0) {
            handleColorCorrectionConfig(doc);
            return true;
        }

        if (strcmp(type, "effects.parameters") == 0) {
            handleEffectParameters(doc);
            return true;
        }

        if (strcmp(type, "cameraMode.changed") == 0) {
            handleCameraModeChanged(doc);
            return true;
        }

        if (strcmp(type, "effectPresets.list") == 0) {
            handleEffectPresetsList(doc);
            return true;
        }

        // Unknown message type - ignore silently
        // (Rate-limited logging could be added here for debugging)
        return false;
    }

private:
    static inline ParameterHandler* s_paramHandler = nullptr;
    static inline WebSocketClient* s_wsClient = nullptr;
    static inline ZoneComposerUI* s_zoneComposerUI = nullptr;
    static inline DisplayUI* s_displayUI = nullptr;
    static inline uint16_t s_lastKnownEffectId = 0xFFFF;  // Track effect changes for auto-request

    /**
     * @brief Handle "status" message from LightwaveOS
     *
     * Applies all parameter values from status message.
     * Echo prevention: uses setValue(..., false) to avoid triggering callbacks.
     * Also extracts uptime if present and updates DisplayUI footer.
     */
    static void handleStatus(JsonDocument& doc) {
        if (s_paramHandler) {
            bool updated = s_paramHandler->applyStatus(doc);
            if (updated) {
                TAB5_WS_PRINTLN("[WsRouter] Status applied");
            }
        }

        // Forward global params to Control Surface (read-only global row)
        if (s_displayUI && s_displayUI->getCurrentScreen() == UIScreen::CONTROL_SURFACE) {
            ControlSurfaceUI* csUI = s_displayUI->getControlSurfaceUI();
            if (csUI) {
                // Global row: BRI(0), SPD(1), MOOD(2), FADE(3), CPLX(4), VAR(5), HUE(6), SAT(7)
                static const char* kFields[] = {"brightness", "speed", "mood", "fadeAmount",
                                                 "complexity", "variation", "hue", "saturation"};
                for (uint8_t i = 0; i < 8; i++) {
                    if (doc[kFields[i]].is<int>()) {
                        csUI->updateGlobalParam(i, static_cast<uint8_t>(doc[kFields[i]].as<int>()));
                    }
                }
            }
        }

        // Extract uptime from status message if present and update footer
        if (doc["uptime"].is<unsigned long>() || doc["uptime"].is<uint32_t>()) {
            uint32_t uptimeSeconds = 0;
            if (doc["uptime"].is<unsigned long>()) {
                uptimeSeconds = doc["uptime"].as<unsigned long>();
            } else if (doc["uptime"].is<uint32_t>()) {
                uptimeSeconds = doc["uptime"].as<uint32_t>();
            }
            
            if (uptimeSeconds > 0) {
                TAB5_WS_PRINTF("[WsRouter] Status message uptime: %lu sec\n", uptimeSeconds);
                
                // Update footer if DisplayUI is available
                if (s_displayUI) {
                    s_displayUI->updateHostUptime(uptimeSeconds);
                }
            }
        }

        // Extract and cache paletteName from status message if present
        // This ensures palette names display immediately without waiting for palettes.list
        if (doc["paletteName"].is<const char*>() && doc["paletteId"].is<uint8_t>()) {
            uint8_t id = doc["paletteId"].as<uint8_t>();
            const char* name = doc["paletteName"].as<const char*>();
            if (name && name[0]) {
                cachePaletteName(id, name);
            }
        }

        // Extract and cache effectName from status message if present.
        // Skip during holdoff (after encoder turn) — status reads stale K1 cache
        // and would overwrite the fresh name from effects.getCurrent.
        if (doc["effectName"].is<const char*>() && !isEffectNameHoldoffActive()) {
            uint16_t effectId = 0;
            bool hasEffectId = false;
            if (doc["effectId"].is<int>()) {
                int effectIdInt = doc["effectId"].as<int>();
                if (effectIdInt >= 0 && effectIdInt <= 0xFFFF) {
                    effectId = static_cast<uint16_t>(effectIdInt);
                    hasEffectId = true;
                }
            } else if (doc["effectId"].is<uint16_t>()) {
                effectId = doc["effectId"].as<uint16_t>();
                hasEffectId = true;
            }

            if (hasEffectId) {
                const char* effectName = doc["effectName"].as<const char*>();
                if (effectName && effectName[0]) {
                    cacheEffectName(effectId, effectName);
                }

                // Auto-request effect params when effect changes and Control Surface is active
                if (effectId != s_lastKnownEffectId) {
                    s_lastKnownEffectId = effectId;
                    if (s_displayUI && s_wsClient &&
                        s_displayUI->getCurrentScreen() == UIScreen::CONTROL_SURFACE) {
                        s_wsClient->setPendingEffectParamsRefresh(effectId);
                        TAB5_WS_PRINTF("[WsRouter] Effect changed to %u, queued params refresh\n", effectId);
                    }
                }
            }
        }
    }

    /**
     * @brief Handle "device.status" message
     *
     * Device information (uptime, firmware version, etc.)
     * Extracts uptime and updates DisplayUI footer.
     */
    static void handleDeviceStatus(JsonDocument& doc) {
        // Extract uptime and update footer
        if (doc["uptime"].is<unsigned long>()) {
            uint32_t uptimeSeconds = doc["uptime"].as<unsigned long>();
            TAB5_WS_PRINTF("[WsRouter] Device uptime: %lu sec\n", uptimeSeconds);
            
            // Update footer if DisplayUI is available
            if (s_displayUI) {
                s_displayUI->updateHostUptime(uptimeSeconds);
            }
        }
    }

    /**
     * @brief Handle "parameters.changed" message
     *
     * Notification that parameters have changed on the server.
     * Could trigger a status refresh request.
     */
    static void handleParametersChanged(JsonDocument& doc) {
        // Optional: Request fresh status from server
        // For now, rely on periodic status broadcasts from LightwaveOS
        TAB5_WS_PRINTLN("[WsRouter] Parameters changed notification");
        (void)doc;
    }

    /**
     * @brief Handle "zone.status" message (NEW for Tab5)
     *
     * Zone-specific parameter sync for multi-zone LED configurations.
     * Supports up to 4 zones, each with independent effect/brightness.
     */
    static void handleZoneStatus(JsonDocument& doc) {
        if (!s_paramHandler) {
            return;
        }

        // Hard gate: Unit-B zone cache updates are only valid while the
        // Zone Composer tab is active. Global screen repurposes Unit-B for
        // effect-modifier slots and must not receive zone writes.
        if (!s_displayUI || s_displayUI->getCurrentScreen() != UIScreen::ZONE_COMPOSER) {
            return;
        }

        // Check if zones array exists (ArduinoJson 7 pattern)
        if (!doc["zones"].is<JsonArray>()) {
            TAB5_WS_PRINTLN("[WsRouter] zone.status: missing zones array");
            return;
        }

        JsonArray zones = doc["zones"].as<JsonArray>();
        int zoneCount = 0;

        for (JsonObject zone : zones) {
            if (!zone["id"].is<uint8_t>()) continue;

            uint8_t zoneId = zone["id"].as<uint8_t>();

            // Map zone parameters to Unit B encoders (indices 8-15)
            // Zone 0 -> Zone0Effect (8), Zone0Speed (9)
            // Zone 1 -> Zone1Effect (10), Zone1Speed (11)
            // etc.
            if (zoneId > 3) {
                TAB5_WS_PRINTF("[WsRouter] Zone %d out of range (max 3)\n", zoneId);
                continue;
            }

            // Map zone ID to the correct ParameterId enum values
            ParameterId effectParam, speedParam;
            switch (zoneId) {
                case 0:
                    effectParam = ParameterId::Zone0Effect;
                    speedParam = ParameterId::Zone0Speed;
                    break;
                case 1:
                    effectParam = ParameterId::Zone1Effect;
                    speedParam = ParameterId::Zone1Speed;
                    break;
                case 2:
                    effectParam = ParameterId::Zone2Effect;
                    speedParam = ParameterId::Zone2Speed;
                    break;
                case 3:
                    effectParam = ParameterId::Zone3Effect;
                    speedParam = ParameterId::Zone3Speed;
                    break;
                default:
                    continue;  // Should not reach here
            }

            // Zone effectId (16-bit — K1 firmware uses hex IDs such as 0x1100)
            if (zone["effectId"].is<int>()) {
                uint16_t effectId = static_cast<uint16_t>(zone["effectId"].as<int>());
                s_paramHandler->setValue(effectParam, static_cast<uint8_t>(effectId & 0xFF));
            }

            // Zone speed
            if (zone["speed"].is<uint8_t>()) {
                uint8_t speed = zone["speed"].as<uint8_t>();
                s_paramHandler->setValue(speedParam, speed);
            }

            // Zone palette (optional, when in palette mode)
            if (zone["paletteId"].is<uint8_t>()) {
                // Note: palette is controlled via the same encoder as speed (toggled)
                // We'll store it but won't update the encoder value directly
                // The UI can display it separately
            }

            zoneCount++;
        }

        if (zoneCount > 0) {
            TAB5_WS_PRINTF("[WsRouter] Zone status: %d zones synced\n", zoneCount);
        }
    }

    /**
     * @brief Handle "effects.changed" message
     *
     * Notification that effects list has changed on the server.
     * Could trigger effect metadata refresh.
     */
    static void handleEffectsChanged(JsonDocument& doc) {
        // Extract effectId and effectName from effects.changed notification
        extractAndCacheEffectName(doc);

        // Auto-request effect params when effect changes and Control Surface is active
        if (s_displayUI && s_wsClient &&
            s_displayUI->getCurrentScreen() == UIScreen::CONTROL_SURFACE) {
            // Extract effectId from data or root
            uint16_t effectId = 0;
            bool hasId = false;
            JsonObject data = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : JsonObject();
            if (data && data["effectId"].is<int>()) {
                effectId = static_cast<uint16_t>(data["effectId"].as<int>());
                hasId = true;
            } else if (data && data["id"].is<int>()) {
                effectId = static_cast<uint16_t>(data["id"].as<int>());
                hasId = true;
            } else if (doc["effectId"].is<int>()) {
                effectId = static_cast<uint16_t>(doc["effectId"].as<int>());
                hasId = true;
            }
            if (hasId && effectId != s_lastKnownEffectId) {
                s_lastKnownEffectId = effectId;
                s_wsClient->setPendingEffectParamsRefresh(effectId);
                TAB5_WS_PRINTF("[WsRouter] effects.changed: queued params refresh for %u\n", effectId);
            }
        }
    }

    /**
     * @brief Handle "effects.current" response
     *
     * Response to effects.getCurrent request. Contains live renderer state
     * (not cached), so effectId/effectName are always up-to-date.
     */
    static void handleEffectsCurrent(JsonDocument& doc) {
        extractAndCacheEffectName(doc);
    }

    /**
     * @brief Extract effectId + effectName from any message that carries them
     * and update the display cache.
     */
    static void extractAndCacheEffectName(JsonDocument& doc) {
        const char* effectName = nullptr;
        uint16_t effectId = 0;
        bool hasEffectId = false;

        // Try "data" sub-object first (effects.current / effects.changed format)
        JsonObject data;
        if (doc["data"].is<JsonObject>()) {
            data = doc["data"].as<JsonObject>();
        }

        // Search for effectName (check data first, then root)
        if (data && data["effectName"].is<const char*>()) {
            effectName = data["effectName"].as<const char*>();
        } else if (data && data["name"].is<const char*>()) {
            effectName = data["name"].as<const char*>();
        } else if (doc["effectName"].is<const char*>()) {
            effectName = doc["effectName"].as<const char*>();
        }

        // Search for effectId
        if (data && data["effectId"].is<int>()) {
            int v = data["effectId"].as<int>();
            if (v >= 0 && v <= 0xFFFF) { effectId = v; hasEffectId = true; }
        } else if (data && data["id"].is<int>()) {
            int v = data["id"].as<int>();
            if (v >= 0 && v <= 0xFFFF) { effectId = v; hasEffectId = true; }
        } else if (doc["effectId"].is<int>()) {
            int v = doc["effectId"].as<int>();
            if (v >= 0 && v <= 0xFFFF) { effectId = v; hasEffectId = true; }
        }

        if (hasEffectId && effectName && effectName[0]) {
            cacheEffectName(effectId, effectName);
        }
    }

    /**
     * @brief Handle "zones.changed" message
     *
     * Notification that zone configuration has changed.
     * Defer zone refresh to next update() to avoid sending inside WS receive callback
     * (prevents deadlock / long block inside _ws.loop()).
     */
    static void handleZonesChanged(JsonDocument& doc) {
        TAB5_WS_PRINTLN("[WsRouter] Zones changed notification");
        if (s_wsClient) {
            s_wsClient->setPendingZonesRefresh();
        }
        (void)doc;
    }

    /**
     * @brief Handle "zones.list" message
     *
     * Zone state list from server (response to zones.get).
     * Updates zone composer UI with current zone states, segments, and blendMode.
     */
    static void handleZonesList(JsonDocument& doc) {
        if (!s_zoneComposerUI) {
            // Fallback to handleZoneStatus if no UI available
            if (doc["zones"].is<JsonArray>()) {
                handleZoneStatus(doc);
            }
            TAB5_WS_PRINTLN("[WsRouter] Zones list received (no UI)");
            return;
        }

        // Parse enabled and zoneCount
        bool enabled = doc["enabled"].as<bool>();
        uint8_t zoneCount = doc["zoneCount"].as<uint8_t>();
        if (zoneCount > zones::MAX_ZONES) {
            zoneCount = zones::MAX_ZONES;
        }

        // Parse segments array if present
        if (doc["segments"].is<JsonArray>()) {
            JsonArray segmentsArray = doc["segments"].as<JsonArray>();
            zones::ZoneSegment segments[zones::MAX_ZONES];
            uint8_t segCount = 0;

            for (JsonObject seg : segmentsArray) {
                if (segCount >= zones::MAX_ZONES) break;

                segments[segCount].zoneId = seg["zoneId"].as<uint8_t>();
                segments[segCount].s1LeftStart = seg["s1LeftStart"].as<uint8_t>();
                segments[segCount].s1LeftEnd = seg["s1LeftEnd"].as<uint8_t>();
                segments[segCount].s1RightStart = seg["s1RightStart"].as<uint8_t>();
                segments[segCount].s1RightEnd = seg["s1RightEnd"].as<uint8_t>();
                segments[segCount].totalLeds = seg["totalLeds"].as<uint8_t>();
                segCount++;
            }

            // Update UI with segments
            s_zoneComposerUI->updateSegments(segments, segCount);
        }

        // Parse zones array for runtime state (effect, palette, blend, speed)
        if (doc["zones"].is<JsonArray>()) {
            JsonArray zones = doc["zones"].as<JsonArray>();

            for (JsonObject zone : zones) {
                if (!zone["id"].is<uint8_t>()) continue;

                uint8_t zoneId = zone["id"].as<uint8_t>();
                if (zoneId >= zones::MAX_ZONES) continue;

                // Update ZoneState for UI
                ZoneState state;
                state.effectId = zone["effectId"].as<uint8_t>();
                if (zone["effectName"].is<const char*>()) {
                    const char* name = zone["effectName"].as<const char*>();
                    strncpy(state.effectName, name, sizeof(state.effectName) - 1);
                    state.effectName[sizeof(state.effectName) - 1] = '\0';
                }
                state.speed = zone["speed"].as<uint8_t>();
                state.paletteId = zone["paletteId"].as<uint8_t>();
                if (zone["paletteName"].is<const char*>()) {
                    const char* name = zone["paletteName"].as<const char*>();
                    strncpy(state.paletteName, name, sizeof(state.paletteName) - 1);
                    state.paletteName[sizeof(state.paletteName) - 1] = '\0';
                }
                state.blendMode = zone["blendMode"].as<uint8_t>();
                if (zone["blendModeName"].is<const char*>()) {
                    const char* name = zone["blendModeName"].as<const char*>();
                    strncpy(state.blendModeName, name, sizeof(state.blendModeName) - 1);
                    state.blendModeName[sizeof(state.blendModeName) - 1] = '\0';
                }
                state.enabled = zone["enabled"].as<bool>();

                // Calculate LED range from segments (if available)
                // For now, use placeholder - will be updated from segments
                state.ledStart = 0;
                state.ledEnd = 0;

                s_zoneComposerUI->updateZone(zoneId, state);
            }

            // Also update parameter handler for encoder sync
            handleZoneStatus(doc);
        }

        TAB5_WS_PRINTF("[WsRouter] Zones list: enabled=%d, count=%d\n", enabled, zoneCount);
    }

    /**
     * @brief Handle "effects.parameters" message for Control Surface.
     */
    static void handleEffectParameters(JsonDocument& doc) {
        if (!s_displayUI) return;
        ControlSurfaceUI* csUI = s_displayUI->getControlSurfaceUI();
        if (!csUI) return;

        if (doc["success"].is<bool>() && !doc["success"].as<bool>()) {
            return;
        }

        JsonObject data = doc["data"].is<JsonObject>()
            ? doc["data"].as<JsonObject>()
            : doc.as<JsonObject>();

        uint16_t effectId = 0;
        if (data["effectId"].is<int>()) {
            int v = data["effectId"].as<int>();
            if (v >= 0 && v <= 0xFFFF) effectId = static_cast<uint16_t>(v);
        } else if (data["effectId"].is<uint16_t>()) {
            effectId = data["effectId"].as<uint16_t>();
        }

        const char* effectName = data["name"].is<const char*>()
            ? data["name"].as<const char*>()
            : "";

        CSEffectParam params[CS_MAX_EFFECT_PARAMS];
        uint8_t paramCount = 0;

        if (data["parameters"].is<JsonArray>()) {
            JsonArray paramArray = data["parameters"].as<JsonArray>();
            for (JsonObject p : paramArray) {
                if (paramCount >= CS_MAX_EFFECT_PARAMS) break;
                if (!p["name"].is<const char*>()) continue;

                CSEffectParam& out = params[paramCount];
                const char* name = p["name"].as<const char*>();
                const char* displayName = p["displayName"].is<const char*>()
                    ? p["displayName"].as<const char*>()
                    : name;

                strncpy(out.name, name, sizeof(out.name) - 1);
                out.name[sizeof(out.name) - 1] = '\0';
                strncpy(out.displayName, displayName, sizeof(out.displayName) - 1);
                out.displayName[sizeof(out.displayName) - 1] = '\0';

                out.minValue = p["min"] | 0.0f;
                out.maxValue = p["max"] | 1.0f;
                if (out.maxValue <= out.minValue) {
                    out.maxValue = out.minValue + 1.0f;
                }

                out.defaultValue = p["default"] | out.minValue;
                out.currentValue = p["value"] | out.defaultValue;

                out.step = p["step"] | ((out.maxValue - out.minValue) / 255.0f);
                if (out.step <= 0.0f) {
                    out.step = (out.maxValue - out.minValue) / 255.0f;
                    if (out.step <= 0.0f) out.step = 0.01f;
                }

                if (p["unit"].is<const char*>()) {
                    const char* unit = p["unit"].as<const char*>();
                    if (unit) {
                        strncpy(out.unit, unit, sizeof(out.unit) - 1);
                        out.unit[sizeof(out.unit) - 1] = '\0';
                    }
                }

                // Infer type when firmware does not provide explicit type metadata.
                const bool minInt = fabsf(out.minValue - roundf(out.minValue)) < 0.001f;
                const bool maxInt = fabsf(out.maxValue - roundf(out.maxValue)) < 0.001f;
                const bool defInt = fabsf(out.defaultValue - roundf(out.defaultValue)) < 0.001f;
                if (minInt && maxInt && defInt &&
                    out.minValue == 0.0f && out.maxValue == 1.0f) {
                    out.type = CSParamType::BOOL;
                } else if (minInt && maxInt) {
                    out.type = CSParamType::INT;
                } else {
                    out.type = CSParamType::FLOAT;
                }

                out.valid = true;
                paramCount++;
            }
        }

        csUI->onEffectParametersReceived(effectId, effectName, params, paramCount);
    }

    /**
     * @brief Handle "cameraMode.changed" broadcast for Control Surface.
     */
    static void handleCameraModeChanged(JsonDocument& doc) {
        if (!s_displayUI) return;
        ControlSurfaceUI* csUI = s_displayUI->getControlSurfaceUI();
        if (!csUI) return;

        JsonObject data = doc["data"].is<JsonObject>()
            ? doc["data"].as<JsonObject>()
            : doc.as<JsonObject>();

        bool enabled = false;
        if (data["enabled"].is<bool>()) {
            enabled = data["enabled"].as<bool>();
        } else if (data["active"].is<bool>()) {
            enabled = data["active"].as<bool>();
        }

        csUI->setCameraMode(enabled);
    }

    /**
     * @brief Handle "effectPresets.list" response for Control Surface.
     */
    static void handleEffectPresetsList(JsonDocument& doc) {
        if (!s_displayUI) return;
        ControlSurfaceUI* csUI = s_displayUI->getControlSurfaceUI();
        if (!csUI) return;

        if (doc["success"].is<bool>() && !doc["success"].as<bool>()) {
            return;
        }

        CSPresetSlot slots[CS_MAX_PRESET_SLOTS];
        for (uint8_t i = 0; i < CS_MAX_PRESET_SLOTS; i++) {
            slots[i] = CSPresetSlot();
        }

        JsonObject data = doc["data"].is<JsonObject>()
            ? doc["data"].as<JsonObject>()
            : doc.as<JsonObject>();

        if (data["presets"].is<JsonArray>()) {
            JsonArray presets = data["presets"].as<JsonArray>();
            for (JsonObject p : presets) {
                if (!p["id"].is<int>()) continue;
                int slot = p["id"].as<int>();
                if (slot < 0 || slot >= CS_MAX_PRESET_SLOTS) continue;

                CSPresetSlot& out = slots[slot];
                out.occupied = p["occupied"] | true;
                if (p["name"].is<const char*>()) {
                    const char* name = p["name"].as<const char*>();
                    if (name) {
                        strncpy(out.name, name, sizeof(out.name) - 1);
                        out.name[sizeof(out.name) - 1] = '\0';
                    }
                }
                if (p["effectId"].is<int>()) {
                    int v = p["effectId"].as<int>();
                    if (v >= 0 && v <= 0xFFFF) {
                        out.effectId = static_cast<uint16_t>(v);
                    }
                }
            }
        }

        csUI->onPresetListReceived(slots, CS_MAX_PRESET_SLOTS);
    }

    /**
     * @brief Handle "colorCorrection.getConfig" response
     *
     * Caches color correction state from LightwaveOS v2 server.
     * Used by PresetManager to capture/apply gamma, auto-exposure, brown guardrail.
     */
    static void handleColorCorrectionConfig(JsonDocument& doc) {
        if (!s_wsClient) {
            TAB5_WS_PRINTLN("[WsRouter] ColorCorrection: no wsClient");
            return;
        }

        // Check for success field (v2 API pattern)
        if (doc["success"].is<bool>() && !doc["success"].as<bool>()) {
            TAB5_WS_PRINTLN("[WsRouter] ColorCorrection: request failed");
            return;
        }

        // Data can be in "data" object or at root level
        JsonObject data = doc["data"].is<JsonObject>()
            ? doc["data"].as<JsonObject>()
            : doc.as<JsonObject>();

        ColorCorrectionState state;
        state.gammaEnabled = data["gammaEnabled"] | true;
        state.gammaValue = data["gammaValue"] | 2.2f;
        state.autoExposureEnabled = data["autoExposureEnabled"] | false;
        state.autoExposureTarget = data["autoExposureTarget"] | 110;
        state.brownGuardrailEnabled = data["brownGuardrailEnabled"] | false;
        state.maxGreenPercentOfRed = data["maxGreenPercentOfRed"] | 28;
        state.maxBluePercentOfRed = data["maxBluePercentOfRed"] | 8;
        state.mode = data["mode"] | 2;
        state.valid = true;

        s_wsClient->setColorCorrectionState(state);
        if (s_displayUI) {
            s_displayUI->setColourCorrectionState(state);
        }

        TAB5_WS_PRINTF("[WsRouter] ColorCorrection synced: gamma=%s (%.1f), ae=%s, brown=%s\n",
                      state.gammaEnabled ? "ON" : "OFF", state.gammaValue,
                      state.autoExposureEnabled ? "ON" : "OFF",
                      state.brownGuardrailEnabled ? "ON" : "OFF");
    }

};
