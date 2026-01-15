/**
 * @file node_timesync.h
 * @brief Node Time Synchronization Manager
 * 
 * Wraps common/clock/timesync with node-specific integration.
 */

#pragma once

#include "../../common/clock/timesync.h"

class NodeTimeSync {
public:
    NodeTimeSync();
    
    void init();
    void processPong(uint32_t seq, uint64_t t1_us, uint64_t t2_us, uint64_t t3_us);
    
    bool isLocked() const { return lw_timesync_is_locked(&ts_); }
    int64_t getOffsetUs() const { return lw_timesync_get_offset_us(&ts_); }
    uint32_t getRttUs() const { return lw_timesync_get_rtt_us(&ts_); }
    int32_t getDriftUs() const;
    
    uint64_t hubToLocal(uint64_t hub_us) const {
        return lw_timesync_hub_to_local(&ts_, hub_us);
    }
    
    void tick();  // Called periodically
    
    lw_timesync_t* getState() { return &ts_; }  // For UDP TS access
    
private:
    lw_timesync_t ts_;
};
