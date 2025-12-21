#ifndef REQUEST_VALIDATOR_H
#define REQUEST_VALIDATOR_H

/**
 * @file RequestValidator.h
 * @brief Lightweight schema-based request validation for LightwaveOS v1 API
 *
 * Provides declarative validation of JSON request payloads with:
 * - Type checking (uint8, uint16, uint32, string, bool, array)
 * - Range validation for numeric types
 * - Required/optional field handling
 * - Human-readable error messages
 *
 * RAM Cost: ~100 bytes for validation logic (schemas stored as constants)
 */

#include "ApiResponse.h"
#include <cJSON.h>

// ============================================================================
// Field Types
// ============================================================================
enum class FieldType : uint8_t {
    UINT8,
    UINT16,
    UINT32,
    INT32,
    BOOL,
    STRING,
    ARRAY,
    OBJECT
};

// ============================================================================
// Field Schema Definition
// ============================================================================
struct FieldSchema {
    const char* name;        // Field name in JSON
    FieldType type;          // Expected type
    bool required;           // Is this field required?
    int32_t minVal;          // Minimum value (for numeric) or min length (for string)
    int32_t maxVal;          // Maximum value (for numeric) or max length (for string)

    // Constructor for convenience
    constexpr FieldSchema(const char* n, FieldType t, bool req = true,
                          int32_t min = 0, int32_t max = 0)
        : name(n), type(t), required(req), minVal(min), maxVal(max) {}
};

// ============================================================================
// Validation Result
// ============================================================================
struct ValidationResult {
    bool valid;
    const char* errorCode;
    const char* errorMessage;
    const char* fieldName;

    static ValidationResult success() {
        return {true, nullptr, nullptr, nullptr};
    }

    static ValidationResult error(const char* code, const char* message, const char* field = nullptr) {
        return {false, code, message, field};
    }
};

