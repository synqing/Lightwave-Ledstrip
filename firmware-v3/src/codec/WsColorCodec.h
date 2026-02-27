/**
 * @file WsColorCodec.h
 * @brief JSON codec for WebSocket color commands parsing and validation
 *
 * Single canonical location for parsing WebSocket color command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from color WS commands.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Group B: Simple Setters
// ============================================================================

/**
 * @brief Decoded color.enableBlend request
 */
struct ColorEnableBlendRequest {
    bool enable;                // Required
    const char* requestId;      // Optional
    
    ColorEnableBlendRequest() : enable(false), requestId("") {}
};

struct ColorEnableBlendDecodeResult {
    bool success;
    ColorEnableBlendRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorEnableBlendDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded color.enableRotation request
 */
struct ColorEnableRotationRequest {
    bool enable;                // Required
    const char* requestId;      // Optional
    
    ColorEnableRotationRequest() : enable(false), requestId("") {}
};

struct ColorEnableRotationDecodeResult {
    bool success;
    ColorEnableRotationRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorEnableRotationDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded color.enableDiffusion request
 */
struct ColorEnableDiffusionRequest {
    bool enable;                // Required
    const char* requestId;      // Optional
    
    ColorEnableDiffusionRequest() : enable(false), requestId("") {}
};

struct ColorEnableDiffusionDecodeResult {
    bool success;
    ColorEnableDiffusionRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorEnableDiffusionDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded color.setDiffusionAmount request
 */
struct ColorSetDiffusionAmountRequest {
    uint8_t amount;             // Required (0-255)
    const char* requestId;      // Optional
    
    ColorSetDiffusionAmountRequest() : amount(0), requestId("") {}
};

struct ColorSetDiffusionAmountDecodeResult {
    bool success;
    ColorSetDiffusionAmountRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorSetDiffusionAmountDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded colorCorrection.setMode request
 */
struct ColorCorrectionSetModeRequest {
    uint8_t mode;               // Required (0-3)
    const char* requestId;      // Optional
    
    ColorCorrectionSetModeRequest() : mode(255), requestId("") {}
};

struct ColorCorrectionSetModeDecodeResult {
    bool success;
    ColorCorrectionSetModeRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorCorrectionSetModeDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded color.setRotationSpeed request
 */
struct ColorSetRotationSpeedRequest {
    float degreesPerFrame;      // Required (float)
    const char* requestId;      // Optional
    
    ColorSetRotationSpeedRequest() : degreesPerFrame(0.0f), requestId("") {}
};

struct ColorSetRotationSpeedDecodeResult {
    bool success;
    ColorSetRotationSpeedRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorSetRotationSpeedDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Group C: Multi-Field
// ============================================================================

/**
 * @brief Decoded color.setBlendPalettes request
 */
struct ColorSetBlendPalettesRequest {
    uint8_t palette1;           // Required (0-255, validated externally)
    uint8_t palette2;           // Required (0-255, validated externally)
    uint8_t palette3;           // Optional (255 means none)
    const char* requestId;      // Optional
    
    ColorSetBlendPalettesRequest() : palette1(255), palette2(255), palette3(255), requestId("") {}
};

struct ColorSetBlendPalettesDecodeResult {
    bool success;
    ColorSetBlendPalettesRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorSetBlendPalettesDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded color.setBlendFactors request
 */
struct ColorSetBlendFactorsRequest {
    uint8_t factor1;            // Required (0-255)
    uint8_t factor2;            // Required (0-255)
    uint8_t factor3;            // Optional (default: 0)
    const char* requestId;      // Optional
    
    ColorSetBlendFactorsRequest() : factor1(255), factor2(255), factor3(0), requestId("") {}
};

struct ColorSetBlendFactorsDecodeResult {
    bool success;
    ColorSetBlendFactorsRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorSetBlendFactorsDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Group D: Complex Optional Fields
// ============================================================================

/**
 * @brief Decoded colorCorrection.setConfig request (10 optional fields)
 */
struct ColorCorrectionSetConfigRequest {
    const char* requestId;      // Optional
    bool hasMode;
    bool hasHsvMinSaturation;
    bool hasRgbWhiteThreshold;
    bool hasRgbTargetMin;
    bool hasAutoExposureEnabled;
    bool hasAutoExposureTarget;
    bool hasGammaEnabled;
    bool hasGammaValue;
    bool hasBrownGuardrailEnabled;
    bool hasMaxGreenPercentOfRed;
    bool hasMaxBluePercentOfRed;
    uint8_t mode;               // If hasMode (0-3)
    uint8_t hsvMinSaturation;   // If hasHsvMinSaturation (0-255)
    uint8_t rgbWhiteThreshold;  // If hasRgbWhiteThreshold (0-255)
    uint8_t rgbTargetMin;       // If hasRgbTargetMin (0-255)
    bool autoExposureEnabled;   // If hasAutoExposureEnabled
    uint8_t autoExposureTarget; // If hasAutoExposureTarget (0-255)
    bool gammaEnabled;          // If hasGammaEnabled
    float gammaValue;           // If hasGammaValue (1.0f-3.0f)
    bool brownGuardrailEnabled; // If hasBrownGuardrailEnabled
    uint8_t maxGreenPercentOfRed;   // If hasMaxGreenPercentOfRed (0-255)
    uint8_t maxBluePercentOfRed;    // If hasMaxBluePercentOfRed (0-255)
    
    ColorCorrectionSetConfigRequest() 
        : requestId(""), hasMode(false), hasHsvMinSaturation(false),
          hasRgbWhiteThreshold(false), hasRgbTargetMin(false),
          hasAutoExposureEnabled(false), hasAutoExposureTarget(false),
          hasGammaEnabled(false), hasGammaValue(false),
          hasBrownGuardrailEnabled(false), hasMaxGreenPercentOfRed(false),
          hasMaxBluePercentOfRed(false),
          mode(0), hsvMinSaturation(120), rgbWhiteThreshold(150), rgbTargetMin(100),
          autoExposureEnabled(false), autoExposureTarget(110),
          gammaEnabled(true), gammaValue(2.2f),
          brownGuardrailEnabled(false), maxGreenPercentOfRed(28), maxBluePercentOfRed(8) {}
};

struct ColorCorrectionSetConfigDecodeResult {
    bool success;
    ColorCorrectionSetConfigRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ColorCorrectionSetConfigDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Color Command JSON Codec
 *
 * Single canonical parser for color WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsColorCodec {
public:
    // Group B: Simple setters
    static ColorEnableBlendDecodeResult decodeEnableBlend(JsonObjectConst root);
    static ColorEnableRotationDecodeResult decodeEnableRotation(JsonObjectConst root);
    static ColorEnableDiffusionDecodeResult decodeEnableDiffusion(JsonObjectConst root);
    static ColorSetDiffusionAmountDecodeResult decodeSetDiffusionAmount(JsonObjectConst root);
    static ColorCorrectionSetModeDecodeResult decodeSetMode(JsonObjectConst root);
    static ColorSetRotationSpeedDecodeResult decodeSetRotationSpeed(JsonObjectConst root);
    
    // Group C: Multi-field
    static ColorSetBlendPalettesDecodeResult decodeSetBlendPalettes(JsonObjectConst root);
    static ColorSetBlendFactorsDecodeResult decodeSetBlendFactors(JsonObjectConst root);
    
    // Group D: Complex optional fields
    static ColorCorrectionSetConfigDecodeResult decodeSetConfig(JsonObjectConst root);
};

} // namespace codec
} // namespace lightwaveos
