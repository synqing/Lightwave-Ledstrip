/**
 * @file node_main.h
 * @brief Node Main Coordinator
 * 
 * Orchestrates all Node subsystems: WiFi STA, WS client, time sync, UDP RX, scheduler, renderer
 */

#pragma once

#include "net/node_wifi_sta.h"
#include "net/node_ws_ctrl.h"
#include "net/node_udp_rx.h"
#include "sync/node_timesync.h"
#include "sync/node_ts_udp.h"
#include "sync/node_scheduler.h"
#include "render/renderer_apply.h"
#include "failover/node_fallback.h"
#include "ota/node_ota_client.h"

typedef enum {
    NODE_SYSTEM_STATE_OFFLINE,
    NODE_SYSTEM_STATE_CONNECTING,
    NODE_SYSTEM_STATE_READY,
    NODE_SYSTEM_STATE_DEGRADED,
    NODE_SYSTEM_STATE_FAILED
} node_system_state_t;

// Forward declaration for v2 integration
namespace lightwaveos { namespace nodes { class NodeOrchestrator; } }

class NodeMain {
public:
    NodeMain() : udp_(&timesync_, &scheduler_), systemState_(NODE_SYSTEM_STATE_OFFLINE), lastUdp_ms_(0), initialized_(false), orchestrator_(nullptr) {}
    
    bool init(const char* hub_ssid, const char* hub_password);
    void loop();
    void renderFrameBoundary();  // Called before each render frame
    
    // Wire to v2 Actor system
    void setOrchestrator(lightwaveos::nodes::NodeOrchestrator* orchestrator) { orchestrator_ = orchestrator; }
    
    node_system_state_t getSystemState() const { return systemState_; }
    bool isReady() const { return systemState_ == NODE_SYSTEM_STATE_READY; }
    
    NodeOtaClient* getOtaClient() { return &ota_; }
    
private:
    NodeWifiSta wifi_;
    NodeWsCtrl ws_;
    NodeTimeSync timesync_;
    NodeTsUdp tsUdp_;
    NodeScheduler scheduler_;
    NodeUdpRx udp_;
    RendererApply renderer_;
    NodeFallback fallback_;
    NodeOtaClient ota_;
    
    lightwaveos::nodes::NodeOrchestrator* orchestrator_;  // v2 Actor system integration
    
    node_system_state_t systemState_;
    uint64_t lastUdp_ms_;
    
    bool initialized_;
    
    void updateSystemState();
    bool checkReadyGate();
    
    // Callbacks from WS
    static void onWelcome(uint8_t nodeId, const char* token);
    static void onTsPong(uint32_t seq, uint64_t t1_us, uint64_t t2_us, uint64_t t3_us);
    
    // Callback from OTA
    static void onOtaStatus(const char* state, uint8_t progress, const char* error);
};

// Global instance (for callbacks)
extern NodeMain g_nodeMain;
