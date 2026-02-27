// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsOtaCodec.cpp
 * @brief JSON codec implementation for WebSocket OTA commands
 */

#include "WsOtaCodec.h"
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace codec {

// Allowed keys for each command type
static const char* OTA_CHECK_ALLOWED[] = {"type", "requestId"};
static const char* OTA_BEGIN_ALLOWED[] = {"type", "size", "md5", "token", "version", "force", "target", "requestId"};
static const char* OTA_CHUNK_ALLOWED[] = {"type", "offset", "data", "requestId"};
static const char* OTA_ABORT_ALLOWED[] = {"type", "requestId"};
static const char* OTA_VERIFY_ALLOWED[] = {"type", "md5", "requestId"};

bool WsOtaCodec::hasUnknownKeys(JsonObjectConst root, const char* const* allowedKeys, size_t keyCount) {
    for (JsonPairConst pair : root) {
        const char* key = pair.key().c_str();
        bool isAllowed = false;
        for (size_t i = 0; i < keyCount; i++) {
            if (strcmp(key, allowedKeys[i]) == 0) {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed) {
            return true;
        }
    }
    return false;
}

OtaCheckDecodeResult WsOtaCodec::decodeOtaCheck(JsonObjectConst root) {
    OtaCheckDecodeResult result;
    
    // Check for unknown keys
    if (hasUnknownKeys(root, OTA_CHECK_ALLOWED, sizeof(OTA_CHECK_ALLOWED) / sizeof(OTA_CHECK_ALLOWED[0]))) {
        strncpy(result.errorMsg, "Unknown keys in ota.check", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    // Extract requestId (optional)
    if (root.containsKey("requestId") && root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }
    
    result.success = true;
    return result;
}

OtaBeginDecodeResult WsOtaCodec::decodeOtaBegin(JsonObjectConst root) {
    OtaBeginDecodeResult result;
    
    // Check for unknown keys
    if (hasUnknownKeys(root, OTA_BEGIN_ALLOWED, sizeof(OTA_BEGIN_ALLOWED) / sizeof(OTA_BEGIN_ALLOWED[0]))) {
        strncpy(result.errorMsg, "Unknown keys in ota.begin", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    // Extract size (required)
    if (!root.containsKey("size") || !root["size"].is<uint32_t>()) {
        strncpy(result.errorMsg, "Missing or invalid 'size' field", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    result.request.size = root["size"].as<uint32_t>();

    // Extract md5 (optional)
    if (root.containsKey("md5") && root["md5"].is<const char*>()) {
        result.request.md5 = root["md5"].as<const char*>();
    }

    // Extract token (optional at codec level; command handler enforces requirement)
    if (root.containsKey("token") && root["token"].is<const char*>()) {
        result.request.token = root["token"].as<const char*>();
    }

    // Extract version (optional - incoming firmware version for validation)
    if (root.containsKey("version") && root["version"].is<const char*>()) {
        result.request.version = root["version"].as<const char*>();
    }

    // Extract force flag (optional - defaults to true for backward compatibility)
    if (root.containsKey("force") && root["force"].is<bool>()) {
        result.request.force = root["force"].as<bool>();
    }
    // If "force" key is absent, default remains true (backward compatible: always allow)

    // Extract target (optional - "firmware" or "filesystem", defaults to "firmware")
    if (root.containsKey("target") && root["target"].is<const char*>()) {
        const char* target = root["target"].as<const char*>();
        if (strcmp(target, "firmware") == 0 || strcmp(target, "filesystem") == 0) {
            result.request.target = target;
        } else {
            strncpy(result.errorMsg, "Invalid 'target': must be 'firmware' or 'filesystem'",
                    sizeof(result.errorMsg) - 1);
            return result;
        }
    }

    // Extract requestId (optional)
    if (root.containsKey("requestId") && root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    result.success = true;
    return result;
}

OtaChunkDecodeResult WsOtaCodec::decodeOtaChunk(JsonObjectConst root) {
    OtaChunkDecodeResult result;
    
    // Check for unknown keys
    if (hasUnknownKeys(root, OTA_CHUNK_ALLOWED, sizeof(OTA_CHUNK_ALLOWED) / sizeof(OTA_CHUNK_ALLOWED[0]))) {
        strncpy(result.errorMsg, "Unknown keys in ota.chunk", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    // Extract offset (required)
    if (!root.containsKey("offset") || !root["offset"].is<uint32_t>()) {
        strncpy(result.errorMsg, "Missing or invalid 'offset' field", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    result.request.offset = root["offset"].as<uint32_t>();
    
    // Extract data (required, base64 string)
    if (!root.containsKey("data") || !root["data"].is<const char*>()) {
        strncpy(result.errorMsg, "Missing or invalid 'data' field", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    result.request.data = root["data"].as<const char*>();
    
    // Extract requestId (optional)
    if (root.containsKey("requestId") && root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }
    
    result.success = true;
    return result;
}

bool WsOtaCodec::decodeOtaAbort(JsonObjectConst root, const char** requestId) {
    // Check for unknown keys
    if (hasUnknownKeys(root, OTA_ABORT_ALLOWED, sizeof(OTA_ABORT_ALLOWED) / sizeof(OTA_ABORT_ALLOWED[0]))) {
        return false;
    }
    
    // Extract requestId (optional)
    *requestId = "";
    if (root.containsKey("requestId") && root["requestId"].is<const char*>()) {
        *requestId = root["requestId"].as<const char*>();
    }
    
    return true;
}

OtaVerifyDecodeResult WsOtaCodec::decodeOtaVerify(JsonObjectConst root) {
    OtaVerifyDecodeResult result;
    
    // Check for unknown keys
    if (hasUnknownKeys(root, OTA_VERIFY_ALLOWED, sizeof(OTA_VERIFY_ALLOWED) / sizeof(OTA_VERIFY_ALLOWED[0]))) {
        strncpy(result.errorMsg, "Unknown keys in ota.verify", sizeof(result.errorMsg) - 1);
        return result;
    }
    
    // Extract md5 (optional)
    if (root.containsKey("md5") && root["md5"].is<const char*>()) {
        result.request.md5 = root["md5"].as<const char*>();
    }
    
    // Extract requestId (optional)
    if (root.containsKey("requestId") && root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }
    
    result.success = true;
    return result;
}

void WsOtaCodec::encodeOtaStatus(JsonObject& data, const char* version,
                                 uint32_t versionNumber,
                                 uint32_t sketchSize, uint32_t freeSpace,
                                 bool otaAvailable) {
    data["version"] = version;
    data["versionNumber"] = versionNumber;
    data["sketchSize"] = sketchSize;
    data["freeSpace"] = freeSpace;
    data["otaAvailable"] = otaAvailable;
    data["maxOtaSize"] = freeSpace;
}

void WsOtaCodec::encodeOtaReady(JsonObject& data, uint32_t totalSize) {
    data["totalSize"] = totalSize;
    data["ready"] = true;
}

void WsOtaCodec::encodeOtaProgress(JsonObject& data, uint32_t offset,
                                   uint32_t total, uint8_t percent) {
    data["offset"] = offset;
    data["total"] = total;
    data["percent"] = percent;
}

void WsOtaCodec::encodeOtaComplete(JsonObject& data, bool rebooting) {
    data["complete"] = true;
    data["rebooting"] = rebooting;
}

void WsOtaCodec::encodeOtaError(JsonObject& data, const char* code,
                                const char* message) {
    JsonObject error = data["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;
}

void WsOtaCodec::encodeOtaAborted(JsonObject& data) {
    data["aborted"] = true;
}

} // namespace codec
} // namespace lightwaveos
