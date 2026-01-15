/**
 * @file node_scheduler.h
 * @brief Node applyAt Scheduler
 * 
 * Wraps common/clock/schedule_queue with node-specific integration.
 * Provides frame-boundary event application.
 */

#pragma once

#include "../../common/clock/schedule_queue.h"
#include "../../common/clock/monotonic.h"

class NodeScheduler {
public:
    NodeScheduler();
    
    void init();
    
    // Enqueue command (called from UDP RX task)
    bool enqueue(const lw_cmd_t* cmd);
    
    // Extract due events at frame boundary (called from render task)
    uint16_t extractDue(lw_cmd_t* out_cmds, uint16_t max_cmds);
    
    // Query
    uint16_t getCount() const { return lw_schedule_count(&queue_); }
    bool isFull() const { return lw_schedule_is_full(&queue_); }
    
    // Statistics
    void getStats(uint32_t* enqueued, uint32_t* drops, uint32_t* coalesced, uint32_t* applied) const {
        lw_schedule_get_stats(&queue_, enqueued, drops, coalesced, applied);
    }
    
    // Clear all (on time sync unlock)
    void clear() { lw_schedule_clear(&queue_); }
    
private:
    lw_schedule_queue_t queue_;
};
