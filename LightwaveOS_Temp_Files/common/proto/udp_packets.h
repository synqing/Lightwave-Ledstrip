/**
 * @file udp_packets.h
 * @brief UDP Binary Packet Structures (100Hz Stream Plane)
 * 
 * Defines packed binary UDP packet format for high-frequency streaming.
 * CRITICAL: All fields are network byte order (big-endian).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "proto_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Payload Types
typedef enum {
    LW_UDP_PARAM_DELTA = 0x01,
    LW_UDP_BEAT_TICK = 0x02,
    LW_UDP_SCENE_CHANGE = 0x03,
    LW_UDP_HEARTBEAT = 0x04,
    LW_UDP_RESERVED = 0x05
} lw_udp_msg_type_t;

// UDP Header (packed, fixed-size, 28 bytes)
// Note: Actual size is 28 bytes for efficiency, not 32
#pragma pack(push, 1)
typedef struct {
    uint8_t proto;           // = LW_PROTO_VER
    uint8_t msgType;         // lw_udp_msg_type_t
    uint16_t payloadLen;     // bytes
    uint32_t seq;            // increments every tick
    uint32_t tokenHash;      // 32-bit derived from session token
    uint64_t hubNow_us;      // authoritative hub time
    uint64_t applyAt_us;     // hubNow_us + LW_APPLY_AHEAD_US
} lw_udp_hdr_t;
#pragma pack(pop)

// Static assert to ensure header is exactly 28 bytes
_Static_assert(sizeof(lw_udp_hdr_t) == 28, "UDP header must be 28 bytes");

// PARAM_DELTA Payload
#pragma pack(push, 1)
typedef struct {
    uint16_t effectId;
    uint16_t paletteId;
    uint8_t brightness;
    uint8_t speed;
    uint16_t hue;
} lw_udp_param_delta_t;
#pragma pack(pop)

// BEAT_TICK Payload
#pragma pack(push, 1)
typedef struct {
    uint16_t bpm_x100;       // BPM * 100
    uint8_t phase;           // 0-255
    uint8_t flags;           // downbeat, etc.
} lw_udp_beat_tick_t;
#pragma pack(pop)

// SCENE_CHANGE Payload
#pragma pack(push, 1)
typedef struct {
    uint16_t effectId;
    uint16_t paletteId;
} lw_udp_scene_change_t;
#pragma pack(pop)

// ============================================================================
// Endian helpers (payloads are network byte order / big-endian)
// ============================================================================

static inline void lw_udp_param_delta_ntoh(lw_udp_param_delta_t* p) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p->effectId = __builtin_bswap16(p->effectId);
    p->paletteId = __builtin_bswap16(p->paletteId);
    p->hue = __builtin_bswap16(p->hue);
    #endif
}

static inline void lw_udp_param_delta_hton(lw_udp_param_delta_t* p) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p->effectId = __builtin_bswap16(p->effectId);
    p->paletteId = __builtin_bswap16(p->paletteId);
    p->hue = __builtin_bswap16(p->hue);
    #endif
}

static inline void lw_udp_scene_change_ntoh(lw_udp_scene_change_t* p) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p->effectId = __builtin_bswap16(p->effectId);
    p->paletteId = __builtin_bswap16(p->paletteId);
    #endif
}

static inline void lw_udp_scene_change_hton(lw_udp_scene_change_t* p) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p->effectId = __builtin_bswap16(p->effectId);
    p->paletteId = __builtin_bswap16(p->paletteId);
    #endif
}

static inline void lw_udp_beat_tick_ntoh(lw_udp_beat_tick_t* p) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p->bpm_x100 = __builtin_bswap16(p->bpm_x100);
    #endif
}

static inline void lw_udp_beat_tick_hton(lw_udp_beat_tick_t* p) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    p->bpm_x100 = __builtin_bswap16(p->bpm_x100);
    #endif
}

/**
 * @brief Compute 32-bit hash of session token (FNV-1a)
 * @param token Session token string
 * @return 32-bit hash
 */
static inline uint32_t lw_token_hash32(const char* token) {
    if (!token) return 0;
    
    uint32_t hash = 2166136261u;  // FNV offset basis
    
    while (*token) {
        hash ^= (uint32_t)(*token++);
        hash *= 16777619u;  // FNV prime
    }
    
    return hash;
}

/**
 * @brief Validate UDP header
 * @param hdr UDP header
 * @param packet_len Total packet length (header + payload)
 * @return true if valid
 */
static inline bool lw_udp_validate_header(const lw_udp_hdr_t* hdr, size_t packet_len) {
    if (!hdr) return false;
    
    // Check protocol version
    if (hdr->proto != LW_PROTO_VER) return false;
    
    // Check message type
    if (hdr->msgType < LW_UDP_PARAM_DELTA || hdr->msgType > LW_UDP_RESERVED) return false;
    
    // Check payload length
    if (hdr->payloadLen > LW_UDP_MAX_PAYLOAD) return false;
    
    // Check total packet length
    if (packet_len < sizeof(lw_udp_hdr_t) + hdr->payloadLen) return false;
    
    return true;
}

/**
 * @brief Convert UDP header from network byte order to host byte order
 * @param hdr UDP header (modified in place)
 */
static inline void lw_udp_hdr_ntoh(lw_udp_hdr_t* hdr) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    hdr->payloadLen = __builtin_bswap16(hdr->payloadLen);
    hdr->seq = __builtin_bswap32(hdr->seq);
    hdr->tokenHash = __builtin_bswap32(hdr->tokenHash);
    hdr->hubNow_us = __builtin_bswap64(hdr->hubNow_us);
    hdr->applyAt_us = __builtin_bswap64(hdr->applyAt_us);
    #endif
}

/**
 * @brief Convert UDP header from host byte order to network byte order
 * @param hdr UDP header (modified in place)
 */
static inline void lw_udp_hdr_hton(lw_udp_hdr_t* hdr) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    hdr->payloadLen = __builtin_bswap16(hdr->payloadLen);
    hdr->seq = __builtin_bswap32(hdr->seq);
    hdr->tokenHash = __builtin_bswap32(hdr->tokenHash);
    hdr->hubNow_us = __builtin_bswap64(hdr->hubNow_us);
    hdr->applyAt_us = __builtin_bswap64(hdr->applyAt_us);
    #endif
}

#ifdef __cplusplus
}
#endif
