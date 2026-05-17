/**
 * @file HttpDeviceCodec.cpp
 * @brief HTTP device codec implementation
 *
 * Single canonical JSON parser for device HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpDeviceCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Encode Functions
// ============================================================================

void HttpDeviceCodec::encodeStatus(const HttpDeviceStatusData& data, JsonObject& obj) {
    obj["uptime"] = data.uptime;
    obj["freeHeap"] = data.freeHeap;
    obj["heapSize"] = data.heapSize;
}

void HttpDeviceCodec::encodeStatusExtended(const HttpDeviceStatusExtendedData& data, JsonObject& obj) {
    obj["uptime"] = data.uptime;
    obj["freeHeap"] = data.freeHeap;
    obj["heapSize"] = data.heapSize;
    obj["cpuFreq"] = data.cpuFreq;
    obj["fps"] = data.fps;
    obj["cpuPercent"] = data.cpuPercent;
    obj["frameBudgetPercent"] = data.cpuPercent;
    obj["framesRendered"] = data.framesRendered;

    JsonObject ledTransport = obj["ledTransport"].to<JsonObject>();
    ledTransport["frameCount"] = data.ledTransportFrameCount;
    ledTransport["showSkips"] = data.ledTransportShowSkips;
    ledTransport["lastShowUs"] = data.ledTransportLastShowUs;
    ledTransport["avgShowUs"] = data.ledTransportAvgShowUs;
    ledTransport["maxShowUs"] = data.ledTransportMaxShowUs;
    ledTransport["lastFastLedShowCallUs"] = data.ledTransportLastFastLedShowCallUs;
    ledTransport["avgFastLedShowCallUs"] = data.ledTransportAvgFastLedShowCallUs;
    ledTransport["lastRmtFenceUs"] = data.ledTransportLastRmtFenceUs;
    ledTransport["avgRmtFenceUs"] = data.ledTransportAvgRmtFenceUs;
    ledTransport["lastLatchWaitUs"] = data.ledTransportLastLatchWaitUs;
    ledTransport["avgLatchWaitUs"] = data.ledTransportAvgLatchWaitUs;
    ledTransport["failures"] = data.ledTransportFailures;
    ledTransport["rmtErrors"] = data.ledTransportRmtErrors;
    ledTransport["underruns"] = data.ledTransportUnderruns;
    
    JsonObject network = obj["network"].to<JsonObject>();
    network["connected"] = data.networkConnected;
    network["apMode"] = data.apMode;
    if (data.networkConnected && data.networkIP != nullptr && data.networkIP[0] != '\0') {
        network["ip"] = data.networkIP;
        network["rssi"] = data.networkRSSI;
    }
    
    obj["wsClients"] = data.wsClients;
}

void HttpDeviceCodec::encodeInfo(const HttpDeviceInfoData& data, JsonObject& obj) {
    obj["firmware"] = data.firmware;
    obj["board"] = data.board;
    obj["sdk"] = data.sdk;
    obj["flashSize"] = data.flashSize;
    obj["sketchSize"] = data.sketchSize;
    obj["freeSketch"] = data.freeSketch;
    obj["architecture"] = data.architecture;
}

} // namespace codec
} // namespace lightwaveos
