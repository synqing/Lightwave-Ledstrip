/**
 * @file node_fallback.h
 * @brief Node Fallback Policy
 * 
 * Handles graceful degradation when Hub connection is lost.
 */

#pragma once

#include <stdint.h>
#include "../../common/proto/proto_constants.h"

typedef enum {
    FALLBACK_IDLE,        // Normal operation
    FALLBACK_DEGRADED,    // Metrics bad but still receiving
    FALLBACK_ACTIVE       // Hub lost, holding stable
} fallback_state_t;

class NodeFallback {
public:
    NodeFallback();
    
    void init();
    void update(uint64_t lastUdp_ms, uint16_t loss_pct, int32_t drift_us);
    
    bool isActive() const { return state_ == FALLBACK_ACTIVE; }
    bool isDegraded() const { return state_ == FALLBACK_DEGRADED; }
    fallback_state_t getState() const { return state_; }
    
    // Get fallback scene (last stable or idle)
    void getFallbackScene(uint16_t* effectId, uint16_t* paletteId);
    
private:
    fallback_state_t state_;
    uint16_t lastStableEffectId_;
    uint16_t lastStablePaletteId_;
    uint64_t lastGoodUdp_ms_;
};
