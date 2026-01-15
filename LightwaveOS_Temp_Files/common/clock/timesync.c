/**
 * Time Synchronization Implementation
 */

#include "timesync.h"
#include "monotonic.h"
#include <string.h>
#include <math.h>

// Per-sample logging is extremely noisy; keep it off by default.
#ifndef LW_TIMESYNC_VERBOSE
#define LW_TIMESYNC_VERBOSE 0
#endif

// LPF alpha for offset smoothing (0.2 = 20% new, 80% old)
#define LPF_ALPHA 0.2f

// RTT variance threshold for lock (microseconds)
#define RTT_VARIANCE_THRESHOLD_US 5000  // Back to 5ms now that UDP eliminates WS queueing

// Maximum RTT considered valid (milliseconds)
#define MAX_VALID_RTT_MS 50  // Back to 50ms now that UDP eliminates WS queueing

void lw_timesync_init(lw_timesync_t* ts) {
    memset(ts, 0, sizeof(lw_timesync_t));
    ts->state = LW_TS_UNLOCKED;
    ts->offset_us = 0;
    ts->rtt_us = 0;
    ts->rtt_variance_us = 0;
    ts->good_samples = 0;
}

void lw_timesync_reset(lw_timesync_t* ts) {
    ts->state = LW_TS_UNLOCKED;
    ts->offset_us = 0;
    ts->rtt_us = 0;
    ts->rtt_variance_us = 0;
    ts->good_samples = 0;
    ts->unlock_count++;
}

void lw_timesync_process_pong(
    lw_timesync_t* ts,
    uint64_t t1_us,
    uint64_t t2_us,
    uint64_t t3_us,
    uint64_t t4_us
) {
    int64_t t1 = (int64_t)t1_us;
    int64_t t2 = (int64_t)t2_us;
    int64_t t3 = (int64_t)t3_us;
    int64_t t4 = (int64_t)t4_us;

    // RFC 5905 delay and offset
    int64_t delay_us  = (t4 - t1) - (t3 - t2);
    if (delay_us < 0 || delay_us > (MAX_VALID_RTT_MS * 1000)) {
#if LW_TIMESYNC_VERBOSE
        printf("[TIMESYNC] REJECTED: delay=%lld us (gate=%d ms), t1=%lld, t2=%lld, t3=%lld, t4=%lld\n",
               (long long)delay_us, (int)MAX_VALID_RTT_MS,
               (long long)t1, (long long)t2, (long long)t3, (long long)t4);
#endif
        return;
    }

    int64_t offset_est = ((t2 - t1) + (t3 - t4)) / 2;

    const float a = 0.8f;
    const float b = 0.2f;

    // Low-pass filter with proper type handling
    float prev_delay = (float)ts->rtt_us;
    float new_offset = (float)ts->offset_us * a + (float)offset_est * b;
    float new_rtt = prev_delay * a + (float)delay_us * b;
    
    float dev = fabsf((float)delay_us - prev_delay);
    float new_variance = (float)ts->rtt_variance_us * a + dev * b;
    
    // Update struct with converted values
    ts->offset_us = (int64_t)new_offset;
    ts->rtt_us = (uint32_t)new_rtt;
    ts->rtt_variance_us = (uint32_t)new_variance;

    ts->good_samples++;
    ts->total_pongs++;
    ts->last_pong_us = t4_us;

#if LW_TIMESYNC_VERBOSE
    printf("[TIMESYNC] ACCEPTED: delay=%lld us, offset=%lld us, samples=%u\n",
           (long long)delay_us, (long long)offset_est, ts->good_samples);
#endif

    // State transitions
    if (ts->state == LW_TS_UNLOCKED || ts->state == LW_TS_LOCKING) {
        if (ts->good_samples >= LW_TS_LOCK_SAMPLES_N &&
            ts->rtt_variance_us < RTT_VARIANCE_THRESHOLD_US) {
            ts->state = LW_TS_LOCKED;
        } else {
            ts->state = LW_TS_LOCKING;
        }
    }
    // LOCKED state stays locked unless tick() detects instability
}

int64_t lw_timesync_hub_to_local(const lw_timesync_t* ts, uint64_t hub_us) {
    return (int64_t)hub_us - ts->offset_us;
}

int64_t lw_timesync_local_to_hub(const lw_timesync_t* ts, uint64_t local_us) {
    return (int64_t)local_us + ts->offset_us;
}

bool lw_timesync_is_locked(const lw_timesync_t* ts) {
    return ts->state == LW_TS_LOCKED;
}

bool lw_timesync_is_degraded(const lw_timesync_t* ts) {
    return ts->state == LW_TS_DEGRADED;
}

int64_t lw_timesync_get_offset_us(const lw_timesync_t* ts) {
    return ts->offset_us;
}

uint32_t lw_timesync_get_rtt_us(const lw_timesync_t* ts) {
    return ts->rtt_us;
}

void lw_timesync_tick(lw_timesync_t* ts, uint64_t now_local_us) {
    // Check for pong timeout
    if (ts->last_pong_us > 0) {
        uint64_t since_last_pong = now_local_us - ts->last_pong_us;
        if (since_last_pong > (LW_KEEPALIVE_TIMEOUT_MS * 1000ULL)) {
            // No pongs for > KEEPALIVE_TIMEOUT, unlock
            if (ts->state == LW_TS_LOCKED) {
                ts->state = LW_TS_DEGRADED;
            }
        }
    }
    
    // Check RTT variance for stability
    if (ts->state == LW_TS_LOCKED && ts->rtt_variance_us > RTT_VARIANCE_THRESHOLD_US * 2) {
        ts->state = LW_TS_DEGRADED;
    }
}

const char* lw_timesync_state_str(lw_ts_state_t state) {
    switch (state) {
        case LW_TS_UNLOCKED: return "UNLOCKED";
        case LW_TS_LOCKING: return "LOCKING";
        case LW_TS_LOCKED: return "LOCKED";
        case LW_TS_DEGRADED: return "DEGRADED";
        default: return "UNKNOWN";
    }
}
