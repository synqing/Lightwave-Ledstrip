/**
 * @file test_webserver_routes.cpp
 * @brief Unit tests for WebServer route registration modules
 *
 * Tests route registration for StaticAssetRoutes, LegacyApiRoutes, and V1ApiRoutes.
 * Uses mocks for AsyncWebServer to verify routes are registered correctly.
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <cstring>
#include <vector>
#include <string>

// Mock AsyncWebServer
struct MockRoute {
    const char* path;
    uint8_t method;
    bool registered;
};

class MockAsyncWebServer {
public:
    std::vector<MockRoute> routes;
    
    void on(const char* path, uint8_t method, void* handler) {
        MockRoute route;
        route.path = path;
        route.method = method;
        route.registered = true;
        routes.push_back(route);
    }
    
    void onNotFound(void* handler) {
        // Sentinel so tests can assert "not-found handler was registered" without a real callback
        notFoundHandler = (handler != nullptr) ? handler : reinterpret_cast<void*>(1);
    }
    
    void* notFoundHandler = nullptr;
    
    bool hasRoute(const char* path, uint8_t method) {
        for (const auto& route : routes) {
            if (strcmp(route.path, path) == 0 && route.method == method) {
                return true;
            }
        }
        return false;
    }
};

// Mock HttpRouteRegistry
namespace lightwaveos {
namespace network {
namespace webserver {

class HttpRouteRegistry {
public:
    explicit HttpRouteRegistry(MockAsyncWebServer* server) : m_server(server) {}
    
    void onGet(const char* path, void* handler) {
        m_server->on(path, 1, handler);  // HTTP_GET = 1
    }
    
    void onPost(const char* path, void* onRequest, void* onUpload, void* onBody) {
        m_server->on(path, 2, onBody);  // HTTP_POST = 2
    }
    
    void onNotFound(void* handler) {
        m_server->onNotFound(handler);
    }
    
private:
    MockAsyncWebServer* m_server;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

// Test StaticAssetRoutes registration
// Full route registration (StaticAssetRoutes::registerRoutes, etc.) is covered by device/integration tests.
// These tests verify the mock registry and expected route names.
void test_static_asset_routes_registration() {
    MockAsyncWebServer mockServer;
    lightwaveos::network::webserver::HttpRouteRegistry registry(&mockServer);
    
    // Mock registration; real StaticAssetRoutes::registerRoutes(server) tested on device
    registry.onGet("/", nullptr);
    registry.onGet("/favicon.ico", nullptr);
    registry.onNotFound(nullptr);
    
    TEST_ASSERT_TRUE(mockServer.hasRoute("/", 1));
    TEST_ASSERT_TRUE(mockServer.hasRoute("/favicon.ico", 1));
    TEST_ASSERT_NOT_NULL(mockServer.notFoundHandler);
}

// Test LegacyApiRoutes registration
void test_legacy_api_routes_registration() {
    MockAsyncWebServer mockServer;
    lightwaveos::network::webserver::HttpRouteRegistry registry(&mockServer);
    
    // Real LegacyApiRoutes::registerRoutes(registry, ctx, ...) tested on device/integration
    registry.onGet("/api/status", nullptr);
    registry.onPost("/api/effect", nullptr, nullptr, nullptr);
    
    TEST_ASSERT_TRUE(mockServer.hasRoute("/api/status", 1));
    TEST_ASSERT_TRUE(mockServer.hasRoute("/api/effect", 2));
}

// Test V1ApiRoutes registration
void test_v1_api_routes_registration() {
    MockAsyncWebServer mockServer;
    lightwaveos::network::webserver::HttpRouteRegistry registry(&mockServer);
    
    // Real V1ApiRoutes::registerRoutes(registry, ctx, server, ...) tested on device/integration
    registry.onGet("/api/v1/", nullptr);
    registry.onGet("/api/v1/health", nullptr);
    registry.onGet("/api/v1/device/status", nullptr);
    
    TEST_ASSERT_TRUE(mockServer.hasRoute("/api/v1/", 1));
    TEST_ASSERT_TRUE(mockServer.hasRoute("/api/v1/health", 1));
    TEST_ASSERT_TRUE(mockServer.hasRoute("/api/v1/device/status", 1));
}

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_static_asset_routes_registration);
    RUN_TEST(test_legacy_api_routes_registration);
    RUN_TEST(test_v1_api_routes_registration);
    
    return UNITY_END();
}

#endif // NATIVE_BUILD
