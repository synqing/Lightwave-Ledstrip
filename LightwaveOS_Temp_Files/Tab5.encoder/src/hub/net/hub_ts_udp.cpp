/**
 * Hub UDP Time Sync Implementation
 */

#include "hub_ts_udp.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"

HubTsUdp::HubTsUdp(HubRegistry* registry) : registry_(registry) {}

bool HubTsUdp::init() {
    if (!udp_.begin(LW_TS_UDP_PORT)) {
        LW_LOGE("Failed to bind UDP time-sync port %d", LW_TS_UDP_PORT);
        return false;
    }
    
    LW_LOGI("UDP time-sync listener started on port %d", LW_TS_UDP_PORT);
    return true;
}

void HubTsUdp::loop() {
    int packetSize = udp_.parsePacket();
    if (packetSize == 0) return;

    rxCount_++;
    
    if (packetSize != sizeof(lw_ts_ping_t)) {
        LW_LOGW("Invalid packet size: %d (expected %d)", packetSize, sizeof(lw_ts_ping_t));
        udp_.flush();
        invalidCount_++;
        return;
    }
    
    // Capture t2 (hub receive time) as early as possible
    uint64_t t2_us = lw_monotonic_us();
    
    // Read ping
    lw_ts_ping_t ping;
    udp_.read((uint8_t*)&ping, sizeof(ping));

    if (verbose_) {
        static uint32_t lastLogMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastLogMs >= 1000) {
            Serial.printf("[HUB-TS-UDP] Ping: seq=%u proto=%u type=%u rx=%lu tx=%lu bad=%lu\n",
                          (unsigned)ping.seq, (unsigned)ping.proto, (unsigned)ping.type,
                          (unsigned long)rxCount_, (unsigned long)txCount_, (unsigned long)invalidCount_);
            lastLogMs = nowMs;
        }
    }
    
    uint32_t remoteIp = udp_.remoteIP();
    uint16_t remotePort = udp_.remotePort();
    
    // Validate protocol version
    if (ping.proto != LW_TS_PROTO_VER) {
        LW_LOGW("Invalid protocol version: %d", ping.proto);
        invalidCount_++;
        return;
    }
    
    // Validate message type
    if (ping.type != LW_TS_MSG_PING) {
        LW_LOGW("Invalid message type: %d", ping.type);
        invalidCount_++;
        return;
    }
    
    // Build pong
    lw_ts_pong_t pong;
    pong.proto = LW_TS_PROTO_VER;
    pong.type = LW_TS_MSG_PONG;
    pong.reserved = 0;
    pong.seq = ping.seq;
    pong.tokenHash = ping.tokenHash;
    pong.t1_us = ping.t1_us;
    pong.t2_us = t2_us;
    
    // Capture t3 (hub send time) just before sendto
    pong.t3_us = lw_monotonic_us();
    
    // Send pong
    udp_.beginPacket(remoteIp, remotePort);
    udp_.write((const uint8_t*)&pong, sizeof(pong));
    int sent = udp_.endPacket();

    if (sent > 0) {
        txCount_++;
    }

    if (verbose_) {
        static uint32_t lastPongLogMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastPongLogMs >= 1000) {
            Serial.printf("[HUB-TS-UDP] Pong: seq=%u sent=%d rx=%lu tx=%lu bad=%lu\n",
                          (unsigned)pong.seq, sent,
                          (unsigned long)rxCount_, (unsigned long)txCount_, (unsigned long)invalidCount_);
            lastPongLogMs = nowMs;
        }
    }
}
