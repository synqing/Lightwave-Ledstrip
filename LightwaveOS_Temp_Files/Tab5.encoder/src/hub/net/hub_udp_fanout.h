/**
 * @file hub_udp_fanout.h
 * @brief UDP Fanout Tick Loop (100Hz Stream Plane)
 * 
 * Sends UDP packets to all READY nodes at 100Hz with applyAt timestamps.
 */

#pragma once

#include <stdint.h>
#include <WiFiUdp.h>
#include "../../common/proto/udp_packets.h"
#include "../../common/proto/proto_constants.h"
#include "hub_registry.h"
#include "../show/hub_clock.h"
#include "../state/HubState.h"

class HubUdpFanout {
public:
    HubUdpFanout(HubRegistry* registry, hub_clock_t* clock);
    
    bool init();
    void tick();  // Called at 100Hz

    void setState(HubState* state) { state_ = state; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setVerbose(bool enabled) { verbose_ = enabled; }
    void setLogIntervalMs(uint32_t intervalMs) { logIntervalMs_ = intervalMs; }
    
    // Statistics
    uint32_t getTotalSent() const { return totalSent_; }
    uint32_t getTickOverruns() const { return tickOverruns_; }
    
private:
    HubRegistry* registry_;
    hub_clock_t* clock_;
    HubState* state_ = nullptr;
    WiFiUDP udp_;
    
    uint32_t seq_;
    uint32_t totalSent_;
    uint32_t tickOverruns_;
    uint64_t lastTick_us_;
    uint16_t lastEffectId_;
    uint16_t lastPaletteId_;
    bool enabled_ = false;
    bool verbose_ = false;
    uint32_t logIntervalMs_ = 5000;
    
    void sendToNode(node_entry_t* node);
    void buildPacket(lw_udp_hdr_t* hdr, lw_udp_param_delta_t* payload, uint32_t tokenHash);
};
