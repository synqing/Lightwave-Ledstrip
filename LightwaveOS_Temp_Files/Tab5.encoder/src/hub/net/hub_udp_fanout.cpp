/**
 * Hub UDP Fanout Implementation
 */

#include "hub_udp_fanout.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include <IPAddress.h>

HubUdpFanout::HubUdpFanout(HubRegistry* registry, hub_clock_t* clock)
    : registry_(registry), clock_(clock), seq_(1), totalSent_(0),
      tickOverruns_(0), lastTick_us_(0),
      lastEffectId_(0xFFFF), lastPaletteId_(0xFFFF) {
}

bool HubUdpFanout::init() {
    if (!udp_.begin(LW_UDP_PORT)) {
        LW_LOGE("Failed to start UDP on port %d", LW_UDP_PORT);
        return false;
    }
    
    LW_LOGI("UDP fanout initialized on port %d", LW_UDP_PORT);
    return true;
}

void HubUdpFanout::tick() {
    if (!enabled_) {
        // Phase 1: control-plane is WebSocket; UDP show fanout is disabled by default.
        // Phase 2 will reintroduce UDP for audio metrics only.
        return;
    }

    uint64_t now_us = lw_monotonic_us();
    
    // Check for tick overrun
    if (lastTick_us_ > 0) {
        uint64_t since_last = now_us - lastTick_us_;
        if (since_last > LW_UDP_TICK_PERIOD_US * 2) {
            tickOverruns_++;
        }
    }
    lastTick_us_ = now_us;
    
    // Update hub clock
    hub_clock_tick(clock_);
    
    // Send only to READY nodes
    // Fanout semantics match system truth: only READY nodes receive show packets
    auto callback = [](node_entry_t* node, void* ctx) {
        HubUdpFanout* self = static_cast<HubUdpFanout*>(ctx);
        self->sendToNode(node);
    };
    
    registry_->forEachReady(callback, this);
    
    seq_++;
}

void HubUdpFanout::sendToNode(node_entry_t* node) {
    // Skip nodes not yet authenticated (tokenHash=0 means WELCOME pending)
    if (node->tokenHash == 0) {
        return;
    }
    
    // Optional fanout summary (throttled; disabled by default)
    if (verbose_) {
        static uint32_t lastLogMs = 0;
        uint32_t nowMs = millis();
        if (nowMs - lastLogMs >= logIntervalMs_) {
            HubState::GlobalParams global;
            if (state_) global = state_->getGlobalSnapshot();
            Serial.printf("[HUB-UDP] nodeId=%u ip=%s tokenHash=0x%08X seq=%u effect=%u palette=%u bright=%u speed=%u\n",
                          node->nodeId, node->ip, node->tokenHash, seq_,
                          (unsigned)global.effectId, (unsigned)global.paletteId, (unsigned)global.brightness, (unsigned)global.speed);
            lastLogMs = nowMs;
        }
    }
    
    lw_udp_hdr_t hdr;
    lw_udp_param_delta_t payload;
    
    buildPacket(&hdr, &payload, node->tokenHash);

    if (verbose_ && (payload.effectId != lastEffectId_ || payload.paletteId != lastPaletteId_)) {
        Serial.printf("[HUB-TRACE] seq=%lu hubNow=%llu applyAt=%llu effect=%u palette=%u bright=%u speed=%u\n",
                      (unsigned long)hdr.seq,
                      (unsigned long long)hdr.hubNow_us,
                      (unsigned long long)hdr.applyAt_us,
                      (unsigned)payload.effectId,
                      (unsigned)payload.paletteId,
                      (unsigned)payload.brightness,
                      (unsigned)payload.speed);
        lastEffectId_ = payload.effectId;
        lastPaletteId_ = payload.paletteId;
    }
    
    // Convert to network byte order
    lw_udp_hdr_hton(&hdr);
    lw_udp_param_delta_hton(&payload);
    
    // Send packet
    IPAddress nodeIp;
    if (!nodeIp.fromString(node->ip)) {
        LW_LOGE("Invalid IP for node %d: %s", node->nodeId, node->ip);
        return;
    }
    
    udp_.beginPacket(nodeIp, LW_UDP_PORT);
    udp_.write((const uint8_t*)&hdr, sizeof(hdr));
    udp_.write((const uint8_t*)&payload, sizeof(payload));
    udp_.endPacket();
    
    totalSent_++;
    node->udp_sent++;
}

void HubUdpFanout::buildPacket(lw_udp_hdr_t* hdr, lw_udp_param_delta_t* payload, uint32_t tokenHash) {
    uint64_t hubNow_us = hub_clock_now_us(clock_);

    HubState::GlobalParams global;
    if (state_) {
        global = state_->getGlobalSnapshot();
    }

    // Build header
    hdr->proto = LW_PROTO_VER;
    hdr->msgType = LW_UDP_PARAM_DELTA;
    hdr->payloadLen = sizeof(lw_udp_param_delta_t);
    hdr->seq = seq_;
    hdr->tokenHash = tokenHash;
    hdr->hubNow_us = hubNow_us;
    hdr->applyAt_us = hubNow_us + LW_APPLY_AHEAD_US;
    
    // Build payload
    payload->effectId = global.effectId;
    payload->paletteId = global.paletteId;
    payload->brightness = global.brightness;
    payload->speed = global.speed;
    payload->hue = (uint16_t)global.hue << 8;  // 0-255 mapped to 0-65535
}
