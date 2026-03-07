/**
 * @file test_ws_router.cpp
 * @brief Unit tests for WsCommandRouter
 *
 * Tests command registration, dispatch routing, error handling,
 * pre-filter optimisation (first char + length), and capacity limits.
 *
 * Build: pio run -e native_test_ws_router
 * Run:   .pio/build/native_test_ws_router/program
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>

// Stub types for WebServerContext construction (must precede WebServerContext.h)
// These complete the forward declarations — never dereferenced in tests.
namespace lightwaveos {
namespace actors {
    class ActorSystem { public: char _pad = 0; };
    class RendererActor {};
}
namespace zones {
    class ZoneComposer {};
}
namespace plugins {
    class PluginManagerActor {};
}
namespace network {
    class WebServer {};
    namespace webserver {
        class RateLimiter { public: char _pad = 0; };
        class LedStreamBroadcaster {};
        class LogStreamBroadcaster {};
    }
}
}

#include "network/webserver/WsCommandRouter.h"
#include "network/webserver/WebServerContext.h"

using namespace lightwaveos::network::webserver;

// ---------------------------------------------------------------------------
// Test state
// ---------------------------------------------------------------------------

static int g_handlerCallCount = 0;
static const char* g_lastHandlerName = nullptr;
static bool g_typeFieldPresent = true;

// Dummy context globals
static lightwaveos::actors::ActorSystem s_dummyActorSystem;
static RateLimiter s_dummyRateLimiter;

static WebServerContext makeDummyContext() {
    return WebServerContext(
        s_dummyActorSystem,
        nullptr,            // renderer
        nullptr,            // zoneComposer
        nullptr,            // webServer
        s_dummyRateLimiter,
        nullptr,            // ledBroadcaster
        nullptr,            // logBroadcaster
        0,                  // startTime
        false               // apMode
    );
}

// ---------------------------------------------------------------------------
// Test handlers
// ---------------------------------------------------------------------------

static void handler_alpha(AsyncWebSocketClient* client, JsonDocument& doc,
                          const WebServerContext& ctx) {
    g_handlerCallCount++;
    g_lastHandlerName = "alpha";
}

static void handler_beta(AsyncWebSocketClient* client, JsonDocument& doc,
                         const WebServerContext& ctx) {
    g_handlerCallCount++;
    g_lastHandlerName = "beta";
}

static void handler_gamma(AsyncWebSocketClient* client, JsonDocument& doc,
                          const WebServerContext& ctx) {
    g_handlerCallCount++;
    g_lastHandlerName = "gamma";
}

static void handler_check_type_stripped(AsyncWebSocketClient* client, JsonDocument& doc,
                                        const WebServerContext& ctx) {
    g_typeFieldPresent = doc.containsKey("type");
    g_handlerCallCount++;
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp() {
    WsCommandRouter::reset();
    g_handlerCallCount = 0;
    g_lastHandlerName = nullptr;
    g_typeFieldPresent = true;
}

void tearDown() {}

// ===========================================================================
// Registration tests
// ===========================================================================

void test_initial_handler_count_is_zero() {
    TEST_ASSERT_EQUAL_size_t(0, WsCommandRouter::getHandlerCount());
}

void test_registration_increases_count() {
    WsCommandRouter::registerCommand("test.alpha", handler_alpha);
    TEST_ASSERT_EQUAL_size_t(1, WsCommandRouter::getHandlerCount());

    WsCommandRouter::registerCommand("test.beta", handler_beta);
    TEST_ASSERT_EQUAL_size_t(2, WsCommandRouter::getHandlerCount());
}

void test_max_handlers_capacity() {
    TEST_ASSERT_EQUAL_size_t(192, WsCommandRouter::getMaxHandlers());
}

void test_registration_at_capacity_is_dropped() {
    // Fill to capacity (pointers all alias cmdBuf — fine for count test)
    char cmdBuf[32];
    for (size_t i = 0; i < WsCommandRouter::getMaxHandlers(); i++) {
        snprintf(cmdBuf, sizeof(cmdBuf), "cap.%zu", i);
        WsCommandRouter::registerCommand(cmdBuf, handler_alpha);
    }
    TEST_ASSERT_EQUAL_size_t(WsCommandRouter::getMaxHandlers(),
                             WsCommandRouter::getHandlerCount());

    // One more should be silently dropped
    WsCommandRouter::registerCommand("cap.overflow", handler_beta);
    TEST_ASSERT_EQUAL_size_t(WsCommandRouter::getMaxHandlers(),
                             WsCommandRouter::getHandlerCount());
}

// ===========================================================================
// Routing tests
// ===========================================================================

void test_route_dispatches_to_correct_handler() {
    WsCommandRouter::registerCommand("test.alpha", handler_alpha);
    WsCommandRouter::registerCommand("test.beta", handler_beta);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "test.beta";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT(1, g_handlerCallCount);
    TEST_ASSERT_EQUAL_STRING("beta", g_lastHandlerName);
}

void test_route_first_registered_handler() {
    WsCommandRouter::registerCommand("test.alpha", handler_alpha);
    WsCommandRouter::registerCommand("test.beta", handler_beta);
    WsCommandRouter::registerCommand("test.gamma", handler_gamma);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "test.alpha";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("alpha", g_lastHandlerName);
}

void test_route_last_registered_handler() {
    WsCommandRouter::registerCommand("test.alpha", handler_alpha);
    WsCommandRouter::registerCommand("test.beta", handler_beta);
    WsCommandRouter::registerCommand("test.gamma", handler_gamma);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "test.gamma";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("gamma", g_lastHandlerName);
}

void test_route_strips_type_before_handler() {
    g_typeFieldPresent = true;
    WsCommandRouter::registerCommand("test.strip", handler_check_type_stripped);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "test.strip";
    doc["payload"] = "data";

    auto ctx = makeDummyContext();
    WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_EQUAL_INT(1, g_handlerCallCount);
    TEST_ASSERT_FALSE(g_typeFieldPresent);
}

// ===========================================================================
// Pre-filter optimisation tests
// ===========================================================================

void test_route_similar_prefix_different_length() {
    WsCommandRouter::registerCommand("device.status", handler_alpha);
    WsCommandRouter::registerCommand("device.statusExtended", handler_beta);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "device.status";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("alpha", g_lastHandlerName);
}

void test_route_same_length_different_first_char() {
    WsCommandRouter::registerCommand("audio.start", handler_alpha);
    WsCommandRouter::registerCommand("zones.start", handler_beta);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "zones.start";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("beta", g_lastHandlerName);
}

// ===========================================================================
// Error handling tests
// ===========================================================================

void test_route_unknown_command_returns_false() {
    WsCommandRouter::registerCommand("test.alpha", handler_alpha);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "nonexistent.command";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(0, g_handlerCallCount);
    // Verify error message was sent to client
    TEST_ASSERT_TRUE(client.m_lastMessage.find("Unknown command") != std::string::npos);
}

void test_route_missing_type_returns_false() {
    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["data"] = "some_value";  // No "type" field

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(0, g_handlerCallCount);
    // Verify error message mentions missing field
    TEST_ASSERT_TRUE(client.m_lastMessage.find("Missing") != std::string::npos);
}

void test_route_empty_type_returns_false() {
    WsCommandRouter::registerCommand("test.alpha", handler_alpha);

    AsyncWebSocketClient client;
    JsonDocument doc;
    doc["type"] = "";

    auto ctx = makeDummyContext();
    bool result = WsCommandRouter::route(&client, doc, ctx);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(0, g_handlerCallCount);
}

// ===========================================================================
// Main
// ===========================================================================

int main(void) {
    UNITY_BEGIN();

    // Registration
    RUN_TEST(test_initial_handler_count_is_zero);
    RUN_TEST(test_registration_increases_count);
    RUN_TEST(test_max_handlers_capacity);
    RUN_TEST(test_registration_at_capacity_is_dropped);

    // Routing
    RUN_TEST(test_route_dispatches_to_correct_handler);
    RUN_TEST(test_route_first_registered_handler);
    RUN_TEST(test_route_last_registered_handler);
    RUN_TEST(test_route_strips_type_before_handler);

    // Pre-filter
    RUN_TEST(test_route_similar_prefix_different_length);
    RUN_TEST(test_route_same_length_different_first_char);

    // Error handling
    RUN_TEST(test_route_unknown_command_returns_false);
    RUN_TEST(test_route_missing_type_returns_false);
    RUN_TEST(test_route_empty_type_returns_false);

    return UNITY_END();
}

#endif // NATIVE_BUILD
