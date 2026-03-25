---
abstract: "Concrete C++ code examples for implementing onset parameters. Includes struct definition, REST handler, WebSocket dispatcher, serial CLI parser, NVS persistence, and OnsetDetector integration."
---

# Onset Parameters — Code Examples

**Date:** 2026-03-25
**Purpose:** Provide copy-paste-ready code templates for implementing the three core onset parameters

---

## 1. OnsetParameters Struct Definition

**File:** `src/audio/contracts/OnsetParameters.h`

```cpp
#pragma once

#include <Preferences.h>  // NVS library
#include <cstdint>

namespace lightwaveos {
namespace audio {

/**
 * @class OnsetParameters
 * @brief User-tunable parameters for silence detection and onset recognition.
 *
 * All parameters are normalized (0–100) for user-friendly exposure.
 * NVS persistence is automatic on save.
 */
class OnsetParameters {
public:
    // User-facing normalized parameters (0–100)
    uint8_t silenceThreshold;      // 0=always active, 100=require silence
    uint8_t onsetSensitivity;      // 0=ignore weak beats, 100=ultra-sensitive
    uint16_t silenceHysteresis;    // 0–1000 ms, prevents jitter

    // Derived backend parameters (computed from normalized values)
    float rmsThreshold;             // Absolute RMS threshold for silence gate
    float onsetConfThreshold;       // Confidence threshold for onset detection (0.3–0.7)

    OnsetParameters();

    /**
     * @brief Compute backend parameters from user-facing normalized values.
     * @param maxRms Maximum expected RMS value in your environment (typically 0.1–0.5)
     */
    void recomputeDerivedParameters(float maxRms = 0.1f);

    /**
     * @brief Load parameters from NVS (Non-Volatile Storage).
     * @return true if load succeeded, false if NVS is empty (will use defaults)
     */
    bool loadFromNVS();

    /**
     * @brief Save parameters to NVS (survives power cycle).
     */
    void saveToNVS() const;

    /**
     * @brief Reset all parameters to hardcoded defaults.
     */
    void resetToDefaults();

    /**
     * @brief Validate parameter bounds and clamp if necessary.
     */
    void validate();

    /**
     * @brief Get singleton instance.
     */
    static OnsetParameters& getInstance();

private:
    static constexpr uint8_t NVS_NAMESPACE[] = "onset";

    // Default values
    static constexpr uint8_t DEFAULT_SILENCE_THRESHOLD = 30;
    static constexpr uint8_t DEFAULT_ONSET_SENSITIVITY = 60;
    static constexpr uint16_t DEFAULT_SILENCE_HYSTERESIS = 200;
};

}  // namespace audio
}  // namespace lightwaveos
```

**File:** `src/audio/contracts/OnsetParameters.cpp`

