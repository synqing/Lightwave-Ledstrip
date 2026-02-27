/**
 * @file test_ws_audio_codec.cpp
 * @brief Unit tests for WsAudioCodec JSON parsing and encoder allow-list validation
 *
 * Tests audio WebSocket command decoding (requestId extraction, optional nested updates)
 * and encoder functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/WsAudioCodec.h"

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

void test_audio_parameters_get_decode_with_request_id() {
    const char* json = R"({"requestId": "test123"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersGetDecodeResult result = WsAudioCodec::decodeParametersGet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_audio_parameters_get_decode_without_request_id() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersGetDecodeResult result = WsAudioCodec::decodeParametersGet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

void test_audio_parameters_set_decode_pipeline_only() {
    const char* json = R"({"pipeline": {"dcAlpha": 0.002, "agcTargetRms": 0.3}})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersSetDecodeResult result = WsAudioCodec::decodeParametersSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasPipeline, "hasPipeline should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.pipeline.hasDcAlpha, "hasDcAlpha should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 0.002f, result.request.pipeline.dcAlpha, "dcAlpha should be 0.002");
    TEST_ASSERT_TRUE_MESSAGE(result.request.pipeline.hasAgcTargetRms, "hasAgcTargetRms should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.3f, result.request.pipeline.agcTargetRms, "agcTargetRms should be 0.3");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasControlBus, "hasControlBus should be false");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasContract, "hasContract should be false");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasResetState, "hasResetState should be false");
}

void test_audio_parameters_set_decode_contract_only() {
    const char* json = R"({"contract": {"bpmMin": 60.0, "beatsPerBar": 3}})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersSetDecodeResult result = WsAudioCodec::decodeParametersSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasContract, "hasContract should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.contract.hasBpmMin, "hasBpmMin should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 60.0f, result.request.contract.bpmMin, "bpmMin should be 60.0");
    TEST_ASSERT_TRUE_MESSAGE(result.request.contract.hasBeatsPerBar, "hasBeatsPerBar should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, result.request.contract.beatsPerBar, "beatsPerBar should be 3");
}

void test_audio_parameters_set_decode_reset_state() {
    const char* json = R"({"resetState": true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioParametersSetDecodeResult result = WsAudioCodec::decodeParametersSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasResetState, "hasResetState should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.resetState, "resetState should be true");
}

void test_audio_zone_agc_set_decode_all_fields() {
    const char* json = R"({"enabled": true, "lookaheadEnabled": false, "attackRate": 0.1, "releaseRate": 0.02, "minFloor": 0.001})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioZoneAgcSetDecodeResult result = WsAudioCodec::decodeZoneAgcSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasEnabled, "hasEnabled should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.enabled, "enabled should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasLookaheadEnabled, "hasLookaheadEnabled should be true");
    TEST_ASSERT_FALSE_MESSAGE(result.request.lookaheadEnabled, "lookaheadEnabled should be false");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasAttackRate, "hasAttackRate should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.1f, result.request.attackRate, "attackRate should be 0.1");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasReleaseRate, "hasReleaseRate should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.02f, result.request.releaseRate, "releaseRate should be 0.02");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasMinFloor, "hasMinFloor should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0001f, 0.001f, result.request.minFloor, "minFloor should be 0.001");
}

void test_audio_zone_agc_set_decode_partial() {
    const char* json = R"({"enabled": false})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioZoneAgcSetDecodeResult result = WsAudioCodec::decodeZoneAgcSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasEnabled, "hasEnabled should be true");
    TEST_ASSERT_FALSE_MESSAGE(result.request.enabled, "enabled should be false");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasLookaheadEnabled, "hasLookaheadEnabled should be false");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasMinFloor, "hasMinFloor should be false");
}

void test_audio_subscribe_decode_with_request_id() {
    const char* json = R"({"requestId": "sub1"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioSubscribeDecodeResult result = WsAudioCodec::decodeSubscribe(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("sub1", result.request.requestId);
}

void test_audio_simple_decode_without_request_id() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    AudioSimpleDecodeResult result = WsAudioCodec::decodeSimple(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Encode tests (response payload allow-lists)
// ============================================================================

void test_audio_encode_parameters_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioPipelineTuningData pipeline{0.001f, 0.25f, 1.0f, 40.0f, 0.03f, 0.015f, 0.90f, 0.01f, 0.0004f, 0.0005f, 0.01f, 1.5f, 1.5f, 0.0005f, -65.0f, -12.0f, -65.0f, -12.0f, -65.0f, -12.0f, 1.0f, 0.35f, 0.12f};
    AudioContractTuningData contract{100.0f, 30.0f, 300.0f, 0.50f, 1.00f, 0.35f, 0.20f, 4, 4};
    AudioDspStateData state{0.1f, 0.2f, 0.15f, 0.3f, 1.5f, 0.001f, 0.0005f, -100, 100, 0, 0.0f, 0};
    AudioCapabilitiesData caps{12800, 256, 512, 512, 8, 12, 128};

    WsAudioCodec::encodeParametersGet(pipeline, contract, state, caps, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("pipeline"), "pipeline object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("controlBus"), "controlBus object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("contract"), "contract object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("state"), "state object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("capabilities"), "capabilities object should be present");

    const char* topKeys[] = {"pipeline", "controlBus", "contract", "state", "capabilities"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 5), "top-level data should only have required keys");

    JsonObject pipelineObj = data["pipeline"].as<JsonObject>();
    const char* pipelineKeys[] = {"dcAlpha", "agcTargetRms", "agcMinGain", "agcMaxGain", "agcAttack", "agcRelease", "agcClipReduce", "agcIdleReturnRate", "noiseFloorMin", "noiseFloorRise", "noiseFloorFall", "gateStartFactor", "gateRangeFactor", "gateRangeMin", "rmsDbFloor", "rmsDbCeil", "bandDbFloor", "bandDbCeil", "chromaDbFloor", "chromaDbCeil", "fluxScale"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(pipelineObj, pipelineKeys, 21), "pipeline object should only have required keys");

    JsonObject controlBusObj = data["controlBus"].as<JsonObject>();
    const char* controlBusKeys[] = {"alphaFast", "alphaSlow"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(controlBusObj, controlBusKeys, 2), "controlBus object should only have required keys");

    JsonObject contractObj = data["contract"].as<JsonObject>();
    const char* contractKeys[] = {"audioStalenessMs", "bpmMin", "bpmMax", "bpmTau", "confidenceTau", "phaseCorrectionGain", "barCorrectionGain", "beatsPerBar", "beatUnit"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(contractObj, contractKeys, 9), "contract object should only have required keys");

    JsonObject stateObj = data["state"].as<JsonObject>();
    const char* stateKeys[] = {"rmsRaw", "rmsMapped", "rmsPreGain", "fluxMapped", "agcGain", "dcEstimate", "noiseFloor", "minSample", "maxSample", "peakCentered", "meanSample", "clipCount"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(stateObj, stateKeys, 12), "state object should only have required keys");

    JsonObject capsObj = data["capabilities"].as<JsonObject>();
    const char* capsKeys[] = {"sampleRate", "hopSize", "fftSize", "goertzelWindow", "bandCount", "chromaCount", "waveformPoints"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(capsObj, capsKeys, 7), "capabilities object should only have required keys");
}

void test_audio_encode_parameters_changed_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsAudioCodec::encodeParametersChanged(true, false, true, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("updated"), "updated array should be present");
    JsonArray updated = data["updated"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, updated.size(), "updated array should have 2 entries (pipeline, state)");

    const char* keys[] = {"updated"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 1), "parameters.changed should only have updated key");
}

void test_audio_encode_subscribed_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsAudioCodec::encodeSubscribed(123, 464, 1, 8, 12, 128, 30, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(123, data["clientId"].as<uint32_t>(), "clientId should be 123");
    TEST_ASSERT_EQUAL_INT_MESSAGE(464, data["frameSize"].as<uint16_t>(), "frameSize should be 464");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, data["streamVersion"].as<uint8_t>(), "streamVersion should be 1");
    TEST_ASSERT_EQUAL_STRING("ok", data["status"].as<const char*>());

    const char* keys[] = {"clientId", "frameSize", "streamVersion", "numBands", "numChroma", "waveformSize", "targetFps", "status"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 8), "subscribed should only have required keys");
}

void test_audio_encode_unsubscribed_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsAudioCodec::encodeUnsubscribed(456, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(456, data["clientId"].as<uint32_t>(), "clientId should be 456");
    TEST_ASSERT_EQUAL_STRING("ok", data["status"].as<const char*>());

    const char* keys[] = {"clientId", "status"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 2), "unsubscribed should only have clientId and status");
}

void test_audio_encode_zone_agc_state_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioZoneAgcZoneData zones[4] = {
        {0, 0.5f, 0.8f},
        {1, 0.6f, 0.9f},
        {2, 0.7f, 1.0f},
        {3, 0.4f, 0.7f}
    };

    WsAudioCodec::encodeZoneAgcState(true, false, zones, 4, data);

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_FALSE_MESSAGE(data["lookaheadEnabled"].as<bool>(), "lookaheadEnabled should be false");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("zones"), "zones array should be present");

    JsonArray zonesArray = data["zones"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, zonesArray.size(), "zones array should have 4 entries");

    const char* topKeys[] = {"enabled", "lookaheadEnabled", "zones"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 3), "zone-agc.state should only have required keys");

    JsonObject zone0 = zonesArray[0].as<JsonObject>();
    const char* zoneKeys[] = {"index", "follower", "maxMag"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(zone0, zoneKeys, 3), "zone object should only have required keys");
}

void test_audio_encode_zone_agc_updated_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsAudioCodec::encodeZoneAgcUpdated(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data["updated"].as<bool>(), "updated should be true");

    const char* keys[] = {"updated"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 1), "zone-agc.updated should only have updated key");
}

void test_audio_encode_spike_detection_state_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    AudioSpikeDetectionStatsData stats{1000, 50, 30, 20, 5.5f, 0.05f, 0.1f};
    WsAudioCodec::encodeSpikeDetectionState(true, stats, data);

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("stats"), "stats object should be present");

    JsonObject statsObj = data["stats"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1000, statsObj["totalFrames"].as<uint32_t>(), "totalFrames should be 1000");

    const char* topKeys[] = {"enabled", "stats"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 2), "spike-detection.state should only have enabled and stats");

    const char* statsKeys[] = {"totalFrames", "spikesDetectedBands", "spikesDetectedChroma", "spikesCorrected", "totalEnergyRemoved", "avgSpikesPerFrame", "avgCorrectionMagnitude"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(statsObj, statsKeys, 7), "stats object should only have required keys");
}

void test_audio_encode_spike_detection_reset_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsAudioCodec::encodeSpikeDetectionReset(data);

    TEST_ASSERT_TRUE_MESSAGE(data["reset"].as<bool>(), "reset should be true");

    const char* keys[] = {"reset"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 1), "spike-detection.reset should only have reset key");
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
    RUN_TEST(test_audio_parameters_get_decode_with_request_id);
    RUN_TEST(test_audio_parameters_get_decode_without_request_id);
    RUN_TEST(test_audio_parameters_set_decode_pipeline_only);
    RUN_TEST(test_audio_parameters_set_decode_contract_only);
    RUN_TEST(test_audio_parameters_set_decode_reset_state);
    RUN_TEST(test_audio_zone_agc_set_decode_all_fields);
    RUN_TEST(test_audio_zone_agc_set_decode_partial);
    RUN_TEST(test_audio_subscribe_decode_with_request_id);
    RUN_TEST(test_audio_simple_decode_without_request_id);

    // Encode allow-lists
    RUN_TEST(test_audio_encode_parameters_get_allow_list);
    RUN_TEST(test_audio_encode_parameters_changed_allow_list);
    RUN_TEST(test_audio_encode_subscribed_allow_list);
    RUN_TEST(test_audio_encode_unsubscribed_allow_list);
    RUN_TEST(test_audio_encode_zone_agc_state_allow_list);
    RUN_TEST(test_audio_encode_zone_agc_updated_allow_list);
    RUN_TEST(test_audio_encode_spike_detection_state_allow_list);
    RUN_TEST(test_audio_encode_spike_detection_reset_allow_list);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
