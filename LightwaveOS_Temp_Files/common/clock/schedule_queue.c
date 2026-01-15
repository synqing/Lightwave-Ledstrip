/**
 * applyAt Scheduler Queue Implementation
 */

#include "schedule_queue.h"
#include <string.h>

void lw_schedule_init(lw_schedule_queue_t* q) {
    memset(q, 0, sizeof(lw_schedule_queue_t));
    q->count = 0;
    q->head = 0;
}

bool lw_schedule_enqueue(lw_schedule_queue_t* q, const lw_cmd_t* cmd) {
    q->total_enqueued++;
    
    // Check for overflow
    if (q->count >= LW_SCHEDULER_QUEUE_SIZE) {
        // Queue full - attempt coalescing to keep the newest intent.
        for (uint16_t i = 0; i < q->count; i++) {
            if (q->queue[i].type != cmd->type) continue;

            // Zone updates coalesce per-zone.
            if (cmd->type == LW_CMD_ZONE_UPDATE) {
                if (q->queue[i].data.zone.zoneId != cmd->data.zone.zoneId) continue;
            }

            q->queue[i] = *cmd;
            q->coalesced++;
            return true;
        }
        
        // Can't coalesce, drop
        q->overflow_drops++;
        return false;
    }
    
    // Find insertion point (maintain sorted order by applyAt_us)
    uint16_t insert_pos = 0;
    for (uint16_t i = 0; i < q->count; i++) {
        if (cmd->applyAt_us < q->queue[i].applyAt_us) {
            break;
        }
        insert_pos++;
    }
    
    // Shift elements to make space
    if (insert_pos < q->count) {
        memmove(&q->queue[insert_pos + 1], &q->queue[insert_pos],
                (q->count - insert_pos) * sizeof(lw_cmd_t));
    }
    
    // Insert new command
    q->queue[insert_pos] = *cmd;
    q->count++;
    
    return true;
}

uint16_t lw_schedule_extract_due(
    lw_schedule_queue_t* q,
    uint64_t now_us,
    lw_cmd_t* out_cmds,
    uint16_t max_cmds
) {
    uint16_t extracted = 0;
    
    // Extract commands from head while they're due and we haven't hit the limit
    while (extracted < max_cmds && q->count > 0 && q->queue[0].applyAt_us <= now_us) {
        out_cmds[extracted++] = q->queue[0];
        
        // Shift remaining elements
        q->count--;
        if (q->count > 0) {
            memmove(&q->queue[0], &q->queue[1], q->count * sizeof(lw_cmd_t));
        }
        
        q->total_applied++;
    }
    
    return extracted;
}

bool lw_schedule_peek_next(const lw_schedule_queue_t* q, uint64_t* applyAt_us) {
    if (q->count == 0) {
        return false;
    }
    
    *applyAt_us = q->queue[0].applyAt_us;
    return true;
}

uint16_t lw_schedule_count(const lw_schedule_queue_t* q) {
    return q->count;
}

bool lw_schedule_is_full(const lw_schedule_queue_t* q) {
    return q->count >= LW_SCHEDULER_QUEUE_SIZE;
}

void lw_schedule_clear(lw_schedule_queue_t* q) {
    q->count = 0;
    q->head = 0;
}

void lw_schedule_get_stats(
    const lw_schedule_queue_t* q,
    uint32_t* total_enqueued,
    uint32_t* overflow_drops,
    uint32_t* coalesced,
    uint32_t* total_applied
) {
    if (total_enqueued) *total_enqueued = q->total_enqueued;
    if (overflow_drops) *overflow_drops = q->overflow_drops;
    if (coalesced) *coalesced = q->coalesced;
    if (total_applied) *total_applied = q->total_applied;
}