```cpp
#include "OnsetParameters.h"
#include "utils/Log.h"

namespace lightwaveos {
namespace audio {

OnsetParameters::OnsetParameters()
    : silenceThreshold(DEFAULT_SILENCE_THRESHOLD),
      onsetSensitivity(DEFAULT_ONSET_SENSITIVITY),
      silenceHysteresis(DEFAULT_SILENCE_HYSTERESIS),
      rmsThreshold(0.025f),
      onsetConfThreshold(0.54f) {
    recomputeDerivedParameters();
}

void OnsetParameters::recomputeDerivedParameters(float maxRms) {
    // Silence threshold: 0–100 -> 0 to maxRms
    rmsThreshold = (silenceThreshold / 100.0f) * maxRms;

    // Onset sensitivity: 0–100 -> 0.3–0.7 confidence threshold
    // 0 = only very strong onsets (0.7), 100 = weak onsets (0.3)
    onsetConfThreshold = 0.7f - (onsetSensitivity / 100.0f) * 0.4f;
}

bool OnsetParameters::loadFromNVS() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only
        Log::warn("OnsetParameters", "Failed to open NVS namespace '%s'", NVS_NAMESPACE);
        return false;
    }

    // Load with defaults if keys don't exist
    silenceThreshold = prefs.getUChar("silThresh", DEFAULT_SILENCE_THRESHOLD);
    onsetSensitivity = prefs.getUChar("onsetSens", DEFAULT_ONSET_SENSITIVITY);
    silenceHysteresis = prefs.getUShort("silHyst", DEFAULT_SILENCE_HYSTERESIS);

    prefs.end();

    validate();
    recomputeDerivedParameters();

    Log::info("OnsetParameters", "Loaded from NVS: silThresh=%u, onsetSens=%u, silHyst=%u",
              silenceThreshold, onsetSensitivity, silenceHysteresis);

    return true;
}

void OnsetParameters::saveToNVS() const {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write
        Log::warn("OnsetParameters", "Failed to open NVS namespace '%s' for write", NVS_NAMESPACE);
        return;
    }

    prefs.putUChar("silThresh", silenceThreshold);
    prefs.putUChar("onsetSens", onsetSensitivity);
    prefs.putUShort("silHyst", silenceHysteresis);

    prefs.end();  // Calls commit()

    Log::info("OnsetParameters", "Saved to NVS: silThresh=%u, onsetSens=%u, silHyst=%u",
              silenceThreshold, onsetSensitivity, silenceHysteresis);
}

void OnsetParameters::resetToDefaults() {
    silenceThreshold = DEFAULT_SILENCE_THRESHOLD;
    onsetSensitivity = DEFAULT_ONSET_SENSITIVITY;
    silenceHysteresis = DEFAULT_SILENCE_HYSTERESIS;
    recomputeDerivedParameters();
}

void OnsetParameters::validate() {
    // Clamp values to valid ranges
    if (silenceThreshold > 100) silenceThreshold = 100;
    if (onsetSensitivity > 100) onsetSensitivity = 100;
    if (silenceHysteresis > 1000) silenceHysteresis = 1000;
}

OnsetParameters& OnsetParameters::getInstance() {
    static OnsetParameters instance;
    return instance;
}

}  // namespace audio
}  // namespace lightwaveos
```

---

## 2. REST Handler

**File:** `src/network/handlers/AudioOnsetHandler.cpp` (or add to `WebServer.cpp`)

