/**
 * Hub Authoritative Clock Implementation
 */

#include "hub_clock.h"
#include "../../common/util/logging.h"

void hub_clock_init(hub_clock_t* clk) {
    clk->start_us = lw_monotonic_us();
    clk->last_tick_us = 0;
    clk->tick_count = 0;
    clk->tick_overruns = 0;
    clk->bpm_x100 = 12000;  // Default 120 BPM
    clk->phase = 0;
    clk->flags = 0;
    
    LW_LOGI("Hub clock initialised (start_us=%llu)", (unsigned long long)clk->start_us);
}

void hub_clock_tick(hub_clock_t* clk) {
    uint64_t now = hub_clock_now_us(clk);
    clk->tick_count++;
    
    // Check for tick overrun
    if (clk->last_tick_us > 0) {
        uint64_t since_last = now - clk->last_tick_us;
        if (since_last > LW_UDP_TICK_PERIOD_US * 2) {
            clk->tick_overruns++;
            LW_LOGW("Tick overrun: %llu us since last (expected %d us)",
                    since_last, LW_UDP_TICK_PERIOD_US);
        }
    }
    
    clk->last_tick_us = now;
}
