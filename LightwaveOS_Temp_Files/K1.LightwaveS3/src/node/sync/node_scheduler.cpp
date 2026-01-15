/**
 * Node Scheduler Implementation
 */

#include "node_scheduler.h"
#include "../../common/util/logging.h"
#include <Arduino.h>

NodeScheduler::NodeScheduler() {
}

void NodeScheduler::init() {
    lw_schedule_init(&queue_);
    LW_LOGI("Scheduler initialized (capacity=%d)", LW_SCHEDULER_QUEUE_SIZE);
}

bool NodeScheduler::enqueue(const lw_cmd_t* cmd) {
    bool success = lw_schedule_enqueue(&queue_, cmd);
    
    if (!success) {
        static uint32_t lastWarn = 0;
        uint32_t now = millis();
        if (now - lastWarn > 1000) {
            LW_LOGW("Scheduler queue full or overflow");
            lastWarn = now;
        }
    }
    
    return success;
}

uint16_t NodeScheduler::extractDue(lw_cmd_t* out_cmds, uint16_t max_cmds) {
    uint64_t now_us = lw_monotonic_us();
    return lw_schedule_extract_due(&queue_, now_us, out_cmds, max_cmds);
}
