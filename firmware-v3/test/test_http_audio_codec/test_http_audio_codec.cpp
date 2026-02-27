/**
 * @file test_http_audio_codec.cpp
 * @brief Unit tests for HttpAudioCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP audio endpoint decoding (optional fields, defaults) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpAudioCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Load JSON from string and parse into JsonDocument
 */
static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

/**
 * @brief Validate keys against allow-list (no extras, no missing)
 */
static bool validateKeysAgainstAllowList(const JsonObject& obj, const char* allowedKeys[], size_t allowedCount) {
    size_t foundCount = 0;
    for (JsonPair kv : obj) {
        const char* key = kv.key().c_str();
        bool isAllowed = false;
        for (size_t i = 0; i < allowedCount; i++) {
            if (strcmp(key, allowedKeys[i]) == 0) {
                isAllowed = true;
                foundCount++;
                break;
            }
        }
        if (!isAllowed) {
            return false;
        }
    }
    for (size_t i = 0; i < allowedCount; i++) {
        if (!obj.containsKey(allowedKeys[i])) {
            return false;
        }
    }
    return foundCount == allowedCount;
}

// ============================================================================
// Decode tests
// ============================================================================

void test_http_audio_parameters_set_decode_pipeline_only() {
    const char* json = R"({"pipeline": {"dcAlpha": 0.002, "agcTargetRms": 0.3}})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersSetDecodeResult result = HttpAudioCodec::decodeParametersSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasPipeline, "hasPipeline should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.pipeline.hasDcAlpha, "hasDcAlpha should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 0.002f, result.request.pipeline.dcAlpha, "dcAlpha should be 0.002");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasResetState, "hasResetState should be false");
}

void test_http_audio_parameters_set_decode_reset_state() {
    const char* json = R"({"resetState": true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersSetDecodeResult result = HttpAudioCodec::decodeParametersSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasResetState, "hasResetState should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.resetState, "resetState should be true");
}

void test_http_audio_control_decode_pause() {
    const char* json = R"({"action": "pause"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioControlDecodeResult result = HttpAudioCodec::decodeControl(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("pause", result.request.action);
}

void test_http_audio_control_decode_resume() {
    const char* json = R"({"action": "resume"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioControlDecodeResult result = HttpAudioCodec::decodeControl(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("resume", result.request.action);
}

void test_http_audio_preset_save_decode_with_name() {
    const char* json = R"({"name": "My Preset"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioPresetSaveDecodeResult result = HttpAudioCodec::decodePresetSave(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("My Preset", result.request.name);
}

void test_http_audio_preset_save_decode_default_name() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioPresetSaveDecodeResult result = HttpAudioCodec::decodePresetSave(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("Unnamed", result.request.name);
}

void test_http_audio_zone_agc_set_decode_all_fields() {
    const char* json = R"({"enabled": true, "lookaheadEnabled": false, "attackRate": 0.1, "releaseRate": 0.02, "minFloor": 0.001})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioZoneAgcSetDecodeResult result = HttpAudioCodec::decodeZoneAgcSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasEnabled, "hasEnabled should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.enabled, "enabled should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.1f, result.request.attackRate, "attackRate should be 0.1");
}

void test_http_audio_calibrate_start_decode_with_params() {
    const char* json = R"({"durationMs": 5000, "safetyMultiplier": 1.5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioCalibrateStartDecodeResult result = HttpAudioCodec::decodeCalibrateStart(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasDurationMs, "hasDurationMs should be true");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5000, result.request.durationMs, "durationMs should be 5000");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasSafetyMultiplier, "hasSafetyMultiplier should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 1.5f, result.request.safetyMultiplier, "safetyMultiplier should be 1.5");
}

void test_http_audio_calibrate_start_decode_defaults() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioCalibrateStartDecodeResult result = HttpAudioCodec::decodeCalibrateStart(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasDurationMs, "hasDurationMs should be false");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3000, result.request.durationMs, "durationMs should default to 3000");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasSafetyMultiplier, "hasSafetyMultiplier should be false");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 1.2f, result.request.safetyMultiplier, "safetyMultiplier should default to 1.2");
}

// ============================================================================
// Encode tests (response payload allow-lists)
// ============================================================================

