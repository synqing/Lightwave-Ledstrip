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

    /// Set the active display page so input routing can be page-aware.
    void setActivePage(uint8_t page) { _activePage = page; }

    /// Return the active display page index.
    uint8_t activePage() const { return _activePage; }

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
            if (!zone["effectId"].isNull()) {
                zoneEffectId[zid] = zone["effectId"];
                syncEffectIndex(zid);
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

    // --- Effect list cache (populated from K1 effects.list response) ---
    static constexpr uint8_t kMaxEffects = 128;
    uint16_t effectList[kMaxEffects] = {};
    uint8_t effectCount = 0;

    /// Called when K1 responds to effects.list — caches the ordered effectId array.
    void setEffectList(const uint16_t* list, uint8_t count) {
        effectCount = (count > kMaxEffects) ? kMaxEffects : count;
        memcpy(effectList, list, effectCount * sizeof(uint16_t));
        // Sync per-zone indices to match current effectIds
        for (uint8_t zi = 0; zi < 3; zi++) {
            syncEffectIndex(zi);
        }
    }

    // --- State (public for display access) ---
    uint8_t zoneBrightness[3] = {128, 128, 128};
    uint8_t masterBrightness = 128;
    uint8_t masterSpeed = 25;
    uint8_t zoneSpeed[3] = {25, 25, 25};
    uint8_t zonePalette[3] = {0, 0, 0};
    uint16_t zoneEffectId[3] = {0, 0, 0};
    uint8_t zoneEffectIdx[3] = {0, 0, 0};
    uint8_t emMode = 0, emSpread = 0, emStrength = 0;
    uint8_t stState = 0;  // spatial/temporal 0-3
    uint8_t zoneCount = 1;
    uint8_t transitionType = 0;
    uint8_t presetSlot = 0;
    bool zoneEnabled[3] = {true, true, true};
    bool cameraModeOn = false;
    bool zoneSystemOn = false;

    /// Toggle zone system on/off — called from AtomS3 button (GPIO41)
    /// or EncB E4 button press (all pages).
    /// Full sequence: layout → enable → loadPreset (same as serial console
    /// 'z' + '1' pattern). Without loadPreset, zones have no effects assigned
    /// and the K1 renders black.
    void toggleZoneSystem() {
        if (!_ws || !_ws->isConnected()) return;
        zoneSystemOn = !zoneSystemOn;

        if (zoneSystemOn) {
            if (zoneCount == 0) zoneCount = 1;

            // 1. Layout first — segment definitions
            sendZonesLayout(zoneCount);

            // 2. Enable the zone system
            JsonDocument enableDoc;
            enableDoc["enable"] = true;
            _ws->sendJSON("zone.enable", enableDoc);

            // 3. Load a preset to assign effects to zones.
            //    Map zone count to preset: 1→0(Unified), 2→1(Dual Split), 3→2(Triple Rings)
            uint8_t presetId = (zoneCount >= 3) ? 2 : (zoneCount >= 2) ? 1 : 0;
            JsonDocument presetDoc;
            presetDoc["presetId"] = presetId;
            _ws->sendJSON("zone.loadPreset", presetDoc);

            Serial.printf("[ZoneMode] ENABLED (preset %d, %d zones)\n", presetId, zoneCount);
        } else {
            JsonDocument doc;
            doc["enable"] = false;
            _ws->sendJSON("zone.enable", doc);
            Serial.println("[ZoneMode] DISABLED");
        }

        if (_leds) _leds->setZoneSystemOn(zoneSystemOn);
    }

    enum class SpeedPalMode : uint8_t { SPEED, PALETTE };
    SpeedPalMode speedPalMode[3] = {};

    enum class ScrollMode : uint8_t { EFFECT, BRIGHTNESS };
    ScrollMode scrollMode[3] = {};  // Default: EFFECT

private:
    void handleDelta(uint8_t id, int32_t delta) {
        if (!_ws || !_ws->isConnected()) return;

        switch (id) {
            // --- Scroll Z1-Z3: effect (default) or brightness (button toggle) ---
            case 16: case 17: case 18: {
                uint8_t zi = id - 16;
                if (scrollMode[zi] == ScrollMode::EFFECT) {
                    if (effectCount == 0) break;
                    int16_t idx = (int16_t)zoneEffectIdx[zi] + ((delta > 0) ? 1 : -1);
                    if (idx < 0) idx = effectCount - 1;
                    if (idx >= effectCount) idx = 0;
                    zoneEffectIdx[zi] = (uint8_t)idx;
                    zoneEffectId[zi] = effectList[idx];
                    sendZoneEffect(zi, zoneEffectId[zi]);
                } else {
                    zoneBrightness[zi] = clamp8(zoneBrightness[zi] + delta, 0, 255);
                    sendZoneBrightness(zi, zoneBrightness[zi]);
                    if (_leds) _leds->setParamValue(id, zoneBrightness[zi]);
                }
                break;
            }
            // --- Scroll: master brightness ---
            case 19: {
                masterBrightness = clamp8(masterBrightness + delta, 0, 255);
                sendGlobalParam("brightness", masterBrightness);
                if (_leds) _leds->setParamValue(19, masterBrightness);
                break;
            }
            // --- EncA E0-E2: per-zone effect select ---
            case 0: case 1: case 2: {
                uint8_t zi = id;
                if (effectCount == 0) break;  // No effect list yet — ignore
                int16_t idx = (int16_t)zoneEffectIdx[zi] + ((delta > 0) ? 1 : -1);
                // Wrap around at boundaries
                if (idx < 0) idx = effectCount - 1;
                if (idx >= effectCount) idx = 0;
                zoneEffectIdx[zi] = (uint8_t)idx;
                zoneEffectId[zi] = effectList[idx];
                sendZoneEffect(zi, zoneEffectId[zi]);
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
            // --- EncB E0-E7: page-dependent routing ---
            case 8: case 9: case 10: case 11:
            case 12: case 13: case 14: case 15:
                handleEncBDelta(id, delta);
                break;
            default:
                break;
        }

        _echo->markLocal(id);
    }

    void handleButton(uint8_t id) {
        if (!_ws) return;

        switch (id) {
            // --- Scroll buttons: toggle effect/brightness mode ---
            case 16: case 17: case 18: {
                uint8_t zi = id - 16;
                scrollMode[zi] = (scrollMode[zi] == ScrollMode::EFFECT)
                    ? ScrollMode::BRIGHTNESS : ScrollMode::EFFECT;
                Serial.printf("[Scroll] Z%d mode: %s\n", zi + 1,
                              scrollMode[zi] == ScrollMode::EFFECT ? "EFFECT" : "BRIGHTNESS");
                if (_leds) _leds->flashButton(id);
                break;
            }
            // --- Master scroll button: reserved (zone toggle moved to AtomS3 btn) ---
            case 19:
                break;
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
            // --- EncB E0-E7: page-dependent routing ---
            case 8: case 9: case 10: case 11:
            case 12: case 13: case 14: case 15:
                handleEncBButton(id);
                break;
            default:
                if (_leds) _leds->flashButton(id);
                break;
        }
    }

    // -----------------------------------------------------------------
    // EncB page-aware delta dispatch
    // -----------------------------------------------------------------

    /// Route EncB delta (IDs 8-15) based on active display page.
    void handleEncBDelta(uint8_t id, int32_t delta) {
        uint8_t encIdx = id - 8;  // 0-7
        switch (_activePage) {
            case 0: return;  // Connection page — EncB inactive
            case 1: handleEncBZonesPageDelta(encIdx, delta); break;
            case 2: handleEncBEffectPageDelta(encIdx, delta); break;
            case 3: handleEncBMixerPageDelta(encIdx, delta); break;
        }
    }

    /// Page 1 (ZONES) — EncB delta routing.
    void handleEncBZonesPageDelta(uint8_t encIdx, int32_t delta) {
        switch (encIdx) {
            case 0: case 1: case 2: {  // E0-E2: per-zone brightness
                zoneBrightness[encIdx] = clamp8(zoneBrightness[encIdx] + delta, 0, 255);
                sendZoneBrightness(encIdx, zoneBrightness[encIdx]);
                if (_leds) _leds->setParamValue(encIdx + 8, zoneBrightness[encIdx]);
                break;
            }
            case 3: {  // E3: master brightness
                masterBrightness = clamp8(masterBrightness + delta, 0, 255);
                sendGlobalParam("brightness", masterBrightness);
                if (_leds) _leds->setParamValue(19, masterBrightness);
                break;
            }
            case 4: {  // E4: zone count 1-3
                zoneCount = clamp8(zoneCount + delta, 1, 3);
                sendZonesLayout(zoneCount);
                break;
            }
            default: break;  // E5-E7: no delta action on Zones page
        }
    }

    /// Page 2 (EFFECT) — EncB delta routing.
    void handleEncBEffectPageDelta(uint8_t encIdx, int32_t delta) {
        switch (encIdx) {
            case 0: case 1: case 2: {  // E0-E2: per-zone speed
                zoneSpeed[encIdx] = clamp8(zoneSpeed[encIdx] + delta, 1, 50);
                sendZoneSpeed(encIdx, zoneSpeed[encIdx]);
                break;
            }
            case 3: {  // E3: master speed
                masterSpeed = clamp8(masterSpeed + delta, 1, 100);
                sendGlobalParam("speed", masterSpeed);
                if (_leds) _leds->setParamValue(3, masterSpeed);
                break;
            }
            case 4: {  // E4: zone count 1-3
                zoneCount = clamp8(zoneCount + delta, 1, 3);
                sendZonesLayout(zoneCount);
                break;
            }
            case 5: {  // E5: transition type
                transitionType = clamp8(transitionType + delta, 0, 11);
                sendTransitionConfig(transitionType);
                break;
            }
            default: break;  // E6-E7: no delta action on Effect page
        }
    }

    /// Page 3 (EDGEMIXER) — EncB delta routing.
    void handleEncBMixerPageDelta(uint8_t encIdx, int32_t delta) {
        switch (encIdx) {
            case 0: {  // E0: EdgeMixer mode 0-6
                emMode = clamp8(emMode + delta, 0, 6);
                sendEdgeMixer("mode", emMode);
                if (_leds) _leds->setEdgeMixerMode(emMode);
                break;
            }
            case 1: {  // E1: spread 0-60
                emSpread = clamp8(emSpread + delta, 0, 60);
                sendEdgeMixer("spread", emSpread);
                if (_leds) _leds->setParamValue(9, emSpread);
                break;
            }
            case 2: {  // E2: strength 0-255
                emStrength = clamp8(emStrength + delta, 0, 255);
                sendEdgeMixer("strength", emStrength);
                if (_leds) _leds->setParamValue(10, emStrength);
                break;
            }
            case 4: {  // E4: zone count 1-3
                zoneCount = clamp8(zoneCount + delta, 1, 3);
                sendZonesLayout(zoneCount);
                break;
            }
            case 6: {  // E6: preset slot 0-7
                presetSlot = clamp8(presetSlot + delta, 0, 7);
                break;
            }
            default: break;  // E3, E5, E7: no delta action on Mixer page
        }
    }

    // -----------------------------------------------------------------
    // EncB page-aware button dispatch
    // -----------------------------------------------------------------

    /// Route EncB button (IDs 8-15) based on active display page.
    void handleEncBButton(uint8_t id) {
        uint8_t encIdx = id - 8;  // 0-7
        switch (_activePage) {
            case 0: return;  // Connection page — EncB inactive
            case 1: handleEncBZonesPageButton(encIdx, id); break;
            case 2: handleEncBEffectPageButton(encIdx, id); break;
            case 3: handleEncBMixerPageButton(encIdx, id); break;
        }
    }

    /// Page 1 (ZONES) — EncB button routing.
    void handleEncBZonesPageButton(uint8_t encIdx, uint8_t id) {
        switch (encIdx) {
            case 4: {  // E4: toggle zone system
                toggleZoneSystem();
                if (_leds) _leds->flashButton(id);
                break;
            }
            case 5: case 6: case 7: {  // E5-E7: toggle zone 1/2/3 enable
                uint8_t zi = encIdx - 5;  // 0, 1, 2
                zoneEnabled[zi] = !zoneEnabled[zi];
                JsonDocument doc;
                doc["zoneId"] = zi;
                doc["enabled"] = zoneEnabled[zi];
                _ws->sendJSON("zone.enableZone", doc);
                if (_leds) {
                    _leds->setZoneEnabled(zi, zoneEnabled[zi]);
                    _leds->flashButton(id);
                }
                Serial.printf("[Zone] Z%d %s\n", zi + 1,
                              zoneEnabled[zi] ? "ENABLED" : "DISABLED");
                break;
            }
            default:
                if (_leds) _leds->flashButton(id);
                break;
        }
    }

    /// Page 2 (EFFECT) — EncB button routing.
    void handleEncBEffectPageButton(uint8_t encIdx, uint8_t id) {
        switch (encIdx) {
            case 4: {  // E4: toggle zone system
                toggleZoneSystem();
                if (_leds) _leds->flashButton(id);
                break;
            }
            case 6: {  // E6: trigger transition
                _ws->sendSimple("nextEffect");
                if (_leds) _leds->flashButton(id);
                break;
            }
            case 7: {  // E7: next effect
                _ws->sendSimple("nextEffect");
                if (_leds) _leds->flashButton(id);
                break;
            }
            default:
                if (_leds) _leds->flashButton(id);
                break;
        }
    }

    /// Page 3 (EDGEMIXER) — EncB button routing.
    void handleEncBMixerPageButton(uint8_t encIdx, uint8_t id) {
        switch (encIdx) {
            case 3: {  // E3: spatial/temporal cycle
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
            case 4: {  // E4: toggle zone system
                toggleZoneSystem();
                if (_leds) _leds->flashButton(id);
                break;
            }
            case 5: {  // E5: camera mode toggle
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
            case 7: {  // E7: load preset
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

    void sendZoneEffect(uint8_t zi, uint16_t effectId) {
        JsonDocument doc;
        doc["zoneId"] = zi;
        doc["effectId"] = effectId;
        _ws->sendJSON("zone.setEffect", doc);
    }

    /// Find effectId in effectList and set zoneEffectIdx for the given zone.
    void syncEffectIndex(uint8_t zi) {
        if (zi > 2) return;
        for (uint8_t i = 0; i < effectCount; i++) {
            if (effectList[i] == zoneEffectId[zi]) {
                zoneEffectIdx[zi] = i;
                return;
            }
        }
        // If not found, default to index 0
        zoneEffectIdx[zi] = 0;
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

    void sendZonesLayout(uint8_t count) {
        JsonDocument doc;
        JsonArray zones = doc["zones"].to<JsonArray>();
        if (count == 1) {
            JsonObject z = zones.add<JsonObject>();
            z["zoneId"] = 0;
            z["s1LeftStart"] = 0;   z["s1LeftEnd"] = 79;
            z["s1RightStart"] = 80; z["s1RightEnd"] = 159;
        } else if (count == 2) {
            JsonObject z0 = zones.add<JsonObject>();
            z0["zoneId"] = 0;
            z0["s1LeftStart"] = 60; z0["s1LeftEnd"] = 79;
            z0["s1RightStart"] = 80; z0["s1RightEnd"] = 99;
            JsonObject z1 = zones.add<JsonObject>();
            z1["zoneId"] = 1;
            z1["s1LeftStart"] = 0;  z1["s1LeftEnd"] = 59;
            z1["s1RightStart"] = 100; z1["s1RightEnd"] = 159;
        } else {
            JsonObject z0 = zones.add<JsonObject>();
            z0["zoneId"] = 0;
            z0["s1LeftStart"] = 65; z0["s1LeftEnd"] = 79;
            z0["s1RightStart"] = 80; z0["s1RightEnd"] = 94;
            JsonObject z1 = zones.add<JsonObject>();
            z1["zoneId"] = 1;
            z1["s1LeftStart"] = 20; z1["s1LeftEnd"] = 64;
            z1["s1RightStart"] = 95; z1["s1RightEnd"] = 139;
            JsonObject z2 = zones.add<JsonObject>();
            z2["zoneId"] = 2;
            z2["s1LeftStart"] = 0;  z2["s1LeftEnd"] = 19;
            z2["s1RightStart"] = 140; z2["s1RightEnd"] = 159;
        }
        _ws->sendJSON("zones.setLayout", doc);
    }

    void sendTransitionConfig(uint8_t type) {
        JsonDocument doc;
        doc["defaultType"] = type;
        _ws->sendJSON("transition.config", doc);
    }

    static uint8_t clamp8(int32_t val, int32_t lo, int32_t hi) {
        if (val < lo) return (uint8_t)lo;
        if (val > hi) return (uint8_t)hi;
        return (uint8_t)val;
    }

    ZMWebSocketClient* _ws = nullptr;
    EchoGuard* _echo = nullptr;
    LedFeedback* _leds = nullptr;
    uint8_t _activePage = 0;
};
