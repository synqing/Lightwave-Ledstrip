// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpSystemCodec.h
 * @brief JSON codec for HTTP system endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP system endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from system HTTP endpoints.
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

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Health response data
 */
struct HttpSystemHealthData {
    uint32_t uptime;           // seconds
    uint32_t freeHeap;          // bytes
    uint32_t totalHeap;         // bytes
    uint32_t minFreeHeap;       // bytes
    bool hasRenderer;
    bool rendererRunning;
    float queueUtilization;
    uint8_t queueLength;
    uint8_t queueCapacity;
    float fps;
    float cpuPercent;
    bool hasWebSocket;
    uint8_t wsClients;
    uint8_t wsMaxClients;
    
    HttpSystemHealthData() 
        : uptime(0), freeHeap(0), totalHeap(0), minFreeHeap(0),
          hasRenderer(false), rendererRunning(false), queueUtilization(0.0f),
          queueLength(0), queueCapacity(32), fps(0.0f), cpuPercent(0.0f),
          hasWebSocket(false), wsClients(0), wsMaxClients(0) {}
};

/**
 * @brief API discovery response data
 */
struct HttpSystemApiDiscoveryData {
    const char* name;
    const char* apiVersion;
    const char* description;
    uint16_t ledsTotal;
    uint8_t strips;
    uint8_t centerPoint;
    uint8_t maxZones;
    
    HttpSystemApiDiscoveryData() 
        : name("LightwaveOS"), apiVersion(""), description(""), 
          ledsTotal(320), strips(2), centerPoint(79), maxZones(4) {}
};

/**
 * @brief HTTP System Command JSON Codec
 *
 * Single canonical parser for system HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpSystemCodec {
public:
    // Encode functions (response building)
    static void encodeHealth(const HttpSystemHealthData& data, JsonObject& obj);
    static void encodeApiDiscovery(const HttpSystemApiDiscoveryData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