void test_http_audio_encode_parameters_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioPipelineTuningData pipeline{0.001f, 0.25f, 1.0f, 40.0f, 0.03f, 0.015f, 0.90f, 0.01f, 0.0004f, 0.0005f, 0.01f, 1.5f, 1.5f, 0.0005f, -65.0f, -12.0f, -65.0f, -12.0f, -65.0f, -12.0f, 1.0f, 0.35f, 0.12f};
    AudioContractTuningData contract{100.0f, 30.0f, 300.0f, 0.50f, 1.00f, 0.35f, 0.20f, 4, 4};
    AudioDspStateData state{0.1f, 0.2f, 0.15f, 0.3f, 1.5f, 0.001f, 0.0005f, -100, 100, 0, 0.0f, 0};
    AudioCapabilitiesData caps{12800, 256, 512, 512, 8, 12, 128};

    HttpAudioCodec::encodeParametersGet(pipeline, contract, state, caps, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("pipeline"), "pipeline object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("controlBus"), "controlBus object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("contract"), "contract object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("state"), "state object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("capabilities"), "capabilities object should be present");

    const char* topKeys[] = {"pipeline", "controlBus", "contract", "state", "capabilities"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 5), "top-level data should only have required keys");
}

void test_http_audio_encode_control_response_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    HttpAudioCodec::encodeControlResponse("PAUSED", "pause", data);

    TEST_ASSERT_EQUAL_STRING("PAUSED", data["state"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("pause", data["action"].as<const char*>());

    const char* keys[] = {"state", "action"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 2), "control response should only have state and action");
}

void test_http_audio_encode_state_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioActorStateData state{"RUNNING", true, 1000, 50000, 2000, 1900, 100};
    HttpAudioCodec::encodeStateGet(state, data);

    TEST_ASSERT_EQUAL_STRING("RUNNING", data["state"].as<const char*>());
    TEST_ASSERT_TRUE_MESSAGE(data["capturing"].as<bool>(), "capturing should be true");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1000, data["hopCount"].as<uint32_t>(), "hopCount should be 1000");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("stats"), "stats object should be present");

    const char* topKeys[] = {"state", "capturing", "hopCount", "sampleIndex", "stats"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 5), "state.get should only have required keys");

    JsonObject statsObj = data["stats"].as<JsonObject>();
    const char* statsKeys[] = {"tickCount", "captureSuccess", "captureFail"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(statsObj, statsKeys, 3), "stats object should only have required keys");
}

void test_http_audio_encode_tempo_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioTempoData tempo{120.0f, 0.8f, 0.5f, 0.25f, 2, 4};
    HttpAudioCodec::encodeTempoGet(tempo, data);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 120.0f, data["bpm"].as<float>(), "bpm should be 120.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 0.8f, data["confidence"].as<float>(), "confidence should be 0.8");

    const char* keys[] = {"bpm", "confidence", "beat_phase", "bar_phase", "beat_in_bar", "beats_per_bar"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 6), "tempo.get should only have required keys");
}

void test_http_audio_encode_zone_agc_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioZoneAgcZoneData zones[4] = {
        {0, 0.5f, 0.8f},
        {1, 0.6f, 0.9f},
        {2, 0.7f, 1.0f},
        {3, 0.4f, 0.7f}
    };

    HttpAudioCodec::encodeZoneAgcGet(true, false, zones, 4, data);

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_FALSE_MESSAGE(data["lookaheadEnabled"].as<bool>(), "lookaheadEnabled should be false");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("zones"), "zones array should be present");

    const char* topKeys[] = {"enabled", "lookaheadEnabled", "zones"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 3), "zone-agc.get should only have required keys");
}

void test_http_audio_encode_spike_detection_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioSpikeDetectionStatsData stats{1000, 50, 30, 20, 5.5f, 0.05f, 0.1f};
    HttpAudioCodec::encodeSpikeDetectionGet(true, stats, data);

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("stats"), "stats object should be present");

    const char* topKeys[] = {"enabled", "stats"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 2), "spike-detection.get should only have enabled and stats");
}

// ============================================================================
// Unity setUp/tearDown (required by Unity framework)
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();

    // Decode
    RUN_TEST(test_http_audio_parameters_set_decode_pipeline_only);
    RUN_TEST(test_http_audio_parameters_set_decode_reset_state);
    RUN_TEST(test_http_audio_control_decode_pause);
    RUN_TEST(test_http_audio_control_decode_resume);
    RUN_TEST(test_http_audio_preset_save_decode_with_name);
    RUN_TEST(test_http_audio_preset_save_decode_default_name);
    RUN_TEST(test_http_audio_zone_agc_set_decode_all_fields);
    RUN_TEST(test_http_audio_calibrate_start_decode_with_params);
    RUN_TEST(test_http_audio_calibrate_start_decode_defaults);

    // Encode allow-lists
    RUN_TEST(test_http_audio_encode_parameters_get_allow_list);
    RUN_TEST(test_http_audio_encode_control_response_allow_list);
    RUN_TEST(test_http_audio_encode_state_get_allow_list);
    RUN_TEST(test_http_audio_encode_tempo_get_allow_list);
    RUN_TEST(test_http_audio_encode_zone_agc_get_allow_list);
    RUN_TEST(test_http_audio_encode_spike_detection_get_allow_list);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
