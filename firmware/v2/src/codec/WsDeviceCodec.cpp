/**
 * @file WsDeviceCodec.cpp
 * @brief WebSocket device codec implementation
 *
 * Single canonical JSON parser for device WebSocket commands.
 * All device commands are no-param (requestId only).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsDeviceCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

DeviceDecodeResult WsDeviceCodec::decode(JsonObjectConst root) {
    DeviceDecodeResult result;
    result.request = DeviceRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    result.success = true;
    return result;
}

} // namespace codec
} // namespace lightwaveos
