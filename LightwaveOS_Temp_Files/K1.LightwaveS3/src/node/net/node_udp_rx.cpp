/**
 * Node UDP RX Implementation
 */

#include "node_udp_rx.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include "../sync/node_scheduler.h"
#include <Arduino.h>

#define P3_PASS(code, fmt, ...) LW_LOGI("[P3-PASS][%s] " fmt, code, ##__VA_ARGS__)
#define P3_FAIL(code, fmt, ...) LW_LOGE("[P3-FAIL][%s] " fmt, code, ##__VA_ARGS__)
#define P3_WARN(code, fmt, ...) LW_LOGW("[P3-WARN][%s] " fmt, code, ##__VA_ARGS__)

// Optional deep diagnostics (VERY noisy at 100Hz); keep disabled by default.
#ifndef LW_NODE_UDP_DIAG
#define LW_NODE_UDP_DIAG 0
#endif

static inline bool p3_every_ms(uint32_t* last_ms, uint32_t period_ms) {
    uint32_t now = (uint32_t)millis();
    if (now - *last_ms >= period_ms) { *last_ms = now; return true; }
    return false;
}

NodeUdpRx::NodeUdpRx(NodeTimeSync* timesync, NodeScheduler* scheduler)
    : timesync_(timesync), scheduler_(scheduler), expectedSeq_(0),
      expectedTokenHash_(0), packetsReceived_(0), packetsDropped_(0), lossCount_(0) {
}

void NodeUdpRx::setTokenHash(uint32_t tokenHash) {
    expectedTokenHash_ = tokenHash;

    // Reset sequence tracking whenever session token changes (prevents misleading loss stats)
    expectedSeq_ = 0;
    lossCount_ = 0;
    packetsReceived_ = 0;
    packetsDropped_ = 0;
    lastEffectId_ = 0xFFFF;
    lastPaletteId_ = 0xFFFF;

    P3_PASS("NUR_TOKEN_SET", "expectedTokenHash=0x%08X (seq/loss counters reset)", expectedTokenHash_);
}

bool NodeUdpRx::init() {
    if (!udp_.begin(LW_UDP_PORT)) {
        LW_LOGE("Failed to start UDP on port %d", LW_UDP_PORT);
        return false;
    }
    
    P3_PASS("NUR_INIT", "UDP RX bound port=%u", (unsigned)LW_UDP_PORT);
    return true;
}

void NodeUdpRx::loop() {
    int packetSize = udp_.parsePacket();
    if (packetSize == 0) return;
    
    if (packetSize > sizeof(rxBuffer_)) {
        LW_LOGW("UDP packet too large: %d bytes", packetSize);
        packetsDropped_++;
        return;
    }
    
    int len = udp_.read(rxBuffer_, sizeof(rxBuffer_));
    if (len > 0) {
        processPacket(rxBuffer_, len);
    }
}

