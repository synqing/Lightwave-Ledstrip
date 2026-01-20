/**
 * @file HttpDeviceCodec.h
 * @brief JSON codec for HTTP device endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP device endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from device HTTP endpoints.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include "WsDeviceCodec.h"  // Reuse MAX_ERROR_MSG

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsDeviceCodec (same namespace)

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Device status response data
 */
struct HttpDeviceStatusData {
    uint32_t uptime;           // seconds
    uint32_t freeHeap;         // bytes
    uint32_t heapSize;         // bytes
    
    HttpDeviceStatusData() : uptime(0), freeHeap(0), heapSize(0) {}
};

/**
 * @brief Device status extended response data (includes renderer stats, network, etc.)
 */
struct HttpDeviceStatusExtendedData {
    uint32_t uptime;           // seconds
    uint32_t freeHeap;         // bytes
    uint32_t heapSize;         // bytes
    uint32_t cpuFreq;          // MHz
    uint16_t fps;              // frames per second
    uint8_t cpuPercent;        // CPU usage percentage
    uint32_t framesRendered;   // total frames rendered
    bool networkConnected;     // WiFi connected
    bool apMode;               // AP mode active
    const char* networkIP;     // IP address (if connected)
    int32_t networkRSSI;       // WiFi signal strength (if connected)
    size_t wsClients;          // WebSocket client count
    
    HttpDeviceStatusExtendedData() 
        : uptime(0), freeHeap(0), heapSize(0), cpuFreq(0), fps(0),
          cpuPercent(0), framesRendered(0), networkConnected(false),
          apMode(false), networkIP(nullptr), networkRSSI(0), wsClients(0) {}
};

/**
 * @brief Device info response data
 */
struct HttpDeviceInfoData {
    const char* firmware;      // Firmware version
    const char* board;         // Board name
    const char* sdk;           // SDK version
    uint32_t flashSize;        // Flash size in bytes
    uint32_t sketchSize;       // Sketch size in bytes
    uint32_t freeSketch;        // Free sketch space in bytes
    const char* architecture;  // Architecture description
    
    HttpDeviceInfoData() 
        : firmware("2.0.0"), board("ESP32-S3-DevKitC-1"), sdk(""), 
          flashSize(0), sketchSize(0), freeSketch(0), architecture("Actor System v2") {}
};

/**
 * @brief HTTP Device Command JSON Codec
 *
 * Single canonical parser for device HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpDeviceCodec {
public:
    // Encode functions (response building)
    static void encodeStatus(const HttpDeviceStatusData& data, JsonObject& obj);
    static void encodeStatusExtended(const HttpDeviceStatusExtendedData& data, JsonObject& obj);
    static void encodeInfo(const HttpDeviceInfoData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
