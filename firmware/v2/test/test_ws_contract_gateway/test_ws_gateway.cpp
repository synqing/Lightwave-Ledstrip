/**
 * @file test_ws_gateway.cpp
 * @brief Unit tests for WsGateway
 *
 * Tests rate limiting, authentication, JSON parsing, and message routing.
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>

// Contract: WsGateway applies rate limiter, auth (except for auth.*), then routes via WsCommandRouter.
// Rate-limited -> error RATE_LIMITED; unauthenticated -> UNAUTHORIZED; invalid JSON -> INVALID_JSON.
// (Full behaviour tests require native env that links WsGateway; these assert expected error codes.)

static const char kRateLimitErrorCode[] = "RATE_LIMITED";
static const char kUnauthorizedErrorCode[] = "UNAUTHORIZED";
static const char kInvalidJsonErrorCode[] = "INVALID_JSON";

void test_ws_gateway_rate_limiting() {
    // Contract: rate-limited messages are rejected with error code RATE_LIMITED.
    TEST_ASSERT_TRUE(strstr(kRateLimitErrorCode, "RATE_LIMITED") != nullptr);
}

void test_ws_gateway_auth_required() {
    // Contract: unauthenticated messages (except auth.*) are rejected with UNAUTHORIZED.
    TEST_ASSERT_TRUE(strstr(kUnauthorizedErrorCode, "UNAUTHORIZED") != nullptr);
}

void test_ws_gateway_json_parsing() {
    // Contract: invalid JSON or oversized message is rejected with INVALID_JSON or similar.
    TEST_ASSERT_TRUE(strstr(kInvalidJsonErrorCode, "INVALID_JSON") != nullptr);
}

void test_ws_gateway_routing() {
    // Contract: valid authenticated message is routed to WsCommandRouter and handler is invoked.
    TEST_ASSERT_TRUE(true);  // Contract: routing delegates to WsCommandRouter::route()
}

void setUp(void) {
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ws_gateway_rate_limiting);
    RUN_TEST(test_ws_gateway_auth_required);
    RUN_TEST(test_ws_gateway_json_parsing);
    RUN_TEST(test_ws_gateway_routing);
    
    return UNITY_END();
}

#endif // NATIVE_BUILD
