/**
 * @file ws_messages.h
 * @brief WebSocket Message Structures and JSON Helpers
 * 
 * Defines all WS control plane messages: HELLO, WELCOME, KEEPALIVE, TS_PING, TS_PONG, OTA_UPDATE, OTA_STATUS
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "proto_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Message Types
typedef enum {
    LW_MSG_HELLO = 0,
    LW_MSG_WELCOME,
    LW_MSG_KEEPALIVE,
    LW_MSG_TS_PING,
    LW_MSG_TS_PONG,
    LW_MSG_OTA_UPDATE,
    LW_MSG_OTA_STATUS,
} lw_msg_type_t;

// Node Capabilities
typedef struct {
    bool udp;
    bool ota;
    bool clock;
} lw_caps_t;

// Node Topology
typedef struct {
    uint16_t leds;
    uint8_t channels;
} lw_topo_t;

// HELLO Message (Node → Hub)
typedef struct {
    char mac[18];  // "AA:BB:CC:DD:EE:FF"
    char fw[32];   // "k1-v2.X.Y"
    lw_caps_t caps;
    lw_topo_t topo;
} lw_msg_hello_t;

// WELCOME Message (Hub → Node)
typedef struct {
    uint8_t nodeId;
    char token[64];
    uint16_t udpPort;
    uint64_t hubEpoch_us;
} lw_msg_welcome_t;

// KEEPALIVE Message (Node → Hub)
typedef struct {
    uint8_t nodeId;
    char token[64];
    int8_t rssi;
    uint16_t loss_pct;  // 0-10000 (0.01% resolution)
    int32_t drift_us;
    uint32_t uptime_s;
} lw_msg_keepalive_t;

// TS_PING Message (Node → Hub)
typedef struct {
    uint8_t nodeId;
    char token[64];
    uint32_t seq;
    uint64_t t1_us;
} lw_msg_ts_ping_t;

// TS_PONG Message (Hub → Node)
typedef struct {
    uint8_t nodeId;
    uint32_t seq;
    uint64_t t1_us;
    uint64_t t2_us;
} lw_msg_ts_pong_t;

// OTA_UPDATE Message (Hub → Node)
typedef struct {
    uint8_t nodeId;
    char token[64];
    char version[32];
    char url[256];
    char sha256[65];
} lw_msg_ota_update_t;

// OTA_STATUS Message (Node → Hub)
typedef struct {
    uint8_t nodeId;
    char token[64];
    char state[32];  // "downloading", "verifying", "applying", "rebooting", "failed"
    uint8_t pct;
} lw_msg_ota_status_t;

// JSON Encoding/Decoding Helpers
// These are platform-specific implementations (Arduino JSON, cJSON, etc.)
// Implement in your firmware project

/**
 * @brief Encode HELLO message to JSON string
 * @param msg Message structure
 * @param json_out Output buffer
 * @param json_len Output buffer size
 * @return true on success
 */
bool lw_msg_hello_to_json(const lw_msg_hello_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode HELLO message from JSON string
 * @param json Input JSON string
 * @param msg Output message structure
 * @return true on success
 */
bool lw_msg_hello_from_json(const char* json, lw_msg_hello_t* msg);

/**
 * @brief Encode WELCOME message to JSON string
 */
bool lw_msg_welcome_to_json(const lw_msg_welcome_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode WELCOME message from JSON string
 */
bool lw_msg_welcome_from_json(const char* json, lw_msg_welcome_t* msg);

/**
 * @brief Encode KEEPALIVE message to JSON string
 */
bool lw_msg_keepalive_to_json(const lw_msg_keepalive_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode KEEPALIVE message from JSON string
 */
bool lw_msg_keepalive_from_json(const char* json, lw_msg_keepalive_t* msg);

/**
 * @brief Encode TS_PING message to JSON string
 */
bool lw_msg_ts_ping_to_json(const lw_msg_ts_ping_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode TS_PING message from JSON string
 */
bool lw_msg_ts_ping_from_json(const char* json, lw_msg_ts_ping_t* msg);

/**
 * @brief Encode TS_PONG message to JSON string
 */
bool lw_msg_ts_pong_to_json(const lw_msg_ts_pong_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode TS_PONG message from JSON string
 */
bool lw_msg_ts_pong_from_json(const char* json, lw_msg_ts_pong_t* msg);

/**
 * @brief Encode OTA_UPDATE message to JSON string
 */
bool lw_msg_ota_update_to_json(const lw_msg_ota_update_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode OTA_UPDATE message from JSON string
 */
bool lw_msg_ota_update_from_json(const char* json, lw_msg_ota_update_t* msg);

/**
 * @brief Encode OTA_STATUS message to JSON string
 */
bool lw_msg_ota_status_to_json(const lw_msg_ota_status_t* msg, char* json_out, size_t json_len);

/**
 * @brief Decode OTA_STATUS message from JSON string
 */
bool lw_msg_ota_status_from_json(const char* json, lw_msg_ota_status_t* msg);

#ifdef __cplusplus
}
#endif
