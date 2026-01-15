/**
 * Hub UDP Time Sync Listener
 * 
 * Dedicated UDP socket for low-latency time sync ping/pong.
 * Eliminates WebSocket queueing from time measurements.
 */

#pragma once

#include <WiFiUdp.h>
#include "../../common/proto/ts_udp.h"
#include "hub_registry.h"

class HubTsUdp {
public:
    HubTsUdp(HubRegistry* registry);
    
    bool init();
    void loop();  // Process incoming pings

    // Debug control (runtime)
    void setVerbose(bool enabled) { verbose_ = enabled; }
    bool isVerbose() const { return verbose_; }
    
private:
    WiFiUDP udp_;
    HubRegistry* registry_;
    uint8_t rxBuf_[sizeof(lw_ts_pong_t)];
    bool verbose_ = false;

    // Minimal counters for ad-hoc debugging
    uint32_t rxCount_ = 0;
    uint32_t txCount_ = 0;
    uint32_t invalidCount_ = 0;
    
    void handlePing(const lw_ts_ping_t* ping, uint32_t remoteIp, uint16_t remotePort);
};