void NodeUdpRx::processPacket(uint8_t* data, size_t len) {
    if (len < sizeof(lw_udp_hdr_t)) {
        packetsDropped_++;
        return;
    }
    
    lw_udp_hdr_t* hdr = (lw_udp_hdr_t*)data;
    
    // Convert from network byte order
    lw_udp_hdr_ntoh(hdr);

#if LW_NODE_UDP_DIAG
    // Diagnostic: token validation visibility (throttled)
    static uint32_t lastTokenLogMs = 0;
    if (p3_every_ms(&lastTokenLogMs, 2000)) {
        Serial.printf("[NODE-UDP] tokenHash=0x%08X expected=0x%08X\n",
                      (unsigned)hdr->tokenHash, (unsigned)expectedTokenHash_);
    }
#endif
    
    // Validate
    if (!validatePacket(hdr, len)) {
        packetsDropped_++;
        return;
    }
    
    packetsReceived_++;
    
    // Log first valid packet (for deterministic Test 1 PASS)
    static bool firstOk = false;
    if (!firstOk) {
        firstOk = true;
        P3_PASS("NUR_FIRST_OK",
                "first valid fanout packet accepted: seq=%lu tokenHash=0x%08X",
                (unsigned long)hdr->seq, (unsigned)hdr->tokenHash);
    }
    
    // Track sequence and loss
    if (expectedSeq_ == 0) {
        // First packet
        expectedSeq_ = hdr->seq + 1;
    } else if (hdr->seq == expectedSeq_) {
        // Expected sequence
        expectedSeq_++;
    } else if (hdr->seq > expectedSeq_) {
        // Gap detected (loss)
        uint32_t gap = hdr->seq - expectedSeq_;
        lossCount_ += gap;
        expectedSeq_ = hdr->seq + 1;
        
        // Throttle loss logs to every 1 second
        static uint32_t lastLossLog = 0;
        if (p3_every_ms(&lastLossLog, 1000)) {
            P3_WARN("NUR_LOSS",
                    "gap=%lu totalLoss=%lu expectedSeq=%lu gotSeq=%lu",
                    (unsigned long)gap, (unsigned long)lossCount_,
                    (unsigned long)(expectedSeq_ - 1), (unsigned long)hdr->seq);
        }
    } else {
        // Duplicate or reordered (old seq)
        LW_LOGD("Duplicate/old UDP packet: seq=%lu (expected %lu)", hdr->seq, expectedSeq_);
        return;
    }
    
    // Convert applyAt from hub time to local time
    if (!timesync_->isLocked()) {
        LW_LOGD("Time sync not locked, dropping packet");
        return;
    }
    
    uint64_t applyAt_local_us = timesync_->hubToLocal(hdr->applyAt_us);
    
    if (hdr->msgType == LW_UDP_PARAM_DELTA) {
        lw_udp_param_delta_t* payload = (lw_udp_param_delta_t*)(data + sizeof(lw_udp_hdr_t));
        lw_udp_param_delta_ntoh(payload);

        // If the Hub includes effect/palette in the PARAM_DELTA payload, treat those as a scene change
        // but only enqueue when they actually change (prevents redundant scene spam at 100Hz).
        if (payload->effectId != lastEffectId_ || payload->paletteId != lastPaletteId_) {
            uint64_t now_us = lw_monotonic_us();
            int64_t offset_us = timesync_ ? timesync_->getOffsetUs() : 0;
            int64_t delta_us = (int64_t)applyAt_local_us - (int64_t)now_us;
            P3_PASS("NUR_SCENE_RX",
                    "seq=%lu effect=%u palette=%u hubNow=%llu applyAtHub=%llu offset=%lld applyAtLocal=%llu now=%llu delta=%lld locked=%d",
                    (unsigned long)hdr->seq,
                    (unsigned)payload->effectId,
                    (unsigned)payload->paletteId,
                    (unsigned long long)hdr->hubNow_us,
                    (unsigned long long)hdr->applyAt_us,
                    (long long)offset_us,
                    (unsigned long long)applyAt_local_us,
                    (unsigned long long)now_us,
                    (long long)delta_us,
                    timesync_ ? (int)timesync_->isLocked() : -1);
            lw_cmd_t scene = {};
            scene.type = LW_CMD_SCENE_CHANGE;
            scene.applyAt_us = applyAt_local_us;
            scene.trace_seq = hdr->seq;
            scene.data.scene.effectId = payload->effectId;
            scene.data.scene.paletteId = payload->paletteId;
            scene.data.scene.transition = 0;
            scene.data.scene.duration_ms = 0;
            scheduler_->enqueue(&scene);
            lastEffectId_ = payload->effectId;
            lastPaletteId_ = payload->paletteId;
        }

        lw_cmd_t params = {};
        params.type = LW_CMD_PARAM_DELTA;
        params.applyAt_us = applyAt_local_us;
        params.trace_seq = hdr->seq;
        params.data.params.brightness = payload->brightness;
        params.data.params.speed = payload->speed;
        params.data.params.paletteId = payload->paletteId;
        params.data.params.intensity = 0;
        params.data.params.saturation = 0;
        params.data.params.complexity = 0;
        params.data.params.variation = 0;
        params.data.params.hue = payload->hue;
        params.data.params.flags = (uint16_t)(LW_PARAM_F_BRIGHTNESS | LW_PARAM_F_SPEED | LW_PARAM_F_HUE);
        scheduler_->enqueue(&params);
    } else if (hdr->msgType == LW_UDP_SCENE_CHANGE) {
        lw_udp_scene_change_t* payload = (lw_udp_scene_change_t*)(data + sizeof(lw_udp_hdr_t));
        lw_udp_scene_change_ntoh(payload);
        lw_cmd_t scene = {};
        scene.type = LW_CMD_SCENE_CHANGE;
        scene.applyAt_us = applyAt_local_us;
        scene.trace_seq = hdr->seq;
        scene.data.scene.effectId = payload->effectId;
        scene.data.scene.paletteId = payload->paletteId;
        scheduler_->enqueue(&scene);
        lastEffectId_ = payload->effectId;
        lastPaletteId_ = payload->paletteId;
    }
    
    // Health summary (every 2 seconds with full metrics)
    static uint32_t lastHealth = 0;
    if (p3_every_ms(&lastHealth, 2000)) {
        uint32_t total = packetsReceived_ + lossCount_;
        float lossPct = (total == 0) ? 0.0f : (100.0f * (float)lossCount_ / (float)total);

        P3_PASS("NUR_HEALTH",
                "seq=%lu rx=%lu drop=%lu loss=%lu (%.2f%%) tsLocked=%d expectedTokenHash=0x%08X",
                (unsigned long)hdr->seq,
                (unsigned long)packetsReceived_,
                (unsigned long)packetsDropped_,
                (unsigned long)lossCount_,
                lossPct,
                timesync_ ? (int)timesync_->isLocked() : -1,
                (unsigned)expectedTokenHash_);
    }
}

