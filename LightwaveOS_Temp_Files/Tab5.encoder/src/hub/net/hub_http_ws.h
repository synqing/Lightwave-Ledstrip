/**
 * @file hub_http_ws.h
 * @brief HTTP + WebSocket Server for Hub
 * 
 * Provides:
 * - HTTP endpoints: /health, /metrics
 * - WebSocket endpoint: /ws (for node control plane)
 * 
 * Uses ESPAsyncWebServer (Arduino-compatible)
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <map>
#include "hub_registry.h"
#include "../state/HubState.h"
#include "../show/hub_clock.h"
#include "../ota/hub_ota_repo.h"
#include "../ota/hub_ota_dispatch.h"

class HubHttpWs {
public:
    HubHttpWs(HubRegistry* registry, hub_clock_t* clock);
    
    bool init(uint16_t port = LW_CTRL_HTTP_PORT);
    void loop();  // Call periodically for cleanupClients()
    
    // Public helpers for async sending
    void sendWelcome(uint32_t clientId, uint8_t nodeId);
    void sendTsPong(uint32_t clientId, uint8_t nodeId, uint32_t seq, uint64_t t1_us, uint64_t t2_us);
    void sendOtaUpdate(uint8_t nodeId, const char* version, const char* url, const char* sha256);

    // Phase 1: modern hub broadcasts (WebSocket control plane)
    void setState(HubState* state) { state_ = state; }
    void sendStateSnapshot(uint32_t clientId, uint8_t nodeId, uint64_t applyAt_us);
    void broadcastGlobalDelta(uint16_t dirtyMask, const HubState::GlobalParams& global, uint64_t applyAt_us);
    void sendZoneDelta(uint8_t nodeId, uint8_t zoneId, uint8_t dirtyMask, const HubState::ZoneSettings& zone, uint64_t applyAt_us);
    
    // OTA setup (called by HubMain after repos are initialized)
    void setOta(HubOtaRepo* repo, HubOtaDispatch* dispatch);
    
private:
    HubRegistry* registry_;
    hub_clock_t* clock_;
    AsyncWebServer server_;
    AsyncWebSocket ws_;
    
    // OTA (set via setOta())
    HubOtaRepo* otaRepo_;
    HubOtaDispatch* otaDispatch_;

    HubState* state_;
    
    // Client tracking (client->id() -> nodeId)
    std::map<uint32_t, uint8_t> clients_;

    struct PendingJoin {
        uint32_t clientId;
        uint8_t nodeId;
        uint64_t applyAt_us;
    };

    static constexpr uint8_t kPendingJoinMax = 4;
    PendingJoin pendingJoins_[kPendingJoinMax];
    uint8_t pendingHead_;
    uint8_t pendingTail_;
    uint8_t pendingCount_;

    bool enqueuePendingJoin(uint32_t clientId, uint8_t nodeId, uint64_t applyAt_us);
    void processPendingJoins(uint8_t maxPerLoop);
    
    // WebSocket event handler
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                   AwsEventType type, void *arg, uint8_t *data, size_t len);
    
    // Message handlers (take parsed JsonDocument to avoid double-parse)
    void handleHello(AsyncWebSocketClient *client, JsonDocument& doc);
    void handleKeepalive(AsyncWebSocketClient *client, JsonDocument& doc);
    void handleTsPing(AsyncWebSocketClient *client, JsonDocument& doc);
    void handleOtaStatus(AsyncWebSocketClient *client, JsonDocument& doc);
};
