/**
 * Node UDP Time Sync Client
 * 
 * Sends UDP ping every 200ms, receives pong, processes 4-timestamp NTP.
 * Eliminates WebSocket queueing from time measurements.
 */

#pragma once

#include <WiFiUdp.h>
#include "../../common/proto/ts_udp.h"
#include "../../common/clock/timesync.h"

class NodeTsUdp {
public:
    NodeTsUdp();
    
    void setTimesync(lw_timesync_t* ts) { ts_ = ts; }
    bool init(const char* hub_ip, uint32_t tokenHash);
    void loop();  // Send pings + process pongs
    void setVerbose(bool enabled) { verbose_ = enabled; }
    
    bool isLocked() const { return lw_timesync_is_locked(ts_); }
    
private:
    WiFiUDP udp_;
    lw_timesync_t* ts_;
    IPAddress hubIp_;
    uint32_t tokenHash_;
    uint32_t seq_;
    uint32_t lastPing_ms_;
    bool verbose_ = false;
    uint8_t rxBuf_[sizeof(lw_ts_pong_t)];
    
    void sendPing();
    void processPong();
};
