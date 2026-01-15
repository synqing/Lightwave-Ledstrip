/**
 * Node Time Sync Manager Implementation
 */

#include "node_timesync.h"
#include "../../common/util/logging.h"
#include "../../common/clock/monotonic.h"
#include <Arduino.h>

NodeTimeSync::NodeTimeSync() {
}

void NodeTimeSync::init() {
    lw_timesync_init(&ts_);
    LW_LOGI("Time sync initialized");
}

void NodeTimeSync::processPong(uint32_t seq, uint64_t t1_us, uint64_t t2_us, uint64_t t3_us) {
    uint64_t t4_us = lw_monotonic_us();  // Node receive time (local)
    
    bool was_locked = lw_timesync_is_locked(&ts_);
    lw_timesync_process_pong(&ts_, t1_us, t2_us, t3_us, t4_us);
    bool is_locked = lw_timesync_is_locked(&ts_);
    
    // Periodic summary only (avoid serial flood; enable deeper tracing via dedicated flags if needed)
    static uint32_t lastSummaryMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastSummaryMs >= 5000 && is_locked) {
        LW_LOGI("Time sync: offset=%lld us, rtt=%lu us, variance=%lu us, samples=%u",
                getOffsetUs(), getRttUs(), ts_.rtt_variance_us, ts_.good_samples);
        lastSummaryMs = nowMs;
    }
    
    if (!was_locked && is_locked) {
        LW_LOGI("Time sync LOCKED: offset=%lld us, delay=%lu us",
                getOffsetUs(), getRttUs());
    }
}

int32_t NodeTimeSync::getDriftUs() const {
    // Offset is expected to be large (boot-time differences). "Drift" should reflect
    // instability (rate) rather than absolute offset. Until we have a rate estimate,
    // report 0 and let `lw_timesync_is_locked()`/variance act as the stability signal.
    return 0;
}

void NodeTimeSync::tick() {
    uint64_t now = lw_monotonic_us();
    lw_timesync_tick(&ts_, now);
}
