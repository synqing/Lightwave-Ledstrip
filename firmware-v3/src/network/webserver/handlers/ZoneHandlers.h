// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../../../effects/zones/ZoneComposer.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"  // For CachedRendererState

namespace lightwaveos {
namespace actors {
class ActorSystem;
class RendererActor;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ZoneHandlers {
public:
    static void handleList(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer);
    static void handleLayout(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleGet(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer);
    static void handleSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::actors::ActorSystem& actors, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetBlend(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetEnabled(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

    // Zone Configuration Persistence
    static void handleConfigGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, void* zoneConfigMgr);
    static void handleConfigSave(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, void* zoneConfigMgr);
    static void handleConfigLoad(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, void* zoneConfigMgr, std::function<void()> broadcastZoneState);

    // Zone Timing
    static void handleTimingGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer);
    static void handleTimingReset(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer);

    // Zone Audio Configuration
    static void handleAudioConfigGet(AsyncWebServerRequest* request, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer);
    static void handleAudioConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

    // Zone Beat Trigger
    static void handleBeatTriggerGet(AsyncWebServerRequest* request, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer);
    static void handleBeatTriggerSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

    // Zone Reorder
    static void handleReorder(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

private:
    static uint8_t extractZoneIdFromPath(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
