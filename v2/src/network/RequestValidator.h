/**
 * @file RequestValidator.h
 * @brief Lightweight schema-based request validation for LightwaveOS v2 API
 *
 * Provides declarative validation of JSON request payloads with:
 * - Type checking (uint8, uint16, uint32, int32, string, bool, array, object)
 * - Range validation for numeric types
 * - Required/optional field handling
 * - Human-readable error messages with field names
 *
 * Error codes returned:
 * - INVALID_JSON - Malformed JSON body
 * - MISSING_FIELD - Required field not present
 * - INVALID_TYPE - Wrong data type for field
 * - OUT_OF_RANGE - Value outside allowed bounds
 *
 * RAM Cost: ~100 bytes for validation logic (schemas stored as constexpr)
 *
 * Usage Example:
 * @code
 * constexpr FieldSchema SetEffectSchema[] = {
 *     {"effectId", FieldType::UINT8, true, 0, 45}
 * };
 *
 * void handleEffectsSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
 *     JsonDocument doc;
 *     ValidationResult result = RequestValidator::parseAndValidate(
 *         data, len, doc, SetEffectSchema, 1);
 *
 *     if (!result.valid) {
 *         sendErrorResponse(request, HttpStatus::BAD_REQUEST,
 *                           result.errorCode, result.errorMessage, result.fieldName);
 *         return;
 *     }
 *     // Process validated request...
 * }
 * @endcode
 */

#pragma once

#include <ArduinoJson.h>
#include "ApiResponse.h"

namespace lightwaveos {
namespace network {

// ============================================================================
// Field Types
// ============================================================================

/**
 * @brief Supported field types for validation
 */
enum class FieldType : uint8_t {
    UINT8,    // 0-255
    UINT16,   // 0-65535
    UINT32,   // 0-4294967295
    INT32,    // -2147483648 to 2147483647
    BOOL,     // true/false
    STRING,   // String value
    ARRAY,    // JSON array
    OBJECT    // JSON object
};

// ============================================================================
// Field Schema Definition
// ============================================================================

/**
 * @brief Schema definition for a single field
 */
struct FieldSchema {
    const char* name;        // Field name in JSON
    FieldType type;          // Expected type
    bool required;           // Is this field required?
    int32_t minVal;          // Min value (numeric) or min length (string/array)
    int32_t maxVal;          // Max value (numeric) or max length (string/array)

    /**
     * @brief Construct a field schema
     * @param n Field name
     * @param t Field type
     * @param req Required flag (default: true)
     * @param min Minimum value/length (default: 0)
     * @param max Maximum value/length (default: 0, meaning no limit)
     */
    constexpr FieldSchema(const char* n, FieldType t, bool req = true,
                          int32_t min = 0, int32_t max = 0)
        : name(n), type(t), required(req), minVal(min), maxVal(max) {}
};

// ============================================================================
// Validation Result
// ============================================================================

/**
 * @brief Result of validation operation
 */
struct ValidationResult {
    bool valid;               // True if validation passed
    const char* errorCode;    // Error code from ErrorCodes namespace
    const char* errorMessage; // Human-readable error message
    const char* fieldName;    // Field that caused the error (if applicable)

    /**
     * @brief Create a success result
     */
    static ValidationResult success() {
        return {true, nullptr, nullptr, nullptr};
    }

    /**
     * @brief Create an error result
     */
    static ValidationResult error(const char* code, const char* message,
                                   const char* field = nullptr) {
        return {false, code, message, field};
    }
};

// ============================================================================
// Request Validator Class
// ============================================================================

/**
 * @brief Validates JSON requests against schemas
 *
 * All methods are static for minimal RAM footprint.
 */
class RequestValidator {
public:
    /**
     * @brief Parse JSON and validate against schema in one call
     *
     * @param data Raw request body
     * @param len Length of request body
     * @param doc JsonDocument to parse into (caller-owned)
     * @param schema Array of FieldSchema definitions
     * @param schemaSize Number of fields in schema
     * @return ValidationResult indicating success or failure with details
     */
    template<size_t N>
    static ValidationResult parseAndValidate(const uint8_t* data, size_t len,
                                              JsonDocument& doc,
                                              const FieldSchema (&schema)[N]) {
        return parseAndValidate(data, len, doc, schema, N);
    }