```cpp
#include "WebServer.h"
#include "audio/contracts/OnsetParameters.h"
#include <ArduinoJson.h>

using namespace lightwaveos::audio;

/**
 * @brief GET /api/v1/audio/onset-parameters
 *
 * Returns current onset detection and silence gate parameters.
 */
void WebServer::handleAudioOnsetParametersGet() {
    DynamicJsonDocument doc(512);

    OnsetParameters& params = OnsetParameters::getInstance();

    doc["success"] = true;
    doc["data"]["silenceThreshold"] = params.silenceThreshold;
    doc["data"]["onsetSensitivity"] = params.onsetSensitivity;
    doc["data"]["silenceHysteresis"] = params.silenceHysteresis;
    doc["data"]["rmsThreshold"] = params.rmsThreshold;
    doc["data"]["onsetConfThreshold"] = params.onsetConfThreshold;
    doc["timestamp"] = time(nullptr);
    doc["version"] = "1.0.0";

    sendJsonResponse(200, doc);
}

/**
 * @brief POST /api/v1/audio/onset-parameters
 *
 * Set one or more onset parameters. All fields optional.
 * Include "save": true to persist to NVS.
 *
 * Request body:
 * {
 *   "silenceThreshold": 35,
 *   "onsetSensitivity": 65,
 *   "silenceHysteresis": 200,
 *   "save": true
 * }
 */
void WebServer::handleAudioOnsetParametersPost() {
    // Parse JSON request
    DynamicJsonDocument reqDoc(512);
    DeserializationError error = deserializeJson(reqDoc, _request->getBody());

    if (error) {
        return sendErrorResponse(400, "INVALID_JSON", "Request body is not valid JSON");
    }

    OnsetParameters& params = OnsetParameters::getInstance();

    DynamicJsonDocument respDoc(512);
    JsonArray updated = respDoc.createNestedArray("updated");

    // Update silenceThreshold if provided
    if (reqDoc.containsKey("silenceThreshold")) {
        uint16_t val = reqDoc["silenceThreshold"];
        if (val > 100) {
            return sendErrorResponse(400, "OUT_OF_RANGE",
                                     "silenceThreshold must be 0–100");
        }
        params.silenceThreshold = val;
        updated.add("silenceThreshold");
    }

    // Update onsetSensitivity if provided
    if (reqDoc.containsKey("onsetSensitivity")) {
        uint16_t val = reqDoc["onsetSensitivity"];
        if (val > 100) {
            return sendErrorResponse(400, "OUT_OF_RANGE",
                                     "onsetSensitivity must be 0–100");
        }
        params.onsetSensitivity = val;
        updated.add("onsetSensitivity");
    }

    // Update silenceHysteresis if provided
    if (reqDoc.containsKey("silenceHysteresis")) {
        uint16_t val = reqDoc["silenceHysteresis"];
        if (val > 1000) {
            return sendErrorResponse(400, "OUT_OF_RANGE",
                                     "silenceHysteresis must be 0–1000 ms");
        }
        params.silenceHysteresis = val;
        updated.add("silenceHysteresis");
    }

    // Recompute derived parameters
    params.validate();
    params.recomputeDerivedParameters();

    // Save to NVS if requested
    bool save = reqDoc["save"] | false;
    if (save) {
        params.saveToNVS();
    }

    // Build response
    respDoc["success"] = true;
    respDoc["data"]["updated"] = updated;
    respDoc["data"]["silenceThreshold"] = params.silenceThreshold;
    respDoc["data"]["onsetSensitivity"] = params.onsetSensitivity;
    respDoc["data"]["silenceHysteresis"] = params.silenceHysteresis;
    respDoc["data"]["rmsThreshold"] = params.rmsThreshold;
    respDoc["data"]["onsetConfThreshold"] = params.onsetConfThreshold;
    respDoc["timestamp"] = time(nullptr);
    respDoc["version"] = "1.0.0";

    sendJsonResponse(200, respDoc);

    // Broadcast update to all WebSocket clients
    broadcastOnsetParametersUpdate();
}

/**
 * @brief PATCH /api/v1/audio/onset-parameters
 *
 * Identical to POST. Both methods use same request/response contract.
 */
void WebServer::handleAudioOnsetParametersPatch() {
    // Delegate to POST handler
    handleAudioOnsetParametersPost();
}

/**
 * @brief Broadcast onset parameter update to all connected WebSocket clients.
 */
void WebServer::broadcastOnsetParametersUpdate() {
    OnsetParameters& params = OnsetParameters::getInstance();

    DynamicJsonDocument doc(512);
    doc["type"] = "onset.parameters.updated";
    doc["silenceThreshold"] = params.silenceThreshold;
    doc["onsetSensitivity"] = params.onsetSensitivity;
    doc["silenceHysteresis"] = params.silenceHysteresis;

    // Send to all connected WebSocket clients
    _ws->textAll(doc.as<String>());
}
```

**In WebServer constructor or route registration:**

```cpp
// Add to route handlers
server->on("/api/v1/audio/onset-parameters", HTTP_GET,
    [this](AsyncWebServerRequest *request) { this->handleAudioOnsetParametersGet(); });
server->on("/api/v1/audio/onset-parameters", HTTP_POST,
    [this](AsyncWebServerRequest *request) { this->handleAudioOnsetParametersPost(); });
server->on("/api/v1/audio/onset-parameters", HTTP_PATCH,
    [this](AsyncWebServerRequest *request) { this->handleAudioOnsetParametersPatch(); });
```

---

## 3. WebSocket Handler

**File:** `src/network/WebSocketManager.cpp` (add to `handleMessage()`)

