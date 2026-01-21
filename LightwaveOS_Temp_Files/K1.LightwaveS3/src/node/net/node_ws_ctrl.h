/**
 * @file node_ws_ctrl.h
 * @brief WebSocket Control Plane Client (ESP-IDF Native)
 * 
 * Handles HELLO/WELCOME/KEEPALIVE/TS_PING messages with Hub.
 * Uses ESP-IDF native esp_websocket_client instead of external library.
 */

#pragma once

#include <stdint.h>
#include "../../common/proto/ws_messages.h"
#include "../../common/proto/proto_constants.h"

// Forward declarations (avoid include order issues)
struct esp_websocket_client;
typedef struct esp_websocket_client* esp_websocket_client_handle_t;
class NodeUdpRx;
class NodeOtaClient;
class NodeTimeSync;
class NodeScheduler;

typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_HELLO_SENT,
    WS_STATE_AUTHENTICATED
} node_ws_state_t;

class NodeWsCtrl {
public:
    NodeWsCtrl();
    ~NodeWsCtrl();
    
    bool init(const char* hub_ip);
    void loop();
    
    void setUdpRx(NodeUdpRx* udpRx) { udpRx_ = udpRx; }
    void setOtaClient(NodeOtaClient* ota) { ota_ = ota; }
    void setTimeSync(NodeTimeSync* timesync) { timesync_ = timesync; }
    void setScheduler(NodeScheduler* scheduler) { scheduler_ = scheduler; }
    void setVerbose(bool enabled) { verbose_ = enabled; }
    
    bool isAuthenticated() const { return state_ == WS_STATE_AUTHENTICATED; }
    node_ws_state_t getState() const { return state_; }
    
    uint8_t getNodeId() const { return nodeId_; }
    const char* getToken() const { return token_; }
    uint32_t getTokenHash() const { return tokenHash_; }
    
    void sendOtaStatus(const char* state, uint8_t progress, const char* error);
    
    // Callbacks (set by application)
    void (*onWelcome)(uint8_t nodeId, const char* token);
    void (*onTsPong)(uint32_t seq, uint64_t t1_us, uint64_t t2_us, uint64_t t3_us);
    
private:
    esp_websocket_client_handle_t ws_;
    node_ws_state_t state_;
    NodeUdpRx* udpRx_;  // For disarming fanout on disconnect
    NodeOtaClient* ota_;  // For handling OTA updates
    NodeTimeSync* timesync_;  // For hub->local applyAt conversion
    NodeScheduler* scheduler_;  // For scheduled command application
    
    uint8_t nodeId_;
    char token_[64];
    uint32_t tokenHash_;
    
    uint32_t lastKeepalive_ms_;
    uint32_t lastTsPing_ms_;
    uint32_t tsPingSeq_;
    uint32_t wsCmdSeq_;
    
    bool started_;  // Track if WS client has been started
    bool restartRequested_;
    uint32_t lastRestartAttempt_ms_;
    char hub_ip_[32];  // Store hub IP for deferred start

    // Last-known state (used to preserve palette across effect-only messages)
    uint8_t lastEffectId_;
    uint8_t lastPaletteId_;
    bool verbose_ = false;
    
    void sendHello();
    void sendKeepalive();
    void sendTsPing();
    void handleMessage(const char* data, size_t len);
    bool recreateClient();

    uint64_t resolveApplyAtLocal(uint64_t applyAt_hub_us) const;
    
    // ESP-IDF event handler
    static void wsEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    static NodeWsCtrl* instance_;
};