    /**
     * @brief Parse JSON and validate against schema (non-template version)
     */
    static ValidationResult parseAndValidate(const uint8_t* data, size_t len,
                                              JsonDocument& doc,
                                              const FieldSchema* schema,
                                              size_t schemaSize) {
        // Parse JSON first
        DeserializationError error = deserializeJson(doc, data, len);
        if (error) {
            return ValidationResult::error(
                ErrorCodes::INVALID_JSON,
                error.c_str()
            );
        }

        // Validate against schema
        return validate(doc.as<JsonObject>(), schema, schemaSize);
    }

    /**
     * @brief Validate an already-parsed JSON document against a schema
     *
     * @param obj JsonObject to validate
     * @param schema Array of FieldSchema definitions
     * @param schemaSize Number of fields in schema
     * @return ValidationResult indicating success or failure with details
     */
    template<size_t N>
    static ValidationResult validate(const JsonObject& obj,
                                       const FieldSchema (&schema)[N]) {
        return validate(obj, schema, N);
    }

    /**
     * @brief Validate an already-parsed JSON document (non-template version)
     */
    static ValidationResult validate(const JsonObject& obj,
                                       const FieldSchema* schema,
                                       size_t schemaSize) {
        if (obj.isNull()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_JSON,
                "Invalid JSON object"
            );
        }

        for (size_t i = 0; i < schemaSize; i++) {
            const FieldSchema& field = schema[i];

            // Check if field exists
            if (!obj.containsKey(field.name)) {
                if (field.required) {
                    return ValidationResult::error(
                        ErrorCodes::MISSING_FIELD,
                        "Required field missing",
                        field.name
                    );
                }
                continue;  // Optional field not present, skip
            }

            // Validate the field value
            JsonVariant value = obj[field.name];
            ValidationResult result = validateField(value, field);
            if (!result.valid) {
                return result;
            }
        }

        return ValidationResult::success();
    }

    /**
     * @brief Validate a single field value against its schema
     */
    static ValidationResult validateField(JsonVariant value,
                                            const FieldSchema& field) {
        switch (field.type) {
            case FieldType::UINT8:
                return validateUint8(value, field);
            case FieldType::UINT16:
                return validateUint16(value, field);
            case FieldType::UINT32:
                return validateUint32(value, field);
            case FieldType::INT32:
                return validateInt32(value, field);
            case FieldType::BOOL:
                return validateBool(value, field);
            case FieldType::STRING:
                return validateString(value, field);
            case FieldType::ARRAY:
                return validateArray(value, field);
            case FieldType::OBJECT:
                return validateObject(value, field);
            default:
                return ValidationResult::error(
                    ErrorCodes::INTERNAL_ERROR,
                    "Unknown field type",
                    field.name
                );
        }
    }

private:
    // ========================================================================
    // Type-specific validators
    // ========================================================================

    static ValidationResult validateUint8(JsonVariant value,
                                           const FieldSchema& field) {
        if (!value.is<int>() && !value.is<unsigned int>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected unsigned integer (0-255)",
                field.name
            );
        }

