/**
 * ParameterMapper — Maps physical inputs to K1 WebSocket commands.
 *
 * Switch-based dispatch on inputId. Tracks accumulated values, handles
 * button toggles/holds, and sends appropriate WS commands.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../network/WebSocketClient.h"
#include "../sync/EchoGuard.h"
#include "../led/LedFeedback.h"

class ParameterMapper {
public:
    void begin(ZMWebSocketClient* ws, EchoGuard* echo, LedFeedback* leds) {
        _ws = ws;
        _echo = echo;
        _leds = leds;
    }

    /// Called from InputManager callback on encoder/scroll change.
    void onInput(uint8_t inputId, int32_t delta, bool button) {
        if (button) {
            handleButton(inputId);
        } else {
            handleDelta(inputId, delta);
        }
    }

    /// Apply incoming K1 status broadcast (with echo guard).
    void applyStatus(JsonDocument& doc, uint32_t now) {
        bool force = !_echo->initialSyncDone;

        // Global parameters from "status" message
        if (!doc["brightness"].isNull()) {
            if (force || !_echo->isHeld(19, now)) {
                masterBrightness = doc["brightness"];
                if (_leds) _leds->setParamValue(19, masterBrightness);
            }
        }
        if (!doc["speed"].isNull()) {
            if (force || !_echo->isHeld(3, now)) {
                masterSpeed = doc["speed"];
                if (_leds) _leds->setParamValue(3, masterSpeed);
            }
        }

        // EdgeMixer fields from broadcastStatus
        if (!doc["edgeMixerMode"].isNull()) {
            if (force || !_echo->isHeld(8, now)) {
                emMode = doc["edgeMixerMode"];
                if (_leds) _leds->setEdgeMixerMode(emMode);
            }
        }
        if (!doc["edgeMixerSpread"].isNull()) {
            if (force || !_echo->isHeld(9, now)) {
                emSpread = doc["edgeMixerSpread"];
                if (_leds) _leds->setParamValue(9, emSpread);
            }
        }
        if (!doc["edgeMixerStrength"].isNull()) {
            if (force || !_echo->isHeld(10, now)) {
                emStrength = doc["edgeMixerStrength"];
                if (_leds) _leds->setParamValue(10, emStrength);
            }
        }

        if (force) _echo->initialSyncDone = true;
    }

    /// Apply incoming zone state from "zones.list" message.
    void applyZoneState(JsonDocument& doc, uint32_t now) {
        bool force = !_echo->initialSyncDone;

        JsonArray zones = doc["zones"];
        if (zones.isNull()) return;

        for (JsonObject zone : zones) {
            uint8_t zid = zone["zoneId"] | 0xFF;
            if (zid > 2) continue;

            if (!zone["brightness"].isNull()) {
                uint8_t id = 16 + zid;
                if (force || !_echo->isHeld(id, now)) {
                    zoneBrightness[zid] = zone["brightness"];
                    if (_leds) _leds->setParamValue(id, zoneBrightness[zid]);
                }
            }
            if (!zone["speed"].isNull()) {
                uint8_t id = 4 + zid;
                if (force || !_echo->isHeld(id, now)) {
                    zoneSpeed[zid] = zone["speed"];
                }
            }
            if (!zone["enabled"].isNull()) {
                zoneEnabled[zid] = zone["enabled"];
                if (_leds) _leds->setZoneEnabled(zid, zoneEnabled[zid]);
            }
        }

        if (!doc["enabled"].isNull()) {
            zoneSystemOn = doc["enabled"];
            if (_leds) _leds->setZoneSystemOn(zoneSystemOn);
        }
        if (!doc["zoneCount"].isNull()) {
            zoneCount = doc["zoneCount"];
        }
    }

    // --- State (public for display access) ---
    uint8_t zoneBrightness[3] = {128, 128, 128};
    uint8_t masterBrightness = 128;
    uint8_t masterSpeed = 25;
    uint8_t zoneSpeed[3] = {25, 25, 25};
    uint8_t zonePalette[3] = {0, 0, 0};
    uint8_t emMode = 0, emSpread = 0, emStrength = 0;
    uint8_t stState = 0;  // spatial/temporal 0-3
    uint8_t zoneCount = 1;
    uint8_t transitionType = 0;
    uint8_t presetSlot = 0;
    bool zoneEnabled[3] = {true, true, true};
    bool cameraModeOn = false;
    bool zoneSystemOn = false;

    enum class SpeedPalMode : uint8_t { SPEED, PALETTE };
    SpeedPalMode speedPalMode[3] = {};

private:
    void handleDelta(uint8_t id, int32_t delta) {
        if (!_ws || !_ws->isConnected()) return;

        switch (id) {
            // --- Scroll: zone brightness ---
            case 16: case 17: case 18: {
                uint8_t zi = id - 16;
                zoneBrightness[zi] = clamp8(zoneBrightness[zi] + delta, 0, 255);
                sendZoneBrightness(zi, zoneBrightness[zi]);
                if (_leds) _leds->setParamValue(id, zoneBrightness[zi]);
                break;
            }
            // --- Scroll: master brightness ---
            case 19: {
                masterBrightness = clamp8(masterBrightness + delta, 0, 255);
                sendGlobalParam("brightness", masterBrightness);
                if (_leds) _leds->setParamValue(19, masterBrightness);
                break;
            }
            // --- EncA E0-E2: zone effect select ---
            case 0: case 1: case 2: {
                uint8_t zi = id;
                // Use nextEffect/prevEffect pattern per zone
                // K1 has no zone.nextEffect, so send absolute effectId
                // For now, just send the delta as a relative change
                if (delta > 0) sendZoneNextEffect(zi);
                else sendZonePrevEffect(zi);
                break;
            }
            // --- EncA E3: master speed ---
            case 3: {
                masterSpeed = clamp8(masterSpeed + delta, 1, 100);
                sendGlobalParam("speed", masterSpeed);
                if (_leds) _leds->setParamValue(3, masterSpeed);
                break;
            }
            // --- EncA E4-E6: zone speed OR palette ---
            case 4: case 5: case 6: {
                uint8_t zi = id - 4;
                if (speedPalMode[zi] == SpeedPalMode::PALETTE) {
                    zonePalette[zi] = clamp8(zonePalette[zi] + delta, 0, 74);
                    sendZonePalette(zi, zonePalette[zi]);
                } else {
                    zoneSpeed[zi] = clamp8(zoneSpeed[zi] + delta, 1, 50);
                    sendZoneSpeed(zi, zoneSpeed[zi]);
                }
                break;
            }
            // --- EncB E0: EdgeMixer mode ---
            case 8: {
                emMode = clamp8(emMode + delta, 0, 6);
                sendEdgeMixer("mode", emMode);
                if (_leds) _leds->setEdgeMixerMode(emMode);
                break;
            }
            // --- EncB E1: EdgeMixer spread ---
            case 9: {
                emSpread = clamp8(emSpread + delta, 0, 60);
                sendEdgeMixer("spread", emSpread);
                if (_leds) _leds->setParamValue(9, emSpread);
                break;
            }
            // --- EncB E2: EdgeMixer strength ---
            case 10: {
                emStrength = clamp8(emStrength + delta, 0, 255);
                sendEdgeMixer("strength", emStrength);
                if (_leds) _leds->setParamValue(10, emStrength);
                break;
            }
            // --- EncB E4: zone count ---
            case 12: {
                zoneCount = clamp8(zoneCount + delta, 1, 3);
                // TODO: send zones.setLayout with precomputed layout
                break;
            }
            // --- EncB E5: transition type ---
            case 13: {
                transitionType = clamp8(transitionType + delta, 0, 11);
                break;
            }
            // --- EncB E7: preset slot ---
            case 15: {
                presetSlot = clamp8(presetSlot + delta, 0, 7);
                break;
            }
            default:
                break;
        }

        _echo->markLocal(id);
    }

    void handleButton(uint8_t id) {
        if (!_ws) return;

        switch (id) {
            // --- Scroll buttons: zone enable toggle ---
            case 16: case 17: case 18: {
                uint8_t zi = id - 16;
                zoneEnabled[zi] = !zoneEnabled[zi];
                JsonDocument doc;
                doc["zoneId"] = zi;
                doc["enabled"] = zoneEnabled[zi];
                _ws->sendJSON("zone.enableZone", doc);
                if (_leds) {
                    _leds->setZoneEnabled(zi, zoneEnabled[zi]);
                    _leds->flashButton(id);
                }
                break;
            }
            // --- Master scroll button: global zone system toggle ---
            case 19: {
                zoneSystemOn = !zoneSystemOn;
                JsonDocument doc;
                doc["enable"] = zoneSystemOn;
                _ws->sendJSON("zone.enable", doc);
                if (_leds) {
                    _leds->setZoneSystemOn(zoneSystemOn);
                    _leds->flashButton(id);
                }
                break;
            }
            // --- EncA E4-E6: speed/palette toggle ---
            case 4: case 5: case 6: {
                uint8_t zi = id - 4;
                speedPalMode[zi] = (speedPalMode[zi] == SpeedPalMode::SPEED)
                    ? SpeedPalMode::PALETTE : SpeedPalMode::SPEED;
                if (_leds) {
                    _leds->setSpeedPalMode(zi, speedPalMode[zi] == SpeedPalMode::PALETTE);
                    _leds->flashButton(id);
                }
                break;
            }
            // --- EncB E3: spatial/temporal cycle ---
            case 11: {
                stState = (stState + 1) % 4;
                uint8_t spatial = (stState == 1 || stState == 3) ? 1 : 0;
                uint8_t temporal = (stState == 2 || stState == 3) ? 1 : 0;
                JsonDocument doc;
                doc["spatial"] = spatial;
                doc["temporal"] = temporal;
                _ws->sendJSON("edge_mixer.set", doc);
                if (_leds) {
                    _leds->setSpatialTemporal(stState);
                    _leds->flashButton(id);
                }
                break;
            }
            // --- EncB E6: camera mode toggle ---
            case 14: {
                cameraModeOn = !cameraModeOn;
                JsonDocument doc;
                doc["enabled"] = cameraModeOn;
                _ws->sendJSON("cameraMode.set", doc);
                if (_leds) {
                    _leds->setCameraMode(cameraModeOn);
                    _leds->flashButton(id);
                }
                break;
            }
            // --- EncB E7: preset load (tap) ---
            case 15: {
                JsonDocument doc;
                doc["id"] = presetSlot;
                _ws->sendJSON("zonePresets.load", doc);
                if (_leds) _leds->flashButton(id);
                break;
            }
            default:
                if (_leds) _leds->flashButton(id);
                break;
        }
    }

    // --- WS send helpers ---

    void sendZoneBrightness(uint8_t zi, uint8_t val) {
        JsonDocument doc;
        doc["zoneId"] = zi;
        doc["brightness"] = val;
        _ws->sendJSON("zone.setBrightness", doc);
    }

    void sendZoneSpeed(uint8_t zi, uint8_t val) {
        JsonDocument doc;
        doc["zoneId"] = zi;
        doc["speed"] = val;
        _ws->sendJSON("zone.setSpeed", doc);
    }

    void sendZonePalette(uint8_t zi, uint8_t val) {
        JsonDocument doc;
        doc["zoneId"] = zi;
        doc["paletteId"] = val;
        _ws->sendJSON("zone.setPalette", doc);
    }

    void sendZoneNextEffect(uint8_t zi) {
        // K1 doesn't have zone.nextEffect — use zone.setEffect with delta
        // For now, use global nextEffect (will be improved in Phase 4b)
        _ws->sendSimple("nextEffect");
    }

    void sendZonePrevEffect(uint8_t zi) {
        _ws->sendSimple("prevEffect");
    }

    void sendGlobalParam(const char* field, uint8_t val) {
        JsonDocument doc;
        doc[field] = val;
        _ws->sendJSON("parameters.set", doc);
    }

    void sendEdgeMixer(const char* field, uint8_t val) {
        JsonDocument doc;
        doc[field] = val;
        _ws->sendJSON("edge_mixer.set", doc);
    }

    static uint8_t clamp8(int32_t val, int32_t lo, int32_t hi) {
        if (val < lo) return (uint8_t)lo;
        if (val > hi) return (uint8_t)hi;
        return (uint8_t)val;
    }

    ZMWebSocketClient* _ws = nullptr;
    EchoGuard* _echo = nullptr;
    LedFeedback* _leds = nullptr;
};
