/**
 * @file node_udp_rx.h
 * @brief UDP Receive Pipeline
 * 
 * Receives UDP packets from Hub, validates, tracks loss, enqueues to scheduler.
 */

#pragma once

#include <WiFiUdp.h>
#include "../../common/proto/udp_packets.h"
#include "../../common/proto/proto_constants.h"
#include "../sync/node_timesync.h"

class NodeScheduler;  // Forward declaration

class NodeUdpRx {
public:
    NodeUdpRx(NodeTimeSync* timesync, NodeScheduler* scheduler);
    
    bool init();
    void loop();  // Call frequently to process incoming packets
    
    // Statistics
    uint32_t getPacketsReceived() const { return packetsReceived_; }
    uint32_t getPacketsDropped() const { return packetsDropped_; }
    uint32_t getLossCount() const { return lossCount_; }
    uint16_t getLossPercent() const;  // 0-10000 (0.01% resolution)
    
    void setTokenHash(uint32_t tokenHash);
    
private:
    WiFiUDP udp_;
    NodeTimeSync* timesync_;
    NodeScheduler* scheduler_;
    
    uint32_t expectedSeq_;
    uint32_t expectedTokenHash_;
    
    uint32_t packetsReceived_;
    uint32_t packetsDropped_;
    uint32_t lossCount_;

    // Last-applied scene state (to avoid enqueuing redundant scene changes at 100Hz).
    uint16_t lastEffectId_ = 0xFFFF;
    uint16_t lastPaletteId_ = 0xFFFF;
    
    uint8_t rxBuffer_[LW_UDP_MAX_PAYLOAD + sizeof(lw_udp_hdr_t)];
    
    void processPacket(uint8_t* data, size_t len);
    bool validatePacket(const lw_udp_hdr_t* hdr, size_t len);
};
