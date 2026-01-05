# Tick Scheduling System History

## Timeline

**Jan 3, 2026 (4 days ago):**
- Commit `937c882`: "refactor(core): rename Actor base class to Node"
- **System used:** Simple timeout-based tick scheduling
- **Implementation:** Wait for message with timeout = tickInterval, call onTick() on timeout

**Jan 4, 2026 (3 days ago):**
- Commit `db47998`: Merge commit - "queue saturation fix and zone.setBrightness field name fix"
- **Attempted change:** Deadline-based tick scheduling with `m_nextTick`
- **Result:** Incomplete merge - deadline logic not fully integrated

**Current (Jan 5, 2026):**
- **System used:** Timeout-based tick scheduling (reverted to original)
- **Status:** Queue saturation prevention added, but deadline-based scheduling removed

---

## Original System (Before Merge - Jan 3)

### Implementation:
```cpp
void Node::run() {
    while (!m_shutdownRequested) {
        Message msg;
        
        // Calculate wait time based on tick interval
        TickType_t waitTime;
        if (m_config.tickInterval > 0) {
            waitTime = m_config.tickInterval;  // e.g., 8ms for 120 FPS
        } else {
            waitTime = portMAX_DELAY;  // Wait forever if no tick needed
        }
        
        // Wait for a message
        BaseType_t received = xQueueReceive(m_queue, &msg, waitTime);
        
        if (received == pdTRUE) {
            // Message arrived - process it
            if (msg.type == MessageType::SHUTDOWN) {
                m_shutdownRequested = true;
                break;
            }
            m_messageCount++;
            onMessage(msg);
        } else {
            // Timeout occurred - tick is due
            if (m_config.tickInterval > 0) {
                onTick();  // Render frame
            }
        }
    }
}
```

### Characteristics:
- **Simple and predictable**
- **Tick happens exactly `tickInterval` after previous tick** (if no messages)
- **No queue saturation handling** - rapid inputs could fill queue
- **Timing drift possible** if `onTick()` duration varies

---

## Attempted New System (Merge Commit - Jan 4)

### What Was Attempted:
1. **Deadline-based scheduling** using `m_nextTick`
2. **Queue saturation prevention** (draining logic)
3. **Active tick checking** (check if due, run immediately)

### Implementation (from merge commit):
```cpp
void Node::run() {
    while (!m_shutdownRequested) {
        Message msg;
        
        // Tick scheduling: deadline-based
        bool ticked = false;
        if (m_config.tickInterval > 0) {
            if (m_nextTick == 0) {
                m_nextTick = xTaskGetTickCount() + m_config.tickInterval;
            }
            TickType_t now = xTaskGetTickCount();
            if (static_cast<int32_t>(now - m_nextTick) >= 0) {
                onTick();
                m_nextTick += m_config.tickInterval;
                ticked = true;
            }
        }
        
        // Queue saturation prevention
        uint8_t queueUtil = getQueueUtilization();
        if (queueUtil > DRAIN_THRESHOLD) {
            // Drain multiple messages...
        } else {
            // Normal operation with calculated wait time
            TickType_t waitTime = m_nextTick - now;
            BaseType_t received = xQueueReceive(m_queue, &msg, waitTime);
            // ...
        }
    }
}
```

### Why It Wasn't Completed:
- Merge was incomplete/haphazard
- `m_nextTick` member added but logic not fully integrated
- Code reverted to old behavior with comment preserving it

---

## Current System (After Cleanup - Jan 5)

### Implementation:
```cpp
void Node::run() {
    while (!m_shutdownRequested) {
        Message msg;
        
        // Queue saturation prevention (KEPT from merge)
        uint8_t queueUtil = getQueueUtilization();
        if (queueUtil > DRAIN_THRESHOLD) {
            // Drain up to 8 messages when queue > 50% full
            // ...
            continue;
        }
        
        // Normal operation: timeout-based (REVERTED to original)
        TickType_t waitTime;
        if (m_config.tickInterval > 0) {
            waitTime = m_config.tickInterval;
        } else {
            waitTime = portMAX_DELAY;
        }
        
        BaseType_t received = xQueueReceive(m_queue, &msg, waitTime);
        
        if (received == pdTRUE) {
            onMessage(msg);
        } else {
            // Timeout - tick is due
            if (m_config.tickInterval > 0) {
                onTick();
            }
        }
    }
}
```

### What Was Kept:
- ✅ **Queue saturation prevention** - drains messages when queue > 50% full
- ✅ **Timeout-based tick scheduling** - original proven behavior

### What Was Removed:
- ❌ **Deadline-based scheduling** - incomplete, unused `m_nextTick` removed
- ❌ **Active tick checking** - not implemented

---

## Conclusion

**Original system (3-4 days ago):** Simple timeout-based scheduling  
**Current system:** Timeout-based + queue saturation prevention  
**Attempted system (incomplete):** Deadline-based + queue saturation prevention

The current system is the **original proven system** with the **queue saturation fix** added. The deadline-based scheduling was never fully implemented and was correctly removed.