        int val = value.as<int>();
        if (val < 0 || val > 255) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value must be 0-255",
                field.name
            );
        }

        // Check custom range if specified
        if (field.maxVal > 0 && (val < field.minVal || val > field.maxVal)) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value out of allowed range",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateUint16(JsonVariant value,
                                            const FieldSchema& field) {
        if (!value.is<int>() && !value.is<unsigned int>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected unsigned integer (0-65535)",
                field.name
            );
        }

        int val = value.as<int>();
        if (val < 0 || val > 65535) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value must be 0-65535",
                field.name
            );
        }

        // Check custom range if specified
        if (field.maxVal > 0 && (val < field.minVal || val > field.maxVal)) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value out of allowed range",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateUint32(JsonVariant value,
                                            const FieldSchema& field) {
        if (!value.is<int>() && !value.is<unsigned int>() &&
            !value.is<long>() && !value.is<unsigned long>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected unsigned integer",
                field.name
            );
        }

        unsigned long val = value.as<unsigned long>();

        // Check custom range if specified
        if (field.maxVal > 0) {
            if (val < (unsigned long)field.minVal ||
                val > (unsigned long)field.maxVal) {
                return ValidationResult::error(
                    ErrorCodes::OUT_OF_RANGE,
                    "Value out of allowed range",
                    field.name
                );
            }
        }

        return ValidationResult::success();
    }

    static ValidationResult validateInt32(JsonVariant value,
                                           const FieldSchema& field) {
        if (!value.is<int>() && !value.is<long>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected integer",
                field.name
            );
        }

        long val = value.as<long>();

        // Check custom range if specified (only if non-zero bounds set)
        if (field.minVal != 0 || field.maxVal != 0) {
            if (val < field.minVal || val > field.maxVal) {
                return ValidationResult::error(
                    ErrorCodes::OUT_OF_RANGE,
                    "Value out of allowed range",
                    field.name
                );
            }
        }

        return ValidationResult::success();
    }

    static ValidationResult validateBool(JsonVariant value,
                                          const FieldSchema& field) {
        if (!value.is<bool>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected boolean",
                field.name
            );
        }
        return ValidationResult::success();
    }

    static ValidationResult validateString(JsonVariant value,
                                            const FieldSchema& field) {
        if (!value.is<const char*>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected string",
                field.name
            );
        }

        const char* str = value.as<const char*>();
        if (str == nullptr) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected string",
                field.name
            );
        }

        size_t len = strlen(str);

        // Check min length
        if (field.minVal > 0 && len < (size_t)field.minVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "String too short",
                field.name
            );
        }

        // Check max length
        if (field.maxVal > 0 && len > (size_t)field.maxVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "String too long",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateArray(JsonVariant value,
                                           const FieldSchema& field) {
        if (!value.is<JsonArray>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected array",
                field.name
            );
        }

        JsonArray arr = value.as<JsonArray>();
        size_t size = arr.size();

        // Check min size
        if (field.minVal > 0 && size < (size_t)field.minVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Array too small",
                field.name
            );
        }

        // Check max size
        if (field.maxVal > 0 && size > (size_t)field.maxVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Array too large",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateObject(JsonVariant value,
                                            const FieldSchema& field) {
        if (!value.is<JsonObject>()) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected object",
                field.name
            );
        }
        return ValidationResult::success();
    }
};

// ============================================================================
// Common Request Schemas
// ============================================================================

namespace RequestSchemas {

    // ========================================================================
    // Effect Schemas
    // ========================================================================

    /**
     * @brief POST /api/v1/effects/set
     * Required: effectId (0-255, typically 0-45)
     * Optional: transition (bool), transitionType (0-15)
     */
    constexpr FieldSchema SetEffect[] = {
        {"effectId", FieldType::UINT8, true, 0, 255}
        // transition and transitionType are optional, validated inline
    };
    constexpr size_t SetEffectSize = sizeof(SetEffect) / sizeof(FieldSchema);

    // ========================================================================
    // Parameter Schemas
    // ========================================================================

    /**
     * @brief POST /api/v1/parameters
     * All fields optional, at least one should be present
     */
    constexpr FieldSchema SetParameters[] = {
        {"brightness", FieldType::UINT8, false, 0, 255},
        {"speed",      FieldType::UINT8, false, 1, 100},  // Extended range
        {"paletteId",  FieldType::UINT8, false, 0, 255},
        {"intensity",  FieldType::UINT8, false, 0, 255},
        {"saturation", FieldType::UINT8, false, 0, 255},
        {"complexity", FieldType::UINT8, false, 0, 255},
        {"variation",  FieldType::UINT8, false, 0, 255},
        {"hue",        FieldType::UINT8, false, 0, 255},
        {"mood",       FieldType::UINT8, false, 0, 255}   // Sensory Bridge mood
    };
    constexpr size_t SetParametersSize = sizeof(SetParameters) / sizeof(FieldSchema);

