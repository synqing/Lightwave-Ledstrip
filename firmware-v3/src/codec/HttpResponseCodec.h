// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpResponseCodec.h
 * @brief Codec for standard HTTP response envelope
 *
 * Single canonical JSON encoder for standard HTTP response fields.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>

namespace lightwaveos {
namespace codec {

/**
 * @brief Data for encoding standard HTTP response envelope
 */
struct HttpResponseEnvelopeData {
    bool success;
    unsigned long timestamp;
    const char* version;
    
    HttpResponseEnvelopeData() : success(true), timestamp(0), version("2.0") {}
};

/**
 * @brief Codec for standard HTTP response envelope
 */
class HttpResponseCodec {
public:
    /**
     * @brief Encode standard HTTP response envelope (success, timestamp, version)
     * @param data Response envelope data
     * @param obj JSON object to encode into
     */
    static void encodeEnvelope(const HttpResponseEnvelopeData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
