/**
 * @file hub_registry.h
 * @brief Node Registry Service
 * 
 * Maintains NodeTable with state transitions: PENDING → AUTHED → READY → DEGRADED/LOST
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <map>
#include "../../common/proto/proto_constants.h"
#include "../../common/proto/ws_messages.h"

// Registry event types
enum {
    EVENT_NODE_HELLO = 1,
    EVENT_NODE_AUTHED = 2,
    EVENT_NODE_READY = 3,
    EVENT_NODE_DEGRADED = 4,
    EVENT_NODE_LOST = 5
};

// Node states (matches Quint spec)
typedef enum {
    NODE_STATE_PENDING,      // HELLO received, not yet welcomed
    NODE_STATE_AUTHED,       // WELCOME sent, waiting for ready gate
    NODE_STATE_READY,        // All gates satisfied, receiving UDP
    NODE_STATE_DEGRADED,     // Was READY, now metrics bad or keepalive missed
    NODE_STATE_LOST          // WS disconnect or timeout, will be cleaned up
} node_state_t;

// Node entry in registry
typedef struct {
    uint8_t nodeId;
    char mac[18];
    char ip[16];
    char token[64];
    uint32_t tokenHash;
    
    node_state_t state;
    uint64_t lastSeen_ms;
    
    // Capabilities & topology
    lw_caps_t caps;
    lw_topo_t topo;
    char fw[32];
    
    // Metrics
    int8_t rssi;
    uint16_t loss_pct;
    int32_t drift_us;
    bool timeSyncLocked;
    
    // Statistics
    uint32_t udp_sent;
    uint32_t keepalives_received;
    
    // OTA state
    char ota_state[16];      // "idle", "downloading", "installing", "complete", "error"
    uint8_t ota_pct;
    char ota_version[32];
    char ota_error[64];
} node_entry_t;

class HubRegistry {
public:
    HubRegistry();
    
    // Node lifecycle
    uint8_t registerNode(const lw_msg_hello_t* hello, const char* ip);
    bool sendWelcome(uint8_t nodeId, lw_msg_welcome_t* welcome);
    void updateKeepalive(uint8_t nodeId, const lw_msg_keepalive_t* ka);
    void markReady(uint8_t nodeId);
    void markDegraded(uint8_t nodeId);
    void markLost(uint8_t nodeId);
    void setOtaState(uint8_t nodeId, const char* state, uint8_t pct, const char* version, const char* error);
    
    // Query
    node_entry_t* getNode(uint8_t nodeId);
    bool isReady(uint8_t nodeId);
    uint8_t getReadyCount();
    uint8_t getTotalCount();
    
    // Event callback (for dashboard logging)
    void setEventCallback(void (*callback)(uint8_t nodeId, uint8_t eventType, const char* message)) {
        eventCallback_ = callback;
    }
    
    // Maintenance
    void tick(uint64_t now_ms);  // Check timeouts, cleanup LOST nodes
    
    // Iteration
    void forEachReady(void (*callback)(node_entry_t* node, void* ctx), void* ctx);
    void forEachAuthed(void (*callback)(node_entry_t* node, void* ctx), void* ctx);  // AUTHED, READY, DEGRADED
    void forEachAll(void (*callback)(node_entry_t* node, void* ctx), void* ctx);     // All states (for dashboard)
    
private:
    std::map<uint8_t, node_entry_t> nodes_;
    uint8_t nextNodeId_;
    uint32_t nextToken_;
    void (*eventCallback_)(uint8_t nodeId, uint8_t eventType, const char* message);
    
    void generateToken(char* token_out, size_t len);
    void cleanupLostNodes(uint64_t now_ms);
};

const char* node_state_str(node_state_t state);
