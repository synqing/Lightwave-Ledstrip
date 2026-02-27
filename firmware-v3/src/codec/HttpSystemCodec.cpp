// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpSystemCodec.cpp
 * @brief HTTP system codec implementation
 *
 * Single canonical JSON parser for system HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpSystemCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Encode Functions
// ============================================================================

void HttpSystemCodec::encodeHealth(const HttpSystemHealthData& data, JsonObject& obj) {
    obj["uptime"] = data.uptime;
    obj["freeHeap"] = data.freeHeap;
    obj["totalHeap"] = data.totalHeap;
    obj["minFreeHeap"] = data.minFreeHeap;
    
    if (data.hasRenderer) {
        obj["rendererRunning"] = data.rendererRunning;
        obj["queueUtilization"] = data.queueUtilization;
        obj["queueLength"] = data.queueLength;
        obj["queueCapacity"] = data.queueCapacity;
        obj["fps"] = data.fps;
        obj["cpuPercent"] = data.cpuPercent;
    }
    
    if (data.hasWebSocket) {
        obj["wsClients"] = data.wsClients;
        obj["wsMaxClients"] = data.wsMaxClients;
    }
}

void HttpSystemCodec::encodeApiDiscovery(const HttpSystemApiDiscoveryData& data, JsonObject& obj) {
    obj["name"] = data.name;
    obj["apiVersion"] = data.apiVersion;
    obj["description"] = data.description;
    
    JsonObject hw = obj["hardware"].to<JsonObject>();
    hw["ledsTotal"] = data.ledsTotal;
    hw["strips"] = data.strips;
    hw["centerPoint"] = data.centerPoint;
    hw["maxZones"] = data.maxZones;
    
    // HATEOAS links are added by handler (they're static)
}

} // namespace codec
} // namespace lightwaveos
