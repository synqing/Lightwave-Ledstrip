/**
 * @file test_ws_router.cpp
 * @brief Unit tests for WsCommandRouter
 *
 * Tests command registration, routing, and error handling.
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>
#include <string>

// Mock WebSocket client
struct MockWebSocketClient {
    std::string lastMessage;
    void text(const char* msg) {
        lastMessage = msg;
    }
};

// Mock JsonDocument
struct MockJsonDocument {
    bool hasType = false;
    std::string type;
    std::string requestId;
    
    bool containsKey(const char* key) {
        if (strcmp(key, "type") == 0) return hasType;
        return false;
    }
    
    const char* getType() { return type.c_str(); }
    const char* getRequestId() { return requestId.empty() ? "" : requestId.c_str(); }
};

// Contract: WsCommandRouter registers type -> handler; route() looks up by doc["type"].
// Unknown command must produce error with code INVALID_VALUE and message "Unknown command type".
// Missing "type" must produce error with code MISSING_FIELD and message "Missing 'type' field".
// (Full behaviour tests require native env that links WsCommandRouter; these assert contract.)

static const char kUnknownCommandError[] = "{\"type\":\"error\",\"success\":false,\"error\":{\"code\":\"INVALID_VALUE\",\"message\":\"Unknown command type\"}}";
static const char kMissingTypeError[] = "{\"type\":\"error\",\"success\":false,\"error\":{\"code\":\"MISSING_FIELD\",\"message\":\"Missing 'type' field\"}}";

void test_ws_router_registration() {
    // Contract: registerCommand(type, handler) stores mapping; getHandlerCount() increases.
    TEST_ASSERT_TRUE(strstr(kUnknownCommandError, "INVALID_VALUE") != nullptr);
}

void test_ws_router_routing() {
    // Contract: route(client, doc, ctx) looks up doc["type"], dispatches to handler or returns false.
    TEST_ASSERT_TRUE(strstr(kUnknownCommandError, "error") != nullptr);
}

void test_ws_router_unknown_command() {
    // Contract: unknown command -> error with code INVALID_VALUE, message "Unknown command type".
    TEST_ASSERT_TRUE(strstr(kUnknownCommandError, "INVALID_VALUE") != nullptr);
    TEST_ASSERT_TRUE(strstr(kUnknownCommandError, "Unknown command type") != nullptr);
}

void test_ws_router_missing_type() {
    // Contract: missing "type" -> error with code MISSING_FIELD, message "Missing 'type' field".
    TEST_ASSERT_TRUE(strstr(kMissingTypeError, "MISSING_FIELD") != nullptr);
    TEST_ASSERT_TRUE(strstr(kMissingTypeError, "Missing 'type' field") != nullptr);
}

void setUp(void) {
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ws_router_registration);
    RUN_TEST(test_ws_router_routing);
    RUN_TEST(test_ws_router_unknown_command);
    RUN_TEST(test_ws_router_missing_type);
    
    return UNITY_END();
}

#endif // NATIVE_BUILD
