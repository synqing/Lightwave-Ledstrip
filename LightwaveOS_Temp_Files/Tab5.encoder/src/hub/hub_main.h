/**
 * @file hub_main.h
 * @brief Hub Main Coordinator
 * 
 * Orchestrates all Hub subsystems: SoftAP, HTTP/WS, Registry, UDP fanout, Clock
 */

#pragma once

#include "net/hub_softap_dhcp.h"
#include "net/hub_http_ws.h"
#include "net/hub_registry.h"
#include "net/hub_udp_fanout.h"
#include "net/hub_ts_udp.h"
#include "show/hub_clock.h"
#include "ota/hub_ota_repo.h"
#include "ota/hub_ota_dispatch.h"
#include "state/HubState.h"

class HubMain {
public:
    HubMain() : httpWs_(&registry_, &clock_), udpFanout_(&registry_, &clock_), tsUdp_(&registry_),
                otaDispatch_(&registry_, &otaRepo_), initialized_(false), lastBatch_ms_(0) {}
    
    bool init(const char* ssid, const char* password);
    void loop();
    void loopNoDisplay();  // Loop without dashboard update (for FreeRTOS task)
    void udpTick();  // Called at 100Hz by timer

    // Runtime debug controls
    void setTimeSyncUdpVerbose(bool enabled) { tsUdp_.setVerbose(enabled); }
    void setFanoutEnabled(bool enabled) { udpFanout_.setEnabled(enabled); }
    void setFanoutVerbose(bool enabled) { udpFanout_.setVerbose(enabled); }
    void setFanoutLogIntervalMs(uint32_t intervalMs) { udpFanout_.setLogIntervalMs(intervalMs); }
    void setControlVerbose(bool enabled) { ctrlVerbose_ = enabled; }
    bool isControlVerbose() const { return ctrlVerbose_; }
    
    // Accessors for dashboard
    HubRegistry* getRegistry() { return &registry_; }
    HubOtaDispatch* getOtaDispatch() { return &otaDispatch_; }
    HubState* getState() { return &HubState::getInstance(); }
    
private:
    hub_clock_t clock_;
    HubRegistry registry_;
    HubSoftApDhcp softap_;
    HubHttpWs httpWs_;
    HubUdpFanout udpFanout_;
    HubTsUdp tsUdp_;
    HubOtaRepo otaRepo_;
    HubOtaDispatch otaDispatch_;
    
    bool initialized_;
    uint32_t lastBatch_ms_;
    bool ctrlVerbose_ = false;
    
    static void onRegistryEvent(uint8_t nodeId, uint8_t eventType, const char* message);
    static HubMain* instance_;
};