```cpp
void WebSocketManager::handleMessage(AsyncWebSocketClient* client, char* msg, size_t msgLen) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, msg);

    if (error) {
        sendErrorResponse(client, "INVALID_JSON");
        return;
    }

    String type = doc["type"];

    // ... existing handlers ...

    // New onset parameter handlers
    if (type == "onset.parameters.get") {
        handleOnsetParametersGet(client, doc);
    }
    else if (type == "onset.parameters.set") {
        handleOnsetParametersSet(client, doc);
    }
    else if (type == "onset.parameters.save") {
        handleOnsetParametersSave(client, doc);
    }
    else {
        sendErrorResponse(client, "UNKNOWN_COMMAND");
    }
}

void WebSocketManager::handleOnsetParametersGet(AsyncWebSocketClient* client, const DynamicJsonDocument& reqDoc) {
    OnsetParameters& params = OnsetParameters::getInstance();

    DynamicJsonDocument respDoc(512);
    respDoc["type"] = "onset.parameters";
    respDoc["success"] = true;
    respDoc["data"]["silenceThreshold"] = params.silenceThreshold;
    respDoc["data"]["onsetSensitivity"] = params.onsetSensitivity;
    respDoc["data"]["silenceHysteresis"] = params.silenceHysteresis;
    respDoc["data"]["rmsThreshold"] = params.rmsThreshold;
    respDoc["data"]["onsetConfThreshold"] = params.onsetConfThreshold;

    client->text(respDoc.as<String>());
}

void WebSocketManager::handleOnsetParametersSet(AsyncWebSocketClient* client, const DynamicJsonDocument& reqDoc) {
    OnsetParameters& params = OnsetParameters::getInstance();

    // Validate and apply each parameter
    if (reqDoc.containsKey("silenceThreshold")) {
        uint16_t val = reqDoc["silenceThreshold"];
        if (val <= 100) params.silenceThreshold = val;
    }
    if (reqDoc.containsKey("onsetSensitivity")) {
        uint16_t val = reqDoc["onsetSensitivity"];
        if (val <= 100) params.onsetSensitivity = val;
    }
    if (reqDoc.containsKey("silenceHysteresis")) {
        uint16_t val = reqDoc["silenceHysteresis"];
        if (val <= 1000) params.silenceHysteresis = val;
    }

    params.validate();
    params.recomputeDerivedParameters();

    // Broadcast to all clients (including sender)
    DynamicJsonDocument bcastDoc(512);
    bcastDoc["type"] = "onset.parameters.updated";
    bcastDoc["silenceThreshold"] = params.silenceThreshold;
    bcastDoc["onsetSensitivity"] = params.onsetSensitivity;
    bcastDoc["silenceHysteresis"] = params.silenceHysteresis;

    _ws->textAll(bcastDoc.as<String>());
}

void WebSocketManager::handleOnsetParametersSave(AsyncWebSocketClient* client, const DynamicJsonDocument& reqDoc) {
    OnsetParameters& params = OnsetParameters::getInstance();
    params.saveToNVS();

    DynamicJsonDocument respDoc(256);
    respDoc["type"] = "onset.parameters.saved";
    respDoc["success"] = true;
    respDoc["message"] = "Parameters saved to NVS";

    client->text(respDoc.as<String>());
}
```

---

## 4. Serial CLI Handler

**File:** `src/serial/SerialCLI.cpp` (add to `handleCommand()`)

```cpp
void SerialCLI::handleCommand(const String& cmd, const String& arg) {
    // ... existing commands ...

    if (cmd == "audio") {
        handleAudioCommand(arg);
    }
    // ... more commands ...
}

void SerialCLI::handleAudioCommand(const String& arg) {
    if (arg == "onset get") {
        handleAudioOnsetGet();
    }
    else if (arg.startsWith("onset set ")) {
        String params = arg.substring(10);  // Skip "onset set "
        handleAudioOnsetSet(params);
    }
    else if (arg == "onset save") {
        handleAudioOnsetSave();
    }
    else {
        Serial.println("Usage:");
        Serial.println("  audio onset get      - Show current parameters");
        Serial.println("  audio onset set S O H - Set silenceThreshold, onsetSens, hysteresis");
        Serial.println("  audio onset save     - Save to NVS");
    }
}

void SerialCLI::handleAudioOnsetGet() {
    OnsetParameters& params = OnsetParameters::getInstance();

    Serial.printf("silenceThreshold: %u\n", params.silenceThreshold);
    Serial.printf("onsetSensitivity: %u\n", params.onsetSensitivity);
    Serial.printf("silenceHysteresis: %u ms\n", params.silenceHysteresis);
    Serial.printf("rmsThreshold: %.4f\n", params.rmsThreshold);
    Serial.printf("onsetConfThreshold: %.4f\n", params.onsetConfThreshold);
}

void SerialCLI::handleAudioOnsetSet(const String& args) {
    // Parse "35 65 200" -> three integers
    int threshold = 0, sensitivity = 0, hysteresis = 0;
    int parsed = sscanf(args.c_str(), "%d %d %d", &threshold, &sensitivity, &hysteresis);

    if (parsed != 3) {
        Serial.println("Error: expected 3 integers (silenceThreshold, onsetSensitivity, silenceHysteresis)");
        Serial.println("Example: audio onset set 35 65 200");
        return;
    }

    OnsetParameters& params = OnsetParameters::getInstance();
    params.silenceThreshold = threshold;
    params.onsetSensitivity = sensitivity;
    params.silenceHysteresis = hysteresis;
    params.validate();
    params.recomputeDerivedParameters();

    Serial.printf("Updated: silenceThreshold=%u, onsetSensitivity=%u, silenceHysteresis=%u\n",
                  params.silenceThreshold, params.onsetSensitivity, params.silenceHysteresis);
}

void SerialCLI::handleAudioOnsetSave() {
    OnsetParameters& params = OnsetParameters::getInstance();
    params.saveToNVS();
    Serial.println("Saved to NVS");
}
```