    // ========================================================================
    // Transition Schemas
    // ========================================================================

    /**
     * @brief POST /api/v1/transitions/trigger
     * Required: toEffect
     * Optional: type, duration, easing, random
     */
    constexpr FieldSchema TriggerTransition[] = {
        {"toEffect", FieldType::UINT8,  true,  0, 255},
        {"type",     FieldType::UINT8,  false, 0, 15},
        {"duration", FieldType::UINT16, false, 100, 10000},
        {"easing",   FieldType::UINT8,  false, 0, 15}
    };
    constexpr size_t TriggerTransitionSize = sizeof(TriggerTransition) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/transitions/config
     * Optional: defaultDuration, defaultType
     */
    constexpr FieldSchema TransitionConfig[] = {
        {"defaultDuration", FieldType::UINT16, false, 100, 10000},
        {"defaultType",     FieldType::UINT8,  false, 0, 15}
    };
    constexpr size_t TransitionConfigSize = sizeof(TransitionConfig) / sizeof(FieldSchema);

    // ========================================================================
    // Zone Schemas
    // ========================================================================

    /**
     * @brief POST /api/v1/zones/layout
     * Required: zoneCount (3 or 4)
     */
    constexpr FieldSchema ZoneLayout[] = {
        {"zoneCount", FieldType::UINT8, true, 3, 4}
    };
    constexpr size_t ZoneLayoutSize = sizeof(ZoneLayout) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/zones/:id/effect
     * Required: effectId
     */
    constexpr FieldSchema ZoneEffect[] = {
        {"effectId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t ZoneEffectSize = sizeof(ZoneEffect) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/zones/:id/brightness
     * Required: brightness (0-255)
     */
    constexpr FieldSchema ZoneBrightness[] = {
        {"brightness", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t ZoneBrightnessSize = sizeof(ZoneBrightness) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/zones/:id/speed
     * Required: speed (1-100)
     */
    constexpr FieldSchema ZoneSpeed[] = {
        {"speed", FieldType::UINT8, true, 1, 100}  // Extended range
    };
    constexpr size_t ZoneSpeedSize = sizeof(ZoneSpeed) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/zones/:id/palette
     * Required: paletteId (0-74)
     */
    constexpr FieldSchema ZonePalette[] = {
        {"paletteId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t ZonePaletteSize = sizeof(ZonePalette) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/zones/:id/blend
     * Required: blendMode (0-7)
     */
    constexpr FieldSchema ZoneBlend[] = {
        {"blendMode", FieldType::UINT8, true, 0, 7}
    };
    constexpr size_t ZoneBlendSize = sizeof(ZoneBlend) / sizeof(FieldSchema);

    /**
     * @brief POST /api/v1/zones/:id/enabled
     * Required: enabled (boolean)
     */
    constexpr FieldSchema ZoneEnabled[] = {
        {"enabled", FieldType::BOOL, true}
    };
    constexpr size_t ZoneEnabledSize = sizeof(ZoneEnabled) / sizeof(FieldSchema);

    // ========================================================================
    // Palette Schemas
    // ========================================================================

    /**
     * @brief POST /api/v1/palettes/set
     * Required: paletteId (0-74)
     */
    constexpr FieldSchema SetPalette[] = {
        {"paletteId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t SetPaletteSize = sizeof(SetPalette) / sizeof(FieldSchema);

    // ========================================================================
    // Batch Operations Schema
    // ========================================================================

    /**
     * @brief POST /api/v1/batch
     * Required: operations (array, 1-10 items)
     */
    constexpr FieldSchema BatchOperations[] = {
        {"operations", FieldType::ARRAY, true, 1, 10}
    };
    constexpr size_t BatchOperationsSize = sizeof(BatchOperations) / sizeof(FieldSchema);

    // ========================================================================
    // Network Schemas
    // ========================================================================

    /**
     * @brief POST /api/network/connect
     * Required: ssid
     * Optional: password
     */
    constexpr FieldSchema NetworkConnect[] = {
        {"ssid",     FieldType::STRING, true, 1, 32},
        {"password", FieldType::STRING, false, 0, 64}
    };
    constexpr size_t NetworkConnectSize = sizeof(NetworkConnect) / sizeof(FieldSchema);

    // ========================================================================
    // Legacy API Schemas
    // ========================================================================

    /**
     * @brief POST /api/effect (legacy)
     */
    constexpr FieldSchema LegacySetEffect[] = {
        {"effect", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t LegacySetEffectSize = sizeof(LegacySetEffect) / sizeof(FieldSchema);

    /**
     * @brief POST /api/brightness (legacy)
     */
    constexpr FieldSchema LegacySetBrightness[] = {
        {"brightness", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t LegacySetBrightnessSize = sizeof(LegacySetBrightness) / sizeof(FieldSchema);

    /**
     * @brief POST /api/speed (legacy)
     */
    constexpr FieldSchema LegacySetSpeed[] = {
        {"speed", FieldType::UINT8, true, 1, 100}  // Extended range
    };
    constexpr size_t LegacySetSpeedSize = sizeof(LegacySetSpeed) / sizeof(FieldSchema);

    /**
     * @brief POST /api/palette (legacy)
     */
    constexpr FieldSchema LegacySetPalette[] = {
        {"paletteId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t LegacySetPaletteSize = sizeof(LegacySetPalette) / sizeof(FieldSchema);

    /**
     * @brief POST /api/zone/count (legacy)
     */
    constexpr FieldSchema LegacyZoneCount[] = {
        {"count", FieldType::UINT8, true, 1, 4}
    };
    constexpr size_t LegacyZoneCountSize = sizeof(LegacyZoneCount) / sizeof(FieldSchema);

    /**
     * @brief POST /api/zone/effect (legacy)
     */
    constexpr FieldSchema LegacyZoneEffect[] = {
        {"zoneId",   FieldType::UINT8, true, 0, 3},
        {"effectId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t LegacyZoneEffectSize = sizeof(LegacyZoneEffect) / sizeof(FieldSchema);

    /**
     * @brief POST /api/zone/preset/load (legacy)
     */
    constexpr FieldSchema LegacyZonePreset[] = {
        {"preset", FieldType::UINT8, true, 0, 4}
    };
    constexpr size_t LegacyZonePresetSize = sizeof(LegacyZonePreset) / sizeof(FieldSchema);

}  // namespace RequestSchemas

// ============================================================================
// Convenience Macro
// ============================================================================

/**
 * @brief Validate request and return on error (v1 API format)
 *
 * Usage:
 * @code
 * JsonDocument doc;
 * VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::SetEffect, request);
 * // Request is valid, doc contains parsed JSON
 * @endcode
 */
#define VALIDATE_REQUEST_OR_RETURN(data, len, doc, schema, request) \
    do { \
        auto vr = RequestValidator::parseAndValidate(data, len, doc, schema); \
        if (!vr.valid) { \
            sendErrorResponse(request, HttpStatus::BAD_REQUEST, \
                              vr.errorCode, vr.errorMessage, vr.fieldName); \
            return; \
        } \
    } while(0)

/**
 * @brief Validate request and return on error (legacy API format)
 *
 * Uses sendLegacyError instead of sendErrorResponse for backward compatibility.
 *
 * Usage:
 * @code
 * JsonDocument doc;
 * VALIDATE_LEGACY_OR_RETURN(data, len, doc, RequestSchemas::LegacySetEffect, request);
 * // Request is valid, doc contains parsed JSON
 * @endcode
 */
#define VALIDATE_LEGACY_OR_RETURN(data, len, doc, schema, request) \
    do { \
        auto vr = RequestValidator::parseAndValidate(data, len, doc, schema); \
        if (!vr.valid) { \
            sendLegacyError(request, vr.errorMessage); \
            return; \
        } \
    } while(0)

}  // namespace network
}  // namespace lightwaveos
