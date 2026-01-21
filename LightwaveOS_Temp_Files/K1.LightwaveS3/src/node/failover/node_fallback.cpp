/**
 * Node Fallback Policy Implementation
 */

#include "node_fallback.h"
#include "../../common/util/logging.h"
#include <Arduino.h>

NodeFallback::NodeFallback()
    : state_(FALLBACK_IDLE), lastStableEffectId_(0), lastStablePaletteId_(0),
      lastGoodUdp_ms_(0) {
}

void NodeFallback::init() {
    state_ = FALLBACK_IDLE;
    lastGoodUdp_ms_ = millis();
    LW_LOGI("Fallback policy initialized");
}

void NodeFallback::update(uint64_t lastUdp_ms, uint16_t loss_pct, int32_t drift_us) {
    uint32_t now = millis();
    uint32_t since_last_udp = (lastUdp_ms > 0) ? (now - lastUdp_ms) : 0;
    
    // Check for UDP silence thresholds
    if (since_last_udp > LW_UDP_SILENCE_FAIL_MS) {
        if (state_ != FALLBACK_ACTIVE) {
            LW_LOGW("Entering FALLBACK_ACTIVE: UDP silence %lu ms", since_last_udp);
            state_ = FALLBACK_ACTIVE;
        }
    } else if (since_last_udp > LW_UDP_SILENCE_DEGRADED_MS || 
               loss_pct > 200 || 
               abs(drift_us) > LW_DRIFT_DEGRADED_US) {
        if (state_ == FALLBACK_IDLE) {
            LW_LOGW("Entering FALLBACK_DEGRADED: silence=%lu ms, loss=%d.%02d%%, drift=%ld us",
                    since_last_udp, loss_pct / 100, loss_pct % 100, drift_us);
            state_ = FALLBACK_DEGRADED;
        }
    } else {
        // Metrics good, return to normal
        if (state_ != FALLBACK_IDLE) {
            LW_LOGI("Returning to normal operation");
            state_ = FALLBACK_IDLE;
        }
        lastGoodUdp_ms_ = now;
    }
}

void NodeFallback::getFallbackScene(uint16_t* effectId, uint16_t* paletteId) {
    if (lastStableEffectId_ > 0) {
        *effectId = lastStableEffectId_;
        *paletteId = lastStablePaletteId_;
    } else {
        // Default idle effect
        *effectId = 0;
        *paletteId = 0;
    }
}
