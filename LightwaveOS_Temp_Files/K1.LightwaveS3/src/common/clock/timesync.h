/**
 * Time Synchronization Algorithm
 * Version: 1.0
 * 
 * Node-side time sync estimator for hub→local clock mapping.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../proto/proto_constants.h"

typedef enum {
    LW_TS_UNLOCKED,       // Not yet locked
    LW_TS_LOCKING,        // Accumulating samples
    LW_TS_LOCKED,         // Stable lock achieved
    LW_TS_DEGRADED        // Was locked, now unstable
} lw_ts_state_t;

typedef struct {
    // State
    lw_ts_state_t state;
    
    // Offset estimation (hub→local mapping)
    int64_t offset_us;          // offset_us = hubTime - localTime
    
    // RTT tracking
    uint32_t rtt_us;            // Smoothed RTT
    uint32_t rtt_variance_us;   // RTT variance for stability check
    
    // Lock criteria
    uint16_t good_samples;      // Count of good samples received
    uint64_t last_ping_us;      // Last ping send time
    uint64_t last_pong_us;      // Last pong receive time
    
    // Drift tracking
    int64_t drift_rate_us_per_s;  // Measured drift rate
    uint64_t last_drift_check_us;
    
    // Diagnostics
    uint32_t total_pings;
    uint32_t total_pongs;
    uint32_t missed_pongs;
    uint32_t unlock_count;
} lw_timesync_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize time sync state
void lw_timesync_init(lw_timesync_t* ts);

// Reset to unlocked state
void lw_timesync_reset(lw_timesync_t* ts);

// Process a pong response (4-timestamp NTP-style)
// t1_us: ping send time (local)
// t2_us: hub receive time
// t3_us: hub send time
// t4_us: pong receive time (local)
void lw_timesync_process_pong(
    lw_timesync_t* ts,
    uint64_t t1_us,
    uint64_t t2_us,
    uint64_t t3_us,
    uint64_t t4_us
);

// Convert hub time to local time
int64_t lw_timesync_hub_to_local(const lw_timesync_t* ts, uint64_t hub_us);

// Convert local time to hub time
int64_t lw_timesync_local_to_hub(const lw_timesync_t* ts, uint64_t local_us);

// Check if time sync is locked
bool lw_timesync_is_locked(const lw_timesync_t* ts);

// Check if time sync is degraded
bool lw_timesync_is_degraded(const lw_timesync_t* ts);

// Get current offset
int64_t lw_timesync_get_offset_us(const lw_timesync_t* ts);

// Get current RTT
uint32_t lw_timesync_get_rtt_us(const lw_timesync_t* ts);

// Periodic maintenance (call every second)
void lw_timesync_tick(lw_timesync_t* ts, uint64_t now_local_us);

// State string for debugging
const char* lw_timesync_state_str(lw_ts_state_t state);

#ifdef __cplusplus
}
#endif