// ============================================================================
// Request Validator
// ============================================================================
class RequestValidator {
public:
    /**
     * @brief Validate a JSON document against a schema
     * @param doc The JSON document to validate
     * @param schema Array of FieldSchema definitions
     * @param schemaSize Number of fields in schema
     * @return ValidationResult indicating success or failure with details
     */
    static ValidationResult validate(const cJSON* doc,
                                       const FieldSchema* schema,
                                       size_t schemaSize) {
        if (doc == nullptr || !cJSON_IsObject(doc)) {
            return ValidationResult::error(ErrorCodes::INVALID_JSON, "Invalid JSON object");
        }

        for (size_t i = 0; i < schemaSize; i++) {
            const FieldSchema& field = schema[i];

            // Check if field exists
            const cJSON* value = cJSON_GetObjectItemCaseSensitive(doc, field.name);
            if (value == nullptr) {
                if (field.required) {
                    return ValidationResult::error(
                        ErrorCodes::MISSING_FIELD,
                        "Required field missing",
                        field.name
                    );
                }
                continue; // Optional field, skip validation
            }

            // Validate type and range
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
    static ValidationResult validateField(const cJSON* value,
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
    static ValidationResult validateUint8(const cJSON* value, const FieldSchema& field) {
        if (!cJSON_IsNumber(value)) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected unsigned integer",
                field.name
            );
        }

        const double dv = value->valuedouble;
        if (dv < 0 || dv > 255) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value must be 0-255",
                field.name
            );
        }
        int val = (int)dv;
        if (val < 0 || val > 255) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value must be 0-255",
                field.name
            );
        }

        if (field.maxVal > 0 && (val < field.minVal || val > field.maxVal)) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value out of allowed range",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateUint16(const cJSON* value, const FieldSchema& field) {
        if (!cJSON_IsNumber(value)) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected unsigned integer",
                field.name
            );
        }

        const double dv = value->valuedouble;
        if (dv < 0 || dv > 65535) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value must be 0-65535",
                field.name
            );
        }
        int val = (int)dv;
        if (val < 0 || val > 65535) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value must be 0-65535",
                field.name
            );
        }

        if (field.maxVal > 0 && (val < field.minVal || val > field.maxVal)) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Value out of allowed range",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateUint32(const cJSON* value, const FieldSchema& field) {
        if (!cJSON_IsNumber(value)) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected unsigned integer",
                field.name
            );
        }

        // Note: ArduinoJson handles large values, we trust the range here
        if (field.maxVal > 0) {
            unsigned long val = (unsigned long)value->valuedouble;
            if (val < (unsigned long)field.minVal || val > (unsigned long)field.maxVal) {
                return ValidationResult::error(
                    ErrorCodes::OUT_OF_RANGE,
                    "Value out of allowed range",
                    field.name
                );
            }
        }

        return ValidationResult::success();
    }

    static ValidationResult validateInt32(const cJSON* value, const FieldSchema& field) {
        if (!cJSON_IsNumber(value)) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected integer",
                field.name
            );
        }

        if (field.maxVal != 0 || field.minVal != 0) {
            int val = (int)value->valuedouble;
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

    static ValidationResult validateBool(const cJSON* value, const FieldSchema& field) {
        (void)field;
        if (!cJSON_IsBool(value)) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected boolean",
                field.name
            );
        }
        return ValidationResult::success();
    }

    static ValidationResult validateString(const cJSON* value, const FieldSchema& field) {
        if (!cJSON_IsString(value) || value->valuestring == nullptr) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected string",
                field.name
            );
        }

        const char* str = value->valuestring;
        size_t len = strlen(str);

        if (field.minVal > 0 && len < (size_t)field.minVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "String too short",
                field.name
            );
        }

        if (field.maxVal > 0 && len > (size_t)field.maxVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "String too long",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateArray(const cJSON* value, const FieldSchema& field) {
        if (!cJSON_IsArray(value)) {
            return ValidationResult::error(
                ErrorCodes::INVALID_TYPE,
                "Expected array",
                field.name
            );
        }

        int size = cJSON_GetArraySize(value);

        if (field.minVal > 0 && size < field.minVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Array too small",
                field.name
            );
        }

        if (field.maxVal > 0 && size > field.maxVal) {
            return ValidationResult::error(
                ErrorCodes::OUT_OF_RANGE,
                "Array too large",
                field.name
            );
        }

        return ValidationResult::success();
    }

    static ValidationResult validateObject(const cJSON* value, const FieldSchema& field) {
        (void)field;
        if (!cJSON_IsObject(value)) {
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
// Common Request Schemas (stored in flash via PROGMEM-like constexpr)
// ============================================================================
namespace RequestSchemas {

    // POST /api/v1/effects/set
    constexpr FieldSchema SetEffect[] = {
        {"effectId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t SetEffectSize = sizeof(SetEffect) / sizeof(FieldSchema);

    // POST /api/v1/parameters
    constexpr FieldSchema SetParameters[] = {
        {"brightness", FieldType::UINT8, false, 0, 255},
        {"speed", FieldType::UINT8, false, 1, 50},
        {"paletteId", FieldType::UINT8, false, 0, 255}
    };
    constexpr size_t SetParametersSize = sizeof(SetParameters) / sizeof(FieldSchema);

    // POST /api/v1/transitions/trigger
    constexpr FieldSchema TriggerTransition[] = {
        {"toEffect", FieldType::UINT8, true, 0, 255},
        {"type", FieldType::UINT8, false, 0, 15},
        {"duration", FieldType::UINT32, false, 100, 10000},
        {"easing", FieldType::UINT8, false, 0, 15}
    };
    constexpr size_t TriggerTransitionSize = sizeof(TriggerTransition) / sizeof(FieldSchema);

    // Zone effect setting
    constexpr FieldSchema SetZoneEffect[] = {
        {"zoneId", FieldType::UINT8, true, 0, 3},
        {"effectId", FieldType::UINT8, true, 0, 255}
    };
    constexpr size_t SetZoneEffectSize = sizeof(SetZoneEffect) / sizeof(FieldSchema);

    // Batch operations
    constexpr FieldSchema BatchOperations[] = {
        {"operations", FieldType::ARRAY, true, 1, 10}
    };
    constexpr size_t BatchOperationsSize = sizeof(BatchOperations) / sizeof(FieldSchema);
}

// ============================================================================
// Helper Macros for Easy Validation
// ============================================================================

/**
 * Usage:
 * VALIDATE_REQUEST(doc, RequestSchemas::SetEffect, RequestSchemas::SetEffectSize, request);
 *
 * Expands to validation code that sends error response and returns on failure
 */
#define VALIDATE_REQUEST(doc, schema, schemaSize, request) \
    do { \
        ValidationResult vr = RequestValidator::validate(doc, schema, schemaSize); \
        if (!vr.valid) { \
            sendErrorResponse(request, HttpStatus::BAD_REQUEST, \
                              vr.errorCode, vr.errorMessage, vr.fieldName); \
            return; \
        } \
    } while(0)

#endif // REQUEST_VALIDATOR_H
