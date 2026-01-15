/**
 * Node UDP Time Sync Implementation
 */

#include "node_ts_udp.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include <WiFi.h>

static const char* TAG = "NodeTsUdp";

NodeTsUdp::NodeTsUdp()
    : ts_(nullptr), tokenHash_(0), seq_(0), lastPing_ms_(0) {
}

bool NodeTsUdp::init(const char* hub_ip, uint32_t tokenHash) {
    if (!hubIp_.fromString(hub_ip)) {
        LW_LOGE("Invalid hub IP: %s", hub_ip);
        return false;
    }
    tokenHash_ = tokenHash;
    
    if (!udp_.begin(0)) {  // Random local port
        LW_LOGE("Failed to create UDP socket");
        return false;
    }
    
    LW_LOGI("UDP time-sync client initialized (target: %s:%d)", hub_ip, LW_TS_UDP_PORT);
    return true;
}

void NodeTsUdp::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        // Avoid UDP send spam while WiFi is down (prevents endPacket(): could not send data: 118 flood).
        return;
    }

    if (tokenHash_ == 0) {
        // Session token not armed yet (no WELCOME) or explicitly disarmed on WS disconnect.
        return;
    }

    // Send pings fast while locking, then slow down once locked.
    uint32_t now = millis();
    const bool locked = (ts_ != nullptr) && lw_timesync_is_locked(ts_);
    const uint32_t interval_ms = locked ? 1000 : 250;

    if (now - lastPing_ms_ >= interval_ms) {
        sendPing();
        lastPing_ms_ = now;
    }
    
    // Process incoming pongs
    processPong();
}

void NodeTsUdp::sendPing() {
    if (WiFi.status() != WL_CONNECTED || tokenHash_ == 0) return;

    lw_ts_ping_t ping;
    ping.proto = LW_TS_PROTO_VER;
    ping.type = LW_TS_MSG_PING;
    ping.reserved = 0;
    ping.seq = seq_++;
    ping.tokenHash = tokenHash_;
    ping.t1_us = lw_monotonic_us();  // Capture send time
    
    udp_.beginPacket(hubIp_, LW_TS_UDP_PORT);
    udp_.write((const uint8_t*)&ping, sizeof(ping));
    int sent = udp_.endPacket();

    if (verbose_) {
        Serial.printf("[NODE-TS-UDP] Sent ping: seq=%u, sent=%d\n", ping.seq, sent);
    }
}

void NodeTsUdp::processPong() {
    int packetSize = udp_.parsePacket();
    if (packetSize == 0) return;

    if (verbose_) {
        Serial.printf("[NODE-TS-UDP] Received packet: size=%d\n", packetSize);
    }
    
    // Safety check
    if (!ts_) {
        LW_LOGE("Time sync not initialized, dropping pong");
        udp_.flush();
        return;
    }
    
    // Capture t4 (receive time) as early as possible
    uint64_t t4_us = lw_monotonic_us();
    
    if (packetSize != sizeof(lw_ts_pong_t)) {
        LW_LOGW("Invalid pong size: %d (expected %d)", packetSize, sizeof(lw_ts_pong_t));
        udp_.flush();
        return;
    }
    
    // Read pong
    lw_ts_pong_t pong;
    udp_.read((uint8_t*)&pong, sizeof(pong));

    if (verbose_) {
        Serial.printf("[NODE-TS-UDP] Pong: seq=%u, proto=%d, type=%d\n", pong.seq, pong.proto, pong.type);
    }
    
    // Validate protocol
    if (pong.proto != LW_TS_PROTO_VER || pong.type != LW_TS_MSG_PONG) {
        LW_LOGW("Invalid pong: proto=%d, type=%d", pong.proto, pong.type);
        return;
    }
    
    if (verbose_) {
        Serial.printf("[NODE-TS-UDP] Pong validated, checking token: got=0x%08X, expected=0x%08X\n", pong.tokenHash, tokenHash_);
    }
    
    // Validate token hash
    if (pong.tokenHash != tokenHash_) {
        LW_LOGW("Token hash mismatch: got 0x%08X, expected 0x%08X", pong.tokenHash, tokenHash_);
        return;
    }
    
    if (verbose_) {
        Serial.printf("[NODE-TS-UDP] Calling lw_timesync_process_pong: t1=%llu, t2=%llu, t3=%llu, t4=%llu\n",
                      pong.t1_us, pong.t2_us, pong.t3_us, t4_us);
    }
    
    // Process 4-timestamp NTP
    bool was_locked = lw_timesync_is_locked(ts_);
    lw_timesync_process_pong(ts_, pong.t1_us, pong.t2_us, pong.t3_us, t4_us);
    bool now_locked = lw_timesync_is_locked(ts_);
    
    if (verbose_) {
        Serial.printf("[NODE-TS-UDP] After process_pong: was_locked=%d, now_locked=%d, samples=%u\n",
                      was_locked, now_locked, ts_->good_samples);
    }
    
    // Log lock transition
    if (!was_locked && now_locked) {
        LW_LOGI("*** TIME SYNC LOCKED via UDP (seq=%u) ***", pong.seq);
    }
    
    // Periodic stats (every 10 seconds when locked)
    static uint32_t lastStats = 0;
    if (now_locked && (millis() - lastStats > 10000)) {
        LW_LOGI("Time sync: offset=%lld us, rtt=%lu us, variance=%lu us",
                ts_->offset_us, ts_->rtt_us, ts_->rtt_variance_us);
        lastStats = millis();
    }
}
