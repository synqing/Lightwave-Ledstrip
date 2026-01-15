/**
 * UDP Time Synchronization Protocol
 * 
 * Dedicated UDP port for low-latency ping/pong time sync.
 * Binary protocol to minimize overhead and eliminate WebSocket queueing.
 */

#ifndef LW_TS_UDP_H
#define LW_TS_UDP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Dedicated time-sync UDP port (separate from fanout traffic)
#define LW_TS_UDP_PORT 49154

// Protocol version
#define LW_TS_PROTO_VER 1

// Message types
typedef enum {
    LW_TS_MSG_PING = 1,
    LW_TS_MSG_PONG = 2
} lw_ts_msg_type_t;

// Node → Hub PING
#pragma pack(push, 1)
typedef struct {
    uint8_t  proto;      // LW_TS_PROTO_VER
    uint8_t  type;       // LW_TS_MSG_PING
    uint16_t reserved;
    uint32_t seq;        // Sequence number
    uint32_t tokenHash;  // Session token hash for validation
    uint64_t t1_us;      // Node send time (lw_monotonic_us)
} lw_ts_ping_t;
#pragma pack(pop)

// Hub → Node PONG
#pragma pack(push, 1)
typedef struct {
    uint8_t  proto;      // LW_TS_PROTO_VER
    uint8_t  type;       // LW_TS_MSG_PONG
    uint16_t reserved;
    uint32_t seq;        // Echoed from ping
    uint32_t tokenHash;  // Echoed for validation
    uint64_t t1_us;      // Echoed from ping
    uint64_t t2_us;      // Hub receive time
    uint64_t t3_us;      // Hub send time (just before sendto)
} lw_ts_pong_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // LW_TS_UDP_H
