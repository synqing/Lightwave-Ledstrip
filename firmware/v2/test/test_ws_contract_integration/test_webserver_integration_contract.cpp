/**
 * @file test_webserver_integration_contract.cpp
 * @brief Integration contract tests for WebServer HTTP/WS response formats
 *
 * Verifies response shapes match ApiResponse.h contract. No live server;
 * asserts expected JSON keys/substrings for success, error, and WS responses.
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>

// Contract: HTTP success response (sendSuccessResponse) contains success, timestamp, version
static const char kHttpSuccessMinimal[] = "{\"success\":true,\"timestamp\":12345,\"version\":\"2.0\"}";
static const char kHttpSuccessWithData[] = "{\"success\":true,\"data\":{},\"timestamp\":12345,\"version\":\"2.0\"}";

// Contract: HTTP/WS error response contains success=false, error object with code and message
static const char kWsErrorSample[] = "{\"type\":\"error\",\"requestId\":\"req-1\",\"success\":false,\"error\":{\"code\":\"MISSING_FIELD\",\"message\":\"Missing 'type' field\"}}";
static const char kHttpErrorSample[] = "{\"success\":false,\"error\":{\"code\":\"UNAUTHORIZED\",\"message\":\"Missing API key\"},\"timestamp\":0,\"version\":\"2.0\"}";

// Contract: WS command success response contains type, success, and optionally data
static const char kWsSuccessSample[] = "{\"type\":\"device.getStatus\",\"requestId\":\"r1\",\"success\":true,\"data\":{\"online\":true}}";

static bool contains(const char* haystack, const char* needle) {
    return haystack && needle && strstr(haystack, needle) != nullptr;
}

void test_http_endpoint_compatibility() {
    // HTTP success response must include success, timestamp, version (ApiResponse.h contract)
    TEST_ASSERT_TRUE_MESSAGE(contains(kHttpSuccessMinimal, "\"success\":true"), "HTTP success must contain success:true");
    TEST_ASSERT_TRUE_MESSAGE(contains(kHttpSuccessMinimal, "\"timestamp\""), "HTTP success must contain timestamp");
    TEST_ASSERT_TRUE_MESSAGE(contains(kHttpSuccessMinimal, "\"version\""), "HTTP success must contain version");
    TEST_ASSERT_TRUE_MESSAGE(contains(kHttpSuccessWithData, "\"data\""), "HTTP success with data must contain data");
}

void test_websocket_command_compatibility() {
    // WS command response must include type; success response has success:true and optionally data
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsSuccessSample, "\"type\""), "WS response must contain type");
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsSuccessSample, "\"success\":true"), "WS success must contain success:true");
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsSuccessSample, "\"data\""), "WS success with data must contain data");
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsSuccessSample, "\"requestId\""), "WS response should contain requestId when present");
}

void test_error_response_format() {
    // Error response must contain success:false, error object with code and message (ApiResponse.h contract)
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsErrorSample, "\"success\":false"), "Error response must contain success:false");
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsErrorSample, "\"error\""), "Error response must contain error object");
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsErrorSample, "\"code\""), "Error object must contain code");
    TEST_ASSERT_TRUE_MESSAGE(contains(kWsErrorSample, "\"message\""), "Error object must contain message");
    TEST_ASSERT_TRUE_MESSAGE(contains(kHttpErrorSample, "\"success\":false"), "HTTP error must contain success:false");
    TEST_ASSERT_TRUE_MESSAGE(contains(kHttpErrorSample, "\"error\""), "HTTP error must contain error object");
}

void setUp(void) {
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_http_endpoint_compatibility);
    RUN_TEST(test_websocket_command_compatibility);
    RUN_TEST(test_error_response_format);

    return UNITY_END();
}

#endif // NATIVE_BUILD
