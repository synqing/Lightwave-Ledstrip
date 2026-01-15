/**
 * applyAt Scheduler Queue
 * Version: 1.0
 * 
 * Bounded sorted queue for future-scheduled events.
 * Hard realtime: no allocations, predictable worst-case.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../proto/proto_constants.h"

// Command types for renderer
typedef enum {
    LW_CMD_SCENE_CHANGE,
    LW_CMD_PARAM_DELTA,
    LW_CMD_BEAT_TICK,
    LW_CMD_ZONE_UPDATE
} lw_cmd_type_t;

// Parameter delta flags (bitmask)
enum {
    LW_PARAM_F_BRIGHTNESS = 0x0001,
    LW_PARAM_F_SPEED      = 0x0002,
    LW_PARAM_F_HUE        = 0x0004,
    LW_PARAM_F_SATURATION = 0x0008,
    LW_PARAM_F_PALETTE    = 0x0010,
    LW_PARAM_F_INTENSITY  = 0x0020,
    LW_PARAM_F_COMPLEXITY = 0x0040,
    LW_PARAM_F_VARIATION  = 0x0080
};

// Zone update flags (bitmask)
enum {
    LW_ZONE_F_EFFECT      = 0x01,
    LW_ZONE_F_BRIGHTNESS  = 0x02,
    LW_ZONE_F_SPEED       = 0x04,
    LW_ZONE_F_PALETTE     = 0x08,
    LW_ZONE_F_BLEND       = 0x10
};

// Generic command structure
typedef struct {
    lw_cmd_type_t type;
    uint64_t applyAt_us;   // Local time to apply
    uint32_t trace_seq;    // UDP sequence (trace only)
    
    // Payload (union for type-specific data)
    union {
        struct {
            uint16_t effectId;
            uint16_t paletteId;
            uint8_t transition;
            uint16_t duration_ms;
        } scene;
        
        struct {
            uint8_t brightness;
            uint8_t speed;
            uint8_t paletteId;
            uint8_t intensity;
            uint8_t saturation;
            uint8_t complexity;
            uint8_t variation;
            uint16_t hue;
            uint16_t flags;
        } params;

        struct {
            uint16_t bpm_x100;
            uint16_t phase;
            uint8_t flags;
        } beat;

        struct {
            uint8_t zoneId;
            uint8_t flags;
            uint8_t effectId;
            uint8_t brightness;
            uint8_t speed;
            uint8_t paletteId;
            uint8_t blendMode;
        } zone;
    } data;
} lw_cmd_t;

// Bounded queue state
typedef struct {
    lw_cmd_t queue[LW_SCHEDULER_QUEUE_SIZE];
    uint16_t count;           // Current number of events
    uint16_t head;            // Next insertion point (sorted)
    
    // Overflow statistics
    uint32_t total_enqueued;
    uint32_t overflow_drops;
    uint32_t coalesced;
    uint32_t total_applied;
} lw_schedule_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize queue
void lw_schedule_init(lw_schedule_queue_t* q);

// Enqueue a command (called from UDP RX task)
// Returns true if enqueued, false if dropped
bool lw_schedule_enqueue(lw_schedule_queue_t* q, const lw_cmd_t* cmd);

// Extract all due events (called from render task at frame boundary)
// Returns number of events extracted
uint16_t lw_schedule_extract_due(
    lw_schedule_queue_t* q,
    uint64_t now_us,
    lw_cmd_t* out_cmds,
    uint16_t max_cmds
);

// Peek at next event time without removing
bool lw_schedule_peek_next(const lw_schedule_queue_t* q, uint64_t* applyAt_us);

// Get queue occupancy
uint16_t lw_schedule_count(const lw_schedule_queue_t* q);

// Check if queue is full
bool lw_schedule_is_full(const lw_schedule_queue_t* q);

// Clear all events (on time sync unlock, etc.)
void lw_schedule_clear(lw_schedule_queue_t* q);

// Get statistics
void lw_schedule_get_stats(
    const lw_schedule_queue_t* q,
    uint32_t* total_enqueued,
    uint32_t* overflow_drops,
    uint32_t* coalesced,
    uint32_t* total_applied
);

#ifdef __cplusplus
}
#endif
