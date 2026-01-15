/**
 * @file hub_clock.h
 * @brief Hub Authoritative Clock (Show Clock)
 * 
 * Provides monotonic hubNow_us for all Hub operations and UDP tick timestamps.
 */

#pragma once

#include <stdint.h>
#include "../../common/clock/monotonic.h"
#include "../../common/proto/proto_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t start_us;         // Reference for uptime (captured at init)
    uint64_t last_tick_us;     // Last UDP tick time (hub epoch, absolute)
    uint32_t tick_count;       // Total ticks sent
    uint32_t tick_overruns;    // Ticks that exceeded budget
    
    // Optional: show/musical timing
    uint16_t bpm_x100;         // BPM * 100 (e.g., 12000 = 120.00 BPM)
    uint8_t phase;             // 0-255 beat phase
    uint8_t flags;             // Downbeat, etc.
} hub_clock_t;

/**
 * @brief Initialize hub clock
 */
void hub_clock_init(hub_clock_t* clk);

/**
 * @brief Get current hub time in microseconds
 */
static inline uint64_t hub_clock_now_us(const hub_clock_t* clk) {
    (void)clk;
    // IMPORTANT: This must share the same epoch as time-sync (esp_timer_get_time/lw_monotonic_us),
    // otherwise nodes will schedule applyAt_us wildly in the past/future.
    return lw_monotonic_us();
}

/**
 * @brief Get hub uptime in microseconds (relative)
 */
static inline uint64_t hub_clock_uptime_us(const hub_clock_t* clk) {
    return lw_monotonic_us() - clk->start_us;
}

/**
 * @brief Mark UDP tick sent
 */
void hub_clock_tick(hub_clock_t* clk);

/**
 * @brief Get uptime in seconds
 */
static inline uint32_t hub_clock_uptime_s(const hub_clock_t* clk) {
    return (uint32_t)(hub_clock_uptime_us(clk) / 1000000ULL);
}

#ifdef __cplusplus
}
#endif