---

## 5. OnsetDetector Integration

**File:** `src/audio/OnsetDetector.cpp` (integration example)

```cpp
#include "OnsetDetector.h"
#include "contracts/OnsetParameters.h"

// In tick() or update method:
void OnsetDetector::tick(const ControlBusFrame& frame) {
    OnsetParameters& params = OnsetParameters::getInstance();

    // 1. Read current RMS from audio frame
    float currentRms = frame.rms;

    // 2. Apply silence gate threshold
    bool isSilent = (currentRms < params.rmsThreshold);

    // 3. Update silence hysteresis counter
    if (isSilent) {
        m_silenceCounter++;
        if (m_silenceCounter * HOP_SIZE_MS > params.silenceHysteresis) {
            m_isSilent = true;
        }
    } else {
        m_silenceCounter = 0;
        m_isSilent = false;
    }

    // 4. Detect onsets (if not silent)
    if (!m_isSilent && frame.flux > FLUX_THRESHOLD) {
        float confidence = calculateOnsetConfidence(frame);

        // 5. Use dynamic sensitivity threshold
        if (confidence > params.onsetConfThreshold) {
            fireOnsetEvent();
        }
    }
}
```

---

## 6. Boot Integration

**File:** `src/core/SystemInit.cpp` (add to boot sequence)

```cpp
void SystemInit::initAudio() {
    // ... existing audio init ...

    // Load onset parameters from NVS
    OnsetParameters& params = OnsetParameters::getInstance();
    bool loaded = params.loadFromNVS();

    if (!loaded) {
        Log::info("SystemInit", "No saved onset parameters; using defaults");
        params.resetToDefaults();
    }

    Log::info("SystemInit", "Onset parameters: silThresh=%u, onsetSens=%u, silHyst=%u",
              params.silenceThreshold, params.onsetSensitivity, params.silenceHysteresis);
}
```

---

## Testing Examples

**C++ Unit Test (mock):**

```cpp
TEST(OnsetParameters, NormalizationAndRecompute) {
    OnsetParameters params;

    // Test default values
    EXPECT_EQ(params.silenceThreshold, 30);
    EXPECT_EQ(params.onsetSensitivity, 60);
    EXPECT_EQ(params.silenceHysteresis, 200);

    // Test derived parameter computation
    params.recomputeDerivedParameters(0.1f);  // maxRms = 0.1
    EXPECT_NEAR(params.rmsThreshold, 0.03f, 0.001f);        // 30% of 0.1
    EXPECT_NEAR(params.onsetConfThreshold, 0.54f, 0.001f);  // 0.7 - (60/100)*0.4

    // Test bounds clamping
    params.silenceThreshold = 150;
    params.validate();
    EXPECT_EQ(params.silenceThreshold, 100);
}

TEST(OnsetParameters, NVSPersistence) {
    OnsetParameters params;
    params.silenceThreshold = 40;
    params.onsetSensitivity = 70;
    params.silenceHysteresis = 300;
    params.saveToNVS();

    // Simulate power cycle
    OnsetParameters params2;
    bool loaded = params2.loadFromNVS();
    EXPECT_TRUE(loaded);
    EXPECT_EQ(params2.silenceThreshold, 40);
    EXPECT_EQ(params2.onsetSensitivity, 70);
    EXPECT_EQ(params2.silenceHysteresis, 300);
}
```

---

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:search | Initial code examples. OnsetParameters struct, REST handler, WebSocket dispatcher, serial CLI parser, NVS persistence, OnsetDetector integration, boot sequence, unit test mocks. |

