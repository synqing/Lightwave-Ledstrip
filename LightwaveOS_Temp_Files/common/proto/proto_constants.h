/**
 * @file proto_constants.h
 * @brief Frozen Protocol Constants for K1.LightwaveOS
 * 
 * CRITICAL: These constants are FROZEN and must NEVER change without protocol version bump.
 * Any change to these values breaks compatibility between Hub and Nodes.
 */

#pragma once

#include <stdint.h>

// Protocol Version (MUST match between Hub and Node)
#define LW_PROTO_VER 1

// Network Configuration
#define LW_HUB_IP "192.168.4.1"
#define LW_WS_PATH "/ws"
#define LW_UDP_PORT 49152
#define LW_CTRL_HTTP_PORT 80

// Timing Constants (milliseconds)
#define LW_KEEPALIVE_PERIOD_MS 1000
#define LW_KEEPALIVE_TIMEOUT_MS 3500
#define LW_TS_PING_PERIOD_MS 200
#define LW_TS_LOCK_SAMPLES_N 10

// UDP Stream Configuration
#define LW_UDP_TICK_HZ 100
#define LW_UDP_TICK_PERIOD_US 10000  // 1/100Hz = 10ms

// Scheduling Constants (microseconds)
#define LW_APPLY_AHEAD_US 30000  // Hub schedules 30ms in the future

// Degradation/Failure Thresholds
#define LW_DRIFT_DEGRADED_US 3000
// In hub-controlled mode, UDP show packets may be intentionally silent (audio inactive),
// and time-sync pongs run at a much lower cadence. Keep liveness thresholds conservative.
#define LW_UDP_SILENCE_DEGRADED_MS 3000
#define LW_UDP_SILENCE_FAIL_MS 10000

// OTA Configuration
#define LW_OTA_NODE_TIMEOUT_S 180
#define LW_OTA_MAX_CONCURRENT 1  // Rolling updates only

// Scheduler Configuration
#define LW_SCHEDULER_QUEUE_SIZE 64
#define LW_MAX_DUE_PER_FRAME 16

// Node Limits
#define LW_MAX_NODES 8  // Max nodes supported by hub

// Cleanup Timeouts
// Cleanup delay for LOST nodes. Keep this generous so node identity (MAC→nodeId) is preserved
// across transient WiFi/WS reconnects (hub reboot recovery, AP roam, etc.).
#define LW_CLEANUP_TIMEOUT_MS 600000  // 10 minutes

// UDP Packet Limits
#define LW_UDP_MAX_PAYLOAD 512

// Token Constants
#define LW_TOKEN_INVALID 0