bool NodeUdpRx::validatePacket(const lw_udp_hdr_t* hdr, size_t len) {
    // Basic header validation
    if (!lw_udp_validate_header(hdr, len)) {
        P3_FAIL("NUR_HDR",
                "invalid header: proto=%u msgType=%u payloadLen=%u seq=%lu len=%u",
                (unsigned)hdr->proto, (unsigned)hdr->msgType, (unsigned)hdr->payloadLen,
                (unsigned long)hdr->seq, (unsigned)len);
        return false;
    }
    
    // Token hash validation
    if (expectedTokenHash_ == 0) {
        // Token not yet set - drop packets until WELCOME arrives
        static uint32_t lastUnset = 0;
        if (p3_every_ms(&lastUnset, 2000)) {
            P3_WARN("NUR_TOKEN_UNSET", "dropping fanout until WELCOME token is set");
        }
        return false;
    }
    
    if (hdr->tokenHash != expectedTokenHash_) {
        // If we JUST rekeyed, one or two stale packets from the Hub are normal.
        // We already reset counters on setTokenHash(), so this is a clean "rekey window" signal.
        if (packetsReceived_ == 0 && expectedSeq_ == 0) {
            P3_WARN("NUR_TOKEN_MISMATCH_REKEY",
                    "transient mismatch during rekey: expected=0x%08X got=0x%08X",
                    expectedTokenHash_, hdr->tokenHash);
        } else {
            static uint32_t lastTokFail = 0;
            if (p3_every_ms(&lastTokFail, 1000)) {
                P3_FAIL("NUR_TOKEN_MISMATCH",
                        "expected=0x%08X got=0x%08X seq=%lu",
                        expectedTokenHash_, hdr->tokenHash, (unsigned long)hdr->seq);
            }
        }
        return false;
    }
    
    return true;
}

uint16_t NodeUdpRx::getLossPercent() const {
    if (packetsReceived_ == 0) return 0;
    uint32_t total = packetsReceived_ + lossCount_;
    return (lossCount_ * 10000) / total;  // 0.01% resolution
}
